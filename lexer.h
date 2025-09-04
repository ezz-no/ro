//
// Created by ezzno on 2025/7/28.
//

#ifndef GLUE_LEXER_H
#define GLUE_LEXER_H

#include <string>
#include <fstream>
#include <unordered_map>

// 令牌类型枚举
enum TokenType {
    // 关键字
    KEYWORD_IF,
    KEYWORD_ELSE,
    KEYWORD_WHILE,
    KEYWORD_FOR,
    KEYWORD_IN,
    KEYWORD_EACH,
    KEYWORD_MEET,
    KEYWORD_INT,
    KEYWORD_FLOAT,
    KEYWORD_VOID,
    KEYWORD_RETURN,
    KEYWORD_PRINT,
    KEYWORD_API,
    KEYWORD_LISTEN,

    // 标识符
    IDENTIFIER,

    // 常量
    CONSTANT_INTEGER,
    CONSTANT_FLOAT,
    CONSTANT_STRING,

    // 运算符
    OP_PLUS,         // +
    OP_PLUS_PLUS,         // ++
    OP_MINUS,        // -
    OP_MINUS_MINUS,        // --
    OP_MULTIPLY,     // *
    OP_DIVIDE,       // /
    OP_ASSIGN,       // =
    OP_EQUALS,       // ==
    OP_NOT_EQUALS,   // !=
    OP_LESS,         // <
    OP_LESS_EQUALS,  // <=
    OP_GREATER,      // >
    OP_GREATER_EQUALS,// >=
    OP_LOGICAL_AND,  // &&
    OP_LOGICAL_OR,   // ||
    OP_NOT,          // !
    OP_RIGHT_ARROW,  // ->
    OP_LEFT_ARROW,  // <-
    OP_DOUBLE_ARROW,  // <->

    // 分隔符
    SEPARATOR_LPAREN,    // (
    SEPARATOR_RPAREN,    // )
    SEPARATOR_LBRACE,    // {
    SEPARATOR_RBRACE,    // }
    SEPARATOR_LBRACKET,  // [
    SEPARATOR_RBRACKET,  // ]
    SEPARATOR_SEMICOLON, // ;
    SEPARATOR_COLON,     // :
    SEPARATOR_COMMA,     // ,
    SEPARATOR_DOT,       // .
    SEPARATOR_NEWLINE, // \n

    // 特殊令牌
    END_OF_FILE,
    UNKNOWN
};

// 令牌结构
struct Token {
    TokenType type;
    std::string value;
    int line;
    int column;

    Token(TokenType t, const std::string& v, int l, int c)
        : type(t), value(v), line(l), column(c) {}
};

// 词法分析器类
class Lexer {
private:
    std::ifstream input_file;
    std::string current_filename;
    char current_char;
    int current_line;
    int current_column;
    std::unordered_map<std::string, TokenType> keyword_map;

    // 读取下一个字符
    void next_char();

    // 跳过空白字符
    void skip_whitespace();

    // 处理标识符和关键字
    Token process_identifier();

    // 处理数字常量
    Token process_number();

    // 处理字符串常量
    Token process_string();

    // 处理运算符和分隔符
    Token process_operator_or_separator();

public:
    // 构造函数
    Lexer(const std::string& filename);

    // 析构函数
    ~Lexer();

    // 检查文件是否成功打开
    bool is_open() const;

    // 获取下一个令牌
    Token get_next_token();
};

#endif // LEXER_H

