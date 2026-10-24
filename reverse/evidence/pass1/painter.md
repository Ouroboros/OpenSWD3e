# 第一遍汇编验证：draw/painter

## 0x00417510 Painter::Init — exact

- 0x004271D0 取像素格式失败返回 0；裁剪宽高取屏幕宽高（0x004B1E90、0x004B1E94），起点 0；0x800 项行偏移 = 行×跨度（原程序以字节存，实现以像素存，用法一致）；BuildBlitTables；返回 1。

## 0x00417570 Painter::BuildBlitTables — exact

- 函数表 0x004EC198 起按模式（低 2 位镜像、翻转）填写：0/0xC/0x14/0x24/0x28/0x30 四项（正、镜像、翻转同正、翻转镜像同镜像），4/8/0x10/0x18/0x20/0x2C 两项，0x1C 一项；原始像素 0x40 起：+0、+1、+4、+5、+8、+0x14；缩放 0x80、0x84、0x90、0x94、0xA4、0xA8、0xAC、0xC0。各项函数地址与实现的 Blit* 一一对应（BlitCopy 0x00418D10、Mirrored 0x00418F80、Additive 0x00419200/0x00419870、AlphaMap 0x00419F30/0x0041AD70、Shadow 0x00420290/0x00420860、Tint 0x0041BC40/0x0041BFE0、Translucent 0x0041DD00/0x0041EF80、EdgeMark 0x0041D6B0/0x0041D9D0、FadeDown 0x0041C3B0、ScaledGlow 0x00421290/0x00421730、Silhouette 0x00421BF0/0x00421F00、Gray 0x00422210/0x004225A0、Subtractive 0x004229F0/0x00422D60、Smear 0x004230F0/0x00423380、原始 0x00418090/0x00418190/0x00418200/0x00418800/0x00418880/0x00418310、缩放 0x00423830/0x00423D40/0x00424620/0x00424E40/0x00425B10/0x00425FF0/0x004265B0/0x00426C80）。

## 0x00417740 Painter::LockSurface、0x00417790 UnlockSurface — exact（平台替换）

- Lock(DDLOCK_WAIT) 失败返回 0，成功记跨度/2 于 0x004EF53C 并返回内存；Unlock 转调。实现直接取模拟表面的像素与宽度（deviations.md 第 285 行，来源改为 asm）。

## 0x004177B0 Painter::SetClip、0x00417820 SetClipClamped — exact

- SetClip：左 <0、上 <0、右 > 屏宽、下 > 屏高（有符号）时返回 0，否则记起点与宽高返回 1。SetClipClamped：四边截到屏幕后记起点与宽高，返回 1（实现无返回值，无调用者使用）。

## 0x00417880 RleCodec::PixelAt — exact

- 行 ≥ 高（有符号）返回 0；有输出时先清 0；不是 0xFFFF 魔数返回 0；行非 0 时逐行跳过（行码 0 返回 0，按 &0x3FFF 前进，计数等于行号为止）；跳过行码后逐段：字面段（种类 0）时列在段内返回 1 并取像素（16 位取字、8 位取字节），否则数据前进 计数×像素字节、位置加计数；种类 0x8000、0xC000 位置加计数；0x4000 不计；下一码为 0 返回 0。

## 0x004179D0 Painter::DrawOutline — exact

- 模式 |0x24，依次画于 (x+1,y+1)、(x−1,y−1)、(x−1,y+1)、(x+1,y−1)。

## 0x00417A60 Painter::DrawPicture — exact（修正）

