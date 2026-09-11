cmake_minimum_required(VERSION 3.20)

project(cpp_thread_pool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(thread_pool src/thread_pool.cpp)
target_include_directories(thread_pool
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(thread_pool PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

find_package(Threads REQUIRED)
target_link_libraries(thread_pool PUBLIC Threads::Threads)

add_executable(task_queue_test tests/task_queue_test.cpp)
target_link_libraries(task_queue_test PRIVATE thread_pool)

enable_testing()
add_test(NAME task_queue_test COMMAND task_queue_test)