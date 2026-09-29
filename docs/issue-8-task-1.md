# 线程池验收

> 以下为本人初稿，保留用于对照；文末为对照当前源码后的验收反馈与修订版。修订版由助手整理，不作为本人已独立掌握的证明。

## 问题

### 1. 画一张流程图或时序图。
从调用 submit() 开始，到 result.get() 获得结果为止。标出任务包装、入队、出队、执行、结果传递这些环节，以及各环节由哪个线程执行。文字箭头即可。

### 2. 说明关键对象的生命周期。
base 的捕获值、参数 2、bound_task、packaged_task、shared_ptr、队列中的 std::function<void()> 和 future，各自保存什么？谁负责让它们存活？为什么 submit() 返回后任务仍然能执行？

### 3. 说明锁与等待。
哪把锁保护什么？worker 在什么条件下等待、继续工作、退出？执行用户任务时是否持锁？get() 是否一定会发生阻塞？

### 4. 回答两个变化。
- 如果任务抛出 std::runtime_error，异常如何到达调用者？worker 后续会怎样？
- 如果任务尚未完成就调用 stop()，已经接收的任务会怎样？stop() 何时返回？

---

## 作答
### 1. 流程图/时序图

调用线程 `submit()` ----> 调用线程 任务包装（包装成 `shared_ptr` 指向 `package_task`对象，并返回`future`） ----> 调用线程 入队（取锁、结束状态判定、入队、`notify_one()`） ----> worker线程 出队（取锁、wait判定、结束状态判定、取任务） ----> worker线程 执行 ----> worker线程 结果传递

### 2. 关键对象的生命周期

base捕获值：按值捕获，复制一份，lambda结束后生命周期结束；引用捕获，被引用的对象销毁，生命周期结束。保存参数，调用线程负责其存活。
参数2：lambda结束后生命周期结束。保存任务，调用线程和worker线程共同负责其存活。
bound_task：带参submit创建到结束。存放不带参闭包，调用线程负责其存活。
packaed_task：无参submit创建到任务执行后结束。保存任务执行状态，调用线程和worker线程共同负责其存活。
shared_ptr：无参submit创建到任务执行后结束。保存可调用对象地址，调用线程和worker线程共同负责其存活。
队列中的std::function<void()>：enqueue开始到任务执行完结束，保存任务，调用线程负责其存货。
future：无参submit创建到任务执行后结束。保存任务执行结果，调用线程负责其存活。

submit结束后，任务的生命管理又线程池负责。

### 3. 锁与等待

哪把锁保护什么？队列中的锁负责队列的存取修改，线程池中的锁负责线程池状态修改以及任务存取。
worker 在什么条件下等待、继续工作、退出？worker在队列为空或结束标志为false时等待；在队列非空时执行任务；在队列为空且结束标志位true时退出。
执行用户任务时是否持锁？执行任务时候不持锁。
get() 是否一定会发生阻塞？当任务没执行时候，关联的future对象得不得结果，阻塞等待。

### 4. 两个变化

如果任务抛出 std::runtime_error，异常如何到达调用者？worker 后续会怎样？会通过future到达调用者，get()会得到异常信息；worker结束执行当前任务，阻塞或执行新任务。
如果任务尚未完成就调用 stop()，已经接收的任务会怎样？stop() 何时返回？当前接受的任务继续执行，stop()待所有任务执行完，worker线程退出后返回。

---

## 任务 1 验收反馈

初次验收暂未通过，需要补齐对象生命周期和等待条件。经后续纠正与复述，任务 1 已通过本轮概念验收，最终记录见文末。流程图的绘制形式不影响验收。

理解正确的部分：任务由 worker 执行；执行用户任务时不持有线程池和队列的内部锁；任务异常可以通过 future 传回；正常关闭会完成已经接收的任务并等待 worker 退出。

需要纠正的部分：

| 初稿表述 | 修正与原因 |
| --- | --- |
| 包装成 shared_ptr 指向 package_task 对象，并返回 future | 正确类型名是 `packaged_task`。它保存可调用对象并关联共享状态；`shared_ptr` 管理这个任务对象的生命周期。先取得 future，成功入队后 submit 才返回 future。 |
| lambda 结束后生命周期结束 | 必须区分“函数体执行结束”和“闭包对象销毁”。按值捕获的数据是闭包对象的成员，不因一次调用结束就立即销毁。 |
| 调用线程和 worker 共同负责存活 | 需要说明具体由哪个对象持有、何时销毁。线程执行代码；对象的成员、移动、智能指针等决定所有权和生命周期。 |
| bound_task：带参 submit 创建到结束 | 这只描述局部变量本身。它保存的函数和参数被移动到后续对象中，最终由 packaged_task 内部的可调用对象保存。 |
| future：创建到任务执行后结束 | future 的生命周期由持有它的变量决定；任务完成不会销毁 future。结果或异常位于它关联的共享状态中。 |
| 队列为空或结束标志为 false 时等待 | 应为“队列为空且 stopping_ 为 false 时等待”。否则无法解释未停止但有任务时为什么应该工作。 |
| 任务没执行时 get 阻塞 | 应为“共享状态尚未就绪时 get 阻塞”。任务已经开始但尚未产生结果时，也需要等待。 |
| get 得到异常信息 | `get()` 会重新抛出保存的异常，调用者用 try/catch 处理；不是把异常信息当作普通返回值返回。 |

## 修订版：线程、对象和锁

### 1. 任务流程

以下针对题目中的带参数 submit，假定提交成功：

```text
调用线程
  创建用户 lambda，按值保存 base 的副本
  调用带参数 submit(lambda, 2)
    创建 bound_task：保存用户 lambda 和参数 tuple
    调用无参数 submit，将绑定后的任务继续移动进去
      创建 shared_ptr 管理的 packaged_task
      从 packaged_task 取得关联同一共享状态的 future
      创建捕获 shared_ptr 的外层 lambda
      转换为 std::function<void()>，调用 enqueue
        持有 state_mutex_：检查 stopping_，成功则入队
        释放 state_mutex_，notify_one()
      返回 future，最终交给调用者的 result
  result.get()：如果共享状态尚未就绪，则等待；就绪后取得结果

worker 线程（与调用线程并发）
  持有 state_mutex_，检查 wait 的谓词
    谓词为 false：等待期间释放锁，醒来重新取得锁并检查
    谓词为 true：尝试取出任务
    没取到任务且 stopping_ 为 true：退出
  释放 state_mutex_
  调用取出的 std::function<void()>
    外层 lambda 调用 (*task)()
      packaged_task 调用保存的 bound_task
        std::apply 用 tuple 中的参数调用用户 lambda
      将返回值或异常存入共享状态，使其就绪
  本轮循环结束，销毁本轮局部任务对象；继续下一轮
```

这两段不是先后执行的两大块。入队后，worker 可能在 submit 返回前开始甚至完成任务；notify_one() 也不保证 worker 立刻执行任务。结果由 worker 写入共享状态，由调用线程通过 get() 取得。

### 2. 关键对象的生命周期

先区分三个概念：局部变量本身、移动后由其他对象保存的内容、独立的结果共享状态。

| 对象 | 保存什么、由谁持有 | 何时结束 |
| --- | --- | --- |
| 原始 base 与按值捕获的副本 | 原始 base 是调用者的局部变量；捕获副本是用户 lambda 闭包的成员，本例值为 40。用户 lambda 随后保存在 bound_task 中。 | 原始 base 按自己的作用域销毁；任务中那份捕获值随持有它的闭包销毁。修改或销毁原始 base 不影响该副本。 |
| 参数 2 | 带参 submit 的 argument 接收这个值，随后将值保存在 bound_task 的 saved_arguments tuple 中。 | argument 随本次函数调用结束而销毁；任务中的那份参数随持有它的 tuple 销毁。整数的移动效果与复制值相同。 |
| bound_task | 无参数闭包，包含 saved_function 和 saved_arguments。局部 bound_task 被移动给无参 submit，后者再将收到的可调用对象移动到 packaged_task 内部。 | 局部 bound_task 在带参 submit 返回时销毁；被移动后的局部对象与接收内容的对象是不同对象，任务中保存的内容可以继续存活。 |
| packaged_task | 保存绑定后的可调用对象，并关联用于存放返回值或异常的共享状态。通过 make_shared 创建，由 shared_ptr 共享管理。 | 最后一个拥有它的 shared_ptr 释放所有权时销毁；“任务执行完”本身不等于对象立刻销毁。 |
| shared_ptr | 无参 submit 中有局部 task；外层 lambda 的按值捕获又持有一份 shared_ptr，指向同一个 packaged_task。 | 各份 shared_ptr 随各自持有者销毁。局部 task 销毁后，队列中或 worker 手中的外层 lambda 仍可保持 packaged_task 存活。 |
| std::function<void()> | 保存外层 lambda，后者捕获 shared_ptr。入队时移入队列，出队时经移动交给 worker 本轮的局部 optional。 | 队列中的元素在 pop 时销毁，但任务内容已被移走；worker 本轮循环结束时销毁局部任务，释放其持有的 shared_ptr。 |
| future | 是访问共享状态的句柄；submit 中的局部 result 经返回交给调用者。结果并不直接存放在这个局部变量内部。 | 调用者的 future 按其作用域销毁。成功调用 get() 后，它不再关联该共享状态，valid() 为 false；future 对象本身仍存在。 |

所有权关系可以用下面的图表达：

```text
队列中的任务 / 出队后 worker 的局部任务
  └─ std::function<void()>
      └─ 外层 lambda
          └─ shared_ptr ──拥有──> packaged_task
                                  ├─ 绑定后的可调用对象
                                  │   ├─ 用户 lambda（含 base 的副本）
                                  │   └─ tuple（含参数 2）
                                  └─ 关联结果共享状态
调用者的 future ────────────────────────关联同一共享状态
```

所以 submit 返回后任务仍能执行，是因为任务对象已经被保存下来，队列或 worker 持有的外层 lambda 通过 shared_ptr 保持 packaged_task 存活。future 关联的是共享状态，并不拥有或延长用户任务闭包的生命周期。即使 packaged_task 已销毁，只要结果已写入且 future 仍关联共享状态，调用者仍能取结果。

补充：引用捕获不会延长被引用对象的生命周期。调用者必须确保对象在任务访问它时仍然存活；否则可能出现悬空引用。这与本题的按值捕获不同。

### 3. 锁与等待

- `ThreadSafeQueue::mutex_`：保护内部队列的 push、pop、empty 等访问。
- `ThreadPool::state_mutex_`：保护 stopping_，并将停止状态检查、入队以及 worker 的等待条件检查和出队协调起来。队列内部的锁仍由队列方法自己获取和释放。
- worker 的 wait 谓词为 `stopping_ || !tasks_.empty()`，所以需要等待的条件是 `!stopping_ && tasks_.empty()`。
- 停止标志为 true 但仍有任务时，worker 继续取任务；停止标志为 true 且没有取到任务时才退出。
- wait 阻塞期间释放 state_mutex_，返回前重新取得它；执行用户任务时，线程池和队列的内部锁都已释放。用户任务自己仍可获取业务所需的锁。
- 对本例中有效的 future，get() 在共享状态未就绪时等待；已经就绪时直接取得结果或重新抛出异常，无须等待任务完成。

### 4. 异常和关闭

用户任务抛出 std::runtime_error 时，packaged_task 将异常保存到共享状态并使其就绪。调用者执行 future.get() 时重新抛出该异常。这个用户异常不会直接逃出 worker 的任务调用；worker 可以继续处理后续任务，也可能在没有任务时等待，或在停止条件满足时退出。

在正常的外部调用场景下，stop() 持锁设置 stopping_，释放锁后通知所有 worker。已经接收的任务继续执行，新提交在 enqueue 检查停止标志时被拒绝。stop() 逐一 join worker，全部 join 完成后返回。因此它既等待任务完成，也等待 worker 线程结束。

这里的“已接收”指提交在状态锁保护下通过检查并成功入队，不能仅凭 submit 的调用开始时间判断。以上说明不把多个线程同时调用 stop() 或 worker 自己调用 stop() 作为当前接口已支持的行为。

## 复述确认题目

以下为本轮复述题目，原作答与纠正后的答案均保留，便于后续复习。

1. 无参 submit 返回时，局部 task 和 function 都会销毁。为什么 worker 仍然能执行用户任务？请说出实际持有关系。
2. 如果任务完成、packaged_task 已销毁，但调用者还没调用 get()，结果为什么还在？它保存在哪里？
3. stopping_ 为 false 且队列非空时，worker 应该等待还是取任务？用这个场景解释为什么初稿的“或”不正确。

---

## 首次复述作答（含错误，保留对照）

1. 局部task是shared_ptr对象，入队时计数+1，submit返回后task不被销毁，由外层lambda持有；function通过std::move交给了packaged_task，由shared_ptr管理，直接转移不被销毁。

2. 结果存放在与之关联的future对象中，不随着packaged_task而销毁。

3. worker应该取任务；如果是或，没有任务也不再等待，但没有任务也不会退出。

## 复述确认后的正确答案

以下根据后续对话整理；其中规范表述由助手补全，不代表首次作答即已独立答对。

### 1. submit 返回后，为什么任务仍能执行？

无参 submit 中的局部 task 是一个 shared_ptr。创建外层 lambda 时，`[task]` 按值捕获并复制这份 shared_ptr，使两份 shared_ptr 共同管理同一个 packaged_task。增加所有者数量的是复制 shared_ptr 的动作，不能笼统地说“入队时计数加一”。

submit 返回时，局部 task 会销毁，但外层 lambda 中的 shared_ptr 仍可保持 packaged_task 存活。这个外层 lambda 由队列中的 std::function，或出队后 worker 的局部任务对象持有。

局部 function 的内容被移动到 packaged_task 内部保存的可调用对象中。局部 function 随后销毁，不影响接收其内容的对象。移动不意味着原对象不会销毁。

当最后一个拥有 packaged_task 的 shared_ptr 释放所有权时，packaged_task 才被销毁。释放所有权既可能发生在 shared_ptr 销毁时，也可能发生在 reset() 或重新赋值时。

本人后续已正确复述：复制 shared_ptr 后，两份 shared_ptr 管理同一个 packaged_task；局部那份销毁不意味着 packaged_task 立即销毁。

### 2. packaged_task 销毁后，为什么还能取得结果？

结果保存在 packaged_task 与 future 关联的共享状态中，不是直接保存在 future 对象内部。

在任务已经完成、结果已经写入的前提下，即使 packaged_task 被销毁，只要调用者的 future 仍关联该共享状态，结果就仍可通过 get() 取得。future 保持的是共享状态，而不是 packaged_task 对象。

不能笼统地说“packaged_task 销毁不会改变共享状态”：如果它在尚未使共享状态就绪时被销毁，共享状态会记录 broken_promise 异常，随后 get() 会抛出相应的 std::future_error。

本人后续已正确指出结果位于共享状态中；“任务已完成”的前提经提示补充。

### 3. worker 在什么条件下等待、工作和退出？

真正需要等待的条件是“未停止且队列为空”：

```cpp
!stopping_ && tasks_.empty()
```

当 stopping_ 为 false 且队列非空时，应取任务执行。初稿使用“队列为空或未停止”，会因为“未停止”为真，错误地让有任务的 worker 继续等待。

`cv_.wait(lock, predicate)` 的谓词表达的是“允许结束等待的条件”，因此源码中写的是：

```cpp
stopping_ || !tasks_.empty()
```

| stopping_ | 队列状态 | worker 行为 |
| --- | --- | --- |
| false | 空 | 等待 |
| false | 非空 | 取任务执行 |
| true | 非空 | 继续取任务执行，排空已接收的任务 |
| true | 空 | 退出 |

上述状态判断在状态锁保护下进行。当前实现先尝试出队，再在没有取到任务且 stopping_ 为 true 时退出。

本人复述时曾把正确等待条件中的“队列为空”写成“队列非空”，经纠正后，已正确回答“停止但队列仍有任务时继续执行任务”。这一组条件仍需在独立实现中巩固。

## 最终验收记录

- 任务 1 已通过本轮概念验收。
- 已澄清局部 shared_ptr 与其管理对象的区别，以及任务对象与结果共享状态的区别。
- 等待条件与 wait 谓词已整理如上，需通过下一步独立实现检验运用情况。
- 本轮经过提示和纠正后完成，不记录为一次独立答对，也不等同于已通过独立编码验收。
