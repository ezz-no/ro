#ifndef GLUE_EXECUTOR_H
#define GLUE_EXECUTOR_H

#include <unordered_map>
#include <variant>
#include <stdexcept>
#include <string>

#include "json.hpp"
#include "parser.h"

#define NULL_VALUE 0

using json = nlohmann::json;

// 支持的数据类型
using ComplexValue = std::pair<int, void*>;
using Value = std::variant<int, float, std::string, bool, ComplexValue>;
using Values = std::vector<Value>;
using ValueMap = std::unordered_map<std::string, Value>;

// 向前声明转换函数
inline std::string value_to_string(const Value& value);
inline void to_json(json& j, const Values& vs);
inline void to_json(json& j, const ValueMap& vm);

// 自定义序列化函数，用于处理 Value variant
inline void to_json(json& j, const Value& v) {
    std::visit([&j](const auto& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, ComplexValue>) {
            switch (arg.first) {
                case 1: { // vector
                    auto vec = *reinterpret_cast<Values*>(arg.second);
                    to_json(j, vec);
                    break;
                }
                case 2: { // unorder_map
                    // 安全转换 void* → std::unordered_map<Value, Value>*
                    auto map = *reinterpret_cast<ValueMap*>(arg.second);
                    to_json(j, map);
                    break;
                }
            }
        } else {
            j = arg;
        }
    }, v);
}

// 自定义序列化函数，用于处理 Values vector
inline void to_json(json& j, const Values& vs) {
    j = json::array();
    for (const auto& v : vs) {
        json temp;
        to_json(temp, v);
        j.push_back(temp);
    }
}

// 自定义序列化函数，用于处理 ValueMap unordered_map
inline void to_json(json& j, const ValueMap& vm) {
    j = json::object();
    for (const auto& [key, value] : vm) {
        json temp;
        to_json(temp, value);
        j[key] = temp;
    }
}

// 自定义序列化函数，用于处理 ComplexValue
inline void to_json(json& j, const ComplexValue& cv) {
    switch (cv.first) {
        case 1: { // vector
            auto vec = *reinterpret_cast<Values*>(cv.second);
            to_json(j, vec);
            break;
        }
        case 2: { // unorder_map
            // 安全转换 void* → std::unordered_map<Value, Value>*
            auto map = *reinterpret_cast<ValueMap*>(cv.second);
            to_json(j, map);
            break;
        }
    }
}

inline std::string value_to_string(const Value& value) {
    json j;
    to_json(j, value);
    return j.dump(4);
}

// 向前声明转换函数
inline Value json_to_value(const nlohmann::json& j);
inline Values json_to_values(const nlohmann::json& j);
inline ValueMap json_to_value_map(const nlohmann::json& j);

// 核心转换函数：将json转换为Value
inline Value json_to_value(const nlohmann::json& j) {
    if (j.is_null()) {
        // 处理null：可根据需求映射为特定值（这里示例映射为int 0）
        return 0;
    }
    else if (j.is_boolean()) {
        // 布尔值 -> bool
        return j.get<bool>();
    }
    else if (j.is_number_integer()) {
        // 整数 -> int
        return j.get<int>();
    }
    else if (j.is_number_float()) {
        // 浮点数 -> float
        return static_cast<float>(j.get<double>());
    }
    else if (j.is_string()) {
        // 字符串 -> std::string
        return j.get<std::string>();
    }
    else if (j.is_array()) {
        // 数组 -> 特殊处理：这里示例将数组包装为ComplexValue（int标记+指针）
        // 注意：实际使用中需管理内存，避免内存泄漏
        Values* arr = new Values(json_to_values(j));
        return ComplexValue(1, arr);  // 用int=1标记这是数组类型
    }
    else if (j.is_object()) {
        // 对象 -> 特殊处理：包装为ComplexValue（int标记+指针）
        ValueMap* obj = new ValueMap(json_to_value_map(j));
        return ComplexValue(2, obj);  // 用int=2标记这是对象类型
    }
    else {
        throw std::invalid_argument("不支持的JSON类型");
    }
}

// 辅助函数：将json数组转换为Values（vector<Value>）
inline Values json_to_values(const nlohmann::json& j) {
    assert(j.is_array() && "输入必须是JSON数组");
    Values values;
    for (const auto& elem : j) {
        values.push_back(json_to_value(elem));
    }
    return values;
}

// 辅助函数：将json对象转换为ValueMap（unordered_map<string, Value>）
inline ValueMap json_to_value_map(const nlohmann::json& j) {
    assert(j.is_object() && "输入必须是JSON对象");
    ValueMap map;
    for (const auto& [key, value] : j.items()) {
        map[key] = json_to_value(value);
    }
    return map;
}

// 执行器类，用于解释执行AST
class Executor {
private:
    std::ostream& os;

    std::ostringstream oss;

    bool eval_ = false;

    bool return_ = false;

    // 结果存储
    Value result_;

    // 变量存储
    std::unordered_map<std::string, Value> variables;

    // 辅助函数：获取值的字符串表示
    [[nodiscard]] std::string value_to_string(const Value& val) const;

    Value evaluate_address_index(const ExprNode* expr);

    // 表达式求值
    Value evaluate_expression(const ExprNode* expr);

    // 语句执行
    void execute_statement(const StmtNode* stmt);

    // 执行函数
    Value execute_function(const FuncNode* func, const std::vector<Value>& args);
public:
    explicit Executor() : os(std::cout) {};

    explicit Executor(bool eval /* true */) : eval_(true), os(oss) {};

    // 执行整个程序
    void execute(const std::unique_ptr<ProgramNode>& program);

    Value execute_api(const APINode*);

    [[nodiscard]] Executor copy() const {
        Executor exe;
        exe.variables = this->variables;
        return exe;
    }

    // 打印当前变量状态
    void print_variables() const;

    std::string result() const {
        return oss.str();
    };
};

// 执行时异常
class ExecutionError : public std::runtime_error {
public:
    explicit ExecutionError(const std::string& message) : std::runtime_error(message) {}
};

inline Executor executor;

#endif // EXECUTOR_H
