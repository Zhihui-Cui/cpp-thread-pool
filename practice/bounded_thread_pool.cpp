#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

class BoundedThreadPool {
public:
    BoundedThreadPool(std::size_t worker_count, std::size_t queue_capacity);
    void submit(std::function<void()> task);
    void stop();
    ~BoundedThreadPool();

private:
    void worker_loop();

    std::queue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;

    std::mutex state_mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
    const std::size_t queue_capacity_;
};

BoundedThreadPool::BoundedThreadPool(std::size_t worker_count, std::size_t queue_capacity)
    : queue_capacity_(queue_capacity) {
    try {
        if (worker_count == 0) {
            throw std::invalid_argument("Invalid worker_count!");
        }

        if (queue_capacity == 0) {
            throw std::invalid_argument("Invalid queue_capacity");
        }

        workers_.reserve(worker_count);

        for (std::size_t i = 0; i < worker_count; ++i) {
            workers_.emplace_back([this] {
                worker_loop();
            });
        }
    } catch (...) {
        this->stop();
        throw;
    }
}

void BoundedThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (stopping_) {
            throw std::runtime_error("Thread pool is stopped!");
        }

        if (tasks_.size() >= queue_capacity_) {
            throw std::runtime_error("Task queue is full!");
        }

        tasks_.push(std::move(task));
    }

    cv_.notify_one();
}

void BoundedThreadPool::worker_loop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(state_mutex_);

            cv_.wait(lock, [this] {
                return !tasks_.empty() || stopping_;
            });

            if (tasks_.empty() && stopping_) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
        }

        task();
    }
}

void BoundedThreadPool::stop() {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        stopping_ = true;
    }

    cv_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

BoundedThreadPool::~BoundedThreadPool() {
    this->stop();
}

void test_rejects_submission_when_queue_full() {
    std::promise<void> start_promise;
    auto start_future = start_promise.get_future();

    std::promise<void> release_promise;
    auto release_future = release_promise.get_future();

    int a_completed = 0;
    int b_completed = 0;
    int c_completed = 0;
    bool rejected = false;

    BoundedThreadPool pool(1, 1);

    auto A = [&] {
        start_promise.set_value();
        release_future.wait();
        ++a_completed;
    };

    auto B = [&b_completed] {
        ++b_completed;
    };

    auto C = [&c_completed] {
        ++c_completed;
    };

    pool.submit(A);

    start_future.wait();

    try {
        pool.submit(B);

        try {
            pool.submit(C);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
    } catch (...) {
        release_promise.set_value();
        pool.stop();
        throw;
    }
    release_promise.set_value();
    pool.stop();

    assert(a_completed == 1);
    assert(b_completed == 1);
    assert(c_completed == 0 && rejected);
}

int main() {
    test_rejects_submission_when_queue_full();
    return 0;
}
