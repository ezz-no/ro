#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/url.hpp>  // 用于解析URL（需要Boost 1.76+）
#include <iostream>

#include "json.hpp"
#include "executor.h"
#include "parser.h"
#include "server.h"

namespace urls = boost::urls; // 从<boost/url.hpp>
using json = nlohmann::json;  // 简化类型名

// 发送 HTTP GET 请求
std::string http_get(const std::string& url) {
    try {
        // 1. 解析URL（提取主机、端口、路径等信息）
        urls::url parsed_url(url);
        if (!parsed_url.has_scheme()) {
            throw std::invalid_argument("无效的URL格式：" + url);
        }

        // 提取URL中的关键信息
        std::string host = parsed_url.host();                  // 主机名（如localhost）
        std::string target = parsed_url.encoded_target().decode();      // 路径（如/hello）
        unsigned short port; // 端口（如8015）
        if (!parsed_url.port().empty()) {
            port = std::stoi(parsed_url.port());
        }
        if (port == 0) { // 如果URL未指定端口，使用默认端口（HTTP默认80）
            port = 80;  // HTTP默认端口
        }

        // 2. 初始化Boost.Asio的IO上下文（事件循环）
        net::io_context ioc;

        // 3. 解析域名获取服务器地址
        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(host, std::to_string(port));

        // 4. 建立TCP连接
        beast::tcp_stream stream(ioc);
        stream.connect(results);

        // 5. 构造HTTP GET请求
        http::request<http::string_body> req{http::verb::get, target, 11};  // HTTP/1.1
        req.set(http::field::host, host);                                   // 设置Host头（必须）
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);       // 设置User-Agent

        // 6. 发送请求到服务器
        http::write(stream, req);

        // 7. 接收服务器响应
        beast::flat_buffer buffer;                // 用于存储响应数据的缓冲区
        http::response<http::string_body> res;    // 用于存储响应的对象
        http::read(stream, buffer, res); // 读取响应

        // 8. 优雅关闭连接（忽略"未连接"的错误，因为可能服务器已主动关闭）
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        if (ec && ec != beast::errc::not_connected) {
            throw beast::system_error{ec};
        }

        // 9. 返回响应体（服务器返回的数据）
        return res.body();

    } catch (const std::exception& e) {
        // 捕获并输出所有错误（如连接失败、URL无效等）
        std::cerr << "请求失败: " << e.what() << std::endl;
        return "";
    }
}

// 辅助函数：获取值的类型名称
static std::string get_type_name(const Value& val) {
    if (std::holds_alternative<int>(val)) return "int";
    if (std::holds_alternative<float>(val)) return "float";
    if (std::holds_alternative<std::string>(val)) return "string";
    if (std::holds_alternative<bool>(val)) return "bool";
    return "unknown";
}

// 辅助函数：检查值是否为指定类型
template<typename T>
static bool is_type(const Value& val) {
    return std::holds_alternative<T>(val);
}

// 辅助函数：安全地获取指定类型的值
template<typename T>
static const T& get_value(const Value& val) {
    if (!is_type<T>(val)) {
        throw ExecutionError("Type mismatch: expected " + std::string(typeid(T).name()) +
                            ", got " + get_type_name(val));
    }
    return std::get<T>(val);
}

// 辅助函数：获取数组元素
static std::vector<Value>& cast_to_array(const Value& array_val) {
    if (!is_type<ComplexValue>(array_val)) {
        throw ExecutionError("Array access on non-array type");
    }

    ComplexValue val_ptr = get_value<ComplexValue>(array_val);
    if (val_ptr.first != 1) {
        throw ExecutionError("Array access on non-array type");
    }

    return *static_cast<std::vector<Value>*>(val_ptr.second);  // 返回引用
}

Value Executor::evaluate_address_index(const ExprNode* expr) {
    if (!expr) {
        throw ExecutionError("Null expression");
    }

    switch (expr->op_type) {
        case ExprNode::OpType::CONSTANT_INT:
            return std::stoi(expr->value);

        case ExprNode::OpType::IDENTIFIER: {
            return expr->value;
        }
        default: ;
    }

    throw ExecutionError("unexpected op type in address index");
}

