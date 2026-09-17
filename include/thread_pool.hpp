#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "task_queue.hpp"

namespace learning {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t worker_count);
    ~ThreadPool();

    void submit(std::function<void()> task);

private:
    void worker_loop();

    ThreadSafeQueue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;

    std::mutex state_mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
};

}  // namespace learning
