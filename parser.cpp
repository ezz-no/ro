//
// Created by ezzno on 2025/7/29.
//

#include "parser.h"
#include <iostream>
#include <iomanip>
#include <execinfo.h>  // 非标准库，但但在Linux/macOS上普遍存在

// Parser类实现
void Parser::consume() {
    current_token = lexer.get_next_token();
}

// 简化版堆栈跟踪（仅支持Linux/macOS）
static void print_backtrace() {
    std::cerr << "Backtrace:" << std::endl;

    const int max_frames = 20;  // 最多显示10层堆栈
    void* frames[max_frames];

    // 获取当前堆栈帧
    int num_frames = backtrace(frames, max_frames);

    // 转换为可读的字符串
    char** frame_symbols = backtrace_symbols(frames, num_frames);
    if (frame_symbols == nullptr) {
        std::cerr << "  Failed to get backtrace symbols" << std::endl;
        return;
    }

    // 打印堆栈（从1开始，跳过当前函数）
    for (int i = 1; i < num_frames; ++i) {
        std::cerr << "  [" << i << "] " << frame_symbols[i] << std::endl;
    }

    free(frame_symbols);  // 释放动态分配的内存
}

void Parser::error(const std::string& message) {
    std::cerr << "Parse error at line " << current_token.line
              << ", column " << current_token.column << ": "
              << message;
    if (current_token.type != END_OF_FILE) {
        std::cerr << " (Unexpected token: " << current_token.value << ")";
    }
    std::cerr << std::endl;

    print_backtrace();
    exit(1);
}

void Parser::expect(TokenType type, const std::string& message) {
    auto ctype = current_token.type;
    if (ctype == SEPARATOR_NEWLINE) {
        ctype = SEPARATOR_SEMICOLON;
    }
    if (ctype != type) {
        error(message);
    }
    consume();
}

// 表达式解析（采用递归下降法，按运算符优先级排序）
std::unique_ptr<ExprNode> Parser::parse_expression() {
    return parse_assignment_expression();
}

std::unique_ptr<ExprNode> Parser::parse_assignment_expression() {
    auto left = parse_logical_or_expression();

    if (current_token.type == OP_ASSIGN) {
        auto op = std::make_unique<ExprNode>(ExprNode::OpType::ASSIGN);
        op->left = std::move(left);
        consume();
        op->right = parse_assignment_expression();
        return op;
    }

    return left;
}

std::unique_ptr<ExprNode> Parser::parse_logical_or_expression() {
    auto left = parse_logical_and_expression();

    while (current_token.type == OP_LOGICAL_OR) {
        auto op = std::make_unique<ExprNode>(ExprNode::OpType::OR);
        op->left = std::move(left);
        consume();
        op->right = parse_logical_and_expression();
        left = std::move(op);
    }

    return left;
}

std::unique_ptr<ExprNode> Parser::parse_logical_and_expression() {
    auto left = parse_equality_expression();

    while (current_token.type == OP_LOGICAL_AND) {
        auto op = std::make_unique<ExprNode>(ExprNode::OpType::AND);
        op->left = std::move(left);
        consume();
        op->right = parse_equality_expression();
        left = std::move(op);
    }

    return left;
}

std::unique_ptr<ExprNode> Parser::parse_equality_expression() {
    auto left = parse_relational_expression();

    while (current_token.type == OP_EQUALS || current_token.type == OP_NOT_EQUALS) {
        auto op = std::make_unique<ExprNode>(
            current_token.type == OP_EQUALS ? ExprNode::OpType::EQ : ExprNode::OpType::NEQ
        );
        op->left = std::move(left);
        consume();
        op->right = parse_relational_expression();
        left = std::move(op);
    }

    return left;
}

