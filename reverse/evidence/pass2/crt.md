# 第二遍：平台导入与静态 CRT（函数表 order 1480–1710，231 个）

## 方法与结论 — exact（身份复核；平台替换与运行库映射）

依据 AGENTS.md“CRT 与命名”：已识别的 MSVC 6 静态 CRT 不重写，游戏的调用处改用当前 C/C++ 运行库，至多在 src/crt/ 包装一层。第二遍不沿用第一遍的结论，重新从汇编独立提取：

1. 每个例程的汇编行数、调用的导入、调用的函数、引用的字符串（逐行表，见文末）。导入与字符串都与函数表所记的 MSVC 6 例程相符，例如 _heap_init 调 HeapCreate、_setargv 调 GetModuleFileNameA 与 parse_cmdline、_NMSG_WRITE 引用 "Runtime Error!" 与 R6002、$I10_OUTPUT 引用 "1#SNAN"、"1#IND"、"1#INF"、"1#QNAN"、_tzset 引用 "TZ"、__crtMessageBoxA 动态载入 user32.dll 的 MessageBoxA；各例程的调用关系闭合在 CRT 之内。
2. 全程序的调用者：只被 CRT 内部调用、或无调用者（启动入口、异常与退出例程）的例程，游戏代码不依赖其内部行为，映射到编译器与当前运行库。被游戏代码直接调用的例程逐个对照汇编与源码映射（下节）。

## 被游戏直接调用的例程

- 0x004A0030 DirectDrawCreate、0x004A1058 DirectInputCreateA、0x004A105E ImmIsIME：导入 thunk（jmp [导入表]），调用者分别为 0x0043C0D0、0x0043BA70、0x0043D620，平台替换见偏差表对应行（SDL3 画面、事件、文字输入）。
- 0x004A1070、0x004A1080、0x004A1090（AudioDevice::~AudioDevice）：atexit(0x004A1080) 后返回 0；0x004A1080 跳 0x004A1090；0x004A1090 jmp [AIL_shutdown]。由 0x0049CE20（AudioSystem::Startup）调用，属偏差 25。
- 0x004A10E8 _ftol：fstcw，控制字或 0xC00（截断）后 fldcw，fistp 64 位，恢复控制字，返回 edx:eax。源码 crt::Ftol = llrint(trunc(x))，NaN 与越界同为 0x8000000000000000。
- 0x004A14E2 srand：种子写 0x004C04E4；0x004A14EC rand：种子 × 0x343FD + 0x269EC3，返回 (种子 >> 16) & 0x7FFF。源码 Random::Seed／Next 用 std::srand／std::rand 并取 15 位，属偏差 254（用户要求的平台替换；该行的种子地址已按汇编更正为 0x004C04E4）。
- 0x004A150A difftime：整数 end − start 经 fild 返回 double。源码 crt::Difftime = std::difftime，同为两个秒数之差（调用者 0x00406EC0 的两个时间相近，32 位相减不会回绕）。
- 0x004A151B time：GetLocalTime 经 __loctotime_t（含时区与夏令时）换成 time_t。源码 crt::Time = std::time，同为 UTC 起算的秒数。
- 0x004A1664 atexit、0x004A15F7 _onexit：静态对象的析构登记；源码由 Game 成员析构代替。
- 0x004A16B0 memcpy：与 0x004A4F20 memmove 同一结构（判断重叠、按方向分段复制），调用者用 std::memmove／std::copy。
- 0x004A19E5 vsprintf：以字符串 FILE 调 _output（0x004A314F），再 _flsbuf 写结尾 0；源码 crt::Sprintf（std::snprintf），浮点舍入差异属偏差 262。
- 0x004A1A50 strstr：逐字节查找，空子串返回原串；调用者用 std::string_view::find，结果相同。
- 0x004A1AD0 operator new（_nh_malloc(n, 1)）、0x004A1ADE malloc（_nh_malloc(n, _newmode)）、0x004A1A36 operator delete 与 0x004A1B52 _free_base：游戏的堆块经 0x0043DA70／0x0043DAB0 与容器；源码用 HeapBlock、std::vector 与 std::unique_ptr。
- 0x004A207E _msize：__sbh_find_block 找到 → 块头 [p−4] − 9，否则 HeapSize。唯一的游戏调用者 0x00489B00 用它减说明字节计数，源码以 HeapBlock::ReportedSize 按 0x0043DA70 的取整、0x004C0534 = 0x3F8 的小块堆阈值与 (n+0x17) & ~0xF 的块大小算出相同的值（0x00489B00 已据此复核，见 org.md）。
- 0x004A1B81 floor：_math_entry 后 frndint（_frnd）向下取整，NaN／无穷经 _sptype 与 _handle_exc；源码 crt::Floor = std::floor（注释已改为 swd3e 中 0x004A45C2..0x004A4E0D、0x004A7185 的各个错误处理例程）。
- 0x004A1E72／0x004A1EF4 eh vector constructor／destructor iterator：0x00459890 等全局初始化片段构造与析构单位数组；源码为 std::array 成员。
- 0x004A2073 atoi → 0x004A1FE8 atol：C 区域设置下跳过空白（0x09..0x0D、0x20）、可带正负号、累加十进制数字；唯一调用者 0x00479360 的颜色代码由 BattleMessages::Draw 的 ParseCode 解析，规则相同（游戏文字中颜色代码只有一位数字）。
- 0x004A11A0 __CxxFrameHandler：函数的异常帧处理，由编译器生成。

