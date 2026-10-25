# 第一遍：平台导入与静态 CRT（第 1480–1710 行）

## 验证方法与结论 — exact（身份核对；平台替换与运行库映射）

- 依据 AGENTS.md“CRT 与命名”：已识别的静态链接 MSVC CRT 函数不重写，游戏的调用处使用当前 C/C++ 运行库，至多在 src/crt/ 包装一层。本遍对这 231 行逐一核对：①汇编确为函数表所记的 MSVC 6 静态 CRT 例程（大小、导入、字符串、被调与调用者、首部指令，见下表，由 IDAT 导出的 structure.json 与 asm/ 清单生成，脚本 reverse/analysis/scripts/crt_verify_features.py）；②映射的运行库调用在游戏用到的范围内结果一致；③平台导入的替换已登记偏差。未发现名字与特征不符者。
- 游戏依赖结果的例程：
  - 0x004A14E2 srand：种子写 0x004C04E4；0x004A14EC rand：种子 × 0x343FD + 0x269EC3，`>> 16 & 0x7FFF`。源码 Random::Seed／Next 用当前运行库的 srand／rand 并取 15 位（用户 2026-10-18 要求使用当前运行库；Windows UCRT 的 rand 为同一 LCG，Linux 只用于验证）。src/crt/msvc.h 中 rand 的地址注释原写 0x00489B20，改为 0x004A14EC。
  - 0x004A10E8 _ftol：fstcw 后置截断舍入（or ah, 0Ch）fistp qword，返回 edx:eax。源码 Ftol = llrint(trunc(x))，NaN 与越界同为 0x8000000000000000。
  - 0x004A150A difftime：(end − start) 整数差 fild 返回；0x004A151B time：GetLocalTime 经 __loctotime_t（含夏令时）转 time_t；0x004A1B81 floor 及其错误处理（_math_entry、_handle_exc、_raise_exc 0x004A46AD、_set_statfp 0x004A4E0D 等内联）。源码 std::difftime、std::time、std::floor。
  - 0x004A19E5 vsprintf → _output（0x004A314F）、%f 经 _cftof（0x004A2461）／_fltout2／$I10_OUTPUT 17 位：源码 Sprintf（std::snprintf），舍入差异见偏差第 262 行（地址改为 swd3e 并引用汇编）；msvc.h 中 vsprintf 的注释原写正传 0x00489D90，改为 0x004A19E5。
  - 0x004A1A50 strstr、0x004A2073 atoi／0x004A1FE8 atol、0x004A16B0 memcpy（实为允许重叠的 memmove 结构）：std::string_view::find、std::atoi、std::memmove／std::copy，语义相同（空子串时 strstr 返回原串、find 返回 0，结果一致）。
  - 0x004A21E6 _setdefaultprecision：_controlfp(0x10000, 0x30000) 即 53 位精度；重建 x64 用 SSE2 double，同为 53 位。
- 平台导入 thunk：0x004A0030 DirectDrawCreate（调用处 0x0043C0D0，偏差第 323 行）、0x004A1058 DirectInputCreateA（0x0043BA70，第 41 行）、0x004A105E ImmIsIME（0x0043D620，第 113 行）、0x004A9328 RtlUnwind（CRT 内部）。0x004A1070／0x004A1080／0x004A1090：atexit(AIL_shutdown)、跳转与导入 thunk，归入新增的 Miles 启动与关闭偏差行。
- 其余为启动、退出、堆（small-block heap）、C++／SEH 异常、printf 浮点、多字节与区域设置、低层 I/O、64 位整数运算等运行库内部例程，映射到编译器与当前运行库，游戏代码不直接依赖其内部结果。

## 逐行身份表

