# 第二遍：界面小件

## 0x0040E1C0 MarchingRect::Draw — exact

汇编：x<0、y<0、x+w ≥ 0x280、y+h ≥ 0x1E0 → 返回。相位局部 = 0x004E6D30；每个像素：c = (0x3F−相位)&0x1F，灰 = c|c<<5|c<<10，复制成双字交给 0x00427680(…, 2) 取低字写入，相位 = (相位+1)&0x1F。上边 (x..x+w−1, y) 向右；右边 (x+w, y..y+h−1) 向下；下边 (x+w−i, y+h) 向左；左边 (x, y+h−i) 向上。最后全局相位 = (全局+1)&0x1F。
对照：一致（kShadeStart 0x3F、kChannelMask 0x1F、kPhases 0x1F）。修正：marching_rect.h 中相位注释由 swd32 地址 0x004CC2D4 改为 0x004E6D30。

## 0x0040E3F0 ShrinkScreenQuarter — exact

汇编：120 行 × 160 列，源指针每像素 +8 字节（每 4 像素取一），每行结束再 +0xF00（共 4 行），目标连续写。
对照：一致（kShrink 4，原地自前向后覆盖顺序相同）。
