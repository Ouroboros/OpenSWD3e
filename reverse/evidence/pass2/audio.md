# 第二遍：音频

## 0x00427040 AudioManager::PlaySound — exact

汇编：音量 = (音量<<7)/11（0x2E8BA2E9 乘法再 sar 1，有符号）；0x0049D7B0(图像 0, 编号, 音量, 声像 0, 次数 1, 大小 0)，返回其值。
对照：一致（ScaledVolume = 音量×0x80/11、Play(id, 音量, 0, 1)）；muted 为平滑重跑的增强（deviations 100）。

## 0x00427070 AudioManager::StopSound — exact

汇编：0x0049D960(编号)。对照：一致。

## 0x00427080 AudioManager::StopAllSounds — exact

汇编：尾跳 0x0049DA00：0x0049D790 与 0x0049D7A0 都为 1 时，逐个取播放链（+0x670）节点：AIL_end_sample，以 AIL_sample_user_data 的编号把声音表（+0x67C，16 字节一项）该项计数 −1，为 0 时释放其内存并清指针；节点推到空闲链头（+0x678）；播放链清空，返回 1；否则返回 0。
对照：一致（StopAll、ReleaseSound、free.push_back 的末项即空闲链头）；Miles 调用为已登记的平台替换。

## 0x00427090 AudioManager::SetSoundPan — exact

汇编：0x0049DB00(编号, 声像)。对照：一致。

## 0x0049C910 MidiPlayer::~MidiPlayer（atexit 回调）— exact

汇编：ecx = 0x00578628，跳 0x0049C920。
对照：MidiPlayer 析构（AudioManager::midi 随 Game 析构）调用 Close。
差异：无。

## 0x0049C920 MidiPlayer::~MidiPlayer — exact

汇编：跳 0x0049C930（Close）。
对照：~MidiPlayer 调用 Close。
差异：无。

## 0x0049C930 MidiPlayer::Close — exact

汇编：未打开（+8≠1）→ 0。否则播放链（+0x424）逐节点：链头 = 下一节点（+0x14），AIL_end_sequence、AIL_release_sequence_handle、释放 MIDI 数据（+4）与节点；空闲链（+0x43C）逐节点：AIL_release_sequence_handle、释放节点；+8 = +0xC = 0，返回 1。
对照：MidiPlayer::Close 一致（device.End / ReleaseVoice 为 Miles 的平台替换，偏差 25 同类；MIDI 数据随 vector 释放）。全程序无代码打开 MIDI 播放器（0x00578628 的引用只有构造、析构与 MusicQueue 的查询与播放，+8 从不置 1），打开分支执行不到。
差异：无。

## 0x0049C9B0 SequencePlayer::IsOpen — exact

汇编：返回 +8 = 1。
对照：IsOpen 一致。
差异：无。

## 0x0049C9C0 SequencePlayer::IsEnabled — exact

汇编：返回 +0xC = 1。
对照：IsEnabled 一致。
差异：无。

## 0x0049C9D0 MidiPlayer::SetMasterVolume — exact

汇编：未打开 → 0。否则 AIL_set_XMIDI_master_volume(驱动, ClampVolume(音量))，返回 AIL_XMIDI_master_volume(驱动)。
对照：SetMasterVolume 未打开返回 0；打开分支执行不到（播放器从不打开），源码记下夹住的音量并返回之。
差异：无。

## 0x0049CA10 MidiPlayer::Play — exact

汇编：编号=0 → 0。AllocNode（空闲链头）为空 → 0。节点 +4 = +8 = 0；new File、以 OPEN_ALWAYS／GENERIC_READ 打开，失败 → 错误文字 = AIL_last_error()、删除文件对象，返回 0（节点丢失）；读入整个文件到 AIL 内存；AIL_init_sequence = 0 → 记错误、释放数据、FreeNode，返回 0；= −1 → 记错误后照常播放；+0xC = ClampVolume(音量) << 4，AIL_set_sequence_user_data(编号)、AIL_set_sequence_volume(+0xC/16)、loop_count、start，PushActive，返回编号。
对照：MidiPlayer::Play 编号为 0 与 AllocNode 为空两个出口一致。播放器从不打开、空闲链恒空（构造清零，FreeNode 只在 AllocNode 之后调用），原程序每次都在 AllocNode 处返回 0；其后的文件读取与 Miles 序列调用执行不到（源码以 AudioDevice 的平台层写出，−1 时不另记错误文字）。
差异：无（可执行路径一致）。

## 0x0049CC20 MidiPlayer::FadeOut — exact

汇编：未打开、未启用、编号=0 或 Find 为空 → 0。AIL_sequence_ms_position（结果不用）；节点 +0x10 |= 0x80000000；SetFadeStep(节点, 毫秒)；返回编号。
对照：FadeOut 一致（Fading 0x80000000）。
差异：无。

## 0x0049CCA0 MidiPlayer::SetFadeStep — exact

汇编：AIL_sequence_ms_position(总长, 当前)；跨度 = min(毫秒, 总长 − 当前)，max(跨度, +4)，≥ +4 << 11 → +4 << 11；节点 +8 = 0x800 / (跨度 / +4)（有符号）；返回之。
对照：SetFadeStep 一致（kFadeRange 0x800、kLongestShift 11、unknown4 0x1E）。
差异：无。

## 0x0049CD00 MidiPlayer::IsStopped — exact

汇编：编号≠0 → Find(编号) 为空；否则播放链头为空。
对照：IsStopped 一致。
差异：无。

## 0x0049CD30 SequencePlayer::FreeNode — exact

汇编：节点为空 → 0；否则节点 +0x14 = 空闲链头、链头 = 节点，返回 1。
对照：FreeNode 一致（free 的末项即链头）。
差异：无。