| 行 | 地址 | 函数表名称与映射 | 特征（大小、导入、字符串、被调） |
| --- | --- | --- | --- |
| 1480 | 0x004A0030 | import:DirectDrawCreate -> platform: SDL3 renderer (Display, Graphics) | size=6 imp=DirectDrawCreate str= calls= |
| 1481 | 0x004A1058 | import:DirectInputCreateA -> platform: SDL3 events (WindowInputDevice, Input) | size=6 imp=DirectInputCreateA str= calls= |
| 1482 | 0x004A105E | import:ImmIsIME -> platform: SDL3 text input | size=6 imp=ImmIsIME str= calls= |
| 1483 | 0x004A1070 | src/platform/audio_device.cpp:AudioDevice::~AudioDevice | size=16 imp= str= calls=src/platform/audio_device.cpp:AudioDevice::~AudioDevice,atexit |
| 1484 | 0x004A1080 | src/platform/audio_device.cpp:AudioDevice::~AudioDevice | size=5 imp= str= calls=src/platform/audio_device.cpp:AudioDevice::~AudioDevice |
| 1485 | 0x004A1090 | src/platform/audio_device.cpp:AudioDevice::~AudioDevice | size=6 imp=_AIL_shutdown@0 str= calls= |
| 1486 | 0x004A1096 | CRT:_fpmath -> C++ runtime floating point initialisation | size=23 imp= str= calls=_cfltcvt_init,_setdefaultprecision,_ms_p5_mp_test_fdiv |
| 1487 | 0x004A10AD | CRT:_fptrap stub -> C++ runtime floating point initialisation | size=1 imp= str= calls= |
| 1488 | 0x004A10AE | CRT:_cfltcvt_init -> C++ runtime printf float support | size=56 imp= str= calls=_forcdecpt,_cropzeros,_positive,_fassign,_cfltcvt |
| 1489 | 0x004A10E8 | src/crt/msvc.h:Ftol | size=39 imp= str= calls= |
| 1490 | 0x004A110F | CRT:_JumpToContinuation -> compiler C++ exception support | size=52 imp= str= calls= |
| 1491 | 0x004A1143 | CRT:_CallMemberFunction0 -> compiler C++ exception support | size=7 imp= str= calls= |
| 1492 | 0x004A114A | CRT:_CallMemberFunction1 -> compiler C++ exception support | size=7 imp= str= calls= |
| 1493 | 0x004A1151 | CRT:_UnwindNestedFrames -> compiler C++ exception support | size=79 imp= str= calls=import:RtlUnwind |
| 1494 | 0x004A11A0 | CRT:__CxxFrameHandler -> compiler C++ exception support | size=54 imp= str= calls=__InternalCxxFrameHandler |
| 1495 | 0x004A11D6 | CRT:_CallCatchBlock2 -> compiler C++ exception support | size=84 imp= str= calls=CatchGuardHandler,_CallSettingFrame |
| 1496 | 0x004A122A | CRT:CatchGuardHandler -> compiler C++ exception support | size=37 imp= str= calls=__InternalCxxFrameHandler |
| 1497 | 0x004A124F | CRT:_CallSETranslator -> compiler C++ exception support | size=180 imp= str= calls=TranslatorGuardHandler |
| 1498 | 0x004A1303 | CRT:TranslatorGuardHandler -> compiler C++ exception support | size=117 imp= str= calls=_UnwindNestedFrames,__InternalCxxFrameHandler |
| 1499 | 0x004A1378 | CRT:_GetRangeOfTrysToCheck -> compiler C++ exception support | size=123 imp= str= calls=_inconsistency |
| 1500 | 0x004A13F4 | CRT:__global_unwind2 -> compiler structured exception support | size=32 imp= str= calls=import:RtlUnwind |
| 1501 | 0x004A1414 | CRT:__unwind_handler -> compiler structured exception support | size=34 imp= str= calls= |
| 1502 | 0x004A1436 | CRT:__local_unwind2 -> compiler structured exception support | size=104 imp= str= calls=__unwind_handler,_NLG_Notify |
| 1503 | 0x004A149E | CRT:__abnormal_termination -> compiler structured exception support | size=35 imp= str= calls=__unwind_handler |
| 1504 | 0x004A14C1 | CRT:_NLG_Notify1 -> compiler structured exception support | size=9 imp= str= calls= |
| 1505 | 0x004A14CA | CRT:_NLG_Notify -> compiler structured exception support | size=24 imp= str= calls= |
| 1506 | 0x004A14E2 | src/crt/msvc.cpp:Random::Seed | size=10 imp= str= calls= |
| 1507 | 0x004A14EC | src/crt/msvc.cpp:Random::Next | size=30 imp= str= calls= |
| 1508 | 0x004A150A | src/crt/msvc.h:Difftime | size=17 imp= str= calls= |
| 1509 | 0x004A151B | src/crt/msvc.h:Time | size=220 imp=GetLocalTime,GetSystemTime,GetTimeZoneInformation str= calls=__loctotime_t |
| 1510 | 0x004A15F7 | CRT:_onexit -> C++ runtime: static destruction | size=109 imp= str= calls=_realloc_base,_msize |
| 1511 | 0x004A1664 | CRT:atexit -> C++ runtime: member destructors of Game (the __xc thunks) | size=18 imp= str= calls=_onexit |
| 1512 | 0x004A1676 | CRT:__onexitinit -> C++ runtime startup | size=47 imp= str= calls=malloc,_amsg_exit |
| 1513 | 0x004A16B0 | CRT:memcpy -> std::copy / std::memmove (overlap allowed as here) | size=821 imp= str= calls= |
| 1514 | 0x004A19E5 | CRT:vsprintf -> src/crt/msvc.h:Sprintf (std::snprintf) | size=81 imp= str= calls=_flsbuf,_output |
| 1515 | 0x004A1A36 | CRT:operator delete -> C++ delete / containers | size=11 imp= str= calls=_free_base |
| 1516 | 0x004A1A50 | CRT:strstr -> std::string_view::find | size=128 imp= str= calls= |
| 1517 | 0x004A1AD0 | CRT:operator new -> C++ new / containers | size=14 imp= str= calls=_nh_malloc |
| 1518 | 0x004A1ADE | CRT:malloc -> std heap (containers) | size=18 imp= str= calls=_nh_malloc |
| 1519 | 0x004A1AF0 | CRT:_nh_malloc -> std heap | size=44 imp= str= calls=_heap_alloc,_callnewh |
| 1520 | 0x004A1B1C | CRT:_heap_alloc -> std heap | size=54 imp=HeapAlloc str= calls=__sbh_alloc_block |
| 1521 | 0x004A1B52 | CRT:_free_base -> std heap | size=47 imp=HeapFree str= calls=__sbh_find_block,__sbh_free_block |
| 1522 | 0x004A1B81 | src/crt/msvc.h:Floor | size=207 imp= str= calls=_math_entry,_handle_exc,_frnd,_sptype,_ctrlfp |
| 1523 | 0x004A1C50 | CRT:_cinit -> C++ runtime startup | size=45 imp= str= calls=_fpmath,_initterm |
| 1524 | 0x004A1C7D | CRT:exit -> return from main | size=17 imp= str= calls=doexit |
| 1525 | 0x004A1C8E | CRT:_exit -> C++ runtime exit | size=17 imp= str= calls=doexit |
| 1526 | 0x004A1C9F | CRT:doexit -> C++ runtime exit | size=153 imp=ExitProcess,GetCurrentProcess,TerminateProcess str= calls=_initterm |
| 1527 | 0x004A1D38 | CRT:_initterm -> C++ runtime startup (static initialisation) | size=26 imp= str= calls= |
| 1528 | 0x004A1D52 | CRT:_realloc_base -> std heap (std::vector growth) | size=288 imp=HeapAlloc,HeapReAlloc str= calls=malloc,_free_base,_callnewh,__sbh_find_block,__sbh_free_block,__sbh_alloc_block, |
| 1529 | 0x004A1E72 | CRT:eh vector constructor iterator -> std::array construction (Game::battleRoles, battleEnemies) | size=130 imp= str= calls=__ArrayUnwind,_except_handler3 |
| 1530 | 0x004A1EF4 | CRT:eh vector destructor iterator -> std::array destruction | size=128 imp= str= calls=__ArrayUnwind,_except_handler3 |
| 1531 | 0x004A1F74 | CRT:__ArrayUnwind -> compiler C++ exception support | size=94 imp= str= calls=ExFilterRethrow,_except_handler3 |
| 1532 | 0x004A1FD2 | CRT:ExFilterRethrow -> C++ exception handling | size=22 imp= str= calls=terminate |
| 1533 | 0x004A1FE8 | CRT:atol -> std::from_chars (not used by the game) | size=139 imp= str= calls=_isctype |
| 1534 | 0x004A2073 | CRT:atoi -> std::atoi (or the caller's parse) | size=11 imp= str= calls=atol |
| 1535 | 0x004A207E | CRT:_msize -> std heap | size=41 imp=HeapSize str= calls=__sbh_find_block |
| 1536 | 0x004A20A7 | CRT:WinMainCRTStartup -> SDL3 main (src/app/main.cpp) | size=246 imp=GetCommandLineA,GetModuleHandleA,GetStartupInfoA,GetVersion str= calls=src/game/game.cpp:Game::Run,_cinit,exit,_exit,_fast_error_exit,_heap_init,_excep |
| 1537 | 0x004A219D | CRT:_amsg_exit -> C++ runtime error exit | size=37 imp= str= calls=_exit,_FF_MSGBANNER,_NMSG_WRITE |
| 1538 | 0x004A21C2 | CRT:_fast_error_exit -> C++ runtime error exit | size=36 imp=ExitProcess str= calls=_FF_MSGBANNER,_NMSG_WRITE |
| 1539 | 0x004A21E6 | CRT:_setdefaultprecision -> C++ runtime (53-bit precision; SSE2 rounding, see CMakeLists.txt) | size=18 imp= str= calls=_controlfp |
| 1540 | 0x004A21F8 | CRT:_ms_p5_test_fdiv -> C++ runtime floating point setup | size=62 imp= str= calls= |
| 1541 | 0x004A2236 | CRT:_ms_p5_mp_test_fdiv -> C++ runtime floating point setup | size=41 imp=GetModuleHandleA,GetProcAddress str=KERNEL32／IsProcessorFeaturePresent calls=_ms_p5_test_fdiv |
| 1542 | 0x004A225F | CRT:_forcdecpt -> std formatting (internal) | size=90 imp= str= calls=_isctype,tolower |
| 1543 | 0x004A22B9 | CRT:_cropzeros -> std formatting (internal) | size=78 imp= str= calls= |
| 1544 | 0x004A2307 | CRT:_positive -> std formatting (internal) | size=24 imp= str= calls= |
| 1545 | 0x004A231F | CRT:_fassign -> std formatting (internal) | size=62 imp= str= calls=_atodbl,_atoflt |
| 1546 | 0x004A235D | CRT:_cftoe -> C++ runtime printf float support | size=260 imp= str=e+000 calls=_shift,strcpy,_fptostr,_fltout2 |
| 1547 | 0x004A2461 | CRT:_cftof -> std formatting (internal) | size=222 imp= str= calls=_shift,_fptostr,_fltout2,memset |
| 1548 | 0x004A253F | CRT:_cftog -> std formatting (internal) | size=155 imp= str= calls=_cftoe_g,_cftof_g,_fptostr,_fltout2 |
| 1549 | 0x004A25DA | CRT:_cftoe_g -> std formatting (internal) | size=39 imp= str= calls=_cftoe |
| 1550 | 0x004A2601 | CRT:_cftof_g -> std formatting (internal) | size=35 imp= str= calls=_cftof |
| 1551 | 0x004A2624 | CRT:_cfltcvt -> C++ runtime printf float support | size=81 imp= str= calls=_cftoe,_cftof,_cftog |
| 1552 | 0x004A2675 | CRT:_shift -> C++ runtime printf float support | size=37 imp= str= calls=memcpy,strlen |
| 1553 | 0x004A269A | CRT:__InternalCxxFrameHandler -> compiler C++ exception support | size=155 imp= str= calls=FindHandler,__FrameUnwindToState,_inconsistency |
| 1554 | 0x004A2735 | CRT:FindHandler -> compiler C++ exception support | size=423 imp= str= calls=_GetRangeOfTrysToCheck,FindHandlerForForeignException,TypeMatch,CatchIt,Destruct |
| 1555 | 0x004A28DC | CRT:FindHandlerForForeignException -> compiler C++ exception support | size=168 imp= str= calls=_CallSETranslator,_GetRangeOfTrysToCheck,CatchIt |
| 1556 | 0x004A2984 | CRT:TypeMatch -> compiler C++ exception support | size=93 imp= str= calls=strcmp |
| 1557 | 0x004A29E1 | CRT:__FrameUnwindToState -> C++ exception handling | size=158 imp= str= calls=ExFilterRethrow,_CallSettingFrame,_inconsistency,_except_handler3 |
| 1558 | 0x004A2A7F | CRT:CatchIt -> C++ exception handling | size=123 imp= str= calls=_JumpToContinuation,_UnwindNestedFrames,__FrameUnwindToState,CallCatchBlock,Buil |
| 1559 | 0x004A2AFA | CRT:CallCatchBlock -> C++ exception handling | size=268 imp= str= calls=_CallCatchBlock2,__local_unwind2,__abnormal_termination,ExFilterRethrow (catch b |
| 1560 | 0x004A2C06 | CRT:ExFilterRethrow (catch block) -> C++ exception handling | size=42 imp= str= calls= |
| 1561 | 0x004A2C30 | CRT:BuildCatchObject -> C++ exception handling | size=452 imp= str= calls=_CallMemberFunction0,_CallMemberFunction1,memcpy,AdjustPointer,terminate,_incons |
| 1562 | 0x004A2DF4 | CRT:DestructExceptionObject -> compiler C++ exception support | size=103 imp= str= calls=_CallMemberFunction0,terminate,_except_handler3 |
| 1563 | 0x004A2E5B | CRT:AdjustPointer -> compiler C++ exception support | size=35 imp= str= calls= |
| 1564 | 0x004A2E80 | CRT:_CallSettingFrame -> compiler C++ exception support | size=76 imp= str= calls=_NLG_Notify1 |
| 1565 | 0x004A2ECC | CRT:terminate -> std::terminate | size=109 imp= str= calls=_exit,_except_handler3,_NMSG_WRITE,raise |
| 1566 | 0x004A2F22 | CRT:_inconsistency -> C++ exception handling | size=109 imp= str= calls=_exit,terminate,_except_handler3,_NMSG_WRITE,raise |
| 1567 | 0x004A2F78 | CRT:__loctotime_t -> crt::Time (src/crt/msvc.cpp) | size=194 imp= str= calls=__tzset,_isindst |
| 1568 | 0x004A303A | CRT:_flsbuf -> std stream output | size=277 imp= str= calls=_lseek,_write,_getbuf,_isatty |
| 1569 | 0x004A314F | CRT:_output -> std formatting (printf family) | size=1825 imp= str= calls=write_char,write_multi_char,write_string,get_int_arg,get_int64_arg,get_short_arg |
| 1570 | 0x004A3890 | CRT:write_char -> std formatting (internal) | size=53 imp= str= calls=_flsbuf |
| 1571 | 0x004A38C5 | CRT:write_multi_char -> std formatting (internal) | size=49 imp= str= calls=write_char |
| 1572 | 0x004A38F6 | CRT:write_string -> std formatting (internal) | size=56 imp= str= calls=write_char |
| 1573 | 0x004A392E | CRT:get_int_arg -> std formatting (internal) | size=13 imp= str= calls= |
| 1574 | 0x004A393B | CRT:get_int64_arg -> std formatting (internal) | size=16 imp= str= calls= |
| 1575 | 0x004A394B | CRT:get_short_arg -> std formatting (internal) | size=14 imp= str= calls= |
| 1576 | 0x004A3970 | CRT:strchr -> std::string_view::find | size=193 imp= str= calls= |
| 1577 | 0x004A3A2C | CRT:_callnewh -> std heap | size=27 imp= str= calls= |
| 1578 | 0x004A3A47 | CRT:_heap_init -> C++ runtime startup (heap) | size=60 imp=HeapCreate,HeapDestroy str= calls=__sbh_heap_init |
| 1579 | 0x004A3A83 | CRT:__sbh_heap_init -> std heap | size=62 imp=HeapAlloc str= calls= |
| 1580 | 0x004A3AC1 | CRT:__sbh_find_block -> std heap | size=43 imp= str= calls= |
| 1581 | 0x004A3AEC | CRT:__sbh_free_block -> std heap | size=811 imp=HeapFree,VirtualFree str= calls=memcpy |
| 1582 | 0x004A3E17 | CRT:__sbh_alloc_block -> std heap | size=777 imp= str= calls=__sbh_alloc_new_region,__sbh_alloc_new_group |
| 1583 | 0x004A4120 | CRT:__sbh_alloc_new_region -> std heap | size=177 imp=HeapAlloc,HeapFree,HeapReAlloc,VirtualAlloc str= calls= |
| 1584 | 0x004A41D1 | CRT:__sbh_alloc_new_group -> std heap | size=251 imp=VirtualAlloc str= calls= |
| 1585 | 0x004A42CC | CRT:__sbh_resize_block -> std heap | size=758 imp= str= calls= |
| 1586 | 0x004A45C2 | CRT:_math_entry -> std math (internal) | size=83 imp= str= calls=_87except,_ctrlfp |
| 1587 | 0x004A4615 | CRT:_handle_exc -> std math (internal) | size=152 imp= str= calls=src/crt/msvc.cpp:Floor,_umatherr,_87except,_errcode,_get_fpe_type,_ctrlfp |
| 1588 | 0x004A46AD | src/crt/msvc.cpp:Floor | size=691 imp=RaiseException str= calls=_statfp,_clrfp |
| 1589 | 0x004A4960 | CRT:_umatherr -> std math (internal) | size=535 imp= str= calls=_decomp,src/crt/msvc.cpp:Floor |
| 1590 | 0x004A4B77 | CRT:_87except -> std math (internal) | size=136 imp= str= calls=_errcode,_trandisp_lookup,_ctrlfp,_matherr |
| 1591 | 0x004A4BFF | CRT:_errcode -> std math (internal) | size=38 imp= str= calls= |
| 1592 | 0x004A4C25 | CRT:_trandisp_lookup -> std math (internal) | size=37 imp= str= calls= |
| 1593 | 0x004A4C4A | CRT:_get_fpe_type -> std math (internal) | size=45 imp= str= calls= |
| 1594 | 0x004A4C77 | CRT:_frnd -> std math (internal) | size=18 imp= str= calls= |
| 1595 | 0x004A4C89 | CRT:_set_exp -> std math (internal) | size=41 imp= str= calls= |
| 1596 | 0x004A4CB2 | CRT:_sptype -> std math (internal) | size=90 imp= str= calls= |
| 1597 | 0x004A4D0C | CRT:_decomp -> std math (internal) | size=193 imp= str= calls=_set_exp |
| 1598 | 0x004A4DCD | CRT:_statfp -> std math (internal) | size=14 imp= str= calls= |
| 1599 | 0x004A4DDB | CRT:_clrfp -> std math (internal) | size=15 imp= str= calls= |
| 1600 | 0x004A4DEA | CRT:_ctrlfp -> std math (internal) | size=35 imp= str= calls= |
| 1601 | 0x004A4E0D | src/crt/msvc.cpp:Floor | size=86 imp= str= calls= |
| 1602 | 0x004A4E63 | CRT:__initstdio -> C++ runtime startup | size=165 imp= str= calls=_amsg_exit,calloc |
| 1603 | 0x004A4F08 | CRT:__endstdio -> C++ runtime shutdown | size=20 imp= str= calls=_flushall,_fcloseall |
| 1604 | 0x004A4F20 | CRT:memmove -> std::memmove | size=821 imp= str= calls= |
| 1605 | 0x004A5260 | CRT:_except_handler3 -> compiler structured exception support | size=189 imp= str= calls=__global_unwind2,__local_unwind2,_NLG_Notify |
| 1606 | 0x004A531D | CRT:_seh_longjmp_unwind -> compiler structured exception support | size=27 imp= str= calls=__local_unwind2 |
| 1607 | 0x004A5338 | CRT:_isctype -> C++ runtime character classes | size=117 imp= str= calls=__crtGetStringTypeA |
| 1608 | 0x004A53B0 | CRT:_allmul -> compiler 64-bit multiplication | size=52 imp= str= calls= |
| 1609 | 0x004A53E4 | CRT:_XcptFilter -> platform: InstallFaultLog (src/platform/system_info_win.cpp) | size=321 imp=UnhandledExceptionFilter str= calls=xcptlookup |
| 1610 | 0x004A5525 | CRT:xcptlookup -> C++ runtime (structured exceptions) | size=67 imp= str= calls= |
| 1611 | 0x004A5568 | CRT:_wincmdln -> C++ runtime startup | size=88 imp= str= calls=_ismbblead,__initmbctable |
| 1612 | 0x004A55C0 | CRT:_setenvp -> C++ runtime startup | size=185 imp= str= calls=malloc,_free_base,_amsg_exit,strcpy,strlen,__initmbctable |
| 1613 | 0x004A5679 | CRT:_setargv -> SDL3 main (argv) | size=153 imp=GetModuleFileNameA str= calls=malloc,_amsg_exit,parse_cmdline,__initmbctable |
| 1614 | 0x004A5712 | CRT:parse_cmdline -> SDL3 main (argv) | size=436 imp= str= calls= |
| 1615 | 0x004A58C6 | CRT:__crtGetEnvironmentStringsA -> C++ runtime startup (environment) | size=306 imp=FreeEnvironmentStringsA,FreeEnvironmentStringsW,GetEnvironmentStrings,GetEnvironmentStringsW,WideCharToMultiByte str= calls=malloc,_free_base,memmove |
| 1616 | 0x004A59F8 | CRT:_ioinit -> C++ runtime startup | size=427 imp=GetFileType,GetStartupInfoA,GetStdHandle,SetHandleCount str= calls=malloc,_amsg_exit |
| 1617 | 0x004A5BA3 | CRT:_FF_MSGBANNER -> C++ runtime error message | size=57 imp= str= calls=_NMSG_WRITE |
| 1618 | 0x004A5BDC | CRT:_NMSG_WRITE -> C++ runtime error message | size=339 imp=GetModuleFileNameA,GetStdHandle,WriteFile str=<program name unknown>／...／Runtime Error!\n\nProgram: ／\n\n／ calls=strcpy,strcat,strlen,__crtMessageBoxA,strncpy |
| 1619 | 0x004A5D2F | CRT:_control87 -> std math (internal) | size=53 imp= str= calls=_abstract_cw,_hw_cw |
| 1620 | 0x004A5D64 | CRT:_controlfp -> std math (internal) | size=22 imp= str= calls=_control87 |
| 1621 | 0x004A5D7A | CRT:_abstract_cw -> std math (internal) | size=146 imp= str= calls= |
| 1622 | 0x004A5E0C | CRT:_hw_cw -> std math (internal) | size=137 imp= str= calls= |
| 1623 | 0x004A5E95 | CRT:tolower -> std::tolower (internal use) | size=203 imp= str= calls=_isctype,__crtLCMapStringA |
| 1624 | 0x004A5F60 | CRT:_ZeroTail -> std number parsing (internal) | size=73 imp= str= calls= |
| 1625 | 0x004A5FA9 | CRT:_IncMan -> std number parsing (internal) | size=86 imp= str= calls=__add_12 |
| 1626 | 0x004A5FFF | CRT:_RoundMan -> std number parsing (internal) | size=140 imp= str= calls=_ZeroTail,_IncMan |
| 1627 | 0x004A608B | CRT:_CopyMan -> std number parsing (internal) | size=27 imp= str= calls= |
| 1628 | 0x004A60A6 | CRT:_FillZeroMan -> std number parsing (internal) | size=12 imp= str= calls= |
| 1629 | 0x004A60B2 | CRT:_IsZeroMan -> std number parsing (internal) | size=27 imp= str= calls= |
| 1630 | 0x004A60CD | CRT:_ShrMan -> std number parsing (internal) | size=141 imp= str= calls= |
| 1631 | 0x004A615A | CRT:_ld12cvt -> std number parsing (internal) | size=364 imp= str= calls=_RoundMan,_CopyMan,_FillZeroMan,_IsZeroMan,_ShrMan |
| 1632 | 0x004A62C6 | CRT:_ld12tod -> std number parsing (internal) | size=22 imp= str= calls=_ld12cvt |
| 1633 | 0x004A62DC | CRT:_ld12tof -> std number parsing (internal) | size=22 imp= str= calls=_ld12cvt |
| 1634 | 0x004A62F2 | CRT:_atodbl -> std number parsing (internal) | size=45 imp= str= calls=_ld12tod,__strgtold12 |
| 1635 | 0x004A631F | CRT:_atoflt -> std number parsing (internal) | size=45 imp= str= calls=_ld12tof,__strgtold12 |
| 1636 | 0x004A6350 | CRT:strcpy -> std::string assignment | size=7 imp= str= calls= |
| 1637 | 0x004A6360 | CRT:strcat -> std::string append | size=224 imp= str= calls= |
| 1638 | 0x004A6440 | CRT:_fptostr -> std formatting (internal) | size=119 imp= str= calls=memcpy,strlen |
| 1639 | 0x004A64B7 | CRT:_fltout2 -> C++ runtime printf float support | size=100 imp= str= calls=__dtold,$I10_OUTPUT |
| 1640 | 0x004A651B | CRT:__dtold -> std formatting (internal) | size=182 imp= str= calls= |
| 1641 | 0x004A65E0 | CRT:memset -> std::fill / value initialisation | size=88 imp= str= calls= |
| 1642 | 0x004A6640 | CRT:strlen -> std::string_view::size | size=123 imp= str= calls= |
| 1643 | 0x004A66BB | CRT:_fptrap -> C++ runtime | size=9 imp= str= calls=_amsg_exit |
| 1644 | 0x004A66C4 | CRT:__CxxUnhandledExceptionFilter -> C++ exception handling | size=70 imp= str= calls=terminate,_ValidateExecute |
| 1645 | 0x004A670A | CRT:__CxxSetUnhandledExceptionFilter -> C++ runtime startup | size=17 imp=SetUnhandledExceptionFilter str= calls=__CxxUnhandledExceptionFilter |
| 1646 | 0x004A671B | CRT:__CxxRestoreUnhandledExceptionFilter -> C++ runtime shutdown | size=13 imp=SetUnhandledExceptionFilter str= calls= |
| 1647 | 0x004A6728 | CRT:_ValidateRead -> compiler C++ exception support | size=28 imp=IsBadReadPtr str= calls= |
| 1648 | 0x004A6744 | CRT:_ValidateWrite -> compiler C++ exception support | size=28 imp=IsBadWritePtr str= calls= |
| 1649 | 0x004A6760 | CRT:_ValidateExecute -> compiler C++ exception support | size=24 imp=IsBadCodePtr str= calls= |
| 1650 | 0x004A6780 | CRT:strcmp -> std::string_view comparison | size=132 imp= str= calls= |
| 1651 | 0x004A681B | CRT:__tzset -> crt::Time (local time zone) | size=21 imp= str= calls=_tzset |
| 1652 | 0x004A6830 | CRT:_tzset -> crt::Time (local time zone) | size=606 imp=GetTimeZoneInformation,WideCharToMultiByte str=TZ calls=malloc,_free_base,atol,strcpy,strlen,strcmp,strncpy,getenv |
| 1653 | 0x004A6A8E | CRT:_isindst -> crt::Time (local time zone) | size=428 imp= str= calls=cvtdate |
| 1654 | 0x004A6C3A | CRT:cvtdate -> crt::Time (local time zone) | size=320 imp= str= calls= |
| 1655 | 0x004A6D7A | CRT:_lseek -> C++ runtime low-level files (the game uses its own cFile) | size=154 imp=GetLastError,SetFilePointer str= calls=_dosmaperr,_get_osfhandle |
| 1656 | 0x004A6E14 | CRT:_write -> std stream output (internal) | size=429 imp=GetLastError,WriteFile str= calls=_lseek,_dosmaperr |
| 1657 | 0x004A6FC1 | CRT:_getbuf -> std stream output (internal) | size=68 imp= str= calls=malloc |
| 1658 | 0x004A7005 | CRT:_isatty -> std stream output (internal) | size=38 imp= str= calls= |
| 1659 | 0x004A702B | CRT:wctomb -> std text conversion (internal) | size=104 imp=WideCharToMultiByte str= calls= |
| 1660 | 0x004A70A0 | CRT:_aulldiv -> compiler 64-bit division | size=104 imp= str= calls= |
| 1661 | 0x004A7110 | CRT:_aullrem -> compiler 64-bit remainder | size=117 imp= str= calls= |
| 1662 | 0x004A7185 | CRT:_matherr -> std math (internal) | size=3 imp= str= calls= |
| 1663 | 0x004A7188 | CRT:fflush -> std stream output (internal) | size=59 imp= str= calls=_flush,flsall,_commit |
| 1664 | 0x004A71C3 | CRT:_flush -> std stream output (internal) | size=92 imp= str= calls=_write |
| 1665 | 0x004A721F | CRT:_flushall -> std stream output (internal) | size=9 imp= str= calls=flsall |
| 1666 | 0x004A7228 | CRT:flsall -> std stream output (internal) | size=109 imp= str= calls=fflush |
| 1667 | 0x004A7295 | CRT:calloc -> std heap | size=125 imp=HeapAlloc str= calls=_callnewh,__sbh_alloc_block,memset |
| 1668 | 0x004A7312 | CRT:_fcloseall -> std stream output (internal) | size=88 imp= str= calls=_free_base,fclose |
| 1669 | 0x004A736A | CRT:__crtGetStringTypeA -> C++ runtime locale | size=329 imp=GetStringTypeA,GetStringTypeW,MultiByteToWideChar str= calls=_except_handler3,memset,_alloca_probe |
| 1670 | 0x004A74B3 | CRT:_ismbblead -> std text (internal) | size=17 imp= str= calls=x_ismbbtype |
| 1671 | 0x004A74C4 | CRT:x_ismbbtype -> std text (internal) | size=49 imp= str= calls= |
| 1672 | 0x004A74F5 | CRT:_setmbcp -> C++ runtime multibyte | size=409 imp=GetCPInfo str= calls=getSystemCP,CPtoLCID,setSBCS,setSBUpLow |
| 1673 | 0x004A768E | CRT:getSystemCP -> C++ runtime multibyte | size=74 imp=GetACP,GetOEMCP str= calls= |
| 1674 | 0x004A76D8 | CRT:CPtoLCID -> C++ runtime multibyte | size=51 imp= str= calls= |
| 1675 | 0x004A770B | CRT:setSBCS -> C++ runtime multibyte | size=41 imp= str= calls= |
| 1676 | 0x004A7734 | CRT:setSBUpLow -> C++ runtime multibyte | size=389 imp=GetCPInfo str= calls=__crtGetStringTypeA,__crtLCMapStringA |
| 1677 | 0x004A78B9 | CRT:__initmbctable -> C++ runtime startup | size=28 imp= str= calls=_setmbcp |
| 1678 | 0x004A78D5 | CRT:__crtMessageBoxA -> platform: SDL message boxes (DialogErrorReporter) | size=137 imp=GetProcAddress,LoadLibraryA str=user32.dll／MessageBoxA／GetActiveWindow／GetLastActivePopup calls= |
| 1679 | 0x004A7960 | CRT:strncpy -> std::string assignment | size=254 imp= str= calls= |
| 1680 | 0x004A7A5E | CRT:__crtLCMapStringA -> C++ runtime locale | size=548 imp=LCMapStringA,LCMapStringW,MultiByteToWideChar,WideCharToMultiByte str= calls=_except_handler3,__addl,_alloca_probe |
| 1681 | 0x004A7C82 | CRT:__addl -> std number parsing (internal) | size=43 imp= str= calls= |
| 1682 | 0x004A7CAD | CRT:__add_12 -> std number parsing (internal) | size=33 imp= str= calls= |
| 1683 | 0x004A7CCE | CRT:__shl_12 -> std number parsing (internal) | size=94 imp= str= calls=__add_12 |
| 1684 | 0x004A7D2C | CRT:__shr_12 -> std number parsing (internal) | size=46 imp= str= calls= |
| 1685 | 0x004A7D5A | CRT:__mtold12 -> std number parsing (internal) | size=45 imp= str= calls= |
| 1686 | 0x004A7D87 | CRT:__ld12mul_part -> std number parsing (internal) | size=199 imp= str= calls=__shl_12,__shr_12 |
| 1687 | 0x004A7E4E | CRT:__strgtold12 -> std number parsing (internal) | size=1185 imp= str= calls=_isctype,__ld12mul_part,__multtenpow12 |
| 1688 | 0x004A831F | CRT:$I10_OUTPUT -> C++ runtime printf float support | size=659 imp= str=1#SNAN／1#IND／1#INF／1#QNAN calls=strcpy,__shl_12,__shr_12,__mtold12,__ld12mul,__multtenpow12 |
| 1689 | 0x004A85B2 | CRT:raise -> C++ runtime (signals) | size=301 imp= str= calls=_exit,siglookup |
| 1690 | 0x004A86DF | CRT:siglookup -> C++ runtime (signals) | size=69 imp= str= calls= |
| 1691 | 0x004A8724 | CRT:getenv -> C++ runtime environment (not used by the game) | size=125 imp= str= calls=strlen,_strnicmp,__wtomb_environ |
| 1692 | 0x004A87A1 | CRT:_dosmaperr -> C++ runtime errors | size=103 imp= str= calls= |
| 1693 | 0x004A8808 | CRT:_free_osfhnd -> C++ runtime low-level files | size=122 imp=SetStdHandle str= calls= |
| 1694 | 0x004A8882 | CRT:_get_osfhandle -> C++ runtime low-level files | size=61 imp= str= calls= |
| 1695 | 0x004A88BF | CRT:_commit -> C++ runtime low-level files | size=87 imp=FlushFileBuffers,GetLastError str= calls=_get_osfhandle |
| 1696 | 0x004A8916 | CRT:fclose -> std stream output (internal) | size=86 imp= str= calls=_free_base,_flush,_close,_freebuf |
| 1697 | 0x004A8970 | CRT:_alloca_probe -> compiler stack probe | size=47 imp= str= calls= |
| 1698 | 0x004A899F | CRT:__ld12mul -> std number parsing (internal) | size=544 imp= str= calls=__add_12,__shr_12,__mtold12 |
| 1699 | 0x004A8BBF | CRT:__multtenpow12 -> std number parsing (internal) | size=124 imp= str= calls=__ld12mul |
| 1700 | 0x004A8C3B | CRT:_strnicmp -> C++ runtime locale (getenv) | size=63 imp= str= calls=__crtCompareStringA |
| 1701 | 0x004A8C7A | CRT:__wtomb_environ -> C++ runtime environment | size=110 imp=WideCharToMultiByte str= calls=malloc,__crtsetenv |
| 1702 | 0x004A8CE8 | CRT:_close -> C++ runtime low-level files | size=179 imp=CloseHandle,GetLastError str= calls=_dosmaperr,_free_osfhnd,_get_osfhandle |
| 1703 | 0x004A8D9B | CRT:_freebuf -> C++ runtime stdio | size=43 imp= str= calls=_free_base |
| 1704 | 0x004A8DC6 | CRT:__crtCompareStringA -> C++ runtime locale | size=637 imp=CompareStringA,CompareStringW,GetCPInfo,MultiByteToWideChar str= calls=_except_handler3,__addl,_alloca_probe |
| 1705 | 0x004A9043 | CRT:__crtsetenv -> C++ runtime environment (not used by the game) | size=391 imp=SetEnvironmentVariableA str= calls=malloc,_free_base,_realloc_base,strcpy,strlen,__wtomb_environ,findenv,copy_envir |
| 1706 | 0x004A91CA | CRT:findenv -> C++ runtime environment (not used by the game) | size=88 imp= str= calls=_strnicmp |
| 1707 | 0x004A9222 | CRT:copy_environ -> C++ runtime environment (not used by the game) | size=103 imp= str= calls=malloc,_amsg_exit,_strdup |
| 1708 | 0x004A9289 | CRT:_mbschr -> C++ runtime multibyte strings | size=115 imp= str= calls=strchr |
| 1709 | 0x004A92FC | CRT:_strdup -> std::string copy | size=43 imp= str= calls=malloc,strcpy,strlen |
| 1710 | 0x004A9328 | import:RtlUnwind -> compiler structured exception support | size=6 imp=RtlUnwind str= calls= |
