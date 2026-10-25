# 动态初始化表与编译器异常数据核对

## 初始化与终止表（.data 0x004AF000..0x004AF0A4）

- 分界取自 _cinit（0x004A1C50）与 doexit（0x004A1C9F）的 _initterm 参数：__xi = [0x004AF078, 0x004AF08C)（0x004A1C60／0x004A1C5B）、__xc = [0x004AF000, 0x004AF074)（0x004A1C6F／0x004A1C6A）、__xp = [0x004AF090, 0x004AF098)、__xt = [0x004AF09C, 0x004AF0A4)（0x004A1D04／0x004A1CFF、0x004A1D15／0x004A1D10）。
- __xc：0x004AF000 为 0（段首），0x004AF004 起 28 个游戏全局对象初始化入口，后以 0 结束；逐项见下表，均在函数表中，第一遍证据见 reverse/evidence/pass1/global_init.md（对象构造与注册析构）与 audio_mss.md（0x0049CDC0、0x0049CE50）。
- __xi：0、0x004A1676 __onexitinit、0x004A4E63 __initstdio、0x004A78B9 __initmbctable、0x004A670A __CxxSetUnhandledExceptionFilter；__xp：0、0x004A4F08 __endstdio；__xt：0、0x004A671B __CxxRestoreUnhandledExceptionFilter。CRT 项的身份见 reverse/evidence/pass1/crt.md。
- 重建：游戏对象由 Game 及其成员的构造与析构承担（global_init.md），CRT 项由当前运行库承担。

| 序号（自 0x004AF004 起，每项 4 字节） | 值 | 函数表映射 |
| --- | --- | --- |
| 0 | 0x00409D70 | src/game/game.cpp:Game::Game |
| 1 | 0x00409DB0 | src/game/game.cpp:Game::Game |
| 2 | 0x00409DD0 | src/game/game.cpp:Game::Game |
| 3 | 0x00409E20 | src/game/game.cpp:Game::Game |
| 4 | 0x00409E60 | src/game/game.cpp:Game::Game |
| 5 | 0x00409EA0 | src/game/game.cpp:Game::Game |
| 6 | 0x00409EE0 | src/game/game.cpp:Game::Game |
| 7 | 0x00409F20 | src/game/game.cpp:Game::Game |
| 8 | 0x00409F60 | src/game/game.cpp:Game::Game |
| 9 | 0x00409FA0 | src/game/game.cpp:Game::Game |
| 10 | 0x00409FE0 | src/game/game.cpp:Game::Game |
| 11 | 0x0040A020 | src/game/game.cpp:Game::Game |
| 12 | 0x0040A060 | src/game/game.cpp:Game::Game |
| 13 | 0x0040A0A0 | src/game/game.cpp:Game::Game |
| 14 | 0x0040A0E0 | src/game/game.cpp:Game::Game |
| 15 | 0x0040A100 | src/game/game.cpp:Game::Game |
| 16 | 0x0042B1B0 | src/map/map_scale.cpp:MapScale::MapScale |
| 17 | 0x0042B480 | src/game/game.cpp:Game::Game |
| 18 | 0x00459880 | src/game/game.cpp:Game::Game |
| 19 | 0x004598E0 | src/game/game.cpp:Game::Game |
| 20 | 0x00459940 | src/game/game.cpp:Game::Game |
| 21 | 0x00459980 | src/game/game.cpp:Game::Game |
| 22 | 0x004599A0 | src/game/game.cpp:Game::Game |
| 23 | 0x0049C8E0 | src/game/game.cpp:Game::Game |
| 24 | 0x0049CDC0 | src/audio/audio_manager.h:AudioManager::system |
| 25 | 0x0049CE50 | src/audio/audio_manager.h:AudioManager::queue |
| 26 | 0x0049D3C0 | src/game/game.cpp:Game::Game |
| 27 | 0x0049E050 | src/game/game.cpp:Game::Game |
| 28 | 0x00000000 | （分隔） |
| 29 | 0x00000000 | （分隔） |
| 30 | 0x004A1676 | CRT:__onexitinit -> C++ runtime startup |
| 31 | 0x004A4E63 | CRT:__initstdio -> C++ runtime startup |
| 32 | 0x004A78B9 | CRT:__initmbctable -> C++ runtime startup |
| 33 | 0x004A670A | CRT:__CxxSetUnhandledExceptionFilter -> C++ runtime startup |
| 34 | 0x00000000 | （分隔） |
| 35 | 0x00000000 | （分隔） |
| 36 | 0x004A4F08 | CRT:__endstdio -> C++ runtime shutdown |
| 37 | 0x00000000 | （分隔） |
| 38 | 0x00000000 | （分隔） |
| 39 | 0x004A671B | CRT:__CxxRestoreUnhandledExceptionFilter |

## C++ EH 与 SEH 表（.rdata 0x004AD010 起的 FuncInfo／unwind map，0x004ACA48 起的 SEH scope table）

- 编译器为带 EH 的函数生成的元数据，只由 __CxxFrameHandler（0x004A11A0）与 _except_handler3（0x004A5260）解释；其 unwind funclet 已作为函数表中的 EH funclet 行逐个核对（多为释放局部对象）。第一遍中核对的这些 funclet 都是 operator new 之后的构造抛出异常时释放刚分配的内存（如 0x004A97E0 释放 [ebp−10h]），只在构造失败时运行。重建由 C++ 的析构与 unique_ptr 承担同样的清理，不复制这些表。