std::unique_ptr<ExprNode> Parser::parse_relational_expression() {
    auto left = parse_additive_expression();

    while (true) {
        if (current_token.type == OP_LESS) {
            auto op = std::make_unique<ExprNode>(ExprNode::OpType::LT);
            op->left = std::move(left);
            consume();
            op->right = parse_additive_expression();
            left = std::move(op);
        } else if (current_token.type == OP_GREATER) {
            auto op = std::make_unique<ExprNode>(ExprNode::OpType::GT);
            op->left = std::move(left);
            consume();
            op->right = parse_additive_expression();
            left = std::move(op);
        } else if (current_token.type == OP_LESS_EQUALS) {
            auto op = std::make_unique<ExprNode>(ExprNode::OpType::LE);
            op->left = std::move(left);
            consume();
            op->right = parse_additive_expression();
            left = std::move(op);
        } else if (current_token.type == OP_GREATER_EQUALS) {
            auto op = std::make_unique<ExprNode>(ExprNode::OpType::GE);
            op->left = std::move(left);
            consume();
            op->right = parse_additive_expression();
            left = std::move(op);
        } else {
            break;
        }
    }

    return left;
}

std::unique_ptr<ExprNode> Parser::parse_additive_expression() {
    auto left = parse_multiplicative_expression();

    while (current_token.type == OP_PLUS || current_token.type == OP_MINUS) {
        auto op = std::make_unique<ExprNode>(
            current_token.type == OP_PLUS ? ExprNode::OpType::ADD : ExprNode::OpType::SUB
        );
        op->left = std::move(left);
        consume();
        op->right = parse_multiplicative_expression();
        left = std::move(op);
    }

    return left;
}

std::unique_ptr<ExprNode> Parser::parse_multiplicative_expression() {
    auto left = parse_curl_expression();

    while (current_token.type == OP_MULTIPLY || current_token.type == OP_DIVIDE) {
        auto op = std::make_unique<ExprNode>(
            current_token.type == OP_MULTIPLY ? ExprNode::OpType::MUL : ExprNode::OpType::DIV
        );
        op->left = std::move(left);
        consume();
        op->right = parse_curl_expression();
        left = std::move(op);
    }

    return left;
}

std::unique_ptr<ExprNode> Parser::parse_curl_expression() {
    auto left = parse_primary_expression();

    while (current_token.type == OP_LEFT_ARROW) {
        auto op = std::make_unique<ExprNode>(ExprNode::OpType::CURL);
        op->left = std::move(left);
        consume();
        op->right = parse_primary_expression();
        left = std::move(op);
    }

    return left;
}

std::unique_ptr<ExprNode> Parser::parse_address_expression() {
    switch (current_token.type) {
        case SEPARATOR_LBRACKET: { // 数组访问
            consume();  // 消耗'['

            // 创建数组访问节点
            auto access_node = std::make_unique<ExprNode>(ExprNode::OpType::ARRAY_ACCESS);
            // access_node->left = std::move(ident);  // 数组名

            // 解析索引表达式
            access_node->right = parse_expression();

            expect(SEPARATOR_RBRACKET, "Expected closing bracket for array access"); // 消耗']'

            return access_node;
        }
        case SEPARATOR_DOT: {
            consume();

            auto node = std::make_unique<ExprNode>(ExprNode::OpType::DOT);
            node->right = parse_expression();

            return node;
        }
        default: {
            return nullptr;
        }
    }
}

