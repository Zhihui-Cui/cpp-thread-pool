#include "single_thread_executor.hpp"

#include <cassert>
#include <vector>

namespace {

void test_submit_defers_execution_and_run_preserves_order() {
    learning::SingleThreadExecutor executor;
    std::vector<int> order;

    for (int i = 1; i <= 3; ++i) {
        executor.submit([&order, i] {
            order.push_back(i);
        });
    }

    assert(order.empty());
    executor.run();
    assert((order == std::vector<int>{1, 2, 3}));
}

void test_run_does_not_repeat_completed_tasks() {
    learning::SingleThreadExecutor executor;
    int execution_count = 0;

    executor.submit([&execution_count] {
        ++execution_count;
    });

    executor.run();
    assert(execution_count == 1);

    executor.run();
    assert(execution_count == 1);
}

}  // namespace

int main() {
    test_submit_defers_execution_and_run_preserves_order();
    test_run_does_not_repeat_completed_tasks();
    return 0;
}
