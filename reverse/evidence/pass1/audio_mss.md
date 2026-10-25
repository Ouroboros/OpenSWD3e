# 第一遍：Miles 音频（audio_mss）

## MidiPlayer 与 SequencePlayer（第 1388–1400 行）— exact（平台替换；MidiPlayer::Open 删除）

- MIDI 播放器（0x00578628）的引用：构造 0x0049C8F0（chunk 0x0049E090：驱动、opened、enabled 清零，+4 = 0x1E，活动链 +0x424 与空闲链 +0x43C 清空）、退出回调 0x0049C910，以及音乐队列 0x0049D060、0x0049D0C0、0x0049D1A0、0x0049D370。全程序没有打开它的代码（正传的 0x00484DD0 在 swd3e 中无对应），IsOpen 恒为 0，原程序本身不播 MIDI。源码删除正传残留的 MidiPlayer::Open 与 AudioDevice::OpenMidi，偏差表“swd32 0x00484DD0 等 MIDI 播放”行删除；单元测试改为检查播放器保持未打开、SetMasterVolume 与 Play 返回 0。SequencePlayer 构造注释改为 chunk 0x0049E090。
- 0x0049C910：ecx = 0x00578628，跳 0x0049C920；0x0049C920：跳 Close。0x0049C930 Close：未打开返回 0；活动链逐个 AIL_end_sequence、AIL_release_sequence_handle、释放 MIDI 数据（+4）与节点；空闲链逐个 release 句柄、释放节点；opened、enabled 清零，返回 1。
- 0x0049C9B0／0x0049C9C0：+8／+0xC == 1。0x0049C9D0 SetMasterVolume：未打开返回 0；AIL_set_XMIDI_master_volume(驱动, ClampVolume(值))，返回 AIL_XMIDI_master_volume(驱动)。源码记下钳位值并返回（不可达）。
- 0x0049CA10 Play：id 为 0 → 0；AllocNode 为空 → 0；节点 +4、+8 清零；File Open(OpenAlways, GENERIC_READ) 失败 → 错误文字 = AIL_last_error，节点丢失，返回 0；读入整文件；AIL_init_sequence 返回 0 → 错误文字、释放数据、FreeNode、返回 0，返回 −1 → 记错误文字后照样播放；volume = ClampVolume(音量) << 4，user data 0 = id，AIL_set_sequence_volume(句柄, volume/16, 0)，loop count，start，PushActive，返回 id。
- 0x0049CC20 FadeOut：未打开、未启用或 id 0 → 0；Find 为空 → 0；（取一次播放位置，结果不用）flags（+0x10）|= 0x80000000，SetFadeStep(节点, 毫秒)，返回 id。0x0049CCA0 SetFadeStep：AIL_sequence_ms_position；span = min(毫秒, 总长 − 当前)，再夹到 [unit, unit << 11]（unit = +4），fadeStep（+8）= 0x800 / (span / unit)（有符号）。
- 0x0049CD00 IsStopped：id 非 0 时 Find 为空，否则活动链为空。0x0049CD30 FreeNode：节点非空时插到空闲链头，返回 1，否则 0。0x0049CD60 AllocNode：取空闲链头。0x0049CD80 Find：活动链中 AIL_sequence_user_data(句柄, 0) == id 者（原程序每节点读两次）。

## AudioManager::system、AudioSystem（第 1401–1405 行）— exact（平台替换）

- 0x0049CDC0：调 0x0049CDD0（ecx = 0x00578A68，空构造 0x0049CE00 返回 this），再 0x0049CDE0 以 atexit 注册 0x0049CDF0（ecx = 0x00578A68，跳 AIL_shutdown 的导入 thunk）。源码 AudioManager::system 成员与默认构造。
- 0x0049CE20 Startup：AIL_set_redist_directory(参数)、0x004A1070（atexit(AIL_shutdown)，返回 0）、AIL_startup。0x0049CE10：跳 AIL_shutdown；0x0049CE40：AIL_shutdown 导入 thunk。源码只记目录与状态，设备由 AudioDevice 管理；新增偏差行登记 Miles 启动与关闭的替换。

## AudioManager::queue、MusicQueue（第 1406–1414 行）与 BattleUnit::Data（第 1415 行）— exact