// 辅助函数：获取数组元素
static Value get_array_element(const Value& array_val, size_t index) {
    if (!is_type<ComplexValue>(array_val)) {
        throw ExecutionError("Array access on non-array type");
    }

    ComplexValue val_ptr = get_value<ComplexValue>(array_val);
    if (val_ptr.first != 1) {
        throw ExecutionError("Array access on non-array type");
    }

    auto array = *static_cast<Values*>(val_ptr.second);

    if (index >= array.size()) {
        throw ExecutionError("Array index out of bounds: " + std::to_string(index) +
                            " (array size: " + std::to_string(array.size()) + ")");
    }

    return array[index];
}

static Value get_object_field(const Value& object_val, std::string index) {
    if (!is_type<ComplexValue>(object_val)) {
        throw ExecutionError("Field access on non-object type");
    }

    ComplexValue val_ptr = get_value<ComplexValue>(object_val);
    if (val_ptr.first != 2) {
        throw ExecutionError("Field access on non-object type");
    }

    auto obj = *static_cast<ValueMap*>(val_ptr.second);

    return obj[index];
}

std::string Executor::value_to_string(const Value& val) const {
    if (std::holds_alternative<int>(val)) {
        return std::to_string(std::get<int>(val));
    } else if (std::holds_alternative<float>(val)) {
        return std::to_string(std::get<float>(val));
    } else if (std::holds_alternative<std::string>(val)) {
        return std::get<std::string>(val);
    } else if (std::holds_alternative<bool>(val)) {
        return std::get<bool>(val) ? "true" : "false";
    }
    return "unknown";
}

