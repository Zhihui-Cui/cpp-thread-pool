# C++ Thread Pool

一个以学习为目标的 C++17 线程池项目，逐步练习任务队列、任务执行、线程同步和工程组织。

当前已实现泛型线程安全队列、单线程任务执行器，以及支持指定 worker 数量和析构回收的线程池。空闲 worker 使用条件变量阻塞等待；模板 `submit()` 支持无参数、带捕获及带多个参数的任务，通过 future 获取返回值或任务异常。公开 `stop()` 支持拒绝新任务、完成已接受任务并等待 worker 退出；Issue #6 已完成验收并关闭。

## 学习路线与进度

下表记录本地实现进度；GitHub Issue 的关闭在测试、总结和推送完成后进行。

| 阶段 | 学习主题 | 实现进度 |
| --- | --- | --- |
| [Issue #1](https://github.com/Zhihui-Cui/cpp-thread-pool/issues/1) | 泛型线程安全任务队列 | 已实现并测试，Issue 已关闭 |
| [Issue #2](https://github.com/Zhihui-Cui/cpp-thread-pool/issues/2) | 单线程任务执行器 | 已实现并测试，Issue 已关闭 |
| [Issue #3](https://github.com/Zhihui-Cui/cpp-thread-pool/issues/3) | 多 worker 生命周期管理 | 已实现并测试，Issue 已关闭 |
| [Issue #4](https://github.com/Zhihui-Cui/cpp-thread-pool/issues/4) | 阻塞等待与唤醒 | 已实现并测试，Issue 已关闭 |
| [Issue #5](https://github.com/Zhihui-Cui/cpp-thread-pool/issues/5) | submit 与 future | 已实现并测试，Issue 已关闭 |
| [Issue #6](https://github.com/Zhihui-Cui/cpp-thread-pool/issues/6) | 优雅关闭 | 已实现并测试，Issue 已关闭 |
| [Issue #7](https://github.com/Zhihui-Cui/cpp-thread-pool/issues/7) | 测试、benchmark 与项目说明 | benchmark 已实现并运行，构建与测试通过；文档待本人查阅，复盘与归档进行中 |

每个阶段完成验收和收尾后，再推进下一阶段。个人理解与踩坑记录写在 [设计与学习记录](docs/design-notes.md)。

## 项目结构

```text
include/
    task_queue.hpp                  模板队列的声明与实现
    single_thread_executor.hpp      单线程执行器声明
    thread_pool.hpp                 线程池声明、模板 submit 与 future 包装
src/
    single_thread_executor.cpp      单线程执行器实现
    thread_pool.cpp                 worker 创建、阻塞等待、任务执行与退出回收
tests/
    task_queue_test.cpp             队列测试
    single_thread_executor_test.cpp 单线程执行器测试
    thread_pool_test.cpp            线程基础、生命周期、返回值、异常与参数传递测试
benchmarks/
    thread_pool_benchmark.cpp       比较 1/2/4 个 worker，校验结果并输出五轮耗时及中位数
docs/
    design-notes.md                 各阶段设计、学习总结与验收记录
CMakeLists.txt                      构建目标、依赖和测试注册
README.md                           项目概览与使用说明
```

模板队列和模板 `submit()` 的实现保留在头文件中，供使用它们的代码按具体类型实例化。普通执行器以及线程池的 worker、入队、构造和析构逻辑采用头文件声明、源文件实现的组织方式。

代码排版与日常编写约定见 [代码风格](docs/coding-style.md)，自动格式化规则保存在项目根目录的 `.clang-format` 中。

## 环境与构建

### 当前开发环境

- Windows + PowerShell。
- CMake 3.20 或更新版本。
- MSYS2 UCRT64 提供的 GCC/G++ 和 Ninja。
- C++17，使用 Debug 配置运行当前的断言测试。

以下工具路径对应当前开发机器；在其他机器上请调整为实际安装路径。

### 首次配置

在项目根目录运行：

```powershell
cmake -S . -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=C:/msys64/ucrt64/bin/ninja.exe
```

### 日常构建与测试

```powershell
cmake --build build-ninja
ctest --test-dir build-ninja --output-on-failure
```

需要沿用已有构建目录的工具链配置、重新生成构建规则时，执行：

```powershell
cmake -S . -B build-ninja
```

`ctest` 运行已编译的程序，不负责重新编译。修改代码后先确认构建成功，再运行测试。当前测试使用 `assert`；定义 `NDEBUG` 的构建会禁用这些断言。

## 当前功能与用法

### 线程安全队列

- `ThreadSafeQueue<T>` 提供 `push()`、`try_pop()` 和 `empty()`。
- 队列为空时，`try_pop()` 返回 `std::nullopt`。
- 三个操作使用同一个互斥量保护队列。
- `empty()` 只表示检查时的状态；取任务时直接处理 `try_pop()` 的结果。

### 单线程任务执行器

- `submit()` 保存任务，不立即执行。
- `run()` 在调用它的线程中按入队顺序执行任务，直到队列为空。
- 当前执行器不创建后台线程。

最小用法：

```cpp
#include <iostream>
#include "single_thread_executor.hpp"

int main() {
    learning::SingleThreadExecutor executor;

    executor.submit([] {
        std::cout << "hello task\n";
    });

    executor.run();
}
```

可运行的使用与验收示例见 [执行器测试](tests/single_thread_executor_test.cpp)。

### 多 worker 线程池

- 构造时指定正的 worker 数量，传入 `0` 抛出 `std::invalid_argument`。
- 构造函数启动 worker；`submit()` 包装用户任务并返回 future，交给私有 `enqueue()` 入队的外层任务仍是 `std::function<void()>`。计算可能在提交返回前开始，甚至已经完成。
- worker 使用带谓词的 `condition_variable::wait()`，在队列为空且没有停止请求时阻塞等待；取到任务后解锁再执行，不保证任务的完成顺序或均匀分配。
- `enqueue()` 在 `state_mutex_` 的同一次持锁期间检查停止状态并入队，已停止时抛出异常；成功入队后解锁并调用 `notify_one()`。等待条件检查与入队使用同一把外层锁配合，队列内部的锁继续保护队列操作。
- `stop()` 设置停止标志，解锁后调用 `notify_all()`，worker 完成剩余任务后退出，再由调用方 join 所有 worker。析构调用 `stop()`；构造中途失败时也按设置状态、解锁、通知、join 的顺序回收已创建的线程，再重新抛出异常。

```cpp
#include "thread_pool.hpp"

#include <cassert>

int main() {
    int result = 0;

    {
        learning::ThreadPool pool(2);
        pool.submit([&result] {
            result = 42;
        });
    } // 析构等待 worker 结束，result 仍然存活。

    assert(result == 42);
}
```

该例只有一个任务写 `result`，主线程在析构等待完成后读取。多个任务并发修改同一份结果时，需要额外同步。提交应在线程池析构开始前结束，引用捕获的数据必须保持存活，不在池自身的 worker 中销毁线程池。任务抛出的 C++ 异常由包装任务保存，需要保留 future 并调用 `get()` 才能在调用方观察到；上例未保存 future，仅用于展示析构等待。

## 测试

| 测试程序 | 当前覆盖的行为 |
| --- | --- |
| `task_queue_test` | 空队列、单元素存取、先进先出、零值、字符串、可调用对象存取 |
| `single_thread_executor_test` | 提交时不执行、按顺序执行、完成的任务不重复执行 |
| `thread_pool_test` | 线程基础、共享队列消费、1/2/4 个 worker 各正确执行 1,000 个任务、拒绝零 worker、空任务析构、三轮析构前完成标记；future 获取 int/double/void 结果、任务异常及异常后继续执行、值捕获、单个及多个参数、mutable 任务、不可复制任务；保留 packaged_task 与 tuple/apply 的基础练习 |

目前 CTest 注册了三个测试程序。一个程序中的多个测试函数不会分别计入 CTest 的测试数量。已检查同步实现并运行上述测试；尚未使用数据竞争检测器，也未模拟构造中途创建线程失败。详细证据与限制见设计笔记。

Issue #7 本人提供的验证输出为 3/3 Passed，总耗时 0.51 秒。2026-09-23 对包含 `bound_task` 重命名的代码再次执行 `cmake --build build-ninja` 和 `ctest --test-dir build-ninja --output-on-failure --timeout 30`，构建成功，3/3 Passed，总耗时 1.17 秒；已确认该目录为 Debug 配置。

Issue #4 的完成标记测试在每轮提交后使用带谓词的 `wait_for()` 等待，并在线程池析构前断言结果；不能据此保证提交时 worker 已进入阻塞，也未测量空闲 CPU 使用率或主动制造虚假唤醒。

Issue #5 的正式接口测试直接调用 `pool.submit()` 并通过 future 检查结果。测试没有固定 worker 与提交返回的先后顺序，也没有证明每次 `get()` 都实际阻塞过。具体断言、运行记录和未覆盖范围见设计笔记。

## 线程池架构

任务从提交到结果获取的流程如下：

```text
提交线程：submit(函数, 参数)
              ↓ 保存函数与参数，包装为 packaged_task，取得 future
           enqueue(捕获 shared_ptr 的可复制任务)
              ↓ 锁内检查停止状态并入队，解锁后 notify_one
共享队列：std::function<void()>
              ↓ worker 在状态锁内等待条件成立并取出任务
工作线程：解锁 → 执行任务 → packaged_task 保存返回值或异常
                                      ↓
调用线程：future.get() ← 等待并读取共享状态中的结果或异常
```

带参数的 `submit()` 先用 lambda 和 tuple 保存函数与参数，再交给无参数重载包装。队列中的外层任务捕获 `shared_ptr`，使不可复制的 `packaged_task` 可以通过可复制的 lambda 放入 `std::function<void()>`，并保持存活到执行时。

worker 由构造函数创建，循环消费共享队列。`state_mutex_` 协调停止状态、入队与等待条件检查，队列内部互斥量保护队列操作。条件变量在“已请求停止或队列非空”时允许 worker 继续；等待期间释放状态锁，醒来后重新持锁检查。用户任务在锁外执行，因此多个 worker 可以并行计算。任务完成顺序不保证与提交顺序一致。

`stop()` 在状态锁内设置停止标志，解锁后唤醒所有 worker，再 join。worker 继续执行已接受任务，直到停止且取不到任务才退出。future 负责结果交付，join 负责线程回收，两者用途不同。

## 返回值与异常

当前接口分为无任务参数和至少一个任务参数两个重载，均返回任务结果类型对应的 future：

```cpp
template <typename F>
auto submit(F function) -> std::future<decltype(function())>;

template <typename F, typename Arg, typename... Args>
auto submit(F function, Arg argument, Args... args)
    -> std::future<decltype(function(argument, args...))>;
```

可运行示例：

```cpp
#include "thread_pool.hpp"

#include <cassert>
#include <stdexcept>

int main() {
    int stored = 0;
    learning::ThreadPool pool(2);

    auto answer = pool.submit([] { return 42; });
    int value = answer.get();
    assert(value == 42);

    auto sum = pool.submit([](int a, int b) { return a + b; }, 20, 22);
    int total = sum.get();
    assert(total == 42);

    auto completed = pool.submit([&stored] { stored = 42; });
    completed.get(); // future<void>：等待任务完成，不返回数值。
    assert(stored == 42);

    auto failed = pool.submit([]() -> int {
        throw std::runtime_error("task failed");
    });

    bool caught = false;
    try {
        failed.get();
    } catch (const std::runtime_error&) {
        caught = true;
    }
    assert(caught);
}
```

主线程准备任务和 future；worker 执行包装任务，将返回值或异常保存到共享状态。`get()` 在状态未就绪时等待，就绪后返回值或重新抛出保存的异常。普通 future 的 `get()` 只能获取一次，包括取出异常的情况；它等待结果，不代替线程池析构中的 `join()`。

`get()` 正常返回后，可读取该任务在完成前写入的数据。它不保护多个任务同时修改同一变量。忽略 future 不会取消任务，也不会让任务异常自动在主线程抛出；需要观察失败时，应保存 future 并处理 `get()`。

带参数重载按值接收任务和参数，将它们移动保存到 lambda 与 tuple 中，再由 worker 使用 `std::apply` 调用。普通变量传给按值参数时先复制，因此后续修改原变量不改变保存的整数参数。需要共享外部数据时，可使用引用捕获，并自行保证生命周期与同步。

## 关闭语义

公开接口为 `void stop();`，采用完成已接受任务后退出的关闭方式。

- **停止接收新任务**：`stop()` 在锁内设置停止标志。此后 `submit()` 在入队检查时直接抛出 `std::runtime_error("thread pool is stopped")`，本次调用不会返回 future。该异常发生在提交线程中，与任务执行异常通过 `future.get()` 取回不同。
- **完成已接受任务**：正在执行和已经入队的任务继续执行，不主动取消任务。正常返回值或任务抛出的 C++ 异常仍由 future 保存；`stop()` 不代替 `get()` 读取结果或报告任务异常。
- **等待关闭完成**：设置停止标志后解锁，唤醒所有等待的 worker，再逐个 join。worker 只有在停止状态下取不到任务时才退出。`stop()` 正常返回时，所有 worker 都已结束并完成 join；如果任务一直不结束，关闭也会一直等待。
- **重复关闭与析构**：支持同一调用线程依次重复调用 `stop()`；已 join 的线程会被跳过。析构也调用 `stop()`，因此显式关闭后仍可正常析构。停止后的对象不能重新启动，需要执行新任务时应创建新线程池。

### 并发与生命周期约定

对象保持存活时，允许提交线程与一个调用 `stop()` 的线程并发工作。检查停止状态、入队和设置停止状态使用同一把 `state_mutex_`：在停止标志设置前成功入队的任务会被执行，之后的提交会被拒绝。仅凭两个函数开始调用的先后，不能判断并发提交是否被接受。

不支持多个线程同时调用 `stop()`，因为 `joinable()` 检查与 `join()` 没有额外的并发保护。也不能在池自身的 worker 中调用 `stop()` 或销毁该线程池，以免尝试 join 当前线程。析构开始前，外部的提交和关闭调用应已结束；引用捕获的数据必须存活到任务执行结束。即使允许并发提交与关闭，也不意味着可以并发访问正在析构的对象。

### 关闭测试与验证范围

以下测试均位于 `tests/thread_pool_test.cpp`，并在 `main()` 中调用：

| 测试 | 验证内容 |
| --- | --- |
| `test_submit_rejected_after_stop` | 显式 stop 后提交抛出预期异常；随后正常析构 |
| `test_stop_completes_submitted_tasks` | 提交 1,000 个任务，stop 返回后、析构前检查每个任务恰好执行一次 |
| `test_repeated_pool_shutdown` | 20 轮创建新池，交替覆盖空池和有任务场景；每轮依次 stop 两次，再析构 |
| `test_stop_drains_pending_tasks` | 单 worker 的 A 等待放行，B、C 排队；关闭线程调用 stop，主线程观察到提交被拒绝后才放行 A；等待关闭线程结束，再检查三个任务结果 |

最后一项通过同步控制任务顺序，不依赖固定睡眠。提交探测有两秒时限；即使探测未成功，也先放行 A 并回收关闭线程，再断言失败。该时限不覆盖整个测试，运行时使用 CTest 超时保护：

```powershell
cmake --build build-ninja
ctest --test-dir build-ninja --output-on-failure --timeout 30
```

本阶段最近一次核验为 2026-09-22：构建输出 `ninja: no work to do.`，CTest 为 `3/3 Passed`。三个测试程序包含上述多个测试函数。测试通过不代表覆盖所有并发调度；尚未运行数据竞争检测器，也没有验证多个并发 stop 或 worker 内部 stop，这些用法不属于当前支持范围。Issue #6 已完成学习总结、验收及 GitHub 归档。

## Benchmark

### 测量环境与负载

- 系统：Windows，使用 PowerShell。
- CPU：12th Gen Intel Core i5-1240P，12 核心、16 逻辑处理器。
- 编译器：MSYS2 UCRT64 GCC 16.2.0（Rev3, Built by MSYS2 project）。
- 构建：CMake + Ninja，C++17，Release。
- 负载：1000 个独立任务，每个任务执行 100000 轮无符号整数乘加运算；任务初始值为 1～1000。
- worker 数量：1、2、4。

### 构建与运行

在项目根目录依次执行，工具路径需按实际安装位置调整：

```powershell
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=C:/msys64/ucrt64/bin/ninja.exe
cmake --build build-release --target thread_pool_benchmark
.\build-release\thread_pool_benchmark.exe
```

benchmark 是独立可执行程序，不注册到 CTest。功能测试继续使用 Debug，保证 `assert` 生效；benchmark 的结果校验使用 `if`，在 Release 中仍然执行。`build-release/` 已加入 Git 忽略规则。

### 测量方法

每种 worker 配置先预热一次，再正式测量五次，将耗时排序并取中位数。每次调用 `run_benchmark()` 都创建新的线程池；预热执行同类负载，不复用上一轮 worker。配置按 1、2、4 的固定顺序运行。

使用 `std::chrono::steady_clock`，从提交第一个任务之前计时，到所有 future 的 `get()` 返回并完成结果汇总后结束。包含任务提交、调度、执行和结果收集；不包含线程池创建、future 容器预留空间、线程池关闭及输出。提交和执行可以重叠。

计时之外先串行计算 `expected_checksum`，预热和每轮正式测量都核对实际 `checksum`。结果不一致时向 `std::cerr` 输出错误，并以非零退出码结束。求和可能碰撞，且基准使用相同计算函数，因此这只是基本结果校验，不能替代任务恰好执行一次等功能测试，也不能独立验证计算函数的算法。

### 本次结果

以下记录来自本人在本阶段提供的运行输出，未经文档编辑时重新测量：

| worker 数量 | 五次耗时范围（ms） | 中位数（ms） | 相对单 worker 加速比 |
| --- | --- | --- | --- |
| 1 | 113.710～118.277 | 116.156 | 1.00 |
| 2 | 60.052～66.277 | 61.059 | 1.90 |
| 4 | 33.690～35.004 | 34.943 | 3.32 |

15 次正式测量的 checksum 均为 `11208273736162531860`。原始五轮耗时保存在 [设计笔记](docs/design-notes.md)。

2026-09-23 重新构建 Release 并复验，1/2/4 worker 的中位数分别为 150.176、67.853、52.239 ms，checksum 仍全部一致。本次波动更大，单独记录在设计笔记，不与上表合并统计；不同运行的耗时会变化，当前数据不足以归因或认定性能回退。

在此次环境和负载下，增加 worker 缩短了耗时，但没有达到线性加速。任务提交、同步、调度及硬件资源等因素可能影响性能，本次没有分别测量这些因素的开销，也不能仅凭某轮耗时偏高确定原因。

加速比以单 worker 线程池为基准，没有测量直接串行执行的耗时。结果仅代表这一种计算负载；固定测量顺序、后台负载、频率与温度变化均可能影响结果。尚未比较更多 worker 数量、不同任务粒度或 I/O 负载，不能据此确定最佳 worker 数量。

## 设计权衡与限制（逐阶段填写）

- 队列继续保存 `std::function<void()>`。外层 lambda 按值捕获管理包装任务的 `shared_ptr`，可被复制，且能延长包装任务的寿命；内部用户任务可以是不可复制、可移动的对象。代价包括共享所有权管理和包装对象的存储开销；当前 benchmark 测量整批任务耗时，尚未单独测量这些开销。
- 当前参数传递是按值保存，再从保存的 tuple 中调用，不是完整的通用调用接口。已测试捕获 `unique_ptr` 的不可复制任务；这不等于支持将 `unique_ptr` 作为任务参数按值转交给计算函数，因为当前调用不会从 tuple 元素再次移动所有权。
- 尚未覆盖引用返回值、成员指针调用或所有引用包装与参数类型组合，不应据现有测试推断全面支持。当前已验证用法见上方示例和测试文件。
- 关闭采用等待全部已接受任务完成的方式，不提供强制取消或重新启动；不支持并发 stop 或 worker 内部 stop。具体使用约定与验证范围见上方关闭语义，更多学习记录见 [设计笔记](docs/design-notes.md)。

## 每个 Issue 的收尾流程

1. 对照 Issue 验收项检查实现与测试。
2. 构建并运行测试，记录实际结果。
3. 在设计笔记中填写自己的理解、问题与解决过程。
4. 更新 README 中的进度、用法和相关说明。
5. 检查 Git 差异，仅暂存本阶段相关文件。
6. 创建关联 Issue 的提交，并推送到 GitHub。
7. 在 Issue 中记录提交编号与验收结果，勾选验收项并关闭。

构建产物和后续阶段的空白文件不作为当前阶段的功能成果提交。
