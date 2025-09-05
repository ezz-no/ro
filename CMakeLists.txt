cmake_minimum_required(VERSION 3.15)
project(ro-glue)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

# 手动指定LLVM配置文件路径（根据你的实际安装路径填写）
set(LLVM_DIR "/opt/homebrew/Cellar/llvm/20.1.5/lib/cmake/llvm")
find_package(LLVM 20.1 REQUIRED CONFIG) # 寻找LLVM库
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}") # 打印LLVM相关信息（调试用）
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
add_definitions(${LLVM_DEFINITIONS}) # 添加LLVM定义
llvm_map_components_to_libnames(llvm_libs # 获取LLVM组件
        Core
        ExecutionEngine
        MC
        MCJIT
        Support
        native
)

# 查找Boost（使用新策略，指定组件）
find_package(Boost 1.88 REQUIRED system url)
if(Boost_FOUND)
    message(STATUS "Found Boost ${Boost_VERSION} at ${BOOST_ROOT}")
    message(STATUS "Boost include dirs: ${Boost_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "Boost not found at ${BOOST_ROOT}")
endif()

# 寻找Abseil库
set (absl_DIR "/opt/homebrew/Cellar/abseil/20240722.0/lib/cmake/absl") # Abseil 安装根目录
find_package(absl 20240722 REQUIRED)

# 添加可执行文件
add_executable(ro-glue
        main.cpp
        lexer.cpp
        parser.cpp
        executor.cpp
        to_string.cpp
        server.cpp
        namespace.cpp
)

target_include_directories(ro-glue PRIVATE
        ${LLVM_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}  # 添加Boost头文件目录
)

target_link_libraries(ro-glue PRIVATE
        # Abseil库
        absl::flags
        absl::flags_parse
        absl::flags_usage
        # 系统库
        pthread
        Boost::system  # 链接Boost.System库
        Boost::url     # Boost.URL库
        # LLVM库
        ${llvm_libs}
)