std::unique_ptr<ExprNode> Parser::parse_primary_expression() {
    switch (current_token.type) {
        case CONSTANT_INTEGER: {
            auto node = std::make_unique<ExprNode>(ExprNode::OpType::CONSTANT_INT, current_token.value);
            consume();
            return node;
        }
        case CONSTANT_FLOAT: {
            auto node = std::make_unique<ExprNode>(ExprNode::OpType::CONSTANT_FLOAT, current_token.value);
            consume();
            return node;
        }
        case CONSTANT_STRING: {
            auto node = std::make_unique<ExprNode>(ExprNode::OpType::CONSTANT_STRING, current_token.value);
            consume();
            return node;
        }

        case OP_NOT: {
            auto node = std::make_unique<ExprNode>(ExprNode::OpType::NOT);
            consume();
            node->right = parse_primary_expression();
            return node;
        }
        case SEPARATOR_LPAREN: {
            consume();
            auto expr = parse_expression();
            expect(SEPARATOR_RPAREN, "Expected ')' after expression");
            return expr;
        }

        case SEPARATOR_LBRACKET: {  // 遇到'['，解析数组
            consume();  // 消耗'['

            auto array_node = std::make_unique<ExprNode>(ExprNode::OpType::ARRAY_LITERAL);

            // 解析数组元素
            while (current_token.type != SEPARATOR_RBRACKET) {
                // 解析数组元素表达式
                auto elem = parse_expression();
                array_node->array_elements.push_back(std::move(elem));

                // 如果有逗号，消耗逗号并继续解析下一个元素
                if (current_token.type == SEPARATOR_COMMA) {
                    consume();  // 消耗','
                }
                // 检查是否有不合法的结束
                else if (current_token.type != SEPARATOR_RBRACKET) {
                    error("Expected comma or closing bracket in array");
                }
            }

            consume();  // 消耗']'
            return array_node;
        }

        case SEPARATOR_LBRACE: {  // 遇到'{'，解析对象字面量
            consume();  // 消耗'{'

            auto object_node = std::make_unique<ExprNode>(ExprNode::OpType::OBJECT_LITERAL);

            // 解析键值对
            while (current_token.type != SEPARATOR_RBRACE) {
                // 解析键（必须是字符串字面量）
                if (current_token.type != CONSTANT_STRING) {
                    error("Expected string literal as key in object");
                }

                // 保存键名并消耗字符串token
                std::string key = current_token.value;
                consume();  // 消耗键名（如"msg"）

                // 检查冒号
                if (current_token.type != SEPARATOR_COLON) {
                    error("Expected colon after key in object");
                }
                consume();  // 消耗':'

                // 解析值表达式（可以是变量、字面量等）
                auto value = parse_expression();
                object_node->object_members[key] = std::move(value);

                // 处理逗号分隔
                if (current_token.type == SEPARATOR_COMMA) {
                    consume();  // 消耗','
                }
                // 检查是否有不合法的结束
                else if (current_token.type != SEPARATOR_RBRACE) {
                    error("Expected comma or closing brace in object");
                } else {
                    break;
                }
            }

            consume();  // 消耗'}'
            return object_node;
        }

        case SEPARATOR_DOT: {
            auto ident = std::make_unique<ExprNode>(ExprNode::OpType::DOT);
            consume();
            ident->left = parse_primary_expression();
            if (ident->left->op_type != ExprNode::OpType::IDENTIFIER) {
                error("Unexpected token in dot expression");
            }

            return ident;
        }
        case IDENTIFIER: { // 处理变量、数组访问，如identifier, identifier[expr]
            auto ident = std::make_unique<ExprNode>(ExprNode::OpType::IDENTIFIER, current_token.value);
            consume();

            while (true) {
                auto addr_node = parse_address_expression();
                if (addr_node == nullptr) {
                    break;
                }
                addr_node->left = std::move(ident);
                ident = std::move(addr_node);
            }

            return ident;
        }
        default:
            error("Unexpected token in primary expression");
            return nullptr; // 不会执行到这里
    }
}

// 语句解析
std::unique_ptr<StmtNode> Parser::parse_statement() {
    switch (current_token.type) {
        case SEPARATOR_LBRACE:
            return parse_block();
        case KEYWORD_IF:
            return parse_if_statement();
        case KEYWORD_WHILE:
            return parse_while_statement();
        case KEYWORD_FOR:
            return parse_for_statement();
        case KEYWORD_EACH:
            return parse_each_statement();
        case KEYWORD_RETURN:
            return parse_return_statement();
        case KEYWORD_PRINT:
            return parse_print_statement();
        case KEYWORD_INT:
        case KEYWORD_FLOAT:
            return parse_declaration();
        case SEPARATOR_NEWLINE:
        case SEPARATOR_SEMICOLON: {
            consume();
            return std::make_unique<StmtNode>(StmtNode::StmtType::EMPTY);
        }
        default: {
            auto stmt = std::make_unique<StmtNode>(StmtNode::StmtType::EXPRESSION);
            stmt->expr = parse_expression();
            expect(SEPARATOR_SEMICOLON, "Expected ';' after expression");
            return stmt;
        }
    }
}

