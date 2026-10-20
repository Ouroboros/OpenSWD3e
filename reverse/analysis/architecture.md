# 代码架构

本工程的代码架构与 swd3-rebuild 的 reverse/analysis/architecture.md（其 GOAL 版本 3）相同，代码以其 src/、tests/ 为底稿复制后按 swd3e 汇编改写。命名空间 `swd3e`，程序名 `swd3er`，设置文件 `swd3er.toml`。

## 代码组织

- 每个原模块是一个 C++ 类或一组相关类型，放在 `src/<模块>/`，用 `.h`／`.cpp` 组织。原程序有类名证据的沿用其职责划分（日志字符串中的 `Menu::`、`Botl::`、`MemMgr` 等）。目录、文件、类名与函数名与 swd3-rebuild 平行。
- 原程序的全局变量归入所属模块类的成员。不允许可写的自由全局变量、全局状态结构或数据镜像。
- 原程序的全局对象成为根对象 `Game` 的成员。子系统通过构造函数取得所依赖子系统的引用；单元测试可单独构造子系统。
- 原程序的静态初始化 thunk（`__xc` 表 `.data 0x4AF004..`）对应 `Game` 成员的构造与析构顺序；函数表 source 指向对应的构造／析构代码。
- 数据记录定义为带命名字段的 struct，布局、大小与偏移以 swd3e 汇编为准，用 `static_assert` 锁定。含义未确定的字段在类型内以 `unkXX`（XX 为十六进制偏移）占位，并注明已知读写者。代码中不得按数字偏移访问记录。外传字段偏移与正传不同的线索见 reverse/analysis/export/match_swd32/offsets.tsv。
- 原文件格式由读取函数按字段解析到 struct，不依赖主机内存布局。
- 只读常量表与可写全局的非零初值放进所属模块（带类型的 `constexpr` 表或成员默认值），由单元测试按原布局序列化后与 swd3e.exe 字节比对，作为数据核对证据。
- 函数指针表按原用途改写为虚函数、成员函数指针或枚举分派。
- 游戏文本为 Big5 字节串；源码中的文本常量以繁体 `u8"…"` 字面量书写，用到时经 `Big5()` 转换；格式化保持原 `wsprintfA` 的输出。
- 动态内存用容器与 `std::unique_ptr` 管理；影响可见行为的链表保留为显式链表类型。

## 平台层（src/platform/）

与 swd3-rebuild 相同：Display（SDL3 窗口，逻辑 640×480 RGB565，窗口默认 1280×960、可拉伸，最近邻等比缩放加黑边，Alt+Enter 无边框全屏）、Surface、Input、AudioDevice（SDL3）、MediaDecoder（FFmpeg：MP3、Bink 视频与音频）、GlyphRasterizer（由 MINGLIU.TTC 生成的 font.bin）、FileSystem（POSIX 上大小写不敏感）、LogFile（可执行文件同目录 logs/）。不模拟 Win32、DirectDraw、DirectInput、Miles 与 GDI 的 API。

资源：third_party/asset/font.bin 由 scripts/make_font_library.py 生成（与正传逐字节相同）；third_party/asset/launcher 由 scripts/make_resources.py 从 swd3e.exe 提取（14 个文件；与正传相比 6 张启动位图不同，窗口图标资源编号为 125 而非 101）。

## 与原程序的已知偏差

只允许下列类别，每一处都在 reverse/analysis/deviations.md 逐条记录函数地址、原行为和新行为：

1. 越界读写、破坏内存类 BUG 不复现，按合理边界处理。
2. 读取未初始化内存：原值依赖堆填充或栈残留时，改为确定的初值。
3. 显示方式：窗口化、缩放与全屏切换。
4. 平台 API 替换：只保证游戏可见行为一致。

逻辑类 BUG（计算、分支、顺序、随机数消费、状态副作用）仍完整保留。

## 对齐期的迁移规则

- 底稿：swd3-rebuild 提交 `6f7214a` 的 src/、tests/、cmake/、scripts/、CMakeLists.txt、CMakePresets.json、.clang-format、docs/。复制后 `swd3`→`swd3e`、`swd3test`→`swd3etest`、`swd3r`→`swd3er`、`OpenSWD3`→`OpenSWD3e`；构建脚本中的 `SWD3_*` 变量名不变。
- 地址注释由 scripts/align_swd32.py 换算：已配对的函数注释为 `// swd3e.exe 0x<地址>`；正传单独登记的 EH funclet 注释为“EH funclet (tail) of 0x<父函数>”；外传无对应的标 `// swd32.exe 0x<地址> (unmatched in swd3e)`，这些代码在外传中是否存在、以何种形式存在须逐个核实，核实前不得作为外传实现。
- 头文件成员的全局地址注释：已映射的为 swd3e 地址；未映射的为 `// swd32 0x…`，不列入 globals.tsv，核实后改为 swd3e 地址。以正传地址命名的 `unk<地址>` 成员名在核实所属函数时一并改名。
- 函数体内的指令地址注释（`// 0x00……`）在函数按 swd3e 改写前仍是正传地址，改写时一律换为 swd3e 地址。
- 测试中与 exe 字节比对的偏移在所属模块改写时换为 swd3e 地址；比对文件为 swd3e.exe。
- 函数表：hash 相同且可复用的函数与配上的 CRT 函数记 implemented_pending_review；其余已配对函数保持 pending_implementation，source 指向平行的源码位置，改写后再改状态。
- scripts/register.sh 会把文件中每个 `// swd3e.exe 0x…` 注释对应的函数登记为已实现；复制来的文件注释已是 swd3e 地址，因此只对其中函数已全部按 swd3e 改写的文件运行 register.sh，未改完的文件按函数逐行更新函数表。
- 单元测试：仍对照 swd32 数据（黄金结果、存档、swd32.exe 字节）的用例列在 tests/pending_swd3e.txt，运行时报告 [PEND] 并跳过；所属模块改写后按 swd3e 重写期望值并移出清单，GOAL 完成时清单为空。
