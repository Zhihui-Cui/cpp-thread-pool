cmake_minimum_required(VERSION 3.20)

# Project settings
project(cpp_thread_pool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# Library
add_library(thread_pool
    src/thread_pool.cpp
    src/single_thread_executor.cpp
)

target_include_directories(thread_pool
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(thread_pool PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

target_link_libraries(thread_pool PUBLIC Threads::Threads)

# Tests: each executable has its own main().
enable_testing()

add_executable(task_queue_test 
    tests/task_queue_test.cpp
)
target_link_libraries(task_queue_test PRIVATE thread_pool)
add_test(
    NAME task_queue_test 
    COMMAND task_queue_test
)

add_executable(single_thread_executor_test
    tests/single_thread_executor_test.cpp
)
target_link_libraries(single_thread_executor_test PRIVATE thread_pool)
add_test(
    NAME single_thread_executor_test
    COMMAND single_thread_executor_test
)
