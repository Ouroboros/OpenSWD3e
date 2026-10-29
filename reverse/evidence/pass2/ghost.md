# 第二遍：灵兽（ghost）

## 0x0049E590 GhostFollower::RemoveUnused — exact

汇编：0x00579A00 非空 → 释放其 +0（动作块）与本体，置 0。
对照：RemoveUnused 释放 unused（全程序无代码建立该槽）。
差异：无。

## 0x0049E5D0 GhostFollower::Spawn — exact

汇编：Remove；0x0043DA70 分配 0x20 字节本体与 0xA4 字节动作（该函数整块清零）→ 0x00579A10；动作 id（+0）= 参数一、动作（+8）= 参数二；x（+4）、y（+8）、+0x14、+0x18 取四个浮点参数；相位（+0x1C）= −0x20。
对照：Spawn 一致（kFirstPhase −0x20；Body 与 ActState 默认清零，与 0x0043DA70 的清零相同）。
差异：无。

## 0x0049E660 GhostFollower::Remove — exact

汇编：0x00579A10 非空 → 释放动作块与本体，置 0。
对照：Remove 一致。
差异：无。

## 0x0049E6A0 GhostFollower::Follow — exact

汇编：本体为空 → 返回。x87（53 位）：dx = 参数x − x、dy = 参数y − y，dy 存为 float dyF；比例 = 1.0f / sqrt(dy·dyF + dx·dx)，存为 float scaleF；+0xC = (float)(比例·dx + +0xC)；+0x10 = (float)(scaleF·dyF + +0x10)；x = (float)(+0xC + x)、y = (float)(+0x10 + y)；+0xC = +0x10 = 0；相位加 1，≥0x40 → 0x40。
对照：Follow 一致（double 运算与 53 位 x87 每步舍入相同；两 float 相加经 53 位再舍入到 24 位与直接舍入相同；kLastPhase 0x40）；落在目标点上时除以零、位置成 NaN，两者相同。
差异：无。

## 0x0049E740 GhostFollower::Draw — exact

汇编：本体为空 → 返回。动作前进；取图片（随即读首双字）；透明度 = 0x10；相位 <0 → (相位 + 0x20)/2，≥0x20 → (0x40 − 相位)/2（有符号向零）；左 = ftol(x − 镜头左（0x004C5118）− 动作.x)、上 = ftol(y − 镜头上（0x004C511C）− 动作.y)；以 0x14 画图。
对照：Draw 一致（kOpaque 0x10、kFadeFrames 0x20、kFadeDivisor 2、Painter::Translucent 0x14）；取不到图片属偏差 357。
差异：无。

## 0x0049E7F0 TalismanGhost::TalismanGhost — exact

汇编：Reset(4)，返回 this（对象在静态存储，其余为 0）。
对照：TalismanGhost() 在默认清零的记录上 Reset(kLastKind 4)。
差异：无。

## 0x0049E800 TalismanGhost::Stat — exact

汇编：编号（无符号）0..15 → +0x88、+0x8C、+0x90、+0x10、+0x94、+0x98、+0x9C、+0xE0、+0xE4、+0xE8、+0xEC、+0xF0、+0xF4、+0xF8、+0xFC、+0x100；16..21 → 0；22 → +0xB8 = 0，返回 0；其余 → 0。
对照：Stat 一致（GhostStat 各值、ClearUnkB8 0x16；负数在两边都落到默认出口）。
差异：无。

## 0x0049E910 TalismanGhost::Save — exact

汇编：0x400 字节路径 = 安装目录（0x004C2B94）+ 文件名；以 OPEN_ALWAYS／GENERIC_WRITE 打开，失败 → 释放后返回；定位 种类 × 0x158；写 GetLocalTime 的 SYSTEMTIME（0x10 字节），再写整个记录（0x148 字节）；关闭、释放。
对照：Save 一致（kSlotBytes 0x158、kTimeBytes 0x10、sizeof(TalismanGhost) 0x148 有 static_assert；SYSTEMTIME 八个字的次序相同；安装目录不超过 MAX_PATH，路径缓冲不会写满）。
差异：无。

## 0x0049EA80 TalismanGhost::Load — exact

汇编：路径同上；以 OPEN_EXISTING／GENERIC_READ 打开，失败 → 释放后返回；定位 槽 × 0x158 + 0x10；读 0x148 字节到记录；关闭、释放。
对照：Load 一致（读不足时其余字段保持原值，两者相同）。
差异：无。

## 0x0049EBB0 TalismanGhost::Reset — exact

汇编：种类（有符号）<0 或 ≥5 → 返回。+0x18、+0x38 各 0x20 字节清零；+0x60 = 0x5A、+0x64 = 0x64、+0x68 = 1、+0x6C = 0x5A、+0x70 = 0x64、+0x78 = 0xE、+0x7C = 0、+0x88 = 种类、+0x8C = 1、+0x90 = +0x10 = +0x94 = +0x98 = +0x9C = 0x32、+0xA0 = +0xA4 = +0xAC = +0xB0 = 0、+0xCC = 0x258、+0xD0 = 0。按种类：+0xE4 = 1、+0xE8 = 1（金）/2、+0xEC = 1、+0xE0 = 0、+0xF0 = 0xA（金）/0x12、+0xF4 = 0x14、+0xF8 = 0xF、+0xFC = 8、+0x100 = 6、+0x110 = 0、+0x114 = 0x28、+0xBC = 6、+0xC0 = 0xA、+0xC4 = 0x18（水 0x12）、+0xC8 = 0、+0x118..+0x124 = 0；strcpy 名字（0x004C04B8/B0/A8/A0/98「金/木/水/火/土符鬼」）；+0x128 = +0x12C = +0x130 = 0。其余字段不动。
对照：Reset 一致（各 kStart* 常量、kKindStarts 三列、KindNames 与 exe 字节一致；u32 种类使负数同样落到返回出口）。
差异：无。

## 0x0049EFC0 TalismanGhost::SetSkill — exact

汇编：[+0x118 + 编号×4] = 值（不检查编号），返回值。
对照：SetSkill 一致；编号越界属偏差 370。
差异：无。

## 0x0049EFE0 TalismanGhost::Skill — exact

汇编：返回 [+0x118 + 编号×4]。
对照：Skill 一致；编号越界属偏差 370。
差异：无。
