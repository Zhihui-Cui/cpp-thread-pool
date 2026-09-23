#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <iomanip>
#include <iostream>
#include <vector>

#include "thread_pool.hpp"

std::uint64_t compute_task(std::uint64_t seed, std::size_t iterations) {
    std::uint64_t value = seed;

    for (std::size_t i = 0; i < iterations; ++i) {
        value = value * 1664525ULL + 1013904223ULL;
    }

    return value;
}

struct BenchmarkResult {
    double elapsed_ms;
    std::uint64_t checksum;
};

BenchmarkResult run_benchmark(std::size_t worker_count, std::size_t task_count,
                              std::size_t iterations) {
    learning::ThreadPool pool(worker_count);

    std::vector<std::future<std::uint64_t>> results;
    results.reserve(task_count);

    std::uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();

    for (std::size_t i = 0; i < task_count; ++i) {
        results.push_back(pool.submit(compute_task, static_cast<std::uint64_t>(i + 1), iterations));
    }

    for (auto& result : results) {
        checksum += result.get();
    }

    const auto end = std::chrono::steady_clock::now();
    const double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    return {elapsed_ms, checksum};
}

int main() {
    constexpr std::size_t task_count = 1000;
    constexpr std::size_t iterations = 100000;

    // 在计时之外串行计算，作为结果校验的基准。
    std::uint64_t expected_checksum = 0;
    for (std::size_t i = 0; i < task_count; ++i) {
        expected_checksum += compute_task(static_cast<std::uint64_t>(i + 1), iterations);
    }

    std::cout << "tasks=" << task_count << ", iterations=" << iterations << '\n';
    std::cout << std::fixed << std::setprecision(3);

    for (std::size_t workers : {1, 2, 4}) {
        const auto warmup = run_benchmark(workers, task_count, iterations);
        if (warmup.checksum != expected_checksum) {
            std::cerr << "Warmup checksum mismatch: workers=" << workers << '\n';
            return 1;
        }

        std::vector<double> elapsed_times;
        elapsed_times.reserve(5);

        for (std::size_t round = 0; round < 5; ++round) {
            const auto result = run_benchmark(workers, task_count, iterations);

            if (result.checksum != expected_checksum) {
                std::cerr << "Checksum mismatch: workers=" << workers << ", round=" << round + 1
                          << '\n';
                return 1;
            }

            elapsed_times.push_back(result.elapsed_ms);

            std::cout << "workers=" << workers << ", round=" << round + 1
                      << ", elapsed_ms=" << result.elapsed_ms << ", checksum=" << result.checksum
                      << '\n';
        }

        std::sort(elapsed_times.begin(), elapsed_times.end());

        std::cout << "workers=" << workers
                  << ", median_ms=" << elapsed_times[elapsed_times.size() / 2] << '\n';
    }

    return 0;
}