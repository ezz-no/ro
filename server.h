//
// Created by ezzno on 2025/9/1.
//

#ifndef GLUE_SERVER_H
#define GLUE_SERVER_H

#include "executor.h"
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <memory>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

/**
 * 监听特定端口的类
 * 负责接受新连接并创建对应的Session处理
 */
class Listener : public std::enable_shared_from_this<Listener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    unsigned short port_;  // 监听的端口号
    std::unordered_map<std::string, std::unique_ptr<APINode>> apis;
    net::thread_pool http_thread_pool_;

public:
    /**
     * 构造函数
     * @param ioc IO上下文
     * @param endpoint 要监听的端点(地址+端口)
    */
    Listener(net::io_context& ioc, tcp::endpoint endpoint, std::unordered_map<std::string, std::unique_ptr<APINode>>&& apis)
        : ioc_(ioc), acceptor_(ioc), port_(endpoint.port()), apis(std::move(apis)), http_thread_pool_(4)
    {
        beast::error_code ec;

        // 打开 acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if(ec)
        {
            fail(ec, ("open (port " + std::to_string(port_) + ")").c_str());
            return;
        }

        // 允许地址重用
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if(ec)
        {
            fail(ec, ("set_option (port " + std::to_string(port_) + ")").c_str());
            return;
        }

        // 绑定到服务器地址
        acceptor_.bind(endpoint, ec);
        if(ec)
        {
            fail(ec, ("bind (port " + std::to_string(port_) + ")").c_str());
            return;
        }

        // 开始监听
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if(ec)
        {
            fail(ec, ("listen (port " + std::to_string(port_) + ")").c_str());
            return;
        }

        std::cout << "Listener started on port " << port_ << std::endl;
    }

    /**
     * 开始监听并接受连接
     */
    void run();

    // 提供给 Session 访问 apis 的接口（const，避免 Session 修改）
    const std::unordered_map<std::string, std::unique_ptr<APINode>>& get_apis() const
    {
        return apis;
    }

private:
    /**
     * 异步接受新连接
     */
    void do_accept();

    /**
     * 接受连接后的回调函数
     * @param ec 错误码
     * @param socket 新建立的socket
     */
    void on_accept(beast::error_code ec, tcp::socket socket);

    /**
     * 错误处理函数
     * @param ec 错误码
     * @param what 发生错误的操作描述
     */
    static void fail(beast::error_code ec, char const* what);
};

#endif //SERVER_H