- 来源首字为 0xFFFF 且无调色板时模式置 0x80000000。模式 &0x3FFFFFFC ≥0x80 时以 (x, y, 宽, 高, &模式) 调用缩放表项返回（选项不复位）。效果为 0x14：透明度 ≤0 时直接复位返回，>0xF 时模式只留 0x80000003。未缩放（0x004EDA10 为 0）：上裁剪（行 = y−上+高，跳过上 = 高−行）、下裁剪（跳过下 = 起y−裁高−裁上+高，行 = 裁高−起y+裁上）；无斜切时左右裁剪（跳过左 = 宽−列）。有缩放：缩放比 = (高<<10)/缩放高（放大时）或 −0x400，上裁剪（行 = 缩放高−裁上+y−1，跳过上 = 裁上−y）、下裁剪（行 = 裁高−起y+裁上−1），斜切为 0 时取 1。斜切路径：方向 斜切>0 ? −1 : 1，步长 (|斜切|<<10)/(缩放高或高)，左右裁剪（跳过左 = 裁左−x）。公共：目标 = 表面 + 行偏移[起y] + 起x×2，终点 = 目标 + 行偏移[行]，跨度 0x004B1E8C；Flip 时跨度取负、跳过上 = 跳过下、目标 = 终点；非 RLE 且 Mirror 时跳过左 = 宽−跳过左−1；非 RLE 时源行 = 来源 + (跳过上×宽+跳过左)×像素字节；抖动相位 (起y+0x1E0)%3；以 (模式&0xFFFF)+(RLE ? 0 : 0x40) 分派（空表项已登记第 11 行，来源改为 asm）。最后复位 8 个选项。
- 修正：提前返回的值按原程序 eax：透明度 ≤0 返回透明度；未缩放上裁剪掉返回高，下或左右裁剪掉返回剩下的行或列；缩放路径上下裁剪掉返回裁剪上边；斜切路径左右裁剪掉返回裁剪左边。实现原先一律返回 0（战斗 HUD 会使用返回值）。

## 0x00417E90 DrawTile16、0x00417EF0 DrawTile16Keyed、0x00417F70 DrawTile8、0x00418010 DrawTile8Keyed — exact

- 16×16 不裁剪：目标 = 表面 + 行偏移[y] + x×2，行进 0x004B1E8C。16 位：逐行复制 32 字节；带键版跳过等于填充色（0x004EEE94 低字）的像素。8 位：经调色板 0x004EDA34；带键版跳过索引 1。超出整个表面的像素不写（实现的保护，原程序越界写）。返回值无调用者使用。

## 0x00418090 BlitRawCopy、0x00418190 BlitRawCopyMirrored、0x00418200 BlitRawKeyed、0x00418800 BlitRawKeyedMirrored — exact（注释更正）

- 共同：行数、列数取放置（0x004ED9EC、0x004ED9F8），目标逐行以正跨度 0x004B1E8C 前进（翻转时从放置的终点起仍向下走），源逐行前进图片宽。16 位路径为 do-while（列、行至少 1，DrawPicture 已保证为正）；8 位路径为前测循环经调色板 0x004EDA34。
- RawCopy：16 位复制或 8 位查调色板。RawCopyMirrored：总按 16 位读，源每像素后退一个字（调色板图片也按 16 位读）。RawKeyed：16 位跳过等于填充色低字的像素，8 位跳过索引 1。RawKeyedMirrored：零扩展的像素与整个填充色对 0x004EEE94 比较（填充色对为 0 时 0 像素透明，否则无透明），注释原写“永不透明”已更正。
- 各绘制函数的返回值为寄存器残留值，无调用者依赖其内容（实现返回 0）。

## 0x00418310 Painter::BlitRawTranslucent — exact

