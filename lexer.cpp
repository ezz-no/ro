//
// Created by ezzno on 2025/7/28.
//

#include "lexer.h"
#include <cctype>
#include <iostream>

// 初始化关键字映射
Lexer::Lexer(const std::string& filename)
    : current_filename(filename), current_line(1), current_column(0) {
    input_file.open(filename);

    // 填充关键字映射表
    keyword_map["if"] = KEYWORD_IF;
    keyword_map["else"] = KEYWORD_ELSE;
    keyword_map["while"] = KEYWORD_WHILE;
    keyword_map["for"] = KEYWORD_FOR;
    keyword_map["in"] = KEYWORD_IN;
    keyword_map["meet"] = KEYWORD_MEET;
    keyword_map["each"] = KEYWORD_EACH;
    keyword_map["int"] = KEYWORD_INT;
    keyword_map["float"] = KEYWORD_FLOAT;
    keyword_map["void"] = KEYWORD_VOID;
    keyword_map["return"] = KEYWORD_RETURN;
    keyword_map["print"] = KEYWORD_PRINT;
    keyword_map["api"] = KEYWORD_API;
    keyword_map["listen"] = KEYWORD_LISTEN;

    if (input_file.is_open()) {
        next_char(); // 读取第一个字符
    }
}

Lexer::~Lexer() {
    if (input_file.is_open()) {
        input_file.close();
    }
}

bool Lexer::is_open() const {
    return input_file.is_open();
}

void Lexer::next_char() {
    if (input_file.eof()) {
        current_char = EOF;
        return;
    }

    input_file.get(current_char);
    current_column++;

    // 处理换行
    if (current_char == '\n') {
        current_line++;
        current_column = 0;
    }
}

void Lexer::skip_whitespace() {
    while (current_char != EOF && current_char != '\n' && std::isspace(current_char)) {
        next_char();
    }
}

Token Lexer::process_identifier() {
    int start_line = current_line;
    int start_column = current_column;
    std::string identifier;

    // 标识符由字母、数字和下划线组成，且不能以数字开头
    while (current_char != EOF &&
           (std::isalnum(current_char) || current_char == '_')) {
        identifier += current_char;
        next_char();
    }

    // 检查是否为关键字
    auto it = keyword_map.find(identifier);
    if (it != keyword_map.end()) {
        return Token(it->second, identifier, start_line, start_column);
    }

    // 否则是普通标识符
    return Token(IDENTIFIER, identifier, start_line, start_column);
}

Token Lexer::process_number() {
    int start_line = current_line;
    int start_column = current_column;
    std::string number_str;
    bool is_float = false;

    // 处理整数部分
    while (current_char != EOF && std::isdigit(current_char)) {
        number_str += current_char;
        next_char();
    }

    // 检查是否为浮点数
    if (current_char == '.') {
        is_float = true;
        number_str += current_char;
        next_char();

        // 处理小数部分
        while (current_char != EOF && std::isdigit(current_char)) {
            number_str += current_char;
            next_char();
        }
    }

    // 检查科学计数法 (可选扩展)
    if (current_char == 'e' || current_char == 'E') {
        is_float = true;
        number_str += current_char;
        next_char();

        // 可选的正负号
        if (current_char == '+' || current_char == '-') {
            number_str += current_char;
            next_char();
        }

        // 指数部分
        while (current_char != EOF && std::isdigit(current_char)) {
            number_str += current_char;
            next_char();
        }
    }

    if (is_float) {
        return Token(CONSTANT_FLOAT, number_str, start_line, start_column);
    } else {
        return Token(CONSTANT_INTEGER, number_str, start_line, start_column);
    }
}

Token Lexer::process_string() {
    int start_line = current_line;
    int start_column = current_column;
    std::string string_value;
    char quote_char = current_char; // 记录是单引号还是双引号

    next_char(); // 跳过开头的引号

    while (current_char != EOF && current_char != quote_char) {
        // 处理转义字符
        if (current_char == '\\') {
            next_char(); // 跳过反斜杠

            switch (current_char) {
                case 'n': string_value += '\n'; break;
                case 't': string_value += '\t'; break;
                case 'r': string_value += '\r'; break;
                case '"': string_value += '"'; break;
                case '\'': string_value += '\''; break;
                case '\\': string_value += '\\'; break;
                default: string_value += current_char; // 未知转义字符，直接添加
            }
        } else {
            string_value += current_char;
        }

        next_char();
    }

    if (current_char == quote_char) {
        next_char(); // 跳过结尾的引号
    } else {
        // 未找到结束引号，可能是一个错误
        std::cerr << "Warning: Unclosed string literal at line "
                  << start_line << ", column " << start_column << std::endl;
    }

    return Token(CONSTANT_STRING, string_value, start_line, start_column);
}

