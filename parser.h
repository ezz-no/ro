//
// Created by ezzno on 2025/7/29.
//

#ifndef GLUE_PARSER_H
#define GLUE_PARSER_H

#include "lexer.h"
#include <utility>
#include <vector>
#include <string>
#include <memory>

using Parameters = std::vector<std::string>;

// 抽象语法树(AST)节点基类
class ASTNode {
public:
    virtual ~ASTNode() = default;
    [[nodiscard]] virtual std::string to_string(int indent = 0) const = 0;
};

// 表达式节点
class ExprNode : public ASTNode {
public:
    enum class OpType {
        ADD, SUB, MUL, DIV,
        EQ, NEQ, LT, GT, LE, GE,
        AND, OR, NOT,
        IN,
        ASSIGN,
        CONSTANT_INT, CONSTANT_FLOAT, CONSTANT_STRING,
        DOT, IDENTIFIER,
        ARRAY_LITERAL, ARRAY_ACCESS, // 数组字面量，如[1, 2, 3]; 数组访问，如arr[0]
        OBJECT_LITERAL,   // 对象字面量 {"xx": xxx}
        CURL,
    };

    TokenType token_type;
    OpType op_type;
    std::string value;  // 用于存储常量值或标识符名称
    std::unique_ptr<ExprNode> left;
    std::unique_ptr<ExprNode> right;

    Parameters parameters;
    std::vector<std::unique_ptr<ExprNode>> array_elements;  // 用于存储数组元素
    std::unordered_map<std::string, std::unique_ptr<ExprNode>> object_members;

    explicit ExprNode(OpType type, std::string val = "", TokenType ttype = UNKNOWN)
        : op_type(type), value(std::move(val)), left(nullptr), right(nullptr), token_type(ttype) {}

    [[nodiscard]] std::string to_string(int indent = 0) const override;
};

// 语句节点
class StmtNode : public ASTNode {
public:
    enum class StmtType {
        EXPRESSION, IF, WHILE, FOR, EACH, RETURN,
        BLOCK, DECLARATION, EMPTY,
        PRINT
    };

    StmtType stmt_type;
    std::vector<std::unique_ptr<StmtNode>> children;
    std::unique_ptr<ExprNode> condition;  // 用于if, while, for
    std::unique_ptr<ExprNode> expr;       // 用于expression, return
    std::vector<std::unique_ptr<ExprNode>> exprs;

    explicit StmtNode(StmtType type) : stmt_type(type) {}

    [[nodiscard]] std::string to_string(int indent = 0) const override;
};

// 函数节点
class FuncNode : public ASTNode {
public:
    std::string return_type;
    std::string name;
    Parameters parameters;  // type, name
    std::unique_ptr<StmtNode> body;

    FuncNode(std::string ret_type, std::string func_name)
        : return_type(std::move(ret_type)), name(std::move(func_name)) {}

    [[nodiscard]] std::string to_string(int indent = 0) const override;
};

// API节点
class APINode : public ASTNode {
public:
    std::string path;
    int port;
    std::unique_ptr<StmtNode> body;

    APINode(std::string path)
        : path(std::move(path)) {}

    [[nodiscard]] std::string to_string(int indent = 0) const override;
};

// 程序节点
class ProgramNode : public ASTNode {
public:
    std::vector<std::unique_ptr<FuncNode>> functions;
    std::vector<std::unique_ptr<APINode>> apis;

    [[nodiscard]] std::string to_string(int indent = 0) const override;
};

// 语法分析器类
class Parser {
private:
    Lexer& lexer;
    Token current_token;

    // 辅助函数：消费当前令牌并获取下一个
    void consume();

    // 错误处理
    void error(const std::string& message);

    // 校验预期的令牌
    void expect(TokenType type, const std::string& message);

    // 表达式解析
    std::unique_ptr<ExprNode> parse_expression();
    std::unique_ptr<ExprNode> parse_assignment_expression();
    std::unique_ptr<ExprNode> parse_logical_or_expression();
    std::unique_ptr<ExprNode> parse_logical_and_expression();
    std::unique_ptr<ExprNode> parse_equality_expression();
    std::unique_ptr<ExprNode> parse_relational_expression();
    std::unique_ptr<ExprNode> parse_additive_expression();
    std::unique_ptr<ExprNode> parse_arrow_expression();
    std::unique_ptr<ExprNode> parse_multiplicative_expression();
    std::unique_ptr<ExprNode> parse_curl_expression();
    std::unique_ptr<ExprNode> parse_primary_expression();
    std::unique_ptr<ExprNode> parse_address_expression();

    // 语句解析
    std::unique_ptr<StmtNode> parse_statement();
    std::unique_ptr<StmtNode> parse_block();
    std::unique_ptr<StmtNode> parse_if_statement();
    std::unique_ptr<StmtNode> parse_while_statement();
    std::unique_ptr<StmtNode> parse_for_statement();
    std::unique_ptr<ExprNode> parse_in_expression();
    std::unique_ptr<StmtNode> parse_each_statement();
    std::unique_ptr<StmtNode> parse_return_statement();
    std::unique_ptr<StmtNode> parse_print_statement();
    std::unique_ptr<StmtNode> parse_declaration();

    // 函数解析
    std::unique_ptr<FuncNode> parse_function();
    Parameters parse_parameters();

public:
    explicit Parser(Lexer& lex) : lexer(lex), current_token(UNKNOWN, "", -1, -1) {
        consume();  // 获取第一个令牌
    }

    // 解析整个程序
    std::unique_ptr<ProgramNode> parse_program();
};

#endif // PARSER_H

