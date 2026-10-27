# 第二遍：界面小件

## 0x0040E1C0 MarchingRect::Draw — exact

汇编：x<0、y<0、x+w ≥ 0x280、y+h ≥ 0x1E0 → 返回。相位局部 = 0x004E6D30；每个像素：c = (0x3F−相位)&0x1F，灰 = c|c<<5|c<<10，复制成双字交给 0x00427680(…, 2) 取低字写入，相位 = (相位+1)&0x1F。上边 (x..x+w−1, y) 向右；右边 (x+w, y..y+h−1) 向下；下边 (x+w−i, y+h) 向左；左边 (x, y+h−i) 向上。最后全局相位 = (全局+1)&0x1F。
对照：一致（kShadeStart 0x3F、kChannelMask 0x1F、kPhases 0x1F）。修正：marching_rect.h 中相位注释由 swd32 地址 0x004CC2D4 改为 0x004E6D30。

## 0x0040E3F0 ShrinkScreenQuarter — exact

汇编：120 行 × 160 列，源指针每像素 +8 字节（每 4 像素取一），每行结束再 +0xF00（共 4 行），目标连续写。
对照：一致（kShrink 4，原地自前向后覆盖顺序相同）。

## 0x0040EF60 ActDrawer::Draw、0x0040EFF0 ActDrawer::DrawPicture、0x0040F030 ActDrawer::DrawWithMode — exact

汇编：Draw／DrawWithMode：0x00436C80(act) 失败 → 错误框「Act Err(Misc::DrawActData)」「設定錯誤 \nact[%d] qq[%d] dir[%d]」(id, +8, +0x34) 返回；否则 0x00436720(+0x4A, +0x4C) 取图，0x004ED9BC = 图像素，0x004E6D58 = 字节 +0x92，0x00417A60(x−(+0x10), y−(+0x14), 宽 +0xC, 高 +0xE, 模式, 0)；模式 Draw 为 +0x18，DrawWithMode 为 (+0x18 & 0x80000003) | 参数。DrawPicture：0x00436720(file, picture) 后以 (x, y, 宽, 高, 0, 0) 绘制，不改透明度。
对照：一致（kKeptModeBits = RleSource|Mirror|Flip = 0x80000003，DrawArgument 默认即 0），两条串逐字一致；取不到图片不画为已登记偏差（deviations 357）。

## 0x00411B50 ActDrawer::DrawCounter — exact

汇编：参数 (right, y, 未用, value)。循环：图 = 0x00436720(0x2377, value%10)（无符号），x −= 宽，0x004E6D58 = 0x004E582C，0x00417A60(x, y+1, 宽, 高, 0x14, 0)，value /= 10，直到 0；标签 0x00436720(0x2355, 0)，同样透明度，绘于 (x−0x18, y)。
对照：一致（透明度由调用者以 counterAlpha 传入，即 0x004E582C；Translucent 0x14、kLabelLeft 0x18）；空图片为已登记偏差（deviations 357）。
