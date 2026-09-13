#pragma once

#include <functional>

#include "task_queue.hpp"

namespace learning {

class SingleThreadExecutor {
public:
    void submit(std::function<void()> task);
    void run();

private:
    ThreadSafeQueue<std::function<void()>> tasks_;
};

}  // namespace learning