- 0x0049CE50：调 0x0049CE60（ecx = 0x00578A70，构造 0x0049CE90），再 0x0049CE70 以 atexit 注册 0x0049CE80（chunk 0x0049CF00：ClearSlots(3)、(2)、(1)）。构造：+0 = 0x1E、+4／+8 = 0、mode／pending（+0xC／+0x10）= 4、volume（+0x2C）= 0x7F、current（+0x430，5 个字）与三张表（+0x444／+0x46C／+0x494，各 10 个字）清零；+0x14..+0x28 不写（全局 bss 为 0）。源码析构注释原写 0x0049CDC0（那是 AudioSystem 的全局构造），改为 0x0049CE80／0x0049CF00。
- 0x0049CF20 SetSlot：id ≤ 0、下标 < 0 或 ≥ 2、表号非 1..3 → 0；表 1 → +0x494、2 → +0x46C、3 → +0x444；释放旧名，类型 = TypeOf(名)，非 0 时复制名字，id，+0xC／+0x10 清零，返回 0。0x0049D000 ClearSlots：表号 1..3 时释放两项名字、10 个字清零，返回 0；其它返回 0。
- 0x0049D060 SetVolume：volume = 参数；MIDI 已开时 SetMasterVolume；流播放器（0x005795C0）已开时 SetVolume(FirstId（0x0049E510）, volume)。
- 0x0049D0C0 Switch：current 类型 1 → 流 FadeOut（0x0049E350），2 → MIDI FadeOut；pending = 表号；1 → +0x24 = 0、+0x28 = 重复（1 或 2，否则 1）；2 → +0x1C／+0x20；3 → +0x14／+0x18；其它 → +0x1C、+0x24 = 0，+0x20、+0x28 = 1；返回 0。
- 0x0049D1A0 Update：current 类型 1／2 时对应播放器 IsStopped 为 0 则返回 0；current 五个字清零（不释放名字）；pending 1..3 → mode = pending、pending = 0、该表下标清零；4 → mode = pending = 0；按 mode 取下标、重复与表，下标 ≥ 2 返回；current = 表项（浅拷贝，名字指针共用；源码深拷贝，行为相同）；名字非空时按类型 Play(名, id, volume, 1)；下标加 1，重复为 1 且 ≥ 2 时归 0；返回 0。
- 0x0049D310 TypeOf：空 → 0；strstr "wav"（0x004B1104）或 "mp3"（0x004C03C4）→ 1；"xmi"（0x004C03C0）→ 2；否则 0。SetSlot 只在类型非 0 时存名字，空串类型为 0，故源码以名字非空代替指针非空等价。
- 0x0049D370 IsStopped：类型 1／2 按播放器，否则 1。0x0049D3B0：返回 +0xC——MusicQueue::Mode（mode）与 BattleUnit::Data（物件指针）同为此码。

## SoundEffects（第 1417–1438 行）与 ClampVolume、ClampPan — exact（平台替换；LoadSound 修正后）

- 0x0049D3D0：ecx = 0x00578F40，构造 0x0049D400（cFile 基类构造 0x0043CAE0，+0x54／+0x58 清零，+0x6C 起 0x200 字节配置文字与 +0x26C 起 0x400 字节错误文字清零），0x0049D3E0 以 atexit 注册 0x0049D3F0（跳析构）。0x0049D430 析构：Close，再 cFile 析构 0x0043CB10。
- 0x0049D480 Open：已开返回 1；LoadIndex 失败 → 错误文字 "SFX file connot open."，返回 0；声道 2、采样率 0xAC44、位数 16，AIL_set_preference(15, 0)；循环：采样率 < 0x2B11 → 驱动（+0x50）清零返回 0；waveOutOpen 失败 → 错误文字，未用 waveOut 时改用（preference 15 = 1），已用时采样率 /= 2（有符号），< 0x2B11 且 16 位时改 0xAC44、8 位；成功取配置文字，用 waveOut 或配置不含 "Emulated" 即成功，否则 waveOutClose、改用 waveOut 再试。成功：opened、enabled = 1，声道数 = preference(1) − 8，上限 16；逐个分配样本句柄并 init 后插入空闲链头（分配失败时记错误文字、返回 1）。源码以 AudioDevice 对应各步（MixerChannels、OpenOutput、Configuration），平台创建声道不会失败。
- 0x0049D6F0 Close：未开返回 1；活动链与空闲链逐个 end、release、释放节点；FreeIndex；waveOutClose；返回 1（opened 不清，源码同）。0x0049D790／0x0049D7A0：+0x54／+0x58 == 1。
- 0x0049D7B0 Play：未开、未启用或 id 0 → 0；图像参数为空时 LoadSound(id)，为空返回 0；AllocNode 为空返回 0；init_sample；图像首双字非 "RIFF" 时 AIL_set_named_sample_file(".mp3", 图像, 大小参数)，否则 AIL_set_sample_file；失败 → 错误文字、释放图像、FreeNode、返回 0；成功：表项用户数加 1、图像指针写入表项，user data 0 = id，音量 ClampVolume、声像 ClampPan、次数、start，PushActive；返回 0。LoadSound 每次新分配图像，表项只记最后一个，旧图像在原程序中泄漏，无行为影响；源码用 shared_ptr。
- 0x0049D960 Stop：未开、未启用或 id 0 → 0；Unlink 为空 → 0；end_sample，表项用户数减 1，为 0 时释放图像并清指针，FreeNode；返回 0。0x0049DAB0 SetVolume：Find 后有节点才设音量，返回 0。0x0049DB00 SetPan：返回钳位后的声像（有无节点都返回）。
- 0x0049DB50 Update：未开或未启用 → 0；活动链中 AIL_sample_status 为 1..2（无符号）者移出并插到空闲链头，音量 0、end，用户数减 1，为 0 时释放图像；返回 1。
- 0x0049DC30 FreeNode：插空闲链头（+0x678）。0x0049DC60 AllocNode：取空闲链头。0x0049DC80 PushActive：插活动链头（+0x670）。0x0049DCA0 Find／0x0049DCE0 Unlink：活动链中 user data 0 == id 者（Unlink 一并摘下）。0x0049DD30 ClampVolume：夹到 0..0x7F（有符号）。0x0049DD50 ClampPan：+0x3F 后夹到 0..0x7F。0x0049DD70 HasDriver：+0x50（驱动句柄）。
- 0x0049DD80 LoadIndex：路径 = 目录 + "all.snd"，File Open(OpenAlways, GENERIC_READ) 失败 → 0；表 3000×16 字节清零；从 0x1C 读 3000×0x2C 字节；每项 sizeFlags = 记录 +0x14 & 0x3FFFFFF | (记录 +0x20 & 3) << 26，offset = +0x18，用户数与图像清零；返回 1。源码另调 file.Preload（偏差第 88 行增强）。0x0049DF00 FreeIndex：无表返回 1；释放各图像与表、关闭文件，返回 1。
- 0x0049DF60 LoadSound：offset 0 → 空；定位；大小 = sizeFlags & 0x3FFFFFF，种类 = sizeFlags >> 26（0..3），种类 0 时前置 0x18；分配 前置 + 大小；抄 43 字节模板（0x004C03FC，"RIFF....WAVEfmt ....0123456789012345data.." 含 NUL，exe 字节已比对）；读入到 图像 + 前置；种类非 1 时 memmove(+0x14 ← +0x18, 0x12)，+0x10 = 0x10、+4 = 大小、+0x26 = "ta"、+0x28 = 大小 − 0x18。
- 修正：源码原把图像最少分配 43 字节，+0x28 的双字写到第 44 字节越界一位；改为至少 0x2C 字节（kRiffHeaderBytes）。偏差第 44 行改写为 swd3e 的实际情况（模板长度、头字段末端与 id 不检查范围）并引用汇编。

