# C++ Thread Pool

一个以学习为目标、可运行并可测试的 C++17 线程池实现。

## 本周目标

完成 ThreadPool v0：

- 线程安全任务队列
- 多 worker 执行任务
- `condition_variable` 阻塞等待
- `submit()` 返回 `std::future`
- 优雅关闭与 `join`
- 单元测试与 benchmark

## 目录结构

```text
include/     公共头文件
src/         实现
tests/       测试
benchmarks/  性能测试
docs/        设计与学习记录
```

## 构建（目标）

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

> 代码将在每个功能完成时逐步加入；不要跳到后续功能，先完成当前 Issue 的验收。

## 设计记录

- 任务队列采用 mutex 保护。
- worker 在队列为空时通过条件变量阻塞，而不是 busy-loop。
- `stop()` 停止接收新任务、唤醒 worker、等待队列处理完成并 join。
- 详细取舍与踩坑记录在 [docs/design-notes.md](./docs/design-notes.md)。

## 学习任务

见本仓库的 GitHub Issues。每个 Issue 代表一个可验收功能，而不是“看一节课”。
