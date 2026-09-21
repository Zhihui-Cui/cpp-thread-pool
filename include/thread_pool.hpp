#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "task_queue.hpp"

namespace learning {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t worker_count);
    ~ThreadPool();

    template <typename F>
    auto submit(F function) -> std::future<decltype(function())> {
        using ReturnType = decltype(function());

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(std::move(function));

        auto result = task->get_future();

        this->enqueue([task] {
            (*task)();
        });

        return result;
    }

    template <typename F, typename Arg, typename... Args>
    auto submit(F function, Arg argument, Args... args)
        -> std::future<decltype(function(argument, args...))> {

        auto lambda = [saved_function = std::move(function),
                       saved_arguments =
                           std::make_tuple(std::move(argument), std::move(args)...)]() mutable {
            return std::apply(saved_function, saved_arguments);
        };

        auto result = this->submit(std::move(lambda));

        return result;
    }

private:
    void worker_loop();
    void enqueue(std::function<void()> task);

    ThreadSafeQueue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;

    std::mutex state_mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
};

}  // namespace learning