Value Executor::evaluate_expression(const ExprNode* expr) {
    if (!expr) {
        throw ExecutionError("Null expression");
    }

    switch (expr->op_type) {
        case ExprNode::OpType::CONSTANT_INT:
            return std::stoi(expr->value);

        case ExprNode::OpType::CONSTANT_FLOAT:
            return std::stof(expr->value);

        case ExprNode::OpType::CONSTANT_STRING:
            return expr->value;

        case ExprNode::OpType::IDENTIFIER: {
            auto it = variables.find(expr->value);
            if (it == variables.end()) {
                // todo: reference ??
                return 0;
                //return 0;
                throw ExecutionError("Undefined variable: " + expr->value);
            }
            return it->second;
        }

        case ExprNode::OpType::ADD: {
            Value left_val = evaluate_expression(expr->left.get());
            Value right_val = evaluate_expression(expr->right.get());

            if (is_type<int>(left_val) && is_type<int>(right_val)) {
                return get_value<int>(left_val) + get_value<int>(right_val);
            } else if (is_type<float>(left_val) || is_type<float>(right_val)) {
                float l = is_type<int>(left_val) ? get_value<int>(left_val) : get_value<float>(left_val);
                float r = is_type<int>(right_val) ? get_value<int>(right_val) : get_value<float>(right_val);
                return l + r;
            } else if (is_type<std::string>(left_val) && is_type<std::string>(right_val)) {
                return get_value<std::string>(left_val) + get_value<std::string>(right_val);
            }

            throw ExecutionError("Addition not supported for types: " +
                                get_type_name(left_val) + " and " + get_type_name(right_val));
        }

        case ExprNode::OpType::SUB: {
            Value left_val = evaluate_expression(expr->left.get());
            Value right_val = evaluate_expression(expr->right.get());

            if (is_type<int>(left_val) && is_type<int>(right_val)) {
                return get_value<int>(left_val) - get_value<int>(right_val);
            } else if (is_type<float>(left_val) || is_type<float>(right_val)) {
                float l = is_type<int>(left_val) ? get_value<int>(left_val) : get_value<float>(left_val);
                float r = is_type<int>(right_val) ? get_value<int>(right_val) : get_value<float>(right_val);
                return l - r;
            }

            throw ExecutionError("Subtraction not supported for types: " +
                                get_type_name(left_val) + " and " + get_type_name(right_val));
        }

        case ExprNode::OpType::MUL: {
            Value left_val = evaluate_expression(expr->left.get());
            Value right_val = evaluate_expression(expr->right.get());

            if (is_type<int>(left_val) && is_type<int>(right_val)) {
                return get_value<int>(left_val) * get_value<int>(right_val);
            } else if (is_type<float>(left_val) || is_type<float>(right_val)) {
                float l = is_type<int>(left_val) ? get_value<int>(left_val) : get_value<float>(left_val);
                float r = is_type<int>(right_val) ? get_value<int>(right_val) : get_value<float>(right_val);
                return l * r;
            }

            throw ExecutionError("Multiplication not supported for types: " +
                                get_type_name(left_val) + " and " + get_type_name(right_val));
        }

        case ExprNode::OpType::DIV: {
            Value left_val = evaluate_expression(expr->left.get());
            Value right_val = evaluate_expression(expr->right.get());

            if (is_type<int>(left_val) && is_type<int>(right_val)) {
                int r = get_value<int>(right_val);
                if (r == 0) throw ExecutionError("Division by zero");
                return get_value<int>(left_val) / r;
            } else if (is_type<float>(left_val) || is_type<float>(right_val)) {
                float l = is_type<int>(left_val) ? get_value<int>(left_val) : get_value<float>(left_val);
                float r = is_type<int>(right_val) ? get_value<int>(right_val) : get_value<float>(right_val);
                if (r == 0.0f) throw ExecutionError("Division by zero");
                return l / r;
            }

            throw ExecutionError("Division not supported for types: " +
                                get_type_name(left_val) + " and " + get_type_name(right_val));
        }

        case ExprNode::OpType::EQ: {
            Value left_val = evaluate_expression(expr->left.get());
            Value right_val = evaluate_expression(expr->right.get());
            return left_val == right_val;
        }

        case ExprNode::OpType::NEQ: {
            Value left_val = evaluate_expression(expr->left.get());
            Value right_val = evaluate_expression(expr->right.get());
            return left_val != right_val;
        }

        case ExprNode::OpType::LT: {
            Value left_val = evaluate_expression(expr->left.get());
            Value right_val = evaluate_expression(expr->right.get());

            if (is_type<int>(left_val) && is_type<int>(right_val)) {
                return get_value<int>(left_val) < get_value<int>(right_val);
            } else if (is_type<float>(left_val) || is_type<float>(right_val)) {
                float l = is_type<int>(left_val) ? get_value<int>(left_val) : get_value<float>(left_val);
                float r = is_type<int>(right_val) ? get_value<int>(right_val) : get_value<float>(right_val);
                return l < r;
            } else if (is_type<std::string>(left_val) && is_type<std::string>(right_val)) {
                return get_value<std::string>(left_val) < get_value<std::string>(right_val);
            }

            throw ExecutionError("Less than comparison not supported for types: " +
                                get_type_name(left_val) + " and " + get_type_name(right_val));
        }

        case ExprNode::OpType::GT: {
            Value left_val = evaluate_expression(expr->left.get());
            Value right_val = evaluate_expression(expr->right.get());

            if (is_type<int>(left_val) && is_type<int>(right_val)) {
                return get_value<int>(left_val) > get_value<int>(right_val);
            } else if (is_type<float>(left_val) || is_type<float>(right_val)) {
                float l = is_type<int>(left_val) ? get_value<int>(left_val) : get_value<float>(left_val);
                float r = is_type<int>(right_val) ? get_value<int>(right_val) : get_value<float>(right_val);
                return l > r;
            } else if (is_type<std::string>(left_val) && is_type<std::string>(right_val)) {
                return get_value<std::string>(left_val) > get_value<std::string>(right_val);
            }

            throw ExecutionError("Greater than comparison not supported for types: " +
                                get_type_name(left_val) + " and " + get_type_name(right_val));
        }

        case ExprNode::OpType::LE: {
            Value left_val = evaluate_expression(expr->left.get());
            Value right_val = evaluate_expression(expr->right.get());

            if (is_type<int>(left_val) && is_type<int>(right_val)) {
                return get_value<int>(left_val) <= get_value<int>(right_val);
            } else if (is_type<float>(left_val) || is_type<float>(right_val)) {
                float l = is_type<int>(left_val) ? get_value<int>(left_val) : get_value<float>(left_val);
                float r = is_type<int>(right_val) ? get_value<int>(right_val) : get_value<float>(right_val);
                return l <= r;
            } else if (is_type<std::string>(left_val) && is_type<std::string>(right_val)) {
                return get_value<std::string>(left_val) <= get_value<std::string>(right_val);
            }

            throw ExecutionError("Less than or equal comparison not supported for types: " +
                                get_type_name(left_val) + " and " + get_type_name(right_val));
        }

        case ExprNode::OpType::GE: {
            Value left_val = evaluate_expression(expr->left.get());
            Value right_val = evaluate_expression(expr->right.get());

            if (is_type<int>(left_val) && is_type<int>(right_val)) {
                return get_value<int>(left_val) >= get_value<int>(right_val);
            } else if (is_type<float>(left_val) || is_type<float>(right_val)) {
                float l = is_type<int>(left_val) ? get_value<int>(left_val) : get_value<float>(left_val);
                float r = is_type<int>(right_val) ? get_value<int>(right_val) : get_value<float>(right_val);
                return l >= r;
            } else if (is_type<std::string>(left_val) && is_type<std::string>(right_val)) {
                return get_value<std::string>(left_val) >= get_value<std::string>(right_val);
            }

            throw ExecutionError("Greater than or equal comparison not supported for types: " +
                                get_type_name(left_val) + " and " + get_type_name(right_val));
        }

        case ExprNode::OpType::AND: {
            Value left_val = evaluate_expression(expr->left.get());
            Value right_val = evaluate_expression(expr->right.get());

            if (is_type<bool>(left_val) && is_type<bool>(right_val)) {
                return get_value<bool>(left_val) && get_value<bool>(right_val);
            }

            throw ExecutionError("Logical AND not supported for types: " +
                                get_type_name(left_val) + " and " + get_type_name(right_val));
        }

        case ExprNode::OpType::OR: {
            Value left_val = evaluate_expression(expr->left.get());
            Value right_val = evaluate_expression(expr->right.get());

            if (is_type<bool>(left_val) && is_type<bool>(right_val)) {
                return get_value<bool>(left_val) || get_value<bool>(right_val);
            }

            throw ExecutionError("Logical OR not supported for types: " +
                                get_type_name(left_val) + " and " + get_type_name(right_val));
        }

        case ExprNode::OpType::NOT: {
            Value val = evaluate_expression(expr->left.get());

            if (is_type<bool>(val)) {
                return !get_value<bool>(val);
            }

            throw ExecutionError("Logical NOT not supported for type: " + get_type_name(val));
        }

        case ExprNode::OpType::ASSIGN: {
            if (!expr->left || expr->left->op_type != ExprNode::OpType::IDENTIFIER) {
                throw ExecutionError("Invalid assignment target");
            }

            std::string var_name = expr->left->value;
            Value val = evaluate_expression(expr->right.get());
            variables[var_name] = val;
            return val;
        }

            // 处理数组字面量
        case ExprNode::OpType::ARRAY_LITERAL: {
            // 在堆上创建数组（手动分配，不会自动销毁）
            auto* array = new std::vector<Value>();
            for (const auto& elem_node : expr->array_elements) {
                array->push_back(evaluate_expression(elem_node.get()));
            }

            // 存入全局容器，确保生命周期
            //g_all_arrays.push_back(array);

            // 转换为void*返回
            return std::make_pair(1, array);
        }

        case ExprNode::OpType::ARRAY_ACCESS: { // 处理数组访问
            if (!expr->left || !expr->right) {
                throw ExecutionError("Invalid array access expression");
            }

            // 获取数组
            Value array_val = evaluate_expression(expr->left.get());

            // 获取索引（必须是整数）
            Value index_val = evaluate_expression(expr->right.get());
            if (!is_type<int>(index_val)) {
                throw ExecutionError("Array index must be an integer");
            }

            int index = get_value<int>(index_val);
            if (index < 0) {
                throw ExecutionError("Negative array index: " + std::to_string(index));
            }

            return get_array_element(array_val, static_cast<size_t>(index));
        }

        case ExprNode::OpType::DOT: {
            if (!expr->left || !expr->right) {
                throw ExecutionError("Invalid array access expression");
            }

            Value obj_val = evaluate_expression(expr->left.get());
            Value index_val = evaluate_address_index(expr->right.get());
            if (is_type<int>(index_val)) {
                int index = get_value<int>(index_val);
                if (index < 0) {
                    return NULL_VALUE;
                }
                return get_array_element(obj_val, static_cast<size_t>(index));
            }

            if (is_type<std::string>(index_val)) {
                std::string index = get_value<std::string>(index_val);
                return get_object_field(obj_val, index);
            }

            throw ExecutionError("Index value must be int or string");
        }

        case ExprNode::OpType::OBJECT_LITERAL: {
            auto* val = new std::unordered_map<std::string, Value>();
            for (const auto& [key, value] : expr->object_members) {
                (*val)[key] = evaluate_expression(value.get());
            }
            return std::make_pair(2, val);
        }

        case ExprNode::OpType::CURL: {
            if (!expr->left || !expr->right) {
                throw ExecutionError("Invalid curl expression");
            }
            if (!expr->left || expr->left->op_type != ExprNode::OpType::IDENTIFIER) {
                throw ExecutionError("Invalid assignment target");
            }

            Value data_val = evaluate_expression(expr->left.get());

            Value url_val = evaluate_expression(expr->right.get());
            if (!is_type<std::string>(url_val)) {
                throw ExecutionError("curl path must be a string");
            }

            std::string url = get_value<std::string>(url_val);
            std::string ret = http_get(url);

            try {
                // 核心：将字符串解析为 json 对象（decode 过程）
                json j = json::parse(ret);
                auto jval = json_to_value(j);
                variables[expr->left->value] = jval;
                return jval;
            } catch (const json::parse_error& e) {
                return 0;
            }
        }

        default:
            throw ExecutionError("Unsupported expression: " + expr->to_string());
    }
}