Token Lexer::process_operator_or_separator() {
    int start_line = current_line;
    int start_column = current_column;
    char current = current_char;

    // 处理可能是双字符的运算符
    switch (current) {
        case '+':
            next_char();
            if (current_char == '+') {
                next_char();
                return Token(OP_PLUS_PLUS, "++", start_line, start_column);
            } else {
                return Token(OP_PLUS, "+", start_line, start_column);
            }

        case '-':
            next_char();
            if (current_char == '-') {
                next_char();
                return Token(OP_MINUS_MINUS, "--", start_line, start_column);
            }
            if (current_char == '>') {
                next_char();
                return Token(OP_RIGHT_ARROW, "-", start_line, start_column);
            }
            return Token(OP_MINUS, "-", start_line, start_column);

        case '*':
            next_char();
            return Token(OP_MULTIPLY, "*", start_line, start_column);

        case '/':
            next_char();
            // 检查是否是注释
            if (current_char == '/') {
                // 单行注释，跳过直到行尾
                while (current_char != EOF && current_char != '\n') {
                    next_char();
                }
                return get_next_token(); // 递归获取下一个令牌
            } else if (current_char == '*') {
                // 多行注释
                next_char(); // 跳过第二个*
                bool found_end = false;

                while (current_char != EOF && !found_end) {
                    if (current_char == '*') {
                        next_char();
                        if (current_char == '/') {
                            found_end = true;
                            next_char(); // 跳过/
                        }
                    } else {
                        next_char();
                    }
                }

                if (!found_end) {
                    std::cerr << "Warning: Unclosed multi-line comment at line "
                              << start_line << ", column " << start_column << std::endl;
                }

                return get_next_token(); // 递归获取下一个令牌
            } else {
                return Token(OP_DIVIDE, "/", start_line, start_column);
            }

        case '=':
            next_char();
            if (current_char == '=') {
                next_char();
                return Token(OP_EQUALS, "==", start_line, start_column);
            } else {
                return Token(OP_ASSIGN, "=", start_line, start_column);
            }

        case '!':
            next_char();
            if (current_char == '=') {
                next_char();
                return Token(OP_NOT_EQUALS, "!=", start_line, start_column);
            } else {
                return Token(OP_NOT, "!", start_line, start_column);
            }

        case '<':
            next_char();
            if (current_char == '=') {
                next_char();
                return Token(OP_LESS_EQUALS, "<=", start_line, start_column);
            }
            if (current_char == '-') {
                next_char();
                return Token(OP_LEFT_ARROW, "<-", start_line, start_column);
            }
            return Token(OP_LESS, "<", start_line, start_column);

        case '>':
            next_char();
            if (current_char == '=') {
                next_char();
                return Token(OP_GREATER_EQUALS, ">=", start_line, start_column);
            } else {
                return Token(OP_GREATER, ">", start_line, start_column);
            }

        case '&':
            next_char();
            if (current_char == '&') {
                next_char();
                return Token(OP_LOGICAL_AND, "&&", start_line, start_column);
            } else {
                return Token(UNKNOWN, std::string(1, current), start_line, start_column);
            }

        case '|':
            next_char();
            if (current_char == '|') {
                next_char();
                return Token(OP_LOGICAL_OR, "||", start_line, start_column);
            } else {
                return Token(UNKNOWN, std::string(1, current), start_line, start_column);
            }

        case '(':
            next_char();
            return Token(SEPARATOR_LPAREN, "(", start_line, start_column);

        case ')':
            next_char();
            return Token(SEPARATOR_RPAREN, ")", start_line, start_column);

        case '{':
            next_char();
            return Token(SEPARATOR_LBRACE, "{", start_line, start_column);

        case '}':
            next_char();
            return Token(SEPARATOR_RBRACE, "}", start_line, start_column);

        case '[':
            next_char();
            return Token(SEPARATOR_LBRACKET, "[", start_line, start_column);

        case ']':
            next_char();
            return Token(SEPARATOR_RBRACKET, "]", start_line, start_column);

        case ';':
            next_char();
            return Token(SEPARATOR_SEMICOLON, ";", start_line, start_column);

        case ':':
            next_char();
            return Token(SEPARATOR_COLON, ";", start_line, start_column);

        case ',':
            next_char();
            return Token(SEPARATOR_COMMA, ",", start_line, start_column);

        case '.':
            next_char();
            return Token(SEPARATOR_DOT, ".", start_line, start_column);

        case '\n':
            next_char();
            return get_next_token(); // 递归获取下一个令牌
            //return Token(SEPARATOR_NEWLINE, "\n", start_line, start_column);

        default:
            next_char();
            return Token(UNKNOWN, std::string(1, current), start_line, start_column);
    }
}

Token Lexer::get_next_token() {
    // 跳过空白字符
    skip_whitespace();

    // 检查是否到达文件末尾
    if (current_char == EOF) {
        return Token(END_OF_FILE, "", current_line, current_column);
    }

    // 处理标识符和关键字 (以字母或下划线开头)
    if (std::isalpha(current_char) || current_char == '_') {
        return process_identifier();
    }

    // 处理数字
    if (std::isdigit(current_char)) {
        return process_number();
    }

    // 处理字符串
    if (current_char == '"' || current_char == '\'') {
        return process_string();
    }

    // 处理运算符和分隔符
    return process_operator_or_separator();
}

