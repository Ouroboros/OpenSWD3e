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
