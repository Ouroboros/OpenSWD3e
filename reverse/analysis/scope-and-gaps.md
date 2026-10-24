# 扫描范围与缺口

数据来源：`export/info_reopen.json`、`export/gap_probe.json`、`export/bounds_fix_log.json`、`export/dinput_table_fix.json`、`export/merge_dead_tail.json`、`export/indirect_reopen.json`。查询脚本位于 `scripts/`。

## 扫描范围

- 二进制：`swd3e.exe` 全部已加载段 `0x401000..0x57C000`（.text/.idata/.rdata/.data）。
- 函数外代码头：在可执行段 `.text` 上按 `idautils.Heads` 枚举，条件 `is_code(flags)` 且 `get_func(ea) is None`。
- 代码指针：扫描全部非 `.text` 段的4字节对齐 dword，值落在 `.text` 内者。
- `.text` 非代码区：`.text` 内非代码且不属于函数的连续字节区间。
- 间接分支：全部函数内 `call/jmp` 的寄存器、内存间接操作数。

## 函数外代码头处理（已闭合）

自动分析后：1398个函数外代码头，125个区域（`export/unowned_regions.json`）。处理后为0。逐项动作与证据见 `export/bounds_fix_log.json`：

| 类别 | 处理 | 数量 | 依据 |
| --- | --- | ---: | --- |
| 动态初始化器 `_E*`、atexit注册桩、atexit回调、析构跳转桩 | 新建函数 | 69 | `.data` 初始化表指针、`push offset` 引用、跨区域直接 `jmp`（目标前为对齐或返回） |
| 由代码以 `offset` 引用的处理函数（含 switch） | 新建函数 | 5 | `0x440CF0`、`0x44DC60` 中的 `offset` 立即数 |
| CRT 初始化/终止项、回调、`_allmul` 等 | 新建函数 | 10 | 初始化表指针、`push offset`、尾跳转；`0x4A531D`、`0x4A53B0` 无引用（见下） |
| `__CxxFrameHandler` | 新建函数 | 1 | 全部 `__ehhandler` 桩的共同跳转目标 |
| `__ehhandler` 桩、unwind funclet | 挂为父函数尾块 | 87 | FuncInfo（magic `0x19930520`）由父函数经桩引用；unwind map 动作地址 |
| 紧随父函数且仅被父函数引用的块 | 延伸父函数 | 4 | `0x418880` 计算跳转目标、`0x436050` `int 3` 后尾声、`0x4A110F` `jmp eax` 后尾声、`start` 的 `__except` 块（scope table `0x4ACA50`） |
| 误反汇编的 `VC20XC00` | 改为字符串 | 1 | 字节为 MSVC EH 魔数串，无引用 |

撤销记录：首轮规则把 `.text` 内跳转表数据与 switch 间接 `jmp` 当作入口证据，导致 `0x4519C0`、`0x453510` 被切碎；已回退数据库并改为仅接受代码指令的 `offset` 与直接近跳转，重跑后两函数完整。

后续修正：

- `0x4A0C64`、`0x4A0C74`、`0x4A0CE4`、`0x4A0CF4` 四个 `nullsub` 为 `c_dfDIKeyboard_objects` 表内 dword 误识别，已删除并恢复为 dword（`export/dinput_table_fix.json`，表头与256项全部核对通过）。
- `0x4A21E5` 无引用 `retn` 紧随 `0x4A21C2` 的 `call ExitProcess`，并入 `0x4A21C2`（`export/merge_dead_tail.json`）。

函数数变化：自动分析1620，新建85，删除伪函数4，合并1，结果1700。

还原菜单时补充（2026-10-22）：菜单处理表 `0x440CF0`（按键处理）与 `0x44DC60`（离开处理）以 `offset` 写入的10个入口被自动分析挂为跳转到它们的函数的尾块，函数表因此缺少这10个独立函数。以 `scripts/ida_split_handler_chunks.py` 对每个父函数调用 `idc.remove_fchunk` 后在入口建函数（父函数的跳转成为尾调用），结果见 `export/split_handler_chunks.json`，重开核对函数1710个、函数外代码头0（`export/ida_reopen_log.txt`）：

| 入口 | 原父函数 | 用途 |
| --- | --- | --- |
| `0x441BB0` | `0x441C80` | 图鉴离开 |
| `0x441EC0` | `0x441C80`、`0x44EB30` | 图鉴 PageDown |
| `0x441FA0` | `0x44EE90` | 图鉴左键 |
| `0x4420A0` | `0x441C80`、`0x44F100`、`0x44F130` | 图鉴 Shift |
| `0x4420C0` | `0x441C80`、`0x44FA60` | 图鉴取消 |
| `0x443150` | `0x443CB0` | 祭坛离开 |
| `0x443660` | `0x4432E0` | 祭坛 PageDown |
| `0x4437A0` | `0x4432E0` | 祭坛左键 |
| `0x443990` | `0x4432E0`、`0x443CB0` | 祭坛选择 |
| `0x445760` | `0x4465D0` | 装备离开 |

