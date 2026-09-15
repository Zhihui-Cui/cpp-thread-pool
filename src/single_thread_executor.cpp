#include "single_thread_executor.hpp"

#include <utility>

namespace learning {

void SingleThreadExecutor::submit(std::function<void()> task) {
    tasks_.push(std::move(task));
}

void SingleThreadExecutor::run() {
    while (true) {
        auto task = tasks_.try_pop();
        if (!task.has_value()) {
            break;
        }

        task.value()();
    }
}

}  // namespace learning
