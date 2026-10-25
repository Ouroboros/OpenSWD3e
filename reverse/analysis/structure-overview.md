# 初步结构概览

依据：`reverse/analysis/swd3e.i64`（1700函数），导出 `export/structure.json`（调用、导入、字符串、数据引用）、`export/functions_reopen.json`。模块划分由 `scripts/build_function_table.py` 生成，规则见文末。全部为初步推断，GOAL执行中按实际逻辑修正。

## 程序骨架

- 编译器：MSVC（C++，静态CRT，C++ EH `__CxxFrameHandler` `0x4A11A0`，FuncInfo magic `0x19930520`，SEH `__except_handler3` 风格scope table）。依据：EH结构、CRT字符串（`Runtime Error!`、`1#QNAN`）、`_WinMain@16` 调用约定。
- 启动链：`start` `0x4A20A7`（CRT）→ 动态初始化表 `.data 0x4AF004..0x4AF0A0`（`_initterm`）→ `_WinMain@16` `0x40A140`。
- 主循环与窗口：`0x40A140..0x40AEC0`（窗口类 `SoftstarSwd3`、`GetMessage/PeekMessage` 循环、`DefWindowProc`、截图 `ScrnShot\%05d.bmp`）。
- 平台层：DirectDraw（`ddraw` `0x43C030..`，错误文本 `ddraw_error` `0x436050..`）、DirectInput（`dinput` `0x43BA60..`，键盘数据格式表 `c_dfDIKeyboard` `0x4A1040`）、GDI文字（`gdi_text`）、Miles音频（`sound` `0x49C900..`，sequence/sample/stream 与 `all.snd`）、Bink视频（`video` `0x49C440..`）。
- 基础设施：文件/映射文件包装（`file` `0x43CB10..`）、堆管理 `MemMgr`（`memmgr` `0x43D120..`）、DBCS字符串辅助（`strutil`）、资源包（`respack`：`all_*.tsw`/`all_*.act`）。
- 游戏逻辑：地图事件（`event`）、寻路（`path`）、存档（`save`：`save\%d.sav`、`Fame.dat`、`swdghost.dat`）、地图绘制（`picpaint`）、地图数据（`mapdata`：`data\%d.cm`、`mcache.dat`、`HUGE.lmf`）、对话（`talk`、`dlg_message`、`figtalk`）、菜单（`menu`）、战斗（`battle`、`battle_msg`）、角色（`role`/`manrole`/`org`，断言路径 `C:\cpp\SWD102e\role.cpp` 等）、动作（`act`：`Attack Act` 等）、作弊对话框（`cheat`）。

## 初步模块表

| module | 函数数 | 字节 | 地址范围 | 主要依据 |
| --- | ---: | ---: | --- | --- |
| event | 7 | 8925 | 0x402AA0..0x404DD0 | `Act Err(KeyCole:Talk*)`、`RoleEventBy`/`MapEvent` |
| path | 7 | 6461 | 0x405150..0x406780 | `Act Err(Path:*)`、`CompanyWalk` |
| save | 18 | 12781 | 0x406B50..0x409C80 | `%ssave\%d.sav`、`Fame.dat` |
| main | 64 | 4447 | 0x409D70..0x40AEC0 | 动态初始化器、`_WinMain@16`、窗口过程 |
| dialog_frame | 3 | 2954 | 0x40B370..0x40BEA0 | `SetupDialog`、`Act Err(Dialog:*)` |
| misc | 60 | 13862 | 0x40C420..0x40FA20 | `Act Err(Misc:*)`、`maps.dat`、`RoleDataBase` |
| cheat | 7 | 6786 | 0x40FBE0..0x4115D0 | `CheatDialog` |
| sysinfo | 8 | 2404 | 0x411D10..0x4121C0 | 磁盘空间、卷标 `SWD3E_1` |
| picpaint | 50 | 22899 | 0x4126A0..0x4277A0 | `Act Err(picPaint:*)` |
| env_config | 4 | 2263 | 0x4277E0..0x427D80 | `%sEnv.dat` |
| startup | 12 | 4297 | 0x428080..0x428EC0 | 初始化失败提示、`Born.avi`、目录创建 |
| mapdata | 19 | 8802 | 0x4291B0..0x42B350 | `data\%d.cm`、`mcache.dat`、`HUGE.lmf` |
| talk | 13 | 32180 | 0x42B4C0..0x433650 | `Act Err(Talk:*)`、`%sTalk%d.dat` |
| dlg_message | 7 | 7170 | 0x433720..0x435AE0 | `Act Err(DlgMessage:*)` |
| ddraw_error | 2 | 774 | 0x436050..0x4360C0 | `DDERR_*` |
| respack | 36 | 8105 | 0x4364D0..0x4386B0 | `all_*.tsw`/`all_*.act` |
| particle | 3 | 2565 | 0x438E10..0x439890 | `particle_put(..)` |
| gdi_text | 29 | 7639 | 0x439BA0..0x43B960 | GDI字体与文字输出 |
| dinput | 3 | 880 | 0x43BA60..0x43BFA0 | `cDINPUT` |
| ddraw | 16 | 2167 | 0x43C030..0x43C8F0 | `DirectDraw * Error!!` |
| file | 23 | 1389 | 0x43CB10..0x43D0F0 | CreateFile/MapViewOfFile 包装 |
| memmgr | 6 | 839 | 0x43D120..0x43D470 | `MemMgr` |
| strutil | 13 | 1269 | 0x43D4A0..0x43DA30 | CharNext/CharPrev |
| menu | 278 | 106564 | 0x43DA70..0x459010 | `Act Err(Menu:*)`、`Botl::*`、`v2.03` |
| battle | 147 | 147355 | 0x4591E0..0x479860 | `battle.ffd`、`Battle01.mp3` |
| figtalk | 3 | 728 | 0x47E270..0x47E420 | `figtalk.dat` |
| manrole | 9 | 2622 | 0x47E930..0x47F3E0 | `manrole.cpp` |
| act | 108 | 37972 | 0x47F5C0..0x489410 | `Attack Act` |
| org | 19 | 5729 | 0x489750..0x48B010 | `org.cpp`、`mon.dat`、`level.dat` |
| role | 19 | 21568 | 0x48B140..0x490F90 | `role.cpp`、`Wait Act`/`Fade Act` |
| battle_msg | 86 | 40106 | 0x490D20..0x49C420 | `$1%s$0` |
| video | 9 | 1004 | 0x49C440..0x49C800 | `Bink*` |
| sound | 67 | 6633 | 0x49C900..0x4A1070 | `AIL_*`、`all.snd` |
| import_thunk | 8 | 46 | 0x49CE10..0x4A9328 | 导入跳转桩（`FUNC_THUNK`且目标为导入） |
| crt | 224 | 33230 | 0x4A1096..0x4A92FC | MSVC静态运行时区段 |
| unknown | 313 | 103055 | 0x401000..0x4A0010 | 处于不同模块锚点之间且无决定性证据 |

`unknown` 主要集中在 `0x401000..0x402AA0`、`0x415960..0x427080`（含 `jmp ebx` 计算跳转绘制例程 `0x418310..0x41FE04`）、`0x479860..0x47E270`、`0x49B1B0..0x49C440` 等锚点间隙。

## 模块划分规则

1. MSVC 按目标文件顺序链接，同一编译单元的函数在 `.text` 连续。取有字符串、导入或断言路径证据的函数为锚点（列表在 `scripts/build_function_table.py` 的 `ANCHORS`）。
2. 两侧锚点模块相同的区间内函数归该模块。
3. 两侧锚点不同时，以调用者与被调用者在两侧模块中的多数归属决定；平票或无连接记 `unknown`。
4. `FUNC_THUNK` 且直接或经一层桩跳至导入的为 `import_thunk`；`0x4A1096` 起为 `crt`。
5. 动态初始化器（`0x409D70..0x40A13A` 等）随所在地址区间归属，未必对应其全局对象所属编译单元，待还原时按对象类型调整。

## 已识别的共享数据与资源（已核对）

- 动态初始化表 `.data 0x4AF000..0x4AF0A4`（CRT `__xc`/`__xi`/`__xp`/`__xt` 四段，分界与逐项已核对，见 `reverse/evidence/scope/init-tables.md`）。
- C++ EH FuncInfo/unwind map `.rdata 0x4AD010..`；SEH scope table `0x4ACA48..`（编译器元数据，处理依据同上）。
- DirectInput 键盘数据格式 `c_dfDIKeyboard` `0x4A1040`（DIDATAFORMAT）及 `c_dfDIKeyboard_objects` `0x4A0040`（256×16字节），`GUID_Key` `0x4ABCB0`（已核对，见 `reverse/evidence/scope/dinput-keyboard-format.md`）。
- 外部数据文件（由字符串确认）：`maps.dat`、`namex.dat`、`Env.dat`、`swdghost.dat`、`Fame.dat`、`save\%d.sav`、`data\%d.cm`、`mcache.dat`、`HUGE.lmf`、`Talk%d.dat`、`all_char/item/magic/sys.tsw/.act`、`figtalk.dat`、`mon.dat`、`level.dat`、`battle.ffd`、`all.snd`、`music\*.mp3`、`Video\*`（Bink）、`Born.avi`。
