#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "task_queue.hpp"

// 入队任务必须有效且不向外抛异常；packaged_task 可在内部捕获用户任务异常。
// stop() 由单一外部线程调用。
// 提交方必须在线程池开始析构前结束对它的访问。
class MinimalThreadPool {
public:
    explicit MinimalThreadPool(std::size_t worker_count);
    void submit(std::function<void()> task);
    void stop();
    ~MinimalThreadPool();

private:
    void worker_loop();

    learning::ThreadSafeQueue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;

    std::mutex state_mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
};

MinimalThreadPool::MinimalThreadPool(std::size_t worker_count) {
    if (worker_count == 0) {
        throw std::invalid_argument("worker_count must be greater than zero");
    }

    try {
        for (std::size_t i = 0; i < worker_count; ++i) {
            workers_.emplace_back([this] {
                worker_loop();
            });
        }
    } catch (...) {
        // 构造失败不会调用本类析构函数，必须回收已经启动的线程。
        stop();
        throw;
    }
}

MinimalThreadPool::~MinimalThreadPool() {
    stop();
}

void MinimalThreadPool::submit(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (stopping_) {
            throw std::runtime_error("thread pool is stopped");
        }
        tasks_.push(std::move(task));
    }

    cv_.notify_one();
}

void MinimalThreadPool::worker_loop() {
    while (true) {
        std::optional<std::function<void()>> task;

        {
            std::unique_lock<std::mutex> lock(state_mutex_);

            cv_.wait(lock, [this] {
                return !tasks_.empty() || stopping_;
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

void MinimalThreadPool::stop() {
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

void test_rejects_zero_workers() {
    bool caught = false;

    try {
        MinimalThreadPool pool(0);
    } catch (const std::invalid_argument&) {
        caught = true;
    }

    assert(caught);
}

void test_stop_completes_each_task_once() {
    for (std::size_t worker_count : {1, 2, 4}) {
        std::vector<int> hits(100, 0);
        std::mutex hits_mutex;
        MinimalThreadPool pool(worker_count);

        for (std::size_t task_id = 0; task_id < hits.size(); ++task_id) {
            pool.submit([&hits, &hits_mutex, task_id] {
                std::lock_guard<std::mutex> lock(hits_mutex);
                ++hits[task_id];
            });
        }

        pool.stop();

        // stop 已 join 所有 worker，此时读取结果不再与写入并发。
        for (int hit : hits) {
            assert(hit == 1);
        }
    }
}

void test_rejects_submission_after_stop() {
    MinimalThreadPool pool(1);
    pool.stop();
    bool caught = false;

    try {
        pool.submit([] {
        });
    } catch (const std::runtime_error&) {
        caught = true;
    }

    assert(caught);
}

void test_stop_can_be_called_repeatedly() {
    int completed = 0;
    MinimalThreadPool pool(1);

    pool.submit([&completed] {
        ++completed;
    });

    pool.stop();
    pool.stop();

    assert(completed == 1);
}

void test_destroys_without_tasks() {
    MinimalThreadPool pool(4);
}

void test_destruction_completes_tasks() {
    // 结果及其锁先于线程池创建，在池析构完成后仍然存活。
    int completed = 0;
    std::mutex completed_mutex;

    {
        MinimalThreadPool pool(4);

        for (int i = 0; i < 100; ++i) {
            pool.submit([&completed, &completed_mutex] {
                std::lock_guard<std::mutex> lock(completed_mutex);
                ++completed;
            });
        }
    }

    assert(completed == 100);
}

void test_task_can_submit_another_task() {
    std::mutex result_mutex;
    std::condition_variable result_cv;
    bool completed = false;
    MinimalThreadPool pool(1);

    pool.submit([&pool, &result_mutex, &result_cv, &completed] {
        // 如果执行用户任务时仍持有状态锁，这次 submit 会死锁。
        pool.submit([&result_mutex, &result_cv, &completed] {
            {
                std::lock_guard<std::mutex> lock(result_mutex);
                completed = true;
            }
            result_cv.notify_one();
        });
    });

    {
        std::unique_lock<std::mutex> lock(result_mutex);
        const bool ready = result_cv.wait_for(lock, std::chrono::seconds(5), [&completed] {
            return completed;
        });
        assert(ready);
    }

    // 确认内部提交已完成后再关闭，避免关闭与内部提交竞争。
    pool.stop();
}

void test_destruction_executes_each_task_once() {
    std::vector<int> hit(100, 0);
    std::mutex hit_mutex;

    {
        MinimalThreadPool pool(4);

        for (int i = 0; i < 100; ++i) {
            pool.submit([&hit, &hit_mutex, i] {
                std::lock_guard<std::mutex> lock(hit_mutex);
                ++hit[i];
            });
        }
    }

    for (int i = 0; i < 100; ++i) {
        assert(hit[i] == 1);
    }
}

void test_pool_executes_packaged_task() {
    MinimalThreadPool pool(1);

    std::future<int> result;

    {
        auto task = std::make_shared<std::packaged_task<int()>>([]() -> int {
            return 42;
        });

        result = task->get_future();

        pool.submit([task] {
            (*task)();
        });
    }

    int value = result.get();

    assert(value == 42);
}

void test_pool_propagates_packaged_task_exception() {
    bool caught = false;
    MinimalThreadPool pool(1);

    std::future<int> result;

    {
        auto task_1 = std::make_shared<std::packaged_task<int()>>([]() -> int {
            throw std::runtime_error("task failed!");
        });

        result = task_1->get_future();

        pool.submit([task_1] {
            (*task_1)();
        });
    }

    try {
        result.get();
    } catch (const std::runtime_error&) {
        caught = true;
    }

    assert(caught);

    {
        auto task_2 = std::make_shared<std::packaged_task<int()>>([]() -> int {
            return 42;
        });

        result = task_2->get_future();

        pool.submit([task_2] {
            (*task_2)();
        });
    }

    int value = result.get();

    assert(value == 42);
}

int main() {
    test_rejects_zero_workers();
    test_stop_completes_each_task_once();
    test_rejects_submission_after_stop();
    test_stop_can_be_called_repeatedly();
    test_destroys_without_tasks();
    test_destruction_completes_tasks();
    test_task_can_submit_another_task();
    test_destruction_executes_each_task_once();
    test_pool_executes_packaged_task();
    test_pool_propagates_packaged_task_exception();
    return 0;
}
