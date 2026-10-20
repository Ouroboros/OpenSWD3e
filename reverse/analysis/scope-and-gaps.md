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

## 无引用函数（保留，待还原时确认）

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

## 间接调用与计算跳转缺口（未闭合）

- 间接分支898处：switch 134；非 switch 764（寄存器/内存间接调用731，间接 `jmp` 33）。目标集合依赖对象 vtable、函数指针表与运行时数据，尚未逐个解析；还原各调用者时补录目标并核对是否落在已登记函数入口。
- `jmp ebx` 计算跳转22处，位于 `0x418310`、`0x418880`、`0x419F30`、`0x41AD70`、`0x41C3B0`、`0x41DD00`、`0x41EF80`；目标由函数内 `mov ebx, offset loc_*` 装入，IDA已纳入函数体，还原时需核对每个目标均在函数范围内。
- `0x44DF50` `jmp dword_50A8B0[eax*4]`：`.data` 中运行时跳转表，表内容与目标待核。
- CRT `0x4A16B0`、`0x4A4F20` 中 `jmp ds:loc_*[reg*4]`：代码内嵌跳转表（memcpy/memmove 形态）被部分识别为代码标签，表边界待核。
- 数据段中7个指向函数内部非指令头的 dword（`0x41656C`、`0x41656E`、`0x416D61`、`0x417478`、`0x41786F`、`0x46A462`、`0x480020`）：可能为巧合数值，待查引用处的数据类型。

## 资源与数据范围（未闭合）

- `.rsrc`（VA `0x57C000`，`0x61000`字节）未加载入数据库；对话框 `SetupDialog`、`CheatDialog`、图标、光标等资源属于范围，需要另行提取并核对。
- 外部数据文件清单见 `structure-overview.md`；格式、读写函数与使用关系在还原对应函数时逐一核对，证据归档至 `reverse/evidence/`。

## 范围结论

- 已识别函数：1700（`reverse/functions.tsv`）。
- 函数外代码头：0。
- 函数全集：待确定，取决于上述间接调用目标、运行时跳转表和资源核对结果。
