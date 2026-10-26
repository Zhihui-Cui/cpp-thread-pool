# W08 阶段记录：S01 进程、线程与阻塞状态

- 学习周：W08。
- 记录类型：checkpoint。
- 截至日期、记录日期：2026-10-26。
- 状态：S01 指导下完成；实际用时未记录，约 3h 是任务预算。
- 任务：[系统路线 S01–S08](https://github.com/Zhihui-Cui/ai-infra-roadmap/issues/2)。
- 证据：本人在 WSL 中执行并贴出的终端输出及口述回答；助手整理，没有代跑或重新核验本地程序。

## 环境与观察对象

WSL Ubuntu-24.04。本人将线程池从 Windows 盘复制到 `~/projects/cpp-thread-pool`，保留原副本，跳过旧构建产物；终端确认新路径和源码文件存在。TCP 项目位于 `~/projects/concurrent-tcp-server`。

复用已有线程池及单连接、单请求 TCP 服务端；未新增线程池功能、未集成并发服务、未重跑历史性能基线。本次观察入口 `practice/s01_observe.cpp` 由助手给出，本人本地编译运行；本次归档没有取得该文件的完整最终内容，因此不冒充已审查或已上传本地源码。

观察命令：

```bash
ps -L -C tcp_server -o pid,tid,stat,pcpu,wchan:30,comm
ps -L -C s01_observe -o pid,tid,stat,pcpu,wchan:30,comm
```

线程池入口复用 `learning::ThreadPool pool(2)`：主线程通过 `std::cin.get()` 等输入；先观察空队列，再提交一个约 20 秒的整数计算任务，观察计算中和计算结束后的状态。给出的编译命令为：

```bash
g++ -std=c++17 -pthread -Iinclude practice/s01_observe.cpp src/thread_pool.cpp -o /tmp/s01_observe
/tmp/s01_observe
```

## 实际输出

以下摘录来自本人贴出的输出，未补造未捕捉到的状态。

### TCP：等连接、等数据、进程退出

未连接客户端：

```text
    PID     TID STAT %CPU WCHAN                          COMMAND
 272761  272761 S+    0.0 inet_csk_accept                tcp_server
```

客户端连接但不发送数据：

```text
    PID     TID STAT %CPU WCHAN                          COMMAND
 272761  272761 S+    0.0 wait_woken                     tcp_server
```

随后按提示发送完整请求，再次查询时仅有表头，未再列出 `tcp_server`。本人未在此处提供客户端回显及服务端退出码，因此本记录只把它作为进程不再被查询到的证据，不扩展为 TCP 全项验收或 S05 完成。

结合操作与[服务端源码](https://github.com/Zhihui-Cui/concurrent-tcp-server/blob/229bcfe200fe0ea048b597b13fdae9f205adc88f/src/server.cpp)，前两次分别对应等待连接与接收数据；不能仅凭 `wait_woken` 名称判定业务原因。

### 线程池：空闲时三个线程都等待

```text
    PID     TID STAT %CPU WCHAN                          COMMAND
 313458  313458 Sl+   0.0 wait_woken                     s01_observe
 313458  313459 Sl+   0.0 futex_do_wait                  s01_observe
 313458  313460 Sl+   0.0 futex_do_wait                  s01_observe
```

同一 PID、三个 TID；主线程等终端输入，两个 worker 等任务。

### 新一次运行：一个 worker 计算，其余线程等待

```text
    PID     TID STAT %CPU WCHAN                          COMMAND
 317576  317576 Sl+   0.0 wait_woken                     s01_observe
 317576  317577 Rl+   100 -                              s01_observe
 317576  317578 Sl+   0.0 futex_do_wait                  s01_observe
```

计算结束后，同一进程与线程再次观察：

```text
    PID     TID STAT %CPU WCHAN                          COMMAND
 317576  317576 Sl+   0.0 wait_woken                     s01_observe
 317576  317577 Sl+   2.8 futex_do_wait                  s01_observe
 317576  317578 Sl+   0.0 futex_do_wait                  s01_observe
```

worker 317577 从 R 返回 S，并重新进入等待路径。2.8% 是包含先前计算的平均 CPU 使用比例，不能解释为当前仍在持续计算。R 同时包含运行和可运行；没有从本次快照独立区分两者，也没有做调度轨迹或性能测量。

## 理解结论与共享关系

```text
一个进程：同一个地址空间
├── 一个 pool 对象：共享 tasks_、state_mutex_、cv_
├── 主线程：自己的调用栈、执行位置和寄存器状态
├── worker A：自己的调用栈，通过 this 访问同一个 pool
└── worker B：自己的调用栈，通过 this 访问同一个 pool
另一次启动：另一个进程、另一个 pool，不自动共享任务队列
```

图由助手整理，本人通过问答检查关系；不记为独立绘图。线程各有调用栈不等于栈内存具有线程间访问隔离；本例主线程的局部 pool 可以由 worker 通过指针访问，仍须保证生命周期和同步。

- 运行：此刻获得 CPU 执行；可运行：条件已具备、等待 CPU；阻塞：等待继续执行所需的事件。
- 通知不保证立即执行：还需要调度、重新取得锁并检查条件。
- 条件变量等待释放状态锁；醒来重新加锁检查谓词。取出任务后释放锁再执行，其他线程才能继续提交或领取任务。
- 本实现用 state_mutex_ 协调停止、入队及等待谓词，任务队列内部另有互斥量保护队列操作；本次没有修改锁结构。
- 一个任务不会由两个 worker 各领一次：锁内取出并移除；后来取得锁的线程重新检查条件，队列为空且未停止时继续等待。
- 同一进程内线程共享数据，但各有执行进度和调度状态，因此可以同时出现一个计算、其他等待。
- 进程存在且 CPU 低不能直接判断卡死；既可能正常等待，也可能异常等待，要结合等待原因和事件发生后能否继续推进判断。
- ps 中 S 为可中断睡眠，l 表示多线程，+ 表示前台进程组；WCHAN 需要结合代码与操作解释。

## 帮助程度与限制

本人起初认为“唤醒后立即执行”和“两个 worker 各有独立任务队列”，经助手解释与追问纠正。本人能解释释放队列锁后其他线程可以访问队列、共享数据与独立线程状态并不冲突，以及正常等待时 CPU 使用率低。关于单任务领取，先回答“同一时刻只有一个线程取得锁”，助手补充了移除任务、后继线程重新检查条件的完整过程。

本次通过助手整理的 OSTEP 要点学习并完成口述检查，未确认本人独立阅读原书段落或完整章节。观察代码和命令由助手提供，实际运行由本人完成；不是独立重写线程池或重新验收 P0。

参考：[OSTEP 进程 §4.4](https://pages.cs.wisc.edu/~remzi/OSTEP/cpu-intro.pdf)、[OSTEP 线程开头两页](https://pages.cs.wisc.edu/~remzi/OSTEP/threads-intro.pdf)、[ps 手册](https://man7.org/linux/man-pages/man1/ps.1.html)。

## 下一步

进入 S02：虚拟内存、页与进程内存。区分申请量、地址空间与 RSS，通过小观察解释差别。S02–S08 未因本次 S01 观察而标记完成。
