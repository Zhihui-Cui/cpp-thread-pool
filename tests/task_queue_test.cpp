#include "task_queue.hpp"

#include <cassert>
#include <functional>
#include <string>

namespace {

void test_empty_queue_returns_no_value() {
    learning::ThreadSafeQueue<int> queue;

    assert(queue.empty());
    auto result = queue.try_pop();
    assert(!result.has_value());
}

void test_push_pop_single_value() {
    learning::ThreadSafeQueue<int> queue;

    queue.push(225);
    assert(!queue.empty());

    auto result = queue.try_pop();
    assert(result.has_value());
    assert(result.value() == 225);
    assert(queue.empty());

    result = queue.try_pop();
    assert(!result.has_value());
}

void test_fifo_order() {
    learning::ThreadSafeQueue<int> queue;
    queue.push(10);
    queue.push(20);
    queue.push(30);

    for (int expected = 10; expected <= 30; expected += 10) {
        auto result = queue.try_pop();
        assert(result.has_value());
        assert(result.value() == expected);
    }

    assert(queue.empty());
    auto result = queue.try_pop();
    assert(!result.has_value());
}

void test_zero_is_a_valid_value() {
    learning::ThreadSafeQueue<int> queue;
    queue.push(0);

    auto result = queue.try_pop();
    assert(result.has_value());
    assert(result.value() == 0);
}

void test_string_value() {
    learning::ThreadSafeQueue<std::string> queue;
    queue.push("hello");

    auto result = queue.try_pop();
    assert(result.has_value());
    assert(result.value() == "hello");

    result = queue.try_pop();
    assert(!result.has_value());
}

void test_callable_is_stored_until_invoked() {
    learning::ThreadSafeQueue<std::function<void()>> queue;
    int value = 0;

    queue.push([&value] {
        value = 42;
    });
    assert(value == 0);

    auto task = queue.try_pop();
    assert(task.has_value());
    assert(queue.empty());
    assert(value == 0);

    task.value()();
    assert(value == 42);
}

}  // namespace

int main() {
    test_empty_queue_returns_no_value();
    test_push_pop_single_value();
    test_fifo_order();
    test_zero_is_a_valid_value();
    test_string_value();
    test_callable_is_stored_until_invoked();
    return 0;
}
