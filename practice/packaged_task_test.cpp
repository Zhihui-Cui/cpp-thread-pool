#include <cassert>
#include <future>
#include <stdexcept>
#include <thread>
#include <utility>

void test_packaged_task_and_future() {
    std::packaged_task<int()> task([] {
        return 42;
    });

    std::future<int> result = task.get_future();

    std::thread worker(std::move(task));

    int value = result.get();

    worker.join();

    assert(value == 42);
}

void test_packaged_task_propagates_exception() {
    std::packaged_task<int()> task([]() -> int {
        throw std::runtime_error("task failed!");
    });

    std::future<int> result = task.get_future();

    std::thread worker(std::move(task));

    worker.join();

    bool caught = false;

    try {
        result.get();
    } catch (const std::runtime_error&) {
        caught = true;
    }

    assert(caught);
}

int main() {
    test_packaged_task_and_future();
    test_packaged_task_propagates_exception();
    return 0;
}