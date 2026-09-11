#include <iostream>
#include <cassert>
#include <string>
#include "task_queue.hpp"

int main() {
    learning::ThreadSafeQueue<int> queue;

    // 1
    auto result = queue.try_pop();
    assert(!result.has_value());

    // 2
    queue.push(225);
    result = queue.try_pop();
    assert(result.has_value());
    assert(result.value() == 225);

    // 3
    result = queue.try_pop();
    assert(!result.has_value());

    // 4
    queue.push(10);
    queue.push(20);
    queue.push(30);

    for(int i = 10; i <= 30; i += 10) {
        result = queue.try_pop();
        assert(result.has_value());
        assert(result.value() == i);
    }

    result = queue.try_pop();
    assert(!result.has_value());

    // 5
    queue.push(0);
    result = queue.try_pop();
    assert(result.has_value());
    assert(result.value() == 0);

    learning::ThreadSafeQueue<std::string> string_queue;
    string_queue.push("hello");

    auto string_result = string_queue.try_pop();
    assert(string_result.has_value());
    assert(string_result.value() == "hello");

    string_result = string_queue.try_pop();
    assert(!string_result.has_value());

    return 0;
}