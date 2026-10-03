# 有界线程池练习（2026-10-03 归档）

本练习在 `practice/bounded_thread_pool.cpp` 中实现独立的 `BoundedThreadPool`，不改变正式 `learning::ThreadPool` 接口。

## 行为与约定

- 接收 `std::function<void()>`，构造时指定线程数与等待队列容量，两者必须大于零，否则抛出 `std::invalid_argument`。
- 容量只计算等待队列中的任务，不包含工作线程已取走的任务。
- `submit()` 在同一把锁内检查停止状态、检查容量并入队；停止或队列已满时抛出 `std::runtime_error`，消息区分原因。
- worker 在锁内等待和取任务，在锁外执行任务。
- `stop()` 停止接受新任务，完成已接受任务，唤醒并回收 worker；析构调用 `stop()`。
- 构造期间创建线程失败时，先停止并回收已创建线程，再重新抛出异常。

练习限定为有效且不会向 worker 外抛出异常的任务；直接调用空任务或让任务异常逃出线程入口会导致程序终止。停止由一个外部调用者负责，不支持多个并发 `stop()`，也不支持在自身 worker 中停止或销毁线程池。被引用的对象必须存活到任务结束。

## 队列满的确定性测试

只使用一个 worker，队列容量为 1：

1. 提交 A，A 通知主线程已经开始，然后等待释放信号。
2. 主线程等待开始信号，确保 A 已从队列取走。
3. 提交 B，B 占满等待队列。
4. 提交 C，捕获预期的 `std::runtime_error`。
5. 释放 A，调用 `stop()` 并等待任务结束。
6. 断言 A、B 各完成一次，C 未执行，且提交 C 被拒绝。

测试不依赖 `sleep`。B 的提交等步骤发生非预期异常时，先释放 A 并回收线程，再重新抛出异常，避免 A 一直等待导致清理卡住。计数器先于线程池声明；结果在 `stop()` 完成 join 后读取。

当前新增测试集中验证队列满时拒绝及已接受任务完成。没有新增零线程数、零容量、停止后提交等独立测试；本人表示已掌握重复的参数检查模式，决定直接进入 P1。未做构造失败注入或数据竞争检测，测试通过不代表覆盖所有并发调度。

## 构建与验证

CTest 目标为 `bounded_thread_pool_test`，始终启用断言，并设置 15 秒超时。

在 Ubuntu 项目目录执行：

```bash
cmake -S . -B out/linux-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/linux-debug -j 2
ctest --test-dir out/linux-debug --output-on-failure --timeout 30
```

验证证据：

- 助手在 Windows/MSYS2 Debug 下构建并运行新增目标，1/1 通过，测试耗时 0.27 秒。
- 本人提供的 WSL Ubuntu 输出显示全部 6/6 通过，总耗时 0.46 秒；新增测试耗时 0.01 秒。
- 本次归档沿用上述最终代码的验证记录，没有声称重新执行 Linux 测试。

## GDB 学习记录与提示范围

在 `BoundedThreadPool::submit` 设置断点，依次观察提交 A、B、C 前的状态。本人正确解释：第一次队列为空；第二次队列仍为空，但 worker 已取走 A；第三次队列有 B，容量已满。A 此时阻塞等待释放，不代表持续占用 CPU 计算。

已练习 `run`、`continue` 和 `print`，并解释正常退出后的 `The program is not being run`：程序已结束，须用 `run` 重启，不能用 `continue` 延续。`bt`、`next`、`info threads`、`thread apply all bt` 已作为后续观察方法讲解；没有把尚未提供输出的异常逐步追踪记为完成。

本次实现由本人分步编写，经多轮提示与审查；异常清理获得局部示例，助手调整了声明顺序并接入 CMake/CTest。因此记录为指导下完成的变体练习，不作为完全无提示独立重建的证明。

## 下一步

进入 P1 本地 TCP 请求路径练习，预算约 6—10 小时：明确消息边界，处理部分收发与断开，使用现有线程池执行确定性任务，验证基本并发和关闭，交付请求流程图与最小运行示例。
