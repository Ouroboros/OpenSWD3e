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

## 0x00433720 DrawWindowFrame — exact

汇编：边框宽 = 参数 &0x7FFFFFFF，位 31 为不画底；半透明值写 0x004E6D58，非 0 时绘制方式 0x14。画底时：图片 0x234A（字比较）先取帧 9 画在 (右+宽−边框, ((上+下)>>1)−0xC)；裁剪 (左,上,右,下)，取帧 4 按其宽高平铺。随后依次：裁剪 (左−b,上−b,左,上) 画帧 0；裁剪 (左,上−b,右,上) 每格重新取帧 1 平铺；裁剪 (右,上−b,右+b,上) 帧 2 画在 右+b−宽；左右两边按右边框（帧 5）的高从上到下：裁剪左边画帧 3、裁剪右边画帧 5；裁剪 (左−b,下,左,下+b) 帧 6 画在 下+b−高；裁剪 (左,下,右,下+b) 帧 7（不重取）平铺在 下+b−高；裁剪 (右,下,右+b,下+b) 帧 8；最后裁剪恢复 (0,0,640,480)。每次取帧后 0x004ED9BC = 图片数据。
对照：顺序、裁剪矩形、坐标与平铺步进一致；图片缺失或尺寸为 0 的保护见 deviations 63。

## 0x00440830 ActDrawer::DrawGhost — diff → exact

汇编：0x00436C80(状态) 失败 → 0x0040C530("Act Err(Menu::DrawActGhost)", "設定錯誤 \nact[%d] qq[%d] dir[%d]", 动作档, 动作, 方向) 返回；取图（不查空），源 = 像素，透明度 0x004E6D58 = 参数 4，模式 = (旗标 & 0x80000017) | 0x14，画在 (x−状态x, y−状态y)。
对照：模式与源码 (旗标 & 0x80000003) | 0x14 相同（0x14 已含 0x10、0x04），其余一致（透明度小数部分为逐帧滑动增强，见 deviations 100）。
差异：取不到图片时原程序经空指针读取，源码不画，此前未登记。修正：并入 deviations 363（空指针）。复核：完整函数重新对照一致。

## 0x004408C0 ActDrawer::DrawBordered — diff → exact

汇编：推进失败同上（"Act Err(Menu::DrawActBorder)"）；取图（不查空），先 0x004179D0(x−状态x, y−状态y, 宽, 高, 0, &−1) 画白色轮廓，再 0x00417A60(同位置, 模式 0)。第 4 个参数不用（其栈位被改作 −1 的局部量）。
对照：一致。
差异：同 DrawGhost，取不到图片未登记。修正：并入 deviations 363。复核：完整函数重新对照一致。

## 0x00440980 FillBox — exact

汇编：先按裁剪 (0x004EAD68, 0x004ED9C8, 宽 0x004EC18C, 高 0x004ED9A8) 截取，宽或高 ≤0 → 返回；色 = MakeColor；模式 0：每行 0x004187A0(行, 色, 宽)（内部 宽>>1 对，do-while）；1：每行 0x00420E50(行, 宽, r, g, b)；2：每行 宽/2（向零）对 (dword>>2) & 0x004EEE9C；3：每行 0x00422970(行, 宽)；4：每行 (dword>>3) & 0x004EEEA0；5：顶行与底行同时、步数 (高+1)/2，色阶初 0，每步 0x00420E50(顶/底, 宽, 色阶/1024（向零）)，顶下移、底上移，色阶 += (分量<<10)/步数。
对照：一致（0x004EEE9C、0x004EEEA0 即 HalfMaskPair(1)、(2)）；宽度 <2 的回绕见 deviations 62。