std::unique_ptr<StmtNode> Parser::parse_block() {
    auto block = std::make_unique<StmtNode>(StmtNode::StmtType::BLOCK);
    expect(SEPARATOR_LBRACE, "Expected '{' to start block");

    while (current_token.type != SEPARATOR_RBRACE && current_token.type != END_OF_FILE) {
        block->children.push_back(parse_statement());
    }

    expect(SEPARATOR_RBRACE, "Expected '}' to end block");
    return block;
}

std::unique_ptr<StmtNode> Parser::parse_if_statement() {
    auto if_stmt = std::make_unique<StmtNode>(StmtNode::StmtType::IF);
    expect(KEYWORD_IF, "Expected 'if'");
    expect(SEPARATOR_LPAREN, "Expected '(' after 'if'");

    if_stmt->condition = parse_expression();

    expect(SEPARATOR_RPAREN, "Expected ')' after if condition");
    if_stmt->children.push_back(parse_statement());

    if (current_token.type == KEYWORD_ELSE) {
        consume();
        if_stmt->children.push_back(parse_statement());
    }

    return if_stmt;
}

std::unique_ptr<StmtNode> Parser::parse_while_statement() {
    auto while_stmt = std::make_unique<StmtNode>(StmtNode::StmtType::WHILE);
    expect(KEYWORD_WHILE, "Expected 'while'");
    expect(SEPARATOR_LPAREN, "Expected '(' after 'while'");

    while_stmt->condition = parse_expression();

    expect(SEPARATOR_RPAREN, "Expected ')' after while condition");
    while_stmt->children.push_back(parse_statement());

    return while_stmt;
}

std::unique_ptr<StmtNode> Parser::parse_for_statement() {
    auto for_stmt = std::make_unique<StmtNode>(StmtNode::StmtType::FOR);
    expect(KEYWORD_FOR, "Expected 'for'");
    expect(SEPARATOR_LPAREN, "Expected '(' after 'for'");

    // 初始化部分
    if (current_token.type != SEPARATOR_SEMICOLON) {
        for_stmt->children.push_back(parse_statement());
    } else {
        consume(); // 跳过;
    }

    // 条件部分
    if (current_token.type != SEPARATOR_SEMICOLON) {
        for_stmt->condition = parse_expression();
    }
    expect(SEPARATOR_SEMICOLON, "Expected ';' in for loop");

    // 更新部分
    if (current_token.type != SEPARATOR_RPAREN) {
        auto update_stmt = std::make_unique<StmtNode>(StmtNode::StmtType::EXPRESSION);
        update_stmt->expr = parse_expression();
        for_stmt->children.push_back(std::move(update_stmt));
    }

    expect(SEPARATOR_RPAREN, "Expected ')' after for loop conditions");
    for_stmt->children.push_back(parse_statement());

    return for_stmt;
}

std::unique_ptr<ExprNode> Parser::parse_in_expression() {
    auto op = std::make_unique<ExprNode>(ExprNode::OpType::IN);

    op->parameters = parse_parameters();
    // in
    expect(KEYWORD_IN, "Expected 'in' in for loop");

    // 变量名
    op->value = current_token.value;
    expect(IDENTIFIER, "Expected identifier in declaration");
    return op;
}

std::unique_ptr<StmtNode> Parser::parse_each_statement() {
    auto stmt = std::make_unique<StmtNode>(StmtNode::StmtType::EACH);
    expect(KEYWORD_EACH, "Expected 'each'");

    stmt->expr = parse_in_expression();

    // meet
    expect(KEYWORD_MEET, "Expected 'meet' in for loop");

    // 条件部分
    stmt->condition = parse_expression();
    stmt->children.emplace_back(std::move(parse_block()));

    return stmt;
}

std::unique_ptr<StmtNode> Parser::parse_return_statement() {
    auto return_stmt = std::make_unique<StmtNode>(StmtNode::StmtType::RETURN);
    expect(KEYWORD_RETURN, "Expected 'return'");

    if (current_token.type != SEPARATOR_SEMICOLON) {
        return_stmt->expr = parse_expression();
    }

    expect(SEPARATOR_SEMICOLON, "Expected ';' after return");
    return return_stmt;
}