脚本要点：`func_parent_iterator_t` 存活时锁住所遍历的尾块，`remove_fchunk` 随之失败，收集父函数后须释放迭代器；尾块属主最后拆除。其余以 `&loc_` 引用的代码地址（`0x41EF80` 的跳转表标签、CRT 内部标签）位于函数内部，不是独立入口。

## 无引用函数（保留，已还原）

| 地址 | 说明 |
| --- | --- |
| `0x43E740` | 手写汇编解压例程（`lodsb; cmp al,11h`，推断为LZO1X变体），无代码/数据引用 |
| `0x4A531D` | CRT区，位于 `0x4A5260` 返回之后，`retn 4` |
| `0x4A53B0` | CRT区，64位乘法形态（推断 `_allmul`），`retn 10h` |

## `.text` 非代码区

自动分析后1470段，其中非填充94段：90段为函数后的 switch 跳转表；其余4段：

- `0x4A0036..0x4A1058`：`c_dfDIKeyboard_objects`/`c_dfDIKeyboard` 数据（`0x43BB82` 引用 `0x4A1040`），已定义为数据，内容属待核对数据。
- `0x4A5255`：`VC20XC00` 字符串（EH魔数）。
- `0x4A3965`、`0x4A6357`：`lea esp,[esp+0]`/`mov edi,edi` 对齐填充。

## 间接调用与计算跳转（已闭合）

依据：`reverse/analysis/scripts/ida_scope_check.py`（在 tmp/ 的数据库副本上只读运行）收集映像内全部“取址值”——非 `.text` 段及 `.text` 数据项中每个对齐 dword、以及每条指令中除直接近调用/跳转目标外的立即数与地址操作数——凡落在 `.text` 的值按目标分类；`scope_summary.py` 与函数表对照。结果见 `reverse/evidence/scope/scope-check-summary.txt`。

- 数据库函数1710个，与 `reverse/functions.tsv` 完全一致（双向无差）。
- 取址值落在函数入口的232个，全部已登记；落在任何函数之外的代码：0。
- 落在函数内部指令头或内部字节的值，按来源逐一核对：
  - 本函数的 switch 跳转表（函数体后的 `.text` 数据）、`jmp ebx` 的 `mov/lea ebx, offset loc_*` 标签（`0x418310` 等7个函数）、CRT `0x4A16B0`/`0x4A4F20` 的内嵌跳转表：目标都在来源所属函数内。
  - `.rdata` `0x4AD034..0x4AD66C` 的 EH 展开表与 `0x4ACA20..0x4AD00C` 的 CRT SEH 作用域表：目标是所属函数的 EH funclet 或过滤/处理标签（funclet 作为尾块挂在父函数下，实现中以父函数的展开与 `unique_ptr` 析构表达）。
  - 巧合数值：`0x4A0452..0x4A1054` 的 DirectInput 数据格式（`c_dfDIKeyboard`）字段；`.rdata` 导入名字串（`0x4ADD14`、`0x4AE91C`、`0x4AE6D4`、`0x4ADCD0`、`0x4ADE40`、`0x4ADD54`、`0x4ADF70`、`0x4B12D0` 处为 “…A\0”+hint）；`.data` Big5 字串（`0x4BECF8`、`0x4B1034`）与字表（`0x4BF9B8`、`0x4C0918`）；`0x4325B4` 为 switch 索引表后的对齐填充 `8D 49 00`。
- `0x44DF50` 的 `jmp dword_50A8B0[eax*4]`：菜单绘制表，静态内容全为 `0xFFFFFFFF`，运行时由菜单页切换写入处理函数（menu-rewrite.md）；写入的值来自取址的函数入口，均已登记。
- 间接分支1551处（432个函数）：目标只能来自上述取址值（已全部落在登记函数或本函数标签内）或导入表；映像中无运行时生成的代码。由此函数全集在静态取址意义下闭合。

## 资源与数据范围

- `.rsrc`：19项资源（WAVE 119～121，CURSOR 1，BITMAP 113～118，ICON 2～5，DIALOG 106、107、112，GROUP_CURSOR 103，GROUP_ICON 125），清单见 `reverse/evidence/scope/rsrc-list.txt`。以 `scripts/make_resources.py` 从 swd3e.exe 重新提取的14个文件与 `third_party/asset/launcher/` 逐字节一致（`reverse/evidence/scope/rsrc-sha256.txt`）。
- 外部数据文件清单见 `structure-overview.md`；格式、读写函数与使用关系在逐函数验证时核对，证据归档至 `reverse/evidence/`。

## 范围结论

- 已识别函数：1710（`reverse/functions.tsv`，含菜单处理表引用的10个尾块入口）。
- 函数外代码头：0；取址目标在函数之外：0。
- 函数全集：1710（静态取址闭合；第4阶段逐函数验证中如发现遗漏，先补表再还原）。
