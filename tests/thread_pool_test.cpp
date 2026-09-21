#include "thread_pool.hpp"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "task_queue.hpp"

void test_thread_updates_value() {
    int value = 0;

    std::thread worker([&value] {
        value = 225;
    });

    worker.join();

    assert(value == 225);
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

void test_packaged_task_provides_result() {
    std::packaged_task<int()> task([] {
        return 225;
    });

    std::future<int> result = task.get_future();

    task();

    int value = result.get();

    assert(value == 225);
}

void test_packaged_task_provides_result_from_worker() {
    std::packaged_task<int()> task([] {
        return 225;
    });

    std::future<int> result = task.get_future();

    std::thread worker([&task] {
        task();
    });

    int value = result.get();

    worker.join();

    assert(value == 225);
}

void test_thread_pool_executes_packaged_task() {
    std::packaged_task<int()> task([] {
        return 225;
    });

    std::future result = task.get_future();

    learning::ThreadPool pool(1);

    pool.submit([&task] {
        task();
    });

    int value = result.get();

    assert(value == 225);
}

void test_thread_pool_executes_shared_packaged_task() {
    auto task = std::make_shared<std::packaged_task<int()>>([] {
        return 225;
    });

    auto result = task->get_future();

    learning::ThreadPool pool(1);

    pool.submit([task] {
        (*task)();
    });

    int value = result.get();

    assert(value == 225);
}

void test_submit_provides_int_result() {
    learning::ThreadPool pool(1);

    auto result = pool.submit([] {
        return 42;
    });

    int value = result.get();

    assert(value == 42);
}

void test_submit_propagates_exception() {
    learning::ThreadPool pool(1);
    bool caught = false;

    auto result = pool.submit([]() -> int {
        throw std::runtime_error("task failed");
    });

    try {
        result.get();
    } catch (const std::runtime_error&) {
        caught = true;
    }

    assert(caught);

    auto next_result = pool.submit([] {
        return 42;
    });

    int value = next_result.get();

    assert(value == 42);
}

void test_submit_provides_double_result() {
    learning::ThreadPool pool(1);

    std::future<double> result = pool.submit([] {
        return 2.5;
    });

    double value = result.get();

    assert(value == 2.5);
}

void test_submit_handles_void_task() {
    int value = 0;
    learning::ThreadPool pool(1);

    std::future<void> result = pool.submit([&value] {
        value = 225;
    });

    result.get();

    assert(value == 225);
}

void test_submit_preserves_value_capture() {
    int number = 40;
    learning::ThreadPool pool(1);

    auto result = pool.submit([number] {
        return number + 2;
    });

    number = 100;

    int value = result.get();

    assert(value == 42);
}

void test_submit_wraps_argument_task() {
    int argument = 40;
    learning::ThreadPool pool(1);
    auto add_two = [](int number) {
        return number + 2;
    };

    auto result = pool.submit([add_two, argument] {
        return add_two(argument);
    });

    argument = 100;

    int value = result.get();

    assert(value == 42);
}

void test_submit_accepts_one_argument() {
    int argument = 40;
    learning::ThreadPool pool(1);
    auto add_two = [](int number) {
        return number + 2;
    };

    std::future<int> result = pool.submit(add_two, argument);

    argument = 100;

    int value = result.get();

    assert(value == 42);
}

void test_submit_accepts_mutable_task_with_argument() {
    int count = 0;
    learning::ThreadPool pool(1);

    auto accumulate = [count](int step) mutable {
        count += step;
        return count;
    };

    auto result = pool.submit(accumulate, 2);

    int value = result.get();

    assert(value == 2);
    assert(count == 0);
}

void test_submit_accepts_move_only_task_with_argument() {
    learning::ThreadPool pool(1);
    auto number = std::make_unique<int>(40);

    auto add = [saved_number = std::move(number)](int step) {
        return *saved_number + step;
    };

    std::future<int> result = pool.submit(std::move(add), 2);

    int value = result.get();

    assert(value == 42);
}

void test_apply_passes_tuple_elements() {
    auto add = [](int a, double b) {
        return a + b;
    };

    auto arguments = std::make_tuple(10, 2.5);

    double value = std::apply(add, arguments);

    assert(value == 12.5);
}

void test_submit_accepts_multiple_arguments() {
    learning::ThreadPool pool(1);

    auto calculate = [](int first, int second, double third) {
        return first - second + third;
    };

    std::future<double> result = pool.submit(calculate, 20, 5, 0.5);

    double value = result.get();

    assert(value == 15.5);
}

void test_submit_handles_void_task_with_arguments() {
    int value = 0;
    learning::ThreadPool pool(1);

    auto store_sum = [&value](int a, int b) {
        value = a + b;
    };

    std::future<void> result = pool.submit(store_sum, 22, 20);

    result.get();

    assert(value == 42);
}

void test_submit_propagates_exception_with_arguments() {
    bool caught = false;
    learning::ThreadPool pool(1);

    auto divide = [](int a, int b) {
        if (b == 0) {
            throw std::runtime_error("division by zero");
        }
        return a / b;
    };

    auto result = pool.submit(divide, 10, 0);

    try {
        result.get();
    } catch (const std::runtime_error&) {
        caught = true;
    }

    assert(caught);
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
    test_packaged_task_provides_result();
    test_packaged_task_provides_result_from_worker();
    test_thread_pool_executes_packaged_task();
    test_thread_pool_executes_shared_packaged_task();
    test_submit_provides_int_result();
    test_submit_propagates_exception();
    test_submit_provides_double_result();
    test_submit_handles_void_task();
    test_submit_preserves_value_capture();
    test_submit_wraps_argument_task();
    test_submit_accepts_one_argument();
    test_submit_accepts_mutable_task_with_argument();
    test_submit_accepts_move_only_task_with_argument();
    test_apply_passes_tuple_elements();
    test_submit_accepts_multiple_arguments();
    test_submit_handles_void_task_with_arguments();
    test_submit_propagates_exception_with_arguments();
    return 0;
}