- 透明度先减 1 再乘 4 作跳转表下标（改写全局 0x004E6D58，DrawPicture 结束时复位）；16 项分支：下标 L 的四项 (>>1、>>2、>>3、>>4，各与 0x004EEE98..0x004EEEA4 掩码）中 L 的位 3..0 为 1 的取源、为 0 的取目标；L=0 不写，L=15 复制源。源总按 16 位读，等于填充色低字的像素跳过。实现 Blend(dest, src, alpha−1) 同此（32 位读入后移位，掩码去掉相邻像素移入的位，与 16 位计算相同）。

## 0x00418880 Painter::BlitRawGradient — exact

- 颜色取来源首字（零扩展）；层级从 15 起，每行后计数加 1，超过 (高>>4) 时清零、层级减 1；每像素 Blend(目标, 颜色, 层级)（层级 0 不写，15 写颜色）；目标逐行以正跨度前进。

## 0x00418D10..0x00423380 RLE 核心与像素函数（31 个）— exact（与 swd32 同码）

- 范围：BlitCopy/Mirrored、BlitAdditive/Mirrored、BlitAlphaMap/Mirrored、BlitTint/Mirrored、BlitFadeDown、BlitEdgeMark/Mirrored、BlitTranslucent/Mirrored、BlitShadow/Mirrored、TintPixels、ShiftRed、ShiftGreenPairs、ShiftBluePairs、MixPixels、BlitScaledGlow/Mirrored、BlitSilhouette/Mirrored、BlitGray/Mirrored、GrayPixels、BlitSubtractive/Mirrored、BlitSmear/Mirrored。
- 汇编：reverse/analysis/scripts/asm_identity.py 把每个函数与其 swd32 配对函数逐条比对（跳转目标换成函数内偏移），31 个全部逐条相同，数据与被调函数地址在 52 个地址上一一对应、跨函数一致（swd3e 绘图全局块 0x004E6D54..0x004EF534 对 swd32 0x004CC2EC..0x004CDE24，波形表 0x004B16B8 对 0x004A06A8）；输出存于 reverse/evidence/pass1/painter_rle_identity.txt。
- 源码：src/draw/painter_rle.cpp 与 swd3-rebuild 中已对照 swd32 汇编验证（assembly_exact）的同名文件除命名空间与地址注释外逐行相同；src/draw/painter_pixels.cpp 另加 PaintRecord 记录钩子（只记录参数，不改像素）。同一份机器码对同一份源码的对照结论因此对 swd3e 成立；本次又抽读 BlitCopy（0x00418D10）全文与实现的 WalkRuns 参数（有符号跨度、字面段后一段视作透明、波形行偏移每行 +4 并回绕、结束时相位 +4 于 0x84 回绕）核对一致。
- 测试：正传遗留黄金结果 1300 个画面情景逐像素一致；波形表与 exe 比对。

## 0x00423630 Painter::SetZoom、0x00423670 Painter::SetZoomSheared、0x004236B0 起建表 — exact

- 两入口写锚点 0x004EC188/0x004ED9C4、倍数 0x004B1E7C/0x004B1E80（初值均 0x400，exe 字节 `00 04 00 00`）、斜切 0x004EEE74（SetZoom 写 0）后同跳 0x004236B0。
- 列表 0x004EAD6C、行表 0x004EC5A0 各 0x500 项：表 i 项 = i*0x400/倍数（idiv 截断）；仅在倍数与“已建值”0x004EEE78/0x004EEE7C 不同时重建，倍数为 0 时先写成 1。斜切表 0x004EDA4C 0x500 项：i*斜切/1024（cdq、and 3FFh、sar 10 的有符号截断）；斜切与 0x004EEE80 不同时：斜切非 0 才重建，已建值无论如何更新。实现 BuildZoomTables 同此。

## 0x00423830..0x004265B0 七个缩放绘制 — exact

- 公共遍历（实现 BlitZoomed）逐段核对 BlitZoomCopy 全文：镜像时列步 −1、锚点 X 取 宽−锚点；翻转时行步 −跨度、行号步 −1（旗标 0x20000000，行后 +1 再 −2）、斜切行号起点 = 高*纵倍/1024、y += 2*锚点Y − 高、锚点 Y 取 高−锚点；左 = x+锚X−锚X*横倍/1024，上 = y+锚Y−锚Y*纵倍/1024，右/下 = 宽(高)*倍/1024+左(上)−1；四边完全在裁剪外时返回；左 ≤ 裁剪左、右 ≥ 裁剪右或斜切非 0 时逐列检查（旗标 0x40000000；0x80000000 写而不读）。行窗口：Copy 为裁剪顶行 clipY 起 clipHeight 行（行表 0x004E6D64[clipY]、[clipHeight]），其余六个为 clipY+1 起 clipHeight−1 行（0x004E6D68、0x004E6D60）。斜切时行起点加 行号步*(斜切表[锚点缩放行]−斜切表[斜切行号])。跳过段（0xC000）按列表推进目标；字面段对列表值 < 段尾的每一项写像素 `literal[列表值−段起]`，段后前进 (段长+1)*2 字节；行后按行表推进源行（行表值等于当前行时重复本行）。
- 归一化寄存器后的整函数对比：Silhouette（0x00425B10）与 Copy 只差裁剪行窗口与像素操作（填 0x004ED9DC，全库无写入，实现 ZoomFill = 0）；Gray（0x00425FF0）与 SubtractTinted（0x004265B0）只差染色设置与像素操作；Tint（0x00424620，未优化形态，全文读过）与 AddTinted（0x00423D40）只差像素操作与栈槽；Translucent（0x00424E40）遍历同优化形态。
- 像素操作：Gray 为三通道右移后和 >>2 再左移相加；Tint 为 (目标&掩码)+步长，反掩码有位时取饱和值（染色 ≥0 为掩码、<0 为 0；步长 = 0x004EF308/0x004EF524/0x004EEED8 × 染色 0x004ED9A4/0x004EC180/0x004EC178）；AddTinted 先逐通道饱和相加再染色；SubtractTinted 为 目标通道 −（源通道+步长，溢出取饱和值），符号位置位时取 0；Translucent 按 0x004E6D58（DrawOptions::alpha）跳 15 项表（0x00425A8C、0x00425AC8 字节核对）：0 不写，1..14 按位取源或目标的 >>1..>>4 四项（掩码 0x004EEE98..0x004EEEA4），越过 14（含负）复制源。实现对应 Gray、TintPixel、Add+TintPixel、SubtractTintedSource、BlendFine（alphaFraction 为 battle-smooth 增强，deviations.md 第 99 行；原逻辑下为 0 即 Blend）。
- 返回值为寄存器残留，无调用者读取。偏差：deviations.md 第 316 行（表外下标、行表递减、画面缓冲外写入）。

## 0x00426C80 Painter::BlitPaletteCopy — exact

- 不缩放：上下左右 = y、y+高−1、x、x+宽−1，四边裁剪后返回；左 ≤ 裁剪左或右 ≥ 裁剪右时逐列检查。列窗口起点 = 行起点 + (裁剪左 − 首列)，终点 = 起点 + 宽−1（原有 BUG：按图宽而非裁剪宽）；行窗口 clipY+1 起 clipHeight−1 行，以列窗口位置判断。跳过段目标前进段长（不看镜像方向，原有 BUG）；字面段每字节经调色板 0x004EDA34 查色，段后紧接下一码；行后按行首码推进源并加行步。锚点、行表等写入不影响结果。偏差：deviations.md 第 317 行（空调色板、画面缓冲外写入）。

## 0x004187A0 Painter::BlendPairsWithColor — exact（第 268 行，前一批漏登）

- 对数 = 像素参数 >> 1；每对 = ((对>>1) & 0x004EEE98) + ((颜色对>>2) & 0x004EEE9C) + ((对>>3) & 0x004EEEA0) + ((对>>4) & 0x004EEEA4)，逐对写回。实现以 halfMaskPairs[0..3] 对应四个掩码（右移 1..4 位）；平滑层录制钩子不改结果。对数为 0 时原程序的 `sub …, 1; jnz` 会绕回循环 2^32 次，调用方（名牌阴影 8×长度+0x1C、FillBox 混色至少 1 对）不会传 0。
