# C++ Thread Pool

一个以学习为目标的 C++17 线程池项目，逐步练习任务队列、任务执行、线程同步和工程组织。

当前已实现泛型线程安全队列、单线程任务执行器，以及支持指定 worker 数量和析构回收的线程池。空闲 worker 使用条件变量阻塞等待；future 与完整的优雅关闭接口属于后续计划。

## 学习路线与进度

下表记录本地实现进度；GitHub Issue 的关闭在测试、总结和推送完成后进行。

| 阶段 | 学习主题 | 实现进度 |
| --- | --- | --- |
| [Issue #1](https://github.com/Zhihui-Cui/cpp-thread-pool/issues/1) | 泛型线程安全任务队列 | 已实现并测试 |
| [Issue #2](https://github.com/Zhihui-Cui/cpp-thread-pool/issues/2) | 单线程任务执行器 | 已实现并测试 |
| [Issue #3](https://github.com/Zhihui-Cui/cpp-thread-pool/issues/3) | 多 worker 生命周期管理 | 已实现并测试，Issue 已关闭 |
| [Issue #4](https://github.com/Zhihui-Cui/cpp-thread-pool/issues/4) | 阻塞等待与唤醒 | 已实现并测试，待提交与归档 |
| [Issue #5](https://github.com/Zhihui-Cui/cpp-thread-pool/issues/5) | submit 与 future | 待实现 |
| [Issue #6](https://github.com/Zhihui-Cui/cpp-thread-pool/issues/6) | 优雅关闭 | 待实现 |
| [Issue #7](https://github.com/Zhihui-Cui/cpp-thread-pool/issues/7) | 测试、benchmark 与项目说明 | 待完成 |

每个阶段完成验收和收尾后，再推进下一阶段。个人理解与踩坑记录写在 [设计与学习记录](docs/design-notes.md)。

## 项目结构

```text
include/
    task_queue.hpp                  模板队列的声明与实现
    single_thread_executor.hpp      单线程执行器声明
    thread_pool.hpp                 线程池构造、析构与提交接口
src/
    single_thread_executor.cpp      单线程执行器实现
    thread_pool.cpp                 worker 创建、阻塞等待、任务执行与退出回收
tests/
    task_queue_test.cpp             队列测试
    single_thread_executor_test.cpp 单线程执行器测试
    thread_pool_test.cpp            线程基础、生命周期与析构前任务执行测试
benchmarks/
    thread_pool_benchmark.cpp       后续性能测试，当前为空文件
docs/
    design-notes.md                 各阶段设计、学习总结与验收记录
CMakeLists.txt                      构建目标、依赖和测试注册
README.md                          项目概览与使用说明
```

模板队列的实现保留在头文件中，供使用它的代码按具体类型实例化。普通执行器采用头文件声明、源文件实现的组织方式。

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
- 构造函数启动 worker，`submit()` 将 `std::function<void()>` 任务入队；任务可能在提交返回前开始执行。
- worker 使用带谓词的 `condition_variable::wait()`，在队列为空且没有停止请求时阻塞等待；取到任务后解锁再执行，不保证任务的完成顺序或均匀分配。
- `submit()` 在 `state_mutex_` 保护下入队，解锁后调用 `notify_one()`；等待条件检查与入队使用同一把外层锁配合，队列内部的锁继续保护队列操作。
- 析构时设置停止标志，解锁后调用 `notify_all()`，worker 完成剩余任务后退出，再由析构函数 join 所有 worker。构造中途失败时也按这一顺序回收已创建的线程，再重新抛出异常。

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

该例只有一个任务写 `result`，主线程在析构等待完成后读取。多个任务并发修改同一份结果时，需要额外同步。当前使用要求是：任务正常返回，提交在线程池析构开始前结束，引用捕获的数据保持存活。任务异常传回调用方的机制尚未实现。

## 测试

| 测试程序 | 当前覆盖的行为 |
| --- | --- |
| `task_queue_test` | 空队列、单元素存取、先进先出、零值、字符串、可调用对象存取 |
| `single_thread_executor_test` | 提交时不执行、按顺序执行、完成的任务不重复执行 |
| `thread_pool_test` | 线程基础、共享队列消费、1/2/4 个 worker 各正确执行 1,000 个任务、拒绝零 worker、空任务析构、同一个单 worker 池连续三轮在析构前观察到任务完成标记 |

目前 CTest 注册了三个测试程序。一个程序中的多个测试函数不会分别计入 CTest 的测试数量。已检查同步实现并运行上述测试；尚未使用数据竞争检测器，也未模拟构造中途创建线程失败。详细证据与限制见设计笔记。

新增测试在每轮提交后使用带谓词的 `wait_for()` 等待完成标记，并在线程池析构前断言结果；不能据此保证提交时 worker 已进入阻塞，也未测量空闲 CPU 使用率或主动制造虚假唤醒。

## 线程池架构（Issue #3～#6 完成后填写）

> 待填写：任务从提交到完成的流程，以及队列、worker、等待状态之间的关系。

## 返回值与异常（Issue #5 完成后填写）

> 待填写：submit 的最终接口、future 的使用示例、任务异常的获取方式。

## 关闭语义（Issue #6 完成后填写）

当前已实现析构时设置停止状态、通过条件变量唤醒所有等待的 worker、完成剩余任务并 join；尚无公开 `stop()` 或停止后的提交拒绝协议。完整关闭语义与边界测试将在 Issue #6 完善。

> 待填写：stop 后提交任务的行为、析构行为、重复调用 stop 的行为，以及对应测试。

## Benchmark（Issue #7 完成后填写）

> 待填写：运行命令、硬件与编译配置、任务负载、测量方法、至少两种 worker 数量的结果，以及对结果的解释。

## 设计权衡与限制（逐阶段填写）

> 待填写：每项设计选择解决了什么问题、带来什么代价，以及当前实现适用的范围。详细推理放入设计笔记，这里保留简要结论。

## 每个 Issue 的收尾流程

1. 对照 Issue 验收项检查实现与测试。
2. 构建并运行测试，记录实际结果。
3. 在设计笔记中填写自己的理解、问题与解决过程。
4. 更新 README 中的进度、用法和相关说明。
5. 检查 Git 差异，仅暂存本阶段相关文件。
6. 创建关联 Issue 的提交，并推送到 GitHub。
7. 在 Issue 中记录提交编号与验收结果，勾选验收项并关闭。

构建产物和后续阶段的空白文件不作为当前阶段的功能成果提交。