## StreamPlayer（第 1442–1454 行）与 SetFadeStep、SequencePlayer::PushActive — exact（平台替换）

- 流播放器（0x005795C0）：全局构造 0x0049E060 跳共用 chunk 0x0049E090（同 MIDI 播放器），0x0049E070 以 atexit 注册 0x0049E080（ecx = 0x005795C0，跳 0x0049E0B0 → Close）。源码补 0x0049E080 注释。
- 0x0049E0C0 Open：驱动（+0）= 参数（音效的数字驱动），opened、enabled = 1，两个 0x18 字节清零节点依次插入空闲链头，返回 1。源码不需要驱动参数（AudioDevice）。
- 0x0049E120 Close：未开返回 0；活动链与空闲链逐个释放名字（+4）、AIL_close_stream、释放节点；opened、enabled 清零，返回 1。空闲节点的句柄在退役时已清零，源码只结束活动流，结果相同。
- 0x0049E1B0 SetVolume：未开 → 0；Find 为空 → −1；volume（+0xC）= ClampVolume(值) << 4，AIL_set_stream_volume(ClampVolume(值))，返回 AIL_stream_volume。
- 0x0049E220 Play：id 0 或已在播 → 0；AllocNode 为空 → 0；+4／+8 清零；AIL_open_stream(驱动, 名, 0) 失败 → 错误文字、FreeNode、返回 0；名字复制到 +4；volume = Clamp << 4，user data 0 = id，set volume、loop count、start，PushActive；返回 id（无 opened／enabled 检查）。
- 0x0049E350 FadeOut：未开、未启用或 id 0 → 0；Find 为空 → 0；取一次位置（结果不用），flags |= 0x80000000，SetFadeStep(节点, 步数)，返回 id。0x0049E3D0 SetFadeStep：步数 0 视为 1，fadeStep = volume / 步数（有符号），为 0 时 1，返回商。
- 0x0049E400 Update：活动链为空 → 0；逐个：淡出中 volume −= fadeStep，Clamp(volume/16)（有符号）非 0 时设音量、标志清零，为 0 时退役；否则按 AIL_stream_status：2 退役，4／8／16 标志清零，其它（含 1）按标志（初值为未初始化栈槽，偏差第 43 行）；退役：close_stream、释放名字，句柄、名字、fadeStep、flags 清零，移到空闲链头，标志置 1；返回 0。
- 0x0049E4F0 IsStopped：Find 为空。0x0049E510 FirstId：活动链头的 user data 0，无则 0。0x0049E530 PushActive：插活动链头（+0x424），返回节点。0x0049E550 Find：活动链中 user data 0 == id 者。
