//
// Created by ezzno on 2025/8/31.
//

#include "parser.h"
#include <iostream>
#include <iomanip>
#include <execinfo.h>  // 非标准库，但但在Linux/macOS上普遍存在

// AST节点的to_string实现
// 完善的to_string方法，支持数组类型
std::string ExprNode::to_string(int indent) const {
    std::string ind(indent, ' ');
    std::string result = ind;

    switch (op_type) {
        case OpType::ADD: result += "ADD"; break;
        case OpType::SUB: result += "SUB"; break;
        case OpType::MUL: result += "MUL"; break;
        case OpType::DIV: result += "DIV"; break;
        case OpType::EQ: result += "EQ"; break;
        case OpType::NEQ: result += "NEQ"; break;
        case OpType::LT: result += "LT"; break;
        case OpType::GT: result += "GT"; break;
        case OpType::LE: result += "LE"; break;
        case OpType::GE: result += "GE"; break;
        case OpType::AND: result += "AND"; break;
        case OpType::OR: result += "OR"; break;
        case OpType::NOT: result += "NOT"; break;
        case OpType::ASSIGN: result += "ASSIGN"; break;
        case OpType::CONSTANT_INT: result += "CONSTANT_INT(" + value + ")"; break;
        case OpType::CONSTANT_FLOAT: result += "CONSTANT_FLOAT(" + value + ")"; break;
        case OpType::CONSTANT_STRING: result += "CONSTANT_STRING(" + value + ")"; break;
        case OpType::IDENTIFIER: result += "IDENTIFIER(" + value + ")"; break;
        // 新增数组类型的字符串表示
        case OpType::ARRAY_LITERAL: {
            result += "ARRAY_LITERAL[\n";
            for (size_t i = 0; i < array_elements.size(); ++i) {
                result += array_elements[i]->to_string(indent + 4);
                if (i != array_elements.size() - 1) {
                    result += ",";
                }
                result += "\n";
            }
            result += ind + "]";
            break;
        }
        case OpType::ARRAY_ACCESS: {
            result += "ARRAY_ACCESS(\n";
            result += ind + "  array: " + left->to_string(indent + 4) + ",\n";
            result += ind + "  index: " + right->to_string(indent + 4) + "\n";
            result += ind + ")";
            break;
        }
        case OpType::IN: {
            for (size_t i = 0; i < parameters.size(); ++i) {
                result += parameters[i];
                if (i != parameters.size() - 1) {
                    result += ",";
                }
            }
            result += " IN " + value + "\n";
            break;
        }
        default: result += "UNKNOWN_OP";
    }

    // 如果有子节点，添加子节点信息
    if (left || right) {
        result += " (\n";
        if (left) {
            result += ind + "  left: " + left->to_string(indent + 4) + "\n";
        }
        if (right) {
            result += ind + "  right: " + right->to_string(indent + 4) + "\n";
        }
        result += ind + ")";
    }

    return result;
}

std::string StmtNode::to_string(int indent) const {
    std::string ind(indent, ' ');
    std::string result = ind;

    switch (stmt_type) {
        case StmtType::EXPRESSION: result += "EXPRESSION_STMT"; break;
        case StmtType::IF: result += "IF_STMT"; break;
        case StmtType::WHILE: result += "WHILE_STMT"; break;
        case StmtType::FOR: result += "FOR_STMT"; break;
        case StmtType::RETURN: result += "RETURN_STMT"; break;
        case StmtType::BLOCK: result += "BLOCK_STMT"; break;
        case StmtType::DECLARATION: result += "DECLARATION_STMT"; break;
        case StmtType::EMPTY: result += "EMPTY_STMT"; break;
    }

    if (condition) {
        result += "\n" + ind + "Condition:";
        result += "\n" + condition->to_string(indent + 4);
    }

    if (expr) {
        result += "\n" + ind + "Expression:";
        result += "\n" + expr->to_string(indent + 4);
    }

    if (!children.empty()) {
        result += "\n" + ind + "Statements:";
        for (const auto& child : children) {
            result += "\n" + child->to_string(indent + 4);
        }
    }

    return result;
}

std::string FuncNode::to_string(int indent) const {
    std::string ind(indent, ' ');
    std::string result = ind + "FUNCTION " + return_type + " " + name + "(";

    for (size_t i = 0; i < parameters.size(); ++i) {
        result += parameters[i];
        if (i < parameters.size() - 1) {
            result += ", ";
        }
    }
    result += ")";

    if (body) {
        result += "\n" + body->to_string(indent + 4);
    }

    return result;
}

std::string APINode::to_string(int indent) const {
    std::string ind(indent, ' ');
    std::string result = ind + "API " + path + "(";

    if (body) {
        result += "\n" + body->to_string(indent + 4);
    }

    return result;
}

std::string ProgramNode::to_string(int indent) const {
    std::string ind(indent, ' ');
    std::string result = ind + "PROGRAM";

    //for (const auto& func : functions) {
    //    result += "\n" + func->to_string(indent + 4);
    //}

    for (const auto& api : apis) {
        result += "\n" + api->to_string(indent + 4);
    }

    return result;
}