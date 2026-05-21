cmake_minimum_required(VERSION 4.2)
project(voyager C)

set(CMAKE_C_STANDARD 11)

execute_process(
        COMMAND php-config --includes
        OUTPUT_VARIABLE PHP_INCLUDE_FLAGS
)

# 找到 php-config
find_program(PHP_CONFIG php-config)

if (NOT PHP_CONFIG)
    message(FATAL_ERROR "php-config not found")
endif()

# 获取 include 路径
execute_process(
        COMMAND ${PHP_CONFIG} --includes
        OUTPUT_VARIABLE PHP_INCLUDE_FLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# 获取扩展目录（可选）
execute_process(
        COMMAND ${PHP_CONFIG} --extension-dir
        OUTPUT_VARIABLE PHP_EXTENSION_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

# 转换 include flags
separate_arguments(PHP_INCLUDE_FLAGS)

# 源文件
file(GLOB SRC_FILES *.c)

add_library(${PROJECT_NAME} SHARED ${SRC_FILES})

# 加 include
target_compile_options(${PROJECT_NAME} PRIVATE ${PHP_INCLUDE_FLAGS})

# Linux 下一般需要
target_link_libraries(${PROJECT_NAME})

# 输出 .so 名字（重要）
set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX ""  # 不要 lib 前缀
        OUTPUT_NAME "demo"  # 对应 extension=demo
)