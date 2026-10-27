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
