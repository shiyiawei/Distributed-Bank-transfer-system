cmake_minimum_required(VERSION 3.14)
project(DistributedBankingSystem VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 添加编译选项
add_compile_options(-Wall -Wextra -pthread)

# 设置包含目录
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/external/labs_headers
)

# Common库
add_library(banking_common STATIC
    src/common/clock.cpp
    src/common/utils.cpp
)
target_include_directories(banking_common PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/external/labs_headers
)

# Shard库
add_library(banking_shard STATIC
    src/shard/account_shard.cpp
    src/shard/shard_manager.cpp
)
target_include_directories(banking_shard PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/external/labs_headers
)
target_link_libraries(banking_shard PUBLIC
    banking_common
    pthread
)

# Process库
add_library(banking_process STATIC
    src/process/parent_controller.cpp
    src/process/child_worker.cpp
)
target_include_directories(banking_process PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/external/labs_headers
)
target_link_libraries(banking_process PUBLIC
    banking_common
    banking_shard
)

# 主可执行文件
add_executable(banking_system
    src/main.cpp
)
target_link_libraries(banking_system PRIVATE
    banking_common
    banking_shard
    banking_process
    pthread
)

# 安装规则
install(TARGETS banking_system DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)