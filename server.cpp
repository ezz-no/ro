//
// Created by ezzno on 2025/9/1.
//
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "server.h"
#include "executor.h"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

// 处理单个HTTP请求并发送响应
class Session : public std::enable_shared_from_this<Session>
{
    tcp::socket socket_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    http::response<http::string_body> res_; // <--- 放在成员里
    unsigned short port_;  // 记录当前连接的端口
    const std::unordered_map<std::string, std::unique_ptr<APINode>>& apis_;
    net::thread_pool& thread_pool_;

public:
    // 构造函数，获取socket和端口号
    Session(tcp::socket socket, unsigned short port,
        const std::unordered_map<std::string, std::unique_ptr<APINode>>& apis, net::thread_pool& thread_pool)
        : socket_(std::move(socket)), port_(port), apis_(apis), thread_pool_(thread_pool) {}

    // 开始处理会话
    void run()
    {
        // 确保对象在操作完成前不会被销毁
        auto self(shared_from_this());

        // 异步读取请求
        http::async_read(socket_, buffer_, req_,
            [self](beast::error_code ec, std::size_t bytes_transferred)
            {
                boost::ignore_unused(bytes_transferred);
                if(!ec)
                    self->handle_request();
            }
        );
    }

private:
    // 处理请求并发送响应
    void handle_request()
    {
        // 输出连接信息
        std::cout << "Received request on port " << port_
                  << " for " << req_.target() << std::endl;

        res_ = http::response<http::string_body>{http::status::ok, req_.version()};
        res_.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res_.set(http::field::content_type, "application/json; charset=utf-8");

        auto self(shared_from_this());

        // 使用线程池执行（替代std::thread，减少线程创建开销）
        net::post(thread_pool_, [self]() {
            auto it = self->apis_.find(self->req_.target());
            if (it != self->apis_.end())
            {
                Executor executor;
                self->res_.body() = value_to_string(executor.execute_api(it->second.get()));
            }
            else
            {
                self->res_.result(http::status::not_found);
                self->res_.body() = "Not Found (on port " + std::to_string(self->port_) + ")";
            }

            self->res_.prepare_payload();
            self->res_.keep_alive(self->req_.keep_alive());

            // 异步发送响应
            http::async_write(self->socket_, self->res_,
                [self](beast::error_code ec, std::size_t bytes_transferred)
                {
                    boost::ignore_unused(bytes_transferred);
                    if (ec) {
                        // 处理错误（如客户端断开连接）
                        return;
                    }

                    // 发送完成后关闭socket
                    beast::error_code ec_close;
                    self->socket_.shutdown(tcp::socket::shutdown_send, ec_close);
                }
            );
        });
    }
};

void Listener::run()
{
    do_accept();
}

void Listener::do_accept()
{
    // 异步接受新连接
    auto self = shared_from_this();
    acceptor_.async_accept(
        net::make_strand(ioc_),
        [self](beast::error_code ec, tcp::socket socket) {
            if (!self->acceptor_.is_open()) {
                return; // 接受器已关闭，直接返回
            }
            self->on_accept(ec, std::move(socket));
        }
    );
}

void Listener::on_accept(beast::error_code ec, tcp::socket socket)
{
    if(ec)
    {
        fail(ec, ("accept (port " + std::to_string(port_) + ")").c_str());
    }
    else
    {
        // 创建新会话并运行，传递端口号
        std::make_shared<Session>(std::move(socket), port_, this->get_apis(), http_thread_pool_)->run();
    }

    // 接受下一个连接
    do_accept();
}

// 错误处理
void Listener::fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}