void Executor::execute_statement(const StmtNode* stmt) {
    if (!stmt) {
        return;
    }

    switch (stmt->stmt_type) {
        case StmtNode::StmtType::EXPRESSION: {
            if (stmt->expr) {
                evaluate_expression(stmt->expr.get());
            }
            break;
        }

        case StmtNode::StmtType::BLOCK: {
            for (const auto& child : stmt->children) {
                execute_statement(child.get());
                if (return_) {
                    return_ = false;
                    break;
                }
            }
            break;
        }

        case StmtNode::StmtType::IF: {
            if (!stmt->condition) {
                throw ExecutionError("If statement missing condition");
            }

            Value cond_val = evaluate_expression(stmt->condition.get());
            if (!is_type<bool>(cond_val)) {
                throw ExecutionError("If condition must be a boolean");
            }

            if (get_value<bool>(cond_val)) {
                if (!stmt->children.empty()) {
                    execute_statement(stmt->children[0].get());
                }
            } else if (stmt->children.size() >= 2) {
                execute_statement(stmt->children[1].get());
            }
            break;
        }

        case StmtNode::StmtType::WHILE: {
            if (!stmt->condition) {
                throw ExecutionError("While statement missing condition");
            }

            while (true) {
                Value cond_val = evaluate_expression(stmt->condition.get());
                if (!is_type<bool>(cond_val) || !get_value<bool>(cond_val)) {
                    break;
                }

                if (!stmt->children.empty()) {
                    execute_statement(stmt->children[0].get());
                }
            }
            break;
        }

        case StmtNode::StmtType::FOR: {
            // 执行初始化语句
            if (stmt->children.size() >= 1 && stmt->children[0]) {
                execute_statement(stmt->children[0].get());
            }

            // 循环条件检查和执行循环体
            while (true) {
                // 检查循环条件
                if (stmt->condition) {
                    Value cond_val = evaluate_expression(stmt->condition.get());
                    if (!is_type<bool>(cond_val) || !get_value<bool>(cond_val)) {
                        break;
                    }
                } else {
                    // 没有条件的for循环是无限循环
                }

                // 执行循环体
                if (stmt->children.size() >= 2 && stmt->children[1]) {
                    execute_statement(stmt->children[1].get());
                }

                // 执行迭代语句
                if (stmt->children.size() >= 3 && stmt->children[2]) {
                    execute_statement(stmt->children[2].get());
                }
            }
            break;
        }

        case StmtNode::StmtType::RETURN: {
            if (stmt->expr) {
                result_ = evaluate_expression(stmt->expr.get());
                return_ = true;
            }
            break;
        }

        case StmtNode::StmtType::PRINT: {
            Values results;
            for (const auto& expr : stmt->exprs) {
                results.emplace_back(evaluate_expression(expr.get()));
            }

            for (auto i = 0; i < results.size(); ++i) {
                std::cout << value_to_string(results[i]);
            }
            std::cout << std::endl;
            break;
        }

        case StmtNode::StmtType::DECLARATION: {
            Value result = evaluate_expression(stmt->expr.get());
            break;
        }

        case StmtNode::StmtType::EACH: {
            const auto expr = stmt->expr.get();

            // 获取数组
            auto array_val = cast_to_array(variables[expr->value]);
            auto p0 = expr->parameters[0];
            auto p1 = expr->parameters[1];

            // 循环条件检查和执行循环体
            for (auto i = 0; i < array_val.size(); i++) {
                for (auto j = i + 1; j < array_val.size(); j++) {
                    variables[p0] = array_val[i];
                    variables[p1] = array_val[j];

                    // 检查循环条件
                    Value cond_val = evaluate_expression(stmt->condition.get());
                    if (!is_type<bool>(cond_val) || !get_value<bool>(cond_val)) {
                        continue;
                    }

                    // 执行循环体
                    execute_statement(stmt->children[0].get());
                }
            }
            break;
        }

        case StmtNode::StmtType::EMPTY:
            // 空语句，什么都不做
            break;

        default:
            throw ExecutionError("Unsupported statement: " + stmt->to_string());
    }
}

Value Executor::execute_function(const FuncNode* func, const std::vector<Value>& args) {
    if (!func) {
        throw ExecutionError("Null function");
    }

    // 检查参数数量
    if (args.size() != func->parameters.size()) {
        throw ExecutionError("Function " + func->name + " expects " +
                            std::to_string(func->parameters.size()) + " arguments, got " +
                            std::to_string(args.size()));
    }

    // 保存当前变量状态，用于函数执行完毕后恢复
    auto prev_variables = variables;

    try {
        // 设置函数参数
        for (size_t i = 0; i < func->parameters.size(); ++i) {
            variables[func->parameters[i]] = args[i];
        }

        // 执行函数体
        if (func->body) {
            execute_statement(func->body.get());
        }

        // 简单实现：返回最后一个表达式的值（实际中应该处理return语句）
        return Value();
    } catch (...) {
        // 恢复变量状态并重新抛出异常
        variables = prev_variables;
        throw;
    }
}

Value Executor::execute_api(const APINode* api) {
    if (!api) {
        throw ExecutionError("Null api");
    }

    // 执行函数体
    execute_statement(api->body.get());

    // 简单实现：返回最后一个表达式的值（实际中应该处理return语句）
    return result_;
}

namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

void Executor::execute(const std::unique_ptr<ProgramNode>& program) {
    if (!program) {
        throw ExecutionError("Null program");
    }

    // 查找并执行main函数
    /*
    auto main_it = functions.find("main");
    if (main_it == functions.end()) {
        throw ExecutionError("No main function found");
    }

    std::cout << "Executing main function..." << std::endl;
    execute_function(main_it->second, {});*/

    // 要监听的端口列表
    std::unordered_map<int, std::unordered_map<std::string, std::unique_ptr<APINode>>> apisByPort;

    // 执行全局语句
    for (auto& api : program->apis) {
        std::cout << "listen :" << api->port << " " << api->path << std::endl;
        apisByPort[api->port][api->path] = std::move(api);
    }

    try
    {
        // 关键：添加容器存储 Listener 的 shared_ptr，确保生命周期
        std::vector<std::shared_ptr<Listener>> listeners;

        net::io_context ioc{3};  // 1 表示线程池大小（单线程）

        // 为每个端口创建并运行监听器
        for (auto& [port, apis] : apisByPort) {
            auto const address = net::ip::make_address("0.0.0.0");
            auto const endpoint = tcp::endpoint{address, static_cast<unsigned short>(port)};

            // 方案A：若需保留 apisByPort（传递 shared_ptr，不移动）
            auto listener = std::make_shared<Listener>(ioc, endpoint, std::move(apis));

            // 方案B：若无需保留 apisByPort（传递移动后的 map，原代码逻辑）
            // auto listener = std::make_shared<Listener>(ioc, endpoint, std::move(apis));

            listeners.push_back(listener);  // 保存到容器，防止销毁
            listener->run();  // 启动监听器
        }

        std::cout << "Servers started." << std::endl;

        // 运行IO服务（此时 listeners 持有 Listener，确保其存活）
        ioc.run();
    }
    catch (std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void Executor::print_variables() const {
    std::cout << "\nFinal Variables:" << std::endl;
    std::cout << "==========" << std::endl;
    for (const auto& [name, val] : variables) {
        std::cout << name << " = " << value_to_string(val) << " (" << get_type_name(val) << ")" << std::endl;
    }
}
