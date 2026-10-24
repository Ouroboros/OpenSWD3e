# 第一遍：音效接口（0x00427040..0x004270B0）

## 0x00427040 AudioManager::PlaySound — exact（增强已登记）

- `SoundEffects::Play(this=0x00578F40, 图像 0, id, 音量*128/11, 声像 0, 次数 1, 大小 0)`；/11 为 0x2E8BA2E9 乘法加 sar 1 与符号修正。实现 ScaledVolume 同此，Play 的图像与大小参数见其注释（所有调用者传 0）。
- 实现多出的 `muted` 检查属菜单 battle-smooth 增强的补间重画（deviations.md 第 99 行“重跑时不播放声音”），原逻辑下恒为 false。

## 0x00427070 AudioManager::StopSound、0x00427090 AudioManager::SetSoundPan — exact

- 直接转调 SoundEffects::Stop（0x0049D960）与 SetPan（0x0049DB00），参数原样。

## 0x00427080 AudioManager::StopAllSounds — exact

- 尾跳到本函数块 0x0049DA00：效果已开且启用时，播放链表（+0x670）逐个 AIL_end_sample、按样本用户数据 0 取 id、该声音用户数减 1（为 0 时释放其内存并清指针）、节点放回空闲链表头（+0x678），最后链表清空、返回 1；否则返回 0。实现 SoundEffects::StopAll 同此（活动链表从头依次放回 `free` 尾端，AllocNode 从尾端取，次序与原链表头插一致）。

## 0x004270B0 Game::PlaySoundAt — exact

- 控制角色（0x004C4B00，角色表 0x004D3FC8 步长 0xE4 的 +4/+8）与 (x, y) 的距离平方经 fild/fsqrt/_ftol 得距离；≥0x200 返回。Play(声音, 音量 0, 声像 0, 1 次)，SetVolume((0x80 − 距离*0x80/0x200)*音效音量 0x004C4F0C/11)，SetPan(((x − 角色x)<<6)/0x200)（/0x200 为 cdq/and 1FFh/sar 9 的有符号截断）。实现同此。
