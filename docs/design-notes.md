# 设计与学习记录

## v0 目标
一个正确、可理解、可验证的 C++17 线程池；不追求生产级复杂度。

## 关键决策（完成后补充）
- 任务队列的同步策略：
1. push()、try_pop()、empty() 都使用同一个成员 mutex_。
2. try_pop() 的判空、取值、删除都在一次持锁期间完成。
3. lock_guard 离开作用域时自动解锁，包括提前 return 的情况。
4. 空队列返回 std::nullopt。
5. empty() 只反映检查那一刻的状态。
- 条件变量的 wait 谓词：
- shutdown 的状态转换：
- 异常如何通过 future 传递：

## 易错点
- `condition_variable::wait` 必须使用谓词处理虚假唤醒。
- 不要在持有队列锁时执行用户任务。
- shutdown 时需要唤醒所有 worker，并保证它们能退出。
