#pragma once

#include <queue>
#include <mutex>
#include <optional>
#include <utility>

namespace learning {
    template <typename T>
    class ThreadSafeQueue {
    public:
        void push(T task) {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.push(std::move(task));
        }

        bool empty() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return tasks_.empty();
        }

        std::optional<T> try_pop() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (tasks_.empty())
                return std::nullopt;
            T task = std::move(tasks_.front());
            tasks_.pop();
            return task;
        }

    private:
        std::queue<T> tasks_;
        mutable std::mutex mutex_;
    };
}