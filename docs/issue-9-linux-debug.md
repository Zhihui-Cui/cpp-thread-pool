# Issue #9：Linux 构建、测试与一次 GDB 调试

日期：2026-09-30。范围沿用 2026-09-29 的约定：Linux 构建现有项目、启用断言并设置超时运行功能测试、用调试器定位一次简单故障，以及记录工具链、命令、结果和需要提示的部分。

## 证据与当前状态

本人在 WSL Ubuntu 中按提示执行命令、修改代码并提供终端输出；助手核对工作区中的 CMake 缓存、Ninja 编译参数、CTest 配置、最终测试日志及源码后整理本记录。本记录不表示助手重新运行了 Linux 测试。

- 首次 Linux CTest：5/5 通过，总耗时 0.43 秒。
- 人工将 void 任务赋值改为 40 后，用 GDB 捕获断言失败，定位到测试源码并查看实际变量值。
- 恢复赋值为 42、重新构建后，CTest 再次 5/5 通过，总耗时 0.80 秒。
- 操作与调试证据已齐；本人已复述配置、编译、链接、运行的职责，正确解释修改源码后需要重新构建。经助手纠正目标文件的术语，本轮概念验收通过。
- 本次整理未提交、推送或操作 GitHub Issue，不据此宣称 Issue 已关闭。

## 环境

| 项目 | 实际环境或证据 |
| --- | --- |
| Linux | Ubuntu 24.04 LTS，`uname -s` 输出 `Linux` |
| WSL | 2.7.14.0；用户提供的内核版本为 6.18.33.2-2 |
| 编译器 | Ubuntu GCC/G++ 13.3.0；CMake 选择 `/usr/bin/c++` |
| CMake | 3.28.3 |
| GDB | 15.1，配置为 `x86_64-linux-gnu` |
| 其他工具 | `which` 确认 Ninja、Git 位于 `/usr/bin/`；未采集其版本号 |
| 构建配置 | Ninja、C++17、Debug；编译参数含 `-g`，未定义 `NDEBUG` |
| 项目路径 | `/mnt/e/Desktop/AI-Infra-Learning/cpp-thread-pool` |
| 构建目录 | `out/linux-debug`，与 Windows 的 `build-ninja` 分开 |

安装 Ubuntu 时指定 `D:\WSL\Ubuntu-24.04` 作为位置；项目源码继续使用 E 盘目录在 WSL 中的挂载路径。VS Code 通过 WSL 连接项目。

## 构建与测试

在项目根目录的 Linux 终端执行：

```bash
cmake -S . -B out/linux-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/linux-debug -j 2
ctest --test-dir out/linux-debug --output-on-failure --timeout 30
```

助手检查 `CMakeCache.txt` 和 `build.ninja`，确认 Debug、`-g`、C++17 和断言配置。`minimal_thread_pool_test`、`packaged_task_test` 还显式使用 `-UNDEBUG`，并在 CTest 中设置 `TIMEOUT 15`；其余测试采用命令行指定的 30 秒超时。

最终复测由本人执行，构建输出以 `[2/2] Linking CXX executable minimal_thread_pool_test` 结束，随后结果如下：

| CTest 程序 | 结果 | 耗时（秒） |
| --- | --- | --- |
| task_queue_test | Passed | 0.01 |
| single_thread_executor_test | Passed | 0.01 |
| thread_pool_test | Passed | 0.18 |
| minimal_thread_pool_test | Passed | 0.03 |
| packaged_task_test | Passed | 0.08 |

CTest 报告 `100% tests passed, 0 tests failed out of 5`，总实际耗时为 0.80 秒。总耗时使用 CTest 原始输出，不以表中单项耗时相加替代。工作区的 `out/linux-debug/Testing/Temporary/LastTest.log` 与本轮单项结果一致；该目录属于忽略的构建产物，日志可能被后续运行覆盖。

## 人工故障与 GDB 定位

这是按助手提示完成的人工故障练习，不是自行发现的线程池并发缺陷。

1. 在 `test_pool_executes_void_packaged_task()` 的任务 lambda 中，将 `value = 42;` 临时改为 `value = 40;`，保留 `assert(value == 42);`。
2. 保存、重新构建，运行 `gdb ./out/linux-debug/minimal_thread_pool_test`。
3. 在 GDB 中逐条执行 `set pagination off`、`run`；断言失败后执行 `bt`。
4. 调用栈显示 `#7` 为测试函数、第 326 行的断言，`#8` 为 `main()`。执行 `frame 7`、`list`、`print value`。

实际输出节选：

```text
Thread 1 "minimal_thread_" received signal SIGABRT, Aborted.
#7  ... in test_pool_executes_void_packaged_task ()
    at .../practice/minimal_thread_pool.cpp:326
#8  ... in main ()
    at .../practice/minimal_thread_pool.cpp:340
(gdb) frame 7
326         assert(value == 42);
(gdb) list
324         result.get();
325
326         assert(value == 42);
(gdb) print value
$1 = 40
```

定位结论：任务写入 40，而断言要求 42。`result.get()` 已返回，说明任务完成，数值与预期不符由后面的断言检测。`future<void>::get()` 提供完成同步及异常传播，不验证任务写入的数值是否符合测试预期。

GDB 中的 `frame 7` 仅选择查看哪个栈帧，不会重新执行程序；`$1` 是 GDB 的打印结果编号。栈帧编号和源码行号来自本次现场，后续调试应按实际 `bt` 输出选择。

修复：退出 GDB，将任务赋值恢复为 42，保存后重新构建并运行完整 CTest，得到上面的 5/5 通过结果。助手确认最终源码已恢复为 `value = 42;`，人工故障没有保留在源码中。

## 遇到的问题与提示

- WSL 更新最初失败：日志表明旧版 2.3.24.0 的 MSI 安装源缺失或不可访问，卸载旧版时报 1612、1714，最终返回 1603。按提示提供桌面上的旧版 `wsl.msi` 后，用户完成更新并报告版本 2.7.14.0。
- `.wslconfig` 错误：`[ws12]`、`Bswap`、`BlocalhostForwarding` 拼写不正确；按提示改为 `[wsl2]`、`swap`、`localhostForwarding`，重启 WSL 后未知键警告消失。
- localhost 代理警告：NAT 模式下 Windows 的本机代理未直接供 WSL 使用；后续工具安装成功，未据此宣称已修复代理配置。
- GDB 分页提示：按提示输入 `c` 显示剩余启动信息；随后用 `set pagination off` 关闭分页。
- GDB 命令输入：一次粘贴多行后出现 `"on" or "off" expected`，且 `bt` 显示 `No stack`。改为逐条输入后成功启动程序；原先程序尚未运行。
- 系统库源码缺失：调试时出现 `pthread_kill.c` 下载失败和文件不存在提示；仍可通过 `bt` 切换到项目栈帧查看源码与变量。

## 学习归属与概念验收

本人实际完成：安装环境、执行配置和构建测试、修改及恢复任务赋值、输入 GDB 命令并提供现场输出。环境修复、命令选择、故障设计、栈帧选择和结果解释均有助手提示；尚未验证脱离提示独立完成整套调试的能力。

以下为本轮讲解的要点，后附本人实际复述及助手纠正记录：

- 配置：CMake 检测工具链、处理构建描述并生成 Ninja 规则。
- 编译：编译器将 C++ 源码翻译为目标文件。
- 链接：将目标文件及所需库组合，解析符号引用并生成可执行程序等产物；`cmake --build` 会调度编译和链接。
- 运行：执行生成的程序；CTest 按注册的测试启动程序，不会替修改后的源码自动重新编译。

2026-09-30 本人复述：配置负责准备工作，例如选择编译器及输出位置；编译转换源文件；链接将 `.o` 文件与库链接成可执行文件；运行就是执行可执行文件。本人正确说明，旧的构建产物仍对应写入 40 的代码，保存源码不会自动更新这些产物，必须重新 build，CTest 才能运行修改后的程序。

术语纠正：本人将编译结果描述为“二级制文本文件”，助手说明应为“二进制目标文件（如 `.o`）”，包含机器码及符号、重定位等信息，不是普通文本。本人还提及汇编文件，助手补充通常构建不一定在磁盘上保留单独的 `.s` 文件。这里以“编译生成目标文件”描述构建步骤；更细分的工具链过程包含预处理、编译和汇编。

验收结论：核心职责和源码到构建产物的关系理解正确，术语纠正已记录，当前约定范围的 #9 学习验收完成；仍保留调试过程由助手提供提示的归属说明，不等同于验证了完全独立调试的能力。

本轮未进行数据竞争检测、额外并发压力测试或性能评估；按当前范围，这些属于可选巩固，不作为本轮必做前置。
