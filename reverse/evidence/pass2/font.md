# 第二遍：文字字体（TextFont）

## 0x00439BA0 TextFont::TextFont — exact

汇编：+0x18、+4、+0xFC0、+0xFCC 清 0，+0xFD0/+0xFD4（字形宽高）= 16；0x0043A120(0xFFFE)；0x0043A070(GetSystemMetrics(0), GetSystemMetrics(1))。
对照：一致；目标取桌面分辨率改为 640×480 见 deviations 314。

## 0x00439BF0 TextFont::~TextFont — exact

汇编：0x00439ED0、0x00439F90；行表 +0x14 非空时释放并清 0。对照：一致。

## 0x00439C20 TextFont::Init — exact

汇编：+0 = 图形对象；0x0043C970 取后备缓冲 pitch 与高；0x0043A070(pitch/2（向零取整）, 高)；0x00439E80(16, 16)；0x00439FC0(0x004B0BF4「細明體」, +0xFD0)；0x0043A110(+0xFD0)；0x0043A120(0xFFFE)；0x0043A130(0)；+0x20 起 1000 个 dword（字码表）清 0，+0xFC4 = 0，返回 0。
对照：一致（字面 Big5 字节 B2 D3 A9 FA C5 E9 核对）。

## 0x00439CB0 TextFont::Shutdown — exact

汇编：字体句柄 +0xFCC 非空时 DeleteObject 并清 0；0x00439D80、0x00439ED0、0x00439F90；行表释放并清 0。
对照：一致（字体句柄换为字体名，见 deviations 38）。

## 0x00439D00 TextFont::CreateGlyphSurface — exact

汇编：0x00439D80；0x0043C620 建离屏面写 +4；0x0043C6C0 填 0；GetSurfaceDesc 取高写 +0xFDC、lPitch/2 写 +0xFD8。
对照：一致（平台面以宽为每行像素数）。

## 0x00439D80 TextFont::ReleaseGlyphSurface — exact

汇编：+4 非空时 Release 并清 0。对照：一致。

## 0x00439DA0 TextFont::RenderString — exact

汇编：面丢失（0x887601C2）时 Restore；填 0；GetDC 成功时选入字体、背景黑色不透明、文字色，先在 (0,0) 输出两个空格再输出字串（坐标参数不用），恢复字体；ReleaseDC。
对照：一致（两个空格在黑底上不留痕迹；GDI 换为字库见 deviations 38）。

## 0x00439E80 TextFont::SetGlyphSize — exact

汇编：0x00439ED0；0x00439D00(0x40, 0x40)；按 宽×高×2 分配写 +0x18，成功时 +0xFD0 = 宽、+0xFD4 = 高、0x0043A110(宽)。
对照：一致；分配失败（尺寸 ≤0）不更新宽高：不可达，调用方只传 16 与 SetFont 的字号（12/16/20）。

## 0x00439ED0 TextFont::FreeGlyphPixels — exact

汇编：+0x18 非空时释放并清 0。对照：一致。

## 0x00439EF0 TextFont::AllocGlyphCache — exact

汇编：0x00439F90；每行字节 +0x1C = 宽/8（向零取整），余数非 0 时加 1；按 高×每行字节×2000 分配写 +0xFC0，成功时字码表清 0、+0xFC4 = 0、整块清 0。
对照：一致（分配失败不可达，同上）。

## 0x00439F90 TextFont::FreeGlyphCache — exact

汇编：+0xFC0 非空时释放并清 0。对照：一致。

## 0x00439FC0 TextFont::SetFont — exact

汇编：字号 >64 时取 64；旧字体 DeleteObject；LOGFONT 高 = −字号、粗细 400、字符集 0x88、字体名复制，CreateFontIndirectA 写 +0xFCC；0x00439E80(字号, 字号)；0x00439EF0。
对照：一致（GDI 字体换为字库，见 deviations 38）。

## 0x0043A070 TextFont::SetTarget — exact

汇编：释放旧行表；按 高×4 分配写 +0x14，失败返回；+8 = 宽、+0xC = 高、+0x10 = 宽×高×2；行表[i] = i×宽；0x0043A0E0(0, 0, 宽, 高)。
对照：一致；分配失败不可达（调用方传 480、后备缓冲高与对话面的行数，都为正）。

## 0x0043A0E0 TextFont::SetClip — exact

汇编：+0xFE8/+0xFEC/+0xFF0/+0xFF4 = x、y、宽、高。对照：一致。

## 0x0043A110 TextFont::SetAdvance — exact

汇编：+0xFE0 = 参数。对照：一致。

## 0x0043A120 TextFont::SetBackground — exact

汇编：+0xFE6（字）= 参数。对照：一致。

## 0x0043A130 TextFont::SetShadow — exact

汇编：+0xFE4（字）= 参数。对照：一致。

## 0x0043A140 TextFont::DrawGlyph — exact

