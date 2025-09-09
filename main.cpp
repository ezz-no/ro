#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <boost/url.hpp>
#include <iostream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "json.hpp"
#include "executor.h"
#include "parser.h"
#include "server.h"
#include "lexer.h"
#include "main.h"

// 定义调试模式和输出文件名标志
ABSL_FLAG(bool, debug, false, "Enable debug mode (print AST and IR)");
ABSL_FLAG(bool, eval, false, "Enable eval mode (no api and listen)");
ABSL_FLAG(int, port, 8080, "Port to listen on");
ABSL_FLAG(std::string, output, "", "Output executable filename");

std::string eval(const std::string& input) {
    try {
        Lexer lexer(input, true);
        Parser parser(lexer);
        std::unique_ptr<ProgramNode> program = parser.parse_program();
        Executor executor(true);
        executor.execute(program);
        return executor.result();
    } catch (const std::runtime_error& e) {
        return e.what();
    }
}

int main(int argc, char* argv[]) {
    // 解析命令行参数
    std::vector<char*> args = absl::ParseCommandLine(argc, argv);

    // 检查源文件参数是否存在
    if (args.size() != 2) {
        std::cerr << "Usage: " << args[0] << " [--debug] [--eval] [--output=filename] <source_file>" << std::endl;
        std::cerr << "  --debug: Enable debug mode (print AST and IR)" << std::endl;
        std::cerr << "  --output: Set output executable filename" << std::endl;
        return 1;
    }

    bool eval_mode = absl::GetFlag(FLAGS_eval);
    if (eval_mode) {
        int port = absl::GetFlag(FLAGS_port);
        try
        {
            net::io_context ioc{1};
            auto const address = net::ip::make_address("0.0.0.0");
            auto const endpoint = tcp::endpoint{address, static_cast<unsigned short>(port)};
            std::unordered_map<std::string, std::unique_ptr<APINode>> apis;
            auto listener = std::make_shared<Listener>(ioc, endpoint, std::move(apis));
            listener->run();  // 启动监听器
            ioc.run();
        }
        catch (std::exception const& e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }

        return 0;
    }

    std::string source_file = args[1];
    bool debug_mode = absl::GetFlag(FLAGS_debug);
    std::string output_file = absl::GetFlag(FLAGS_output);

    Lexer lexer(source_file);
    Parser parser(lexer);
    std::unique_ptr<ProgramNode> program = parser.parse_program();

    // 调试模式下输出AST
    if (debug_mode) {
        std::cout << "Successfully parsed the program!\n" << std::endl;
        std::cout << "Abstract Syntax Tree:\n" << std::endl;
        std::cout << program->to_string(4) << std::endl;
        std::cout << std::endl;
    }

    // 编译并运行
    executor.execute(program);

    return 0;
}
