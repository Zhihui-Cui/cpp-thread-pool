#include "thread_pool.hpp"

#include <optional>
#include <stdexcept>
#include <utility>

namespace learning {

ThreadPool::ThreadPool(std::size_t worker_count) {
    if (worker_count == 0) {
        throw std::invalid_argument("参数不合法");
    }

    try {
        for (std::size_t i = 0; i < worker_count; i++) {
            workers_.emplace_back([this] {
                worker_loop();
            });
        }
    } catch (...) {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            stopping_ = true;
        }

        cv_.notify_all();

        for (auto& worker : workers_) {
            worker.join();
        }

        throw;
    }
}

void ThreadPool::worker_loop() {
    while (true) {
        std::optional<std::function<void()>> task;

        {
            std::unique_lock<std::mutex> lock(state_mutex_);

            cv_.wait(lock, [this] {
                return stopping_ || !tasks_.empty();
            });

            task = tasks_.try_pop();

            if (!task.has_value() && stopping_) {
                return;
            }
        }

        if (task.has_value()) {
            task.value()();
        }
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        stopping_ = true;
    }

    cv_.notify_all();

    for (auto& worker : workers_) {
        worker.join();
    }
}

void ThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        tasks_.push(std::move(task));
    }

    cv_.notify_one();
}

}  // namespace learning