差异：无（偏差 254 的种子地址笔误已更正；_msize 的使用者 0x00489B00 已复核修正）。

## 逐行身份表（第二遍独立提取）

由 reverse/analysis/asm/ 逐个读出：汇编行数、调用的导入、调用的函数（按函数表命名）、引用的字符串，以及全程序的调用者模块。

| 行 | 地址 | 函数表名称 | 行数 | 导入 | 调用 | 字符串 | 调用者 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1480 | 0x004A0030 | import:DirectDrawCreate | 1 | DirectDrawCreate |  |  | CRT 0；游戏 gfx:0x0043C0D0 |
| 1481 | 0x004A1058 | import:DirectInputCreateA | 1 | DirectInputCreateA |  |  | CRT 0；游戏 input:0x0043BA70 |
| 1482 | 0x004A105E | import:ImmIsIME | 1 | ImmIsIME |  |  | CRT 0；游戏 text_dbcs:0x0043D620 |
| 1483 | 0x004A1070 | src/platform/audio_device.cpp:AudioDevice::~ | 5 |  | atexit |  | CRT 0；游戏 audio_mss:0x0049CE20 |
| 1484 | 0x004A1080 | src/platform/audio_device.cpp:AudioDevice::~ | 1 | AIL_shutdown |  |  | CRT 0；游戏 audio_mss:0x004A1070 |
| 1485 | 0x004A1090 | src/platform/audio_device.cpp:AudioDevice::~ | 1 | AIL_shutdown |  |  | CRT 0；游戏 audio_mss:0x004A1080 |
| 1486 | 0x004A1096 | CRT:_fpmath | 6 |  | _cfltcvt_init,_ms_p5_mp_test_fdiv,_setdefaultprecision |  | CRT 1 |
| 1487 | 0x004A10AD | CRT:_fptrap stub | 1 |  |  |  | CRT 0 |
| 1488 | 0x004A10AE | CRT:_cfltcvt_init | 8 |  |  |  | CRT 1 |
| 1489 | 0x004A10E8 | src/crt/msvc.h:Ftol | 15 |  |  |  | CRT 0；游戏 battle:0x00438700,0x00439890,0x00458330,0x00459200+23;core:0x0043CA70,0x0047E450;game:0x00406EC0,0x00411DA0,0x004270B0;ghost:0x0049E740;input:0x0043BF80;map:0x004143B0,0x00414CC0,0x00415120,0x00435EF0;menu:0x004405E0,0x00449840,0x00449BE0,0x00449EB0;minigame:0x0049FF60;role:0x00405220;talk:0x0042BAF0 |
| 1490 | 0x004A110F | CRT:_JumpToContinuation | 22 | eax |  |  | CRT 1 |
| 1491 | 0x004A1143 | CRT:_CallMemberFunction0 | 4 | eax |  |  | CRT 2 |
| 1492 | 0x004A114A | CRT:_CallMemberFunction1 | 4 | eax |  |  | CRT 1 |
| 1493 | 0x004A1151 | CRT:_UnwindNestedFrames | 29 | RtlUnwind |  |  | CRT 2 |
| 1494 | 0x004A11A0 | CRT:__CxxFrameHandler | 27 |  | __InternalCxxFrameHandler |  | CRT 0；游戏 audio_mss:0x0049CA10,0x0049D430;battle:0x0045BEA0,0x0045EE20,0x0047E660,0x0047E6D0+3;config:0x004278C0,0x00427BD0,0x00427D80;config_env:0x004277E0;game:0x00406EC0,0x00409B90,0x00409C80,0x0040A380+1;ghost:0x0049E910,0x0049EA80;map:0x004291B0,0x00429520,0x004295F0,0x004296B0+3;menu:0x00451B60;movie_bink:0x0049C600;role:0x004061B0,0x00406780,0x00432630,0x00432800;saveload:0x00409320,0x00409480,0x004098C0,0x00409AC0;talk:0x004120A0,0x0042BAF0 |
| 1495 | 0x004A11D6 | CRT:_CallCatchBlock2 | 26 |  | _CallSettingFrame |  | CRT 1 |
| 1496 | 0x004A122A | CRT:CatchGuardHandler | 16 |  | __InternalCxxFrameHandler |  | CRT 1 |
| 1497 | 0x004A124F | CRT:_CallSETranslator | 56 | dword_579AD8 |  |  | CRT 1 |
| 1498 | 0x004A1303 | CRT:TranslatorGuardHandler | 47 | dword | __InternalCxxFrameHandler,_UnwindNestedFrames |  | CRT 1 |
| 1499 | 0x004A1378 | CRT:_GetRangeOfTrysToCheck | 51 |  | _inconsistency |  | CRT 2 |
| 1500 | 0x004A13F4 | CRT:__global_unwind2 | 18 | RtlUnwind |  |  | CRT 1 |
| 1501 | 0x004A1414 | CRT:__unwind_handler | 9 |  |  |  | CRT 2 |
| 1502 | 0x004A1436 | CRT:__local_unwind2 | 33 | dword | _NLG_Notify |  | CRT 3 |
| 1503 | 0x004A149E | CRT:__abnormal_termination | 10 |  |  |  | CRT 1 |
| 1504 | 0x004A14C1 | CRT:_NLG_Notify1 | 4 |  |  |  | CRT 1 |
| 1505 | 0x004A14CA | CRT:_NLG_Notify | 10 |  |  |  | CRT 2 |
| 1506 | 0x004A14E2 | src/crt/msvc.cpp:Random::Seed | 3 |  |  |  | CRT 0；游戏 battle:0x00439250;game:0x0040A140 |
| 1507 | 0x004A14EC | src/crt/msvc.cpp:Random::Next | 7 |  |  |  | CRT 0；游戏 battle:0x00439250,0x00439890;map:0x00435EF0;minigame:0x0049F640;role:0x00406590 |
| 1508 | 0x004A150A | src/crt/msvc.h:Difftime | 8 |  |  |  | CRT 0；游戏 game:0x00406EC0 |
| 1509 | 0x004A151B | src/crt/msvc.h:Time | 75 | GetLocalTime,GetSystemTime,GetTimeZoneInformation | __loctotime_t |  | CRT 0；游戏 battle:0x00439250;game:0x00406EC0,0x0040A140,0x0040A7B0;menu:0x00451DA0 |
| 1510 | 0x004A15F7 | CRT:_onexit | 32 |  | _msize,_realloc_base |  | CRT 1 |
| 1511 | 0x004A1664 | CRT:atexit | 8 |  | _onexit |  | CRT 0；游戏 audio_mss:0x0049CDD0,0x0049CE60,0x0049D3D0,0x004A1070;game:0x00409D90,0x00409E00,0x00409E40,0x00409E80+17;map:0x0042B1C0 |
| 1512 | 0x004A1676 | CRT:__onexitinit | 14 |  | malloc,_amsg_exit |  | CRT 0 |
| 1513 | 0x004A16B0 | CRT:memcpy | 270 | jpt_4A16E5,dword,jpt_4A170C,jpt_4A1894,jpt_4A1872 |  |  | CRT 4；游戏 audio_mss:0x0049DF60;config:0x004278C0;game:0x0040DB00;map:0x004126A0;menu:0x0044AE10;talk:0x0040BEA0;text_dbcs:0x0043DA30 |
| 1514 | 0x004A19E5 | CRT:vsprintf | 31 |  | _output,_flsbuf |  | CRT 0；游戏 core:0x0040C530;map:0x004351F0 |
| 1515 | 0x004A1A36 | CRT:operator delete | 4 |  | _free_base |  | CRT 0；游戏 audio_mss:0x0049CA10;battle:0x0045BEA0,0x0045EE20,0x00493EB0,0x0049B9D0;config:0x004278C0,0x00427BD0,0x00427D80;config_env:0x004277E0;file:0x0043CB10,0x0043CC30;game:0x0040F9E0,0x004172A0;ghost:0x0049E910,0x0049EA80;map:0x00417230,0x004291B0,0x00429520,0x004295F0+1;menu:0x00451B60,0x00451DA0;movie_bink:0x0049C600,0x0049C7B0;talk:0x004120A0,0x0042BAF0;text_dbcs:0x0043D540,0x0043D920 |
| 1516 | 0x004A1A50 | CRT:strstr | 66 |  |  |  | CRT 0；游戏 audio_mss:0x0049D310,0x0049D480;movie_bink:0x0049C600;talk:0x004120A0 |
| 1517 | 0x004A1AD0 | CRT:operator new | 6 |  | _nh_malloc |  | CRT 0；游戏 audio_mss:0x0049CA10;battle:0x0045BEA0,0x0045EE20,0x0049B9D0;config:0x004278C0,0x00427BD0,0x00427D80;config_env:0x004277E0;file:0x0043CB90;ghost:0x0049E910,0x0049EA80;map:0x004291B0,0x00429520,0x004295F0,0x004296B0;menu:0x00451B60;movie_bink:0x0049C600;talk:0x004120A0,0x0042BAF0;text_dbcs:0x0043D4A0,0x0043D920 |
| 1518 | 0x004A1ADE | CRT:malloc | 6 |  | _nh_malloc |  | CRT 12；游戏 memmgr:0x0043D2D0,0x0043DA70 |
| 1519 | 0x004A1AF0 | CRT:_nh_malloc | 16 |  | _heap_alloc,_callnewh |  | CRT 2 |
| 1520 | 0x004A1B1C | CRT:_heap_alloc | 21 | HeapAlloc | __sbh_alloc_block |  | CRT 1 |
| 1521 | 0x004A1B52 | CRT:_free_base | 21 | HeapFree | __sbh_find_block,__sbh_free_block |  | CRT 9；游戏 memmgr:0x0043D310,0x0043D350,0x0043DAB0 |
| 1522 | 0x004A1B81 | src/crt/msvc.h:Floor | 94 |  | _ctrlfp,_sptype,_math_entry,_frnd,_handle_exc |  | CRT 0；游戏 core:0x0043CA70 |
| 1523 | 0x004A1C50 | CRT:_cinit | 12 |  | _initterm |  | CRT 1 |
| 1524 | 0x004A1C7D | CRT:exit | 6 |  | doexit |  | CRT 1 |
| 1525 | 0x004A1C8E | CRT:_exit | 6 |  | doexit |  | CRT 5 |
| 1526 | 0x004A1C9F | CRT:doexit | 49 | GetCurrentProcess,TerminateProcess,eax,ExitProcess | _initterm |  | CRT 2 |
| 1527 | 0x004A1D38 | CRT:_initterm | 12 | eax |  |  | CRT 2 |
| 1528 | 0x004A1D52 | CRT:_realloc_base | 115 | HeapAlloc,HeapReAlloc | malloc,_free_base,__sbh_find_block,__sbh_resize_block,__sbh_alloc_block,memmove,__sbh_free_block,_callnewh |  | CRT 2 |
| 1529 | 0x004A1E72 | CRT:eh vector constructor iterator | 44 |  | __ArrayUnwind |  | CRT 0；游戏 game:0x00459890,0x004598F0 |
| 1530 | 0x004A1EF4 | CRT:eh vector destructor iterator | 42 |  | __ArrayUnwind |  | CRT 0；游戏 game:0x004598C0,0x00459920 |
| 1531 | 0x004A1F74 | CRT:__ArrayUnwind | 35 |  | ExFilterRethrow |  | CRT 2 |
| 1532 | 0x004A1FD2 | CRT:ExFilterRethrow | 7 |  | terminate |  | CRT 2 |
| 1533 | 0x004A1FE8 | CRT:atol | 59 |  | _isctype |  | CRT 2 |
| 1534 | 0x004A2073 | CRT:atoi | 4 |  | atol |  | CRT 0；游戏 battle:0x00479360 |
| 1535 | 0x004A207E | CRT:_msize | 17 | HeapSize | __sbh_find_block |  | CRT 1；游戏 org:0x00489B00 |
| 1536 | 0x004A20A7 | CRT:WinMainCRTStartup | 78 | GetVersion,GetCommandLineA,GetStartupInfoA,GetModuleHandleA,WinMain | _heap_init,_fast_error_exit,_ioinit,__crtGetEnvironmentStringsA,_setargv,_setenvp,_cinit,_wincmdln,exit,_XcptFilter,_exit |  | CRT 0 |
| 1537 | 0x004A219D | CRT:_amsg_exit | 10 | off_4C04F4 | _FF_MSGBANNER,_NMSG_WRITE |  | CRT 7 |
| 1538 | 0x004A21C2 | CRT:_fast_error_exit | 9 | ExitProcess | _FF_MSGBANNER,_NMSG_WRITE |  | CRT 1 |
| 1539 | 0x004A21E6 | CRT:_setdefaultprecision | 6 |  | _controlfp |  | CRT 1 |
| 1540 | 0x004A21F8 | CRT:_ms_p5_test_fdiv | 24 |  |  |  | CRT 1 |
| 1541 | 0x004A2236 | CRT:_ms_p5_mp_test_fdiv | 13 | GetModuleHandleA,GetProcAddress,eax | _ms_p5_test_fdiv | KERNEL32／IsProcessorFeaturePresent | CRT 1 |
| 1542 | 0x004A225F | CRT:_forcdecpt | 37 |  | tolower,_isctype |  | CRT 1 |
| 1543 | 0x004A22B9 | CRT:_cropzeros | 38 |  |  |  | CRT 1 |
| 1544 | 0x004A2307 | CRT:_positive | 11 |  |  |  | CRT 1 |
| 1545 | 0x004A231F | CRT:_fassign | 29 |  | _atodbl,_atoflt |  | CRT 1 |
| 1546 | 0x004A235D | CRT:_cftoe | 115 |  | _shift,_fltout2,_fptostr,strcpy | e+000 | CRT 2 |
| 1547 | 0x004A2461 | CRT:_cftof | 92 |  | _fltout2,_fptostr,_shift,memset |  | CRT 2 |
| 1548 | 0x004A253F | CRT:_cftog | 63 |  | _fltout2,_fptostr,_cftof_g,_cftoe_g |  | CRT 1 |
| 1549 | 0x004A25DA | CRT:_cftoe_g | 9 |  | _cftoe |  | CRT 1 |
| 1550 | 0x004A2601 | CRT:_cftof_g | 8 |  | _cftof |  | CRT 1 |
| 1551 | 0x004A2624 | CRT:_cfltcvt | 29 |  | _cftof,_cftog,_cftoe |  | CRT 1 |
| 1552 | 0x004A2675 | CRT:_shift | 18 |  | strlen,memcpy |  | CRT 2 |
| 1553 | 0x004A269A | CRT:__InternalCxxFrameHandler | 61 | ecx | _inconsistency,__FrameUnwindToState,FindHandler |  | CRT 3 |
| 1554 | 0x004A2735 | CRT:FindHandler | 146 |  | _inconsistency,_ValidateRead,_GetRangeOfTrysToCheck,TypeMatch,CatchIt,DestructExceptionObject,FindHandlerForForeignException,terminate |  | CRT 1 |
| 1555 | 0x004A28DC | CRT:FindHandlerForForeignException | 67 |  | _CallSETranslator,_GetRangeOfTrysToCheck,CatchIt |  | CRT 1 |
| 1556 | 0x004A2984 | CRT:TypeMatch | 42 |  | strcmp |  | CRT 1 |
| 1557 | 0x004A29E1 | CRT:__FrameUnwindToState | 56 |  | _inconsistency,_CallSettingFrame,ExFilterRethrow |  | CRT 2 |
| 1558 | 0x004A2A7F | CRT:CatchIt | 50 |  | BuildCatchObject,_UnwindNestedFrames,__FrameUnwindToState,CallCatchBlock,_JumpToContinuation |  | CRT 2 |
| 1559 | 0x004A2AFA | CRT:CallCatchBlock | 89 |  | _CallCatchBlock2,ExFilterRethrow (catch block),__local_unwind2,__abnormal_termination,DestructExceptionObject |  | CRT 1 |
| 1560 | 0x004A2C06 | CRT:ExFilterRethrow (catch block) | 15 |  |  |  | CRT 1 |
| 1561 | 0x004A2C30 | CRT:BuildCatchObject | 169 |  | _ValidateRead,_ValidateWrite,AdjustPointer,memcpy,_ValidateExecute,_CallMemberFunction1,_CallMemberFunction0,_inconsistency,terminate |  | CRT 1 |
| 1562 | 0x004A2DF4 | CRT:DestructExceptionObject | 39 |  | _CallMemberFunction0,terminate |  | CRT 2 |
| 1563 | 0x004A2E5B | CRT:AdjustPointer | 15 |  |  |  | CRT 1 |
| 1564 | 0x004A2E80 | CRT:_CallSettingFrame | 34 | eax | _NLG_Notify1 |  | CRT 2 |
| 1565 | 0x004A2ECC | CRT:terminate | 37 | eax | _NMSG_WRITE,raise,_exit |  | CRT 6 |
| 1566 | 0x004A2F22 | CRT:_inconsistency | 37 |  | terminate,_NMSG_WRITE,raise,_exit |  | CRT 5 |
| 1567 | 0x004A2F78 | CRT:__loctotime_t | 65 |  | __tzset,_isindst |  | CRT 1 |
| 1568 | 0x004A303A | CRT:_flsbuf | 105 |  | _isatty,_getbuf,_write,_lseek |  | CRT 2 |
| 1569 | 0x004A314F | CRT:_output | 595 | jpt_4A31BD,def_4A31BD,off_4C04FC,off_4C0508,off_4C0500 | get_int_arg,write_char,get_short_arg,wctomb,strlen,get_int64_arg,_aullrem,_aulldiv,write_multi_char,write_string | (null) | CRT 1 |
| 1570 | 0x004A3890 | CRT:write_char | 25 |  | _flsbuf |  | CRT 3 |
| 1571 | 0x004A38C5 | CRT:write_multi_char | 22 |  | write_char |  | CRT 1 |
| 1572 | 0x004A38F6 | CRT:write_string | 27 |  | write_char |  | CRT 1 |
| 1573 | 0x004A392E | CRT:get_int_arg | 5 |  |  |  | CRT 1 |
| 1574 | 0x004A393B | CRT:get_int64_arg | 6 |  |  |  | CRT 1 |
| 1575 | 0x004A394B | CRT:get_short_arg | 5 |  |  |  | CRT 1 |
| 1576 | 0x004A3970 | CRT:strchr | 89 |  |  |  | CRT 1 |
| 1577 | 0x004A3A2C | CRT:_callnewh | 13 | eax |  |  | CRT 3 |
| 1578 | 0x004A3A47 | CRT:_heap_init | 20 | HeapCreate,HeapDestroy | __sbh_heap_init |  | CRT 1 |
| 1579 | 0x004A3A83 | CRT:__sbh_heap_init | 15 | HeapAlloc |  |  | CRT 1 |
| 1580 | 0x004A3AC1 | CRT:__sbh_find_block | 14 |  |  |  | CRT 3 |
| 1581 | 0x004A3AEC | CRT:__sbh_free_block | 264 | edi,HeapFree | memcpy |  | CRT 2 |
| 1582 | 0x004A3E17 | CRT:__sbh_alloc_block | 275 |  | __sbh_alloc_new_region,__sbh_alloc_new_group |  | CRT 3 |
| 1583 | 0x004A4120 | CRT:__sbh_alloc_new_region | 53 | HeapReAlloc,HeapAlloc,VirtualAlloc,HeapFree |  |  | CRT 1 |
| 1584 | 0x004A41D1 | CRT:__sbh_alloc_new_group | 85 | VirtualAlloc |  |  | CRT 1 |
| 1585 | 0x004A42CC | CRT:__sbh_resize_block | 259 |  |  |  | CRT 1 |
| 1586 | 0x004A45C2 | CRT:_math_entry | 32 |  | _87except,_ctrlfp |  | CRT 1 |
| 1587 | 0x004A4615 | CRT:_handle_exc | 60 |  | _umatherr,Floor,_get_fpe_type,_87except,_errcode,_ctrlfp |  | CRT 1 |
| 1588 | 0x004A46AD | src/crt/msvc.cpp:Floor | 258 | RaiseException | _statfp,_clrfp |  | CRT 1 |
| 1589 | 0x004A4960 | CRT:_umatherr | 189 |  | Floor,_decomp |  | CRT 1 |
| 1590 | 0x004A4B77 | CRT:_87except | 49 |  | _trandisp_lookup,_ctrlfp,_matherr,_errcode |  | CRT 2 |
| 1591 | 0x004A4BFF | CRT:_errcode | 10 |  |  |  | CRT 2 |
| 1592 | 0x004A4C25 | CRT:_trandisp_lookup | 13 |  |  | exp | CRT 1 |
| 1593 | 0x004A4C4A | CRT:_get_fpe_type | 22 |  |  |  | CRT 1 |
| 1594 | 0x004A4C77 | CRT:_frnd | 10 |  |  |  | CRT 1 |
| 1595 | 0x004A4C89 | CRT:_set_exp | 16 |  |  |  | CRT 1 |
| 1596 | 0x004A4CB2 | CRT:_sptype | 35 |  |  |  | CRT 1 |
| 1597 | 0x004A4D0C | CRT:_decomp | 72 |  | _set_exp |  | CRT 1 |
| 1598 | 0x004A4DCD | CRT:_statfp | 7 |  |  |  | CRT 1 |
| 1599 | 0x004A4DDB | CRT:_clrfp | 8 |  |  |  | CRT 1 |
| 1600 | 0x004A4DEA | CRT:_ctrlfp | 15 |  |  |  | CRT 4 |
| 1601 | 0x004A4E0D | src/crt/msvc.cpp:Floor | 36 |  |  |  | CRT 1 |
| 1602 | 0x004A4E63 | CRT:__initstdio | 59 |  | calloc,_amsg_exit |  | CRT 0 |
| 1603 | 0x004A4F08 | CRT:__endstdio | 5 |  | _flushall,_fcloseall |  | CRT 0 |
| 1604 | 0x004A4F20 | CRT:memmove | 270 | jpt_4A4F55,dword,jpt_4A4F7C,jpt_4A5104,jpt_4A50E2 |  |  | CRT 2 |
| 1605 | 0x004A5260 | CRT:_except_handler3 | 73 | dword | __global_unwind2,__local_unwind2,_NLG_Notify |  | CRT 13 |
| 1606 | 0x004A531D | CRT:_seh_longjmp_unwind | 11 |  | __local_unwind2 |  | CRT 0 |
| 1607 | 0x004A5338 | CRT:_isctype | 46 |  | __crtGetStringTypeA |  | CRT 4 |
| 1608 | 0x004A53B0 | CRT:_allmul | 19 |  |  |  | CRT 0 |
| 1609 | 0x004A53E4 | CRT:_XcptFilter | 89 | ebx,UnhandledExceptionFilter | xcptlookup |  | CRT 1 |
| 1610 | 0x004A5525 | CRT:xcptlookup | 22 |  |  |  | CRT 1 |
| 1611 | 0x004A5568 | CRT:_wincmdln | 39 |  | __initmbctable,_ismbblead |  | CRT 1 |
| 1612 | 0x004A55C0 | CRT:_setenvp | 71 |  | __initmbctable,strlen,malloc,_amsg_exit,strcpy,_free_base |  | CRT 1 |
| 1613 | 0x004A5679 | CRT:_setargv | 62 | GetModuleFileNameA | __initmbctable,parse_cmdline,malloc,_amsg_exit |  | CRT 1 |
| 1614 | 0x004A5712 | CRT:parse_cmdline | 187 |  |  |  | CRT 1 |
| 1615 | 0x004A58C6 | CRT:__crtGetEnvironmentStringsA | 132 | ebp,GetEnvironmentStrings,edi,FreeEnvironmentStringsW,FreeEnvironmentStringsA | malloc,_free_base,memmove |  | CRT 1 |
| 1616 | 0x004A59F8 | CRT:_ioinit | 143 | GetStartupInfoA,GetFileType,GetStdHandle,SetHandleCount | malloc,_amsg_exit |  | CRT 1 |
| 1617 | 0x004A5BA3 | CRT:_FF_MSGBANNER | 18 | eax | _NMSG_WRITE |  | CRT 2 |
| 1618 | 0x004A5BDC | CRT:_NMSG_WRITE | 100 | GetModuleFileNameA,GetStdHandle,WriteFile | strcpy,strlen,strncpy,strcat,__crtMessageBoxA | <program name unknown>／...／Runtime Error!\n\nProgram: ／\n\n／R6002\r\n- floating  | CRT 5 |
| 1619 | 0x004A5D2F | CRT:_control87 | 24 |  | _abstract_cw,_hw_cw |  | CRT 1 |
| 1620 | 0x004A5D64 | CRT:_controlfp | 8 |  | _control87 |  | CRT 1 |
| 1621 | 0x004A5D7A | CRT:_abstract_cw | 58 |  |  |  | CRT 1 |
| 1622 | 0x004A5E0C | CRT:_hw_cw | 53 |  |  |  | CRT 1 |
| 1623 | 0x004A5E95 | CRT:tolower | 78 |  | _isctype,__crtLCMapStringA |  | CRT 1 |
| 1624 | 0x004A5F60 | CRT:_ZeroTail | 36 |  |  |  | CRT 1 |
| 1625 | 0x004A5FA9 | CRT:_IncMan | 43 |  | __add_12 |  | CRT 1 |
| 1626 | 0x004A5FFF | CRT:_RoundMan | 71 |  | _ZeroTail,_IncMan |  | CRT 1 |
| 1627 | 0x004A608B | CRT:_CopyMan | 13 |  |  |  | CRT 1 |
| 1628 | 0x004A60A6 | CRT:_FillZeroMan | 8 |  |  |  | CRT 1 |
| 1629 | 0x004A60B2 | CRT:_IsZeroMan | 13 |  |  |  | CRT 1 |
| 1630 | 0x004A60CD | CRT:_ShrMan | 64 |  |  |  | CRT 1 |
| 1631 | 0x004A615A | CRT:_ld12cvt | 138 |  | _IsZeroMan,_FillZeroMan,_CopyMan,_RoundMan,_ShrMan |  | CRT 2 |
| 1632 | 0x004A62C6 | CRT:_ld12tod | 6 |  | _ld12cvt |  | CRT 1 |
| 1633 | 0x004A62DC | CRT:_ld12tof | 6 |  | _ld12cvt |  | CRT 1 |
| 1634 | 0x004A62F2 | CRT:_atodbl | 21 |  | __strgtold12,_ld12tod |  | CRT 1 |
| 1635 | 0x004A631F | CRT:_atoflt | 21 |  | __strgtold12,_ld12tof |  | CRT 1 |
| 1636 | 0x004A6350 | CRT:strcpy | 3 |  |  |  | CRT 7 |
| 1637 | 0x004A6360 | CRT:strcat | 84 |  |  |  | CRT 1 |
| 1638 | 0x004A6440 | CRT:_fptostr | 57 |  | strlen,memcpy |  | CRT 3 |
| 1639 | 0x004A64B7 | CRT:_fltout2 | 34 |  | __dtold,$I10_OUTPUT |  | CRT 3 |
| 1640 | 0x004A651B | CRT:__dtold | 70 |  |  |  | CRT 1 |
| 1641 | 0x004A65E0 | CRT:memset | 40 |  |  |  | CRT 3 |
| 1642 | 0x004A6640 | CRT:strlen | 44 |  |  |  | CRT 9 |
| 1643 | 0x004A66BB | CRT:_fptrap | 4 |  | _amsg_exit |  | CRT 1 |
| 1644 | 0x004A66C4 | CRT:__CxxUnhandledExceptionFilter | 24 | lpTopLevelExceptionFilter | terminate,_ValidateExecute |  | CRT 1 |
| 1645 | 0x004A670A | CRT:__CxxSetUnhandledExceptionFilter | 4 | SetUnhandledExceptionFilter |  |  | CRT 0 |
| 1646 | 0x004A671B | CRT:__CxxRestoreUnhandledExceptionFilter | 3 | SetUnhandledExceptionFilter |  |  | CRT 0 |
| 1647 | 0x004A6728 | CRT:_ValidateRead | 12 | IsBadReadPtr |  |  | CRT 2 |
| 1648 | 0x004A6744 | CRT:_ValidateWrite | 12 | IsBadWritePtr |  |  | CRT 1 |
| 1649 | 0x004A6760 | CRT:_ValidateExecute | 11 | IsBadCodePtr |  |  | CRT 2 |
| 1650 | 0x004A6780 | CRT:strcmp | 56 |  |  |  | CRT 2 |
| 1651 | 0x004A681B | CRT:__tzset | 5 |  | _tzset |  | CRT 1 |
| 1652 | 0x004A6830 | CRT:_tzset | 196 | GetTimeZoneInformation,esi | getenv,strcmp,_free_base,strlen,malloc,strcpy,strncpy,atol | TZ | CRT 1 |
| 1653 | 0x004A6A8E | CRT:_isindst | 149 |  | cvtdate |  | CRT 1 |
| 1654 | 0x004A6C3A | CRT:cvtdate | 101 |  |  |  | CRT 1 |
| 1655 | 0x004A6D7A | CRT:_lseek | 51 | SetFilePointer,GetLastError | _get_osfhandle,_dosmaperr |  | CRT 2 |
| 1656 | 0x004A6E14 | CRT:_write | 139 | WriteFile,GetLastError | _lseek,_dosmaperr |  | CRT 2 |
| 1657 | 0x004A6FC1 | CRT:_getbuf | 19 |  | malloc |  | CRT 1 |
| 1658 | 0x004A7005 | CRT:_isatty | 12 |  |  |  | CRT 1 |
| 1659 | 0x004A702B | CRT:wctomb | 37 | WideCharToMultiByte |  |  | CRT 1 |
| 1660 | 0x004A70A0 | CRT:_aulldiv | 43 |  |  |  | CRT 1 |
| 1661 | 0x004A7110 | CRT:_aullrem | 44 |  |  |  | CRT 1 |
| 1662 | 0x004A7185 | CRT:_matherr | 2 |  |  |  | CRT 1 |
| 1663 | 0x004A7188 | CRT:fflush | 29 |  | flsall,_flush,_commit |  | CRT 1 |
| 1664 | 0x004A71C3 | CRT:_flush | 40 |  | _write |  | CRT 2 |
| 1665 | 0x004A721F | CRT:_flushall | 4 |  | flsall |  | CRT 1 |
| 1666 | 0x004A7228 | CRT:flsall | 45 |  | fflush |  | CRT 2 |
| 1667 | 0x004A7295 | CRT:calloc | 52 | HeapAlloc | __sbh_alloc_block,_callnewh,memset |  | CRT 1 |
| 1668 | 0x004A7312 | CRT:_fcloseall | 34 |  | fclose,_free_base |  | CRT 1 |
| 1669 | 0x004A736A | CRT:__crtGetStringTypeA | 123 | GetStringTypeW,GetStringTypeA,MultiByteToWideChar,_alloca_probe | memset |  | CRT 2 |
| 1670 | 0x004A74B3 | CRT:_ismbblead | 6 |  | x_ismbbtype |  | CRT 1 |
| 1671 | 0x004A74C4 | CRT:x_ismbbtype | 16 |  |  |  | CRT 1 |
| 1672 | 0x004A74F5 | CRT:_setmbcp | 135 | GetCPInfo | getSystemCP,CPtoLCID,setSBCS,setSBUpLow |  | CRT 1 |
| 1673 | 0x004A768E | CRT:getSystemCP | 15 | GetOEMCP,GetACP |  |  | CRT 1 |
| 1674 | 0x004A76D8 | CRT:CPtoLCID | 19 |  |  |  | CRT 1 |
| 1675 | 0x004A770B | CRT:setSBCS | 17 |  |  |  | CRT 1 |
| 1676 | 0x004A7734 | CRT:setSBUpLow | 124 | GetCPInfo | __crtGetStringTypeA,__crtLCMapStringA |  | CRT 1 |
| 1677 | 0x004A78B9 | CRT:__initmbctable | 7 |  | _setmbcp |  | CRT 3 |
| 1678 | 0x004A78D5 | CRT:__crtMessageBoxA | 50 | LoadLibraryA,esi,eax,dword_579D04 |  | user32.dll／MessageBoxA／GetActiveWindow／GetLastActivePopup | CRT 1 |
| 1679 | 0x004A7960 | CRT:strncpy | 109 |  |  |  | CRT 2 |
| 1680 | 0x004A7A5E | CRT:__crtLCMapStringA | 193 | LCMapStringW,LCMapStringA,MultiByteToWideChar,_alloca_probe,WideCharToMultiByte | __addl |  | CRT 2 |
| 1681 | 0x004A7C82 | CRT:__addl | 20 |  |  |  | CRT 2 |
| 1682 | 0x004A7CAD | CRT:__add_12 | 15 |  |  |  | CRT 3 |
| 1683 | 0x004A7CCE | CRT:__shl_12 | 38 |  | __add_12 |  | CRT 2 |
| 1684 | 0x004A7D2C | CRT:__shr_12 | 21 |  |  |  | CRT 3 |
| 1685 | 0x004A7D5A | CRT:__mtold12 | 21 |  |  |  | CRT 2 |
| 1686 | 0x004A7D87 | CRT:__ld12mul_part | 76 |  | __shr_12,__shl_12 |  | CRT 1 |
| 1687 | 0x004A7E4E | CRT:__strgtold12 | 417 | jpt_4A7EB1 | _isctype,__ld12mul_part,__multtenpow12 |  | CRT 2 |
| 1688 | 0x004A831F | CRT:$I10_OUTPUT | 232 |  | strcpy,__multtenpow12,__ld12mul,__shr_12,__mtold12,__shl_12 | 1#SNAN／1#IND／1#INF／1#QNAN | CRT 1 |
| 1689 | 0x004A85B2 | CRT:raise | 109 | esi | siglookup,_exit |  | CRT 2 |
| 1690 | 0x004A86DF | CRT:siglookup | 22 |  |  |  | CRT 1 |
| 1691 | 0x004A8724 | CRT:getenv | 51 |  | __wtomb_environ,strlen,_strnicmp |  | CRT 1 |
| 1692 | 0x004A87A1 | CRT:_dosmaperr | 26 |  |  |  | CRT 3 |
| 1693 | 0x004A8808 | CRT:_free_osfhnd | 45 | SetStdHandle |  |  | CRT 1 |
| 1694 | 0x004A8882 | CRT:_get_osfhandle | 16 |  |  |  | CRT 3 |
| 1695 | 0x004A88BF | CRT:_commit | 26 | FlushFileBuffers,GetLastError | _get_osfhandle |  | CRT 1 |
| 1696 | 0x004A8916 | CRT:fclose | 35 |  | _flush,_freebuf,_close,_free_base |  | CRT 1 |
| 1697 | 0x004A8970 | CRT:_alloca_probe | 17 |  |  |  | CRT 3 |
| 1698 | 0x004A899F | CRT:__ld12mul | 177 |  | __add_12,__shr_12,__mtold12 |  | CRT 2 |
| 1699 | 0x004A8BBF | CRT:__multtenpow12 | 52 |  | __ld12mul |  | CRT 2 |
| 1700 | 0x004A8C3B | CRT:_strnicmp | 24 |  | __crtCompareStringA |  | CRT 2 |
| 1701 | 0x004A8C7A | CRT:__wtomb_environ | 58 | ebx | malloc,__crtsetenv |  | CRT 2 |
| 1702 | 0x004A8CE8 | CRT:_close | 66 | CloseHandle,GetLastError | _get_osfhandle,_free_osfhnd,_dosmaperr |  | CRT 1 |
| 1703 | 0x004A8D9B | CRT:_freebuf | 17 |  | _free_base |  | CRT 1 |
| 1704 | 0x004A8DC6 | CRT:__crtCompareStringA | 239 | CompareStringW,CompareStringA,GetCPInfo,MultiByteToWideChar,_alloca_probe,esi | __addl |  | CRT 1 |
| 1705 | 0x004A9043 | CRT:__crtsetenv | 155 | SetEnvironmentVariableA | _mbschr,copy_environ,__wtomb_environ,malloc,findenv,_free_base,_realloc_base,strlen,strcpy |  | CRT 1 |
| 1706 | 0x004A91CA | CRT:findenv | 35 |  | _strnicmp |  | CRT 1 |
| 1707 | 0x004A9222 | CRT:copy_environ | 49 |  | malloc,_amsg_exit,_strdup |  | CRT 1 |
| 1708 | 0x004A9289 | CRT:_mbschr | 49 |  | strchr |  | CRT 1 |
| 1709 | 0x004A92FC | CRT:_strdup | 23 |  | strlen,malloc,strcpy |  | CRT 1 |
| 1710 | 0x004A9328 | import:RtlUnwind | 1 | RtlUnwind |  |  | CRT 2 |