汇编：裁剪 左 = +0xFE8、上 = +0xFEC、右 = 左+0xFF0、下 = 上+0xFF4；字形号 > +0xFC4（有符号）→ 返回；位图 = +0xFC0 + 每行字节×高×字形号；样式 0x100 → 每行颜色 −1，否则 0x80 → +1；逐行：行在 [上, 下) 时取行表[y]+x，每字节 8 位，第 k 位的列 p 满足 p ≥ 左 且 p+1 < 右 才写颜色低 16 位；行末颜色加步进，位图前进一行。
对照：一致（最右一列不画；字形号等于计数时照画，DrawText 中计数最多 1998，不越出缓存）；行表之外见 deviations 35。

## 0x0043A380 TextFont::DrawGlyphBold — exact

汇编：同 0x0043A140，每位以 (色<<16)|色 写一对像素，条件 p ≥ 左 且 p+2 < 右；颜色每行重组。
对照：一致。

## 0x0043A5B0 TextFont::DrawGlyphShadow — exact

汇编：行条件 y ≥ 上 且 y+1 < 下；每位条件 p ≥ 左 且 p+1 < 右，写 p = 颜色，p+目标宽（+8）起一对 = 阴影（+0xFE4 成对）。
对照：一致。

## 0x0043A840 TextFont::DrawGlyphBoldShadow — exact

汇编：行条件同上；每位条件 p ≥ 左 且 p+2 < 右，写 p 起一对 = 颜色，p+目标宽+1 起一对 = 阴影（8 位的偏移与阈值逐条核对）。
对照：一致。

## 0x0043AAF0 TextFont::DrawGlyphOutline — exact

汇编：行条件 y > 上 且 y+1 < 下；每位条件 p ≥ 左−1 且 p+1 < 右，写 p−宽 = 阴影、p−1 起一对、p+宽−1 起一对、p+宽+1 = 阴影；x 每行复位；描边画完后以已按全部行累加的颜色调 0x0043A140。
对照：一致。

## 0x0043AED0 TextFont::DrawGlyphBoldOutline — exact

汇编：行条件同上；每位条件 p ≥ 左−1 且 p+2 < 右，写 p−宽、p−1、p+2 = 阴影，p+宽−1 起一对、p+宽+1 起一对 = 阴影（以 shl/add 组成，与 or 相同）；之后以累加后的颜色调 0x0043A380。
对照：一致。

## 0x0043B300 TextFont::CaptureGlyphSurface — exact

汇编：锁定字形面（0x00417740），逐行复制 宽×2 字节：源偏移每行加 +0xFD8，目标 +0x18 每行加宽；解锁（0x00417790）。
对照：一致。

## 0x0043B390 TextFont::RasterizeGlyph — exact

汇编：0x00439DA0(字体, 0, 0, 字串, 0xFFFFFF)；0x0043B300；逐行逐字节，位从 0x80 起右移，列 = 字节×8+位，列 ≥ 宽即停；像素非 0 时把该位或入位图。
对照：一致（字串为 +0xFC8 的当前字符，第二字节为 0 时长 1，否则后随的填充字节为 0、长 2）。

## 0x0043B440 TextFont::FindGlyph — exact

汇编：上界 = 计数−1，为负 → −1；中 = (上+下)>>1（算术）；字码 < 表值（无符号）→ 上 = 中−1；相等 → 中；否则下 = 中+1；下 > 上 → −1。
对照：一致。

## 0x0043B480 TextFont::InsertGlyph — exact

汇编：计数 ≤0 → 表[0] = 字码，返回 0（不清位图）；否则找第一个 字码 ≤ 表值（无符号）的位置，找不到为计数；从计数到位置+1 逐个后移字码与位图；表[位置] = 字码，位图清 0，返回位置。
对照：一致。

## 0x0043B590 TextFont::DrawText — diff → exact

汇编：长度 = strlen(字串)，末下标 = 长度−1，<0 → 返回。逐字节：首字节写 +0xFC8，≥0x80 时取下一字节写 +0xFC9、下标加 1、半宽 = 0，否则 +0xFC9 = 0、半宽 = 前进量>>1；0x0043B440 查字码；未找到时 0x0043B480 插入并 0x0043B390 栅格化。背景 +0xFE6 ≠ 0xFFFE 时：下标 = 末下标 → 0x0043B960(x+偏移, y, 字宽−半宽+2, 字高, 背景)，否则宽取前进量；样式按 1、2、4、8、0x10 依次分派到六种字形绘制之一（无匹配则不画）；偏移 += 前进量−半宽；新插入的字形计数加 1，≥ 0x7CF 时计数减 1 并清该格位图。
对照：分派、背景、偏移与缓存规则一致。
差异：原程序按 strlen 停在第一个 0 字节，源码按 string_view 全长绘制，调用方传入含结尾 0 的缓冲时会多画字。修正：入口处截到第一个 0 字节。复核：完整函数重新对照一致。

## 0x0043B960 TextFont::FillRect — exact

汇编：右 = x+宽、下 = y+高（未截断）；x<0 → 0；右 ≥ 目标宽 → 目标宽；y<0 → 0；下 ≥ 目标高 → 目标高；左 < 右 时首行逐像素写色，行字节 = (右−左)×2；其后每行从首行复制该字节数。
对照：一致。
