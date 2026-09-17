#include "thread_pool.hpp"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

#include "task_queue.hpp"

void test_thread_updates_value() {
    int value = 0;

    std::thread worker([&value] {
        value = 42;
    });

    worker.join();

    assert(value == 42);
}

void test_multiple_workers_update_count() {
    int count = 0;
    std::mutex mutex;
    std::vector<std::thread> workers;

    for (int i = 0; i < 4; i++) {
        workers.emplace_back([&mutex, &count] {
            std::lock_guard<std::mutex> lock(mutex);
            ++count;
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    assert(count == 4);
}

void test_multiple_workers_execute_queued_tasks() {
    learning::ThreadSafeQueue<std::function<void()>> tasks;
    std::vector<int> hits(1000, 0);
    std::mutex result_mutex;
    std::vector<std::thread> workers;

    for (int i = 0; i < 1000; ++i) {
        tasks.push([&hits, &result_mutex, i] {
            std::lock_guard<std::mutex> lock(result_mutex);
            ++hits[i];
        });
    }

    for (int i = 0; i < 4; ++i) {
        workers.emplace_back([&tasks] {
            while (true) {
                auto result = tasks.try_pop();
                if (!result.has_value()) {
                    break;
                }

                result.value()();
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    for (int hit : hits) {
        assert(hit == 1);
    }
    assert(tasks.empty());
}

void test_thread_pool_executes_each_task_once(std::size_t worker_count) {
    std::vector<int> hits(1000, 0);
    std::mutex result_mutex;

    {
        learning::ThreadPool pool(worker_count);

        for (int i = 0; i < 1000; ++i) {
            pool.submit([&hits, &result_mutex, i] {
                std::lock_guard<std::mutex> lock(result_mutex);
                ++hits[i];
            });
        }
    }

    for (int i = 0; i < 1000; ++i) {
        assert(hits[i] == 1);
    }
}

void test_thread_pool_rejects_zero_workers() {
    bool caught = false;

    try {
        learning::ThreadPool pool(0);
    } catch (const std::invalid_argument&) {
        caught = true;
    }

    assert(caught);
}

void test_thread_pool_destroys_without_tasks() {
    learning::ThreadPool pool(4);
}

void test_task_completes_before_destruction() {
    bool completed = false;
    std::mutex result_mutex;
    std::condition_variable result_cv;
    learning::ThreadPool pool(1);

    for (int i = 0; i < 3; ++i) {
        {
            std::lock_guard<std::mutex> lock(result_mutex);
            completed = false;
        }

        pool.submit([&] {
            {
                std::lock_guard<std::mutex> lock(result_mutex);
                completed = true;
            }

            result_cv.notify_one();
        });

        std::unique_lock<std::mutex> lock(result_mutex);

        bool res = result_cv.wait_for(lock, std::chrono::seconds(2), [&completed] {
            return completed;
        });

        assert(res);
    }
}

int main() {
    test_thread_updates_value();
    test_multiple_workers_update_count();
    test_multiple_workers_execute_queued_tasks();

    for (std::size_t i = 1; i <= 4; i *= 2) {
        test_thread_pool_executes_each_task_once(i);
    }

    test_thread_pool_rejects_zero_workers();
    test_thread_pool_destroys_without_tasks();
    test_task_completes_before_destruction();
    return 0;
}