## 0x0049CD60 SequencePlayer::AllocNode — exact

汇编：取空闲链头，链头清 0，非空 → 链头 = 其 +0x14；返回取出的节点。
对照：AllocNode 一致。
差异：无。

## 0x0049CD80 MidiPlayer::Find — exact

汇编：沿播放链（+0x424、下一节点 +0x14）：AIL_sequence_user_data(句柄, 0) 调两次（第一次结果不用），= 编号 → 返回节点；链尽 → 0。
对照：Find 以 node->id 比较，一致（用户数据即 Play 写入的编号）。
差异：无。

## 0x0049CDC0 AudioManager::system（全局初始化片段）— exact

汇编：调用 0x0049CDD0（0x00578A68 的构造，只返回 this），再 atexit(0x0049CDF0：AIL_shutdown)。
对照：AudioManager::system 随 Game 构造，析构时 Shutdown；Miles 的启动与关闭属偏差 25。
差异：无。

## 0x0049CDD0 AudioSystem::AudioSystem — exact

汇编：mov eax, ecx；返回。
对照：AudioSystem() = default。
差异：无。

## 0x0049CE10 AudioSystem::Shutdown — exact

汇编：跳 0x0049CE40。
对照：Shutdown 记下未启动；AIL_shutdown 属偏差 25。
差异：无。

## 0x0049CE20 AudioSystem::Startup — exact

汇编：AIL_set_redist_directory(参数)、0x004A1070（atexit 注册 AIL_shutdown）、AIL_startup。
对照：Startup 记下目录与启动状态，属偏差 25（平台替换）。
差异：无。

## 0x0049CE40 AudioSystem::Shutdown — exact

汇编：jmp [AIL_shutdown]。
对照：同 0x0049CE10，属偏差 25。
差异：无。

## 0x0049CE50 AudioManager::queue（全局初始化片段）— exact

汇编：调用 0x0049CE60 构造 0x00578A70，再 atexit(0x0049CE80：0x0049CF00 依次 ClearSlots(3)、(2)、(1))。
对照：AudioManager::queue 随 Game 构造；默认析构释放三组曲目名，结果相同。
差异：无。

## 0x0049CE60 MusicQueue::MusicQueue — exact

汇编：0x0049CE90：+0 = 0x1E、+4 = +8 = 0、+0xC = +0x10 = 4、+0x2C = 0x7F；+0x430 起 5 双字与三组曲目（+0x444、+0x46C、+0x494 各 0x28 字节）清零；+0x14..+0x28 不写（静态存储为 0）。
对照：MusicQueue 构造一致（mode = pending = Silence 4、volume = MaxVolume 0x7F、index/repeat 默认 0；+0..+8 无人读取）。
差异：无。

## 0x0049CF20 MusicQueue::SetSlot — exact

汇编：编号 ≤0、序号 <0 或 ≥2 → 0；列表 1/2/3 → +0x494/+0x46C/+0x444 的序号项，其余 → 0。释放旧名、名 = 0；类型 = TypeOf(名)；类型≠0 → 复制名字；编号 = 参数、+0xC = +0x10 = 0；返回 0。
对照：SetSlot 一致（size_t 序号把负数变为大数，同样落在 ≥ Pieces 的出口）。
差异：无。

## 0x0049D000 MusicQueue::ClearSlots — exact

汇编：列表 1/2/3 → 释放两项的名字，0x28 字节清零；其余不做；返回 0。
对照：ClearSlots 一致。
差异：无。

## 0x0049D060 MusicQueue::SetVolume — exact

汇编：+0x2C = 参数；MIDI 已打开 → SetMasterVolume(+0x2C)；流播放器已打开 → SetVolume(FirstId(), +0x2C)（音量先入栈，FirstId 无参数）。
对照：SetVolume 一致。
差异：无。

## 0x0049D0C0 MusicQueue::Switch — exact

汇编：当前类型 1 → 流 FadeOut(当前编号, 参数三)；2 → MIDI FadeOut。+0x10 = 列表。列表 1/2/3 → 对应序号 = 0、重复 = 1，参数二为 1 或 2 → 重复 = 参数二；其余 → +0x1C = 0、+0x20 = 1、+0x24 = 0、+0x28 = 1。返回 0。
对照：Switch 一致（Stream 1、Midi 2、RepeatList 1、OnceThrough 2）。
差异：无。

## 0x0049D1A0 MusicQueue::Update — exact

汇编：当前类型 1/2 → 对应 IsStopped(当前编号) 为假 → 0。当前 5 双字清零。待切换 1..3 → 模式 = 它、待切换 = 0、该列表序号 = 0；=4 → 模式 = 待切换 = 0。模式 1/2/3 → (序号, 重复, 曲目组)，其余 → 0；序号 ≥2（有符号）→ 0。当前 = 曲目组[序号]（浅复制）；名非空 → 类型 1：流 Play(名, 编号, +0x2C, 1)，2：MIDI Play；序号加 1，重复 = 1 且序号 ≥2 → 0。返回 0。
对照：Update 一致（名字以 std::string 深复制，非空判定与原指针非 0 等价：类型非 0 的名字必含扩展名）。
差异：无。

## 0x0049D310 MusicQueue::TypeOf — exact

汇编：名为空 → 0；strstr "wav"（0x004B1104）或 "mp3"（0x004C03C4）→ 1；"xmi"（0x004C03C0）→ 2；否则 0。
对照：TypeOf 一致（三串与 exe 字节一致）。
差异：无。

## 0x0049D370 MusicQueue::IsStopped — exact

汇编：当前类型 1 → 流 IsStopped(编号)；2 → MIDI IsStopped(编号)；否则 1。
对照：IsStopped 一致。
差异：无。
