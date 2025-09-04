#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "lexer.h"
#include "parser.h"
#include "executor.h"  // 使用LLVM执行器替代原来的解释器
#include <iostream>
#include <string>

// 定义调试模式和输出文件名标志
ABSL_FLAG(bool, debug, false, "Enable debug mode (print AST and IR)");
ABSL_FLAG(std::string, output, "", "Output executable filename");

int main(int argc, char* argv[]) {
    // 解析命令行参数
    std::vector<char*> args = absl::ParseCommandLine(argc, argv);

    // 检查源文件参数是否存在
    if (args.size() != 2) {
        std::cerr << "Usage: " << args[0] << " [--debug] [--output=filename] [--run=false] <source_file>" << std::endl;
        std::cerr << "  --debug: Enable debug mode (print AST and IR)" << std::endl;
        std::cerr << "  --output: Set output executable filename" << std::endl;
        return 1;
    }

    std::string source_file = args[1];
    bool debug_mode = absl::GetFlag(FLAGS_debug);
    std::string output_file = absl::GetFlag(FLAGS_output);

    // 词法分析
    Lexer lexer(source_file);
    if (!lexer.is_open()) {
        std::cerr << "Error: Could not open file " << source_file << std::endl;
        return 1;
    }

    // 语法分析（生成AST）
    Parser parser(lexer);
    std::unique_ptr<ProgramNode> program = parser.parse_program();

    // 调试模式下输出AST
    if (debug_mode) {
        std::cout << "Successfully parsed the program!\n" << std::endl;
        std::cout << "Abstract Syntax Tree:\n" << std::endl;
        std::cout << program->to_string(4) << std::endl;
        std::cout << std::endl;
    }

    // 创建LLVM执行器
    Executor executor;

    // 编译并运行
    executor.execute(program);

    return 0;
}
