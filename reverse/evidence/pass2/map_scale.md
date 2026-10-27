# 第二遍：地图深度比例（MapScale）

## 0x0042B1B0 MapScale 全局初始化 — exact

汇编：调用 0x0042B1C0 构造，再 atexit(0x0042B1E0)，后者以对象 0x004F3DC0 跳到 0x0042B250。
对照：对象由 Field 持有，构造与析构对应 MapScale::MapScale／~MapScale。

## 0x0042B1C0 MapScale::MapScale — exact

汇编：ecx = 0x004F3DC0，跳 0x0042B1F0：类别 +0 = 0，默认倍数 +0x610 = 0x400，行表 +4 = 0、个数 +8 = 0；256 次把 +0x610 写入 +0xC 的 dword 与 +0x40C 的 word；图片 +0x60C = 0。
对照：成员默认值（类别 None、defaultScale 0x400、行表空、图片空）加 unk00C 与 levels 填默认倍数，一致。

## 0x0042B250 MapScale::~MapScale — exact

汇编：jmp 0x0042B350。
对照：析构调用 Clear，一致。

## 0x0042B260 MapScale::SetScales — exact

汇编：类别 = 0；数据 +0 为 1 → 从 +0x10 复制 0x100 个 dword 到 +0xC，类别 1，返回 0（行表不释放）；为 2 → 释放行表，个数 = +4，分配 个数×4，从 +0x10 复制，类别 2，返回 0；其他返回 0。
对照：一致；读超出数据长度见 deviations 340。

## 0x0042B2E0 MapScale::SetPicture — exact

汇编：释放旧图片；分配 大小 字节（0x0043DA70 清零）；从数据复制 0x200 字节到级别表 +0x40C；从数据 +0x200 复制 大小−0x200 字节到图片开头；返回 1。
对照：图片缓冲为整个大小、末 0x200 字节为 0（分配清零），一致；数据短于 0x200 见 deviations 340。

## 0x0042B350 MapScale::Clear — exact

汇编：类别 0，默认倍数 0x400；释放行表并清指针与个数；256 次填 +0xC dword 与 +0x40C word；释放图片并清指针；返回 1。
对照：一致。

## 0x0042B3D0 MapScale::LevelAt — exact

汇编：类别 2 → y 有符号除以 16（向零取整）；类别 1 且有图片 → 0x00417880(图片, x, y, &y槽)，真 → movsx 级别表[输出]；其余返回 +0x610。
对照：0x00417880 只在写入输出值之后返回真，输出槽的初值 y 不会被读到；源码 y / TileSize 与 cdq/and/sar 一致；索引不小于 256 见 deviations 338。

## 0x0042B430 MapScale::Scale — exact

汇编：类别 1 → 0 ≤ 级别 < 0x100 时 movsx 级别表；类别 2 → 0 ≤ 级别 < 个数 时行表 dword；其余 +0x610。
对照：一致。

## 0x0043C8E0 MapScale::GetKind — exact

汇编：返回 [this+0]（同一段代码也作 Graphics 的设备读取）。对照：一致。