std::unique_ptr<StmtNode> Parser::parse_print_statement() {
    auto stmt = std::make_unique<StmtNode>(StmtNode::StmtType::PRINT);
    expect(KEYWORD_PRINT, "Expected 'print'");

    while (true) {
        stmt->exprs.emplace_back(parse_expression());

        if (current_token.type != SEPARATOR_COMMA) {
            break;
        }
        consume(); // 跳过逗号
    }

    expect(SEPARATOR_SEMICOLON, "Expected ';' after return");
    return stmt;
}

std::unique_ptr<StmtNode> Parser::parse_declaration() {
    auto decl_stmt = std::make_unique<StmtNode>(StmtNode::StmtType::DECLARATION);

    // 类型（这里简化处理，只记录类型字符串）
    //std::string type = current_token.value;
    //consume();

    // 变量名
    if (current_token.type != IDENTIFIER) {
        error("Expected identifier in declaration");
    }
    std::string name = current_token.value;
    consume();

    // 处理初始化
    if (current_token.type == OP_ASSIGN) {
        consume();
        auto assign = std::make_unique<ExprNode>(ExprNode::OpType::ASSIGN);
        assign->left = std::make_unique<ExprNode>(ExprNode::OpType::IDENTIFIER, name);
        assign->right = parse_expression();

        decl_stmt->expr = std::move(assign);
    }

    expect(SEPARATOR_SEMICOLON, "Expected ';' after declaration");
    return decl_stmt;
}

// 函数解析
Parameters Parser::parse_parameters() {
    Parameters params;

    if (current_token.type == SEPARATOR_RPAREN) {
        return params; // 无参数
    }

    while (true) {
        // 参数类型
        /*if (current_token.type != KEYWORD_INT && current_token.type != KEYWORD_FLOAT
            && current_token.type != KEYWORD_VOID) {
            error("Expected type in parameter list");
        }
        std::string type = current_token.value;
        consume();*/

        // 参数名
        if (current_token.type != IDENTIFIER) {
            error("Expected identifier in parameter list");
        }
        std::string name = current_token.value;
        consume();

        params.emplace_back(name);

        if (current_token.type != SEPARATOR_COMMA) {
            break;
        }
        consume(); // 跳过逗号
    }

    return params;
}

std::unique_ptr<FuncNode> Parser::parse_function() {
    // 函数返回类型
    if (current_token.type != KEYWORD_INT && current_token.type != KEYWORD_FLOAT
        && current_token.type != KEYWORD_VOID) {
        error("Expected return type for function");
    }
    std::string return_type = current_token.value;
    consume();

    // 函数名
    if (current_token.type != IDENTIFIER) {
        error("Expected function name");
    }
    std::string func_name = current_token.value;
    consume();

    // 参数列表
    expect(SEPARATOR_LPAREN, "Expected '(' after function name");
    auto params = parse_parameters();
    expect(SEPARATOR_RPAREN, "Expected ')' after parameter list");

    // 函数体
    auto func = std::make_unique<FuncNode>(return_type, func_name);
    func->parameters = std::move(params);
    func->body = parse_block();

    return func;
}

// 解析整个程序
std::unique_ptr<ProgramNode> Parser::parse_program() {
    auto program = std::make_unique<ProgramNode>();
    int port = 80;

    //while (current_token.type != END_OF_FILE) {
    //    program->functions.push_back(parse_function());
    //}

    while (current_token.type != END_OF_FILE) {
        switch (current_token.type) {
            case KEYWORD_API: {
                expect(KEYWORD_API, "Expected 'api'");

                // 函数名
                if (current_token.type != CONSTANT_STRING) {
                    error("Expected api path");
                }
                std::string api_path = current_token.value;
                consume();

                // 函数体
                auto api = std::make_unique<APINode>(api_path);
                api->body = parse_block();
                api->port = port;

                program->apis.push_back(std::move(api));
                break;
            }
            case KEYWORD_LISTEN: {
                expect(KEYWORD_LISTEN, "Expected 'listen'");

                if (current_token.type != CONSTANT_INTEGER) {
                    error("Expected listen port");
                }
                port = std::stoi(current_token.value);
                consume();
                break;
            }
            default:
                error("Unexpected token in top level declaration");
        }
    }

    return program;
}
