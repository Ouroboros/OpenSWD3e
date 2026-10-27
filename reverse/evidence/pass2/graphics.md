# 第二遍：图形设备（Graphics）

DirectDraw 部分整体为平台替换（deviations 315）；以下逐函数核对出口、失败文字与需保持原计算的记录量（跨度、高度、显存计数、格式分类、表面编号）。

## 0x0043C020 Graphics::Graphics — exact

汇编：+0、+4、+8 清 0。对照：一致（设备与表面初始为空）。

## 0x0043C030 Graphics::Init — exact

汇编：+0x84 = 窗口，+0x38/+0x3C = 宽高；0x0043C0D0 ≠1 → 0；模式 = 0x4E21 时协作级别 8，否则 0x13，0x0043C140 ≠1 → 0；0x0043C190(窗口, 宽, 高, 位深) ≠1 → 0；0x0043C290 ≠1 → 0；0x0043C8F0；返回 1。
对照：顺序、协作级别与各出口一致。

## 0x0043C0D0 Graphics::CreateDevice — exact

汇编：DirectDrawCreate 失败 → 0x004360C0("DirectDraw Create Error!!") 返回 0；QueryInterface 取 IDirectDraw4 失败 → 0；释放旧接口，返回 1。对照：平台替换，总是成功。

## 0x0043C140 Graphics::SetCooperativeLevel — exact

汇编：SetCooperativeLevel(窗口, 级别)，+0x34 = 级别；失败 → "DirectDraw SetCooper Error!!"、释放设备、返回 0；否则 1。对照：记录级别一致，平台调用总是成功。

## 0x0043C190 Graphics::SetDisplayMode — exact

汇编：SetDisplayMode(宽, 高, 位深) 失败 → "DirectDraw SetDisplayMode Error!!"、释放设备、返回 0；否则 1。
对照：不改桌面显示模式；只接受 16 位，其他位深按同样的错误文字失败（deviations 315）。

## 0x0043C1E0 Graphics::CreateClipper — exact

汇编：CreateClipper、SetHWnd、主表面 SetClipper，各失败时对应文字（"DirectDraw CreateClipper Error!!"、"DirectDraw Clipper SetHwnd error!!"、"DirectDraw SetClipper Error!!"）并返回 0。对照：平台替换，窗口裁剪由显示层负责。

## 0x0043C290 Graphics::CreateSurfaces — exact

汇编：0x0043C880；协作级别 8（窗口）时建主表面、0x0043C1E0，宽高改为桌面大小，0x0043C620 建同大小后台缓冲写 +8，记录主表面与后台缓冲的跨度/高度到 +0x54/+0x58 与 +0x5C/+0x60；全屏时建带后台缓冲的翻页链，失败 → "DirectDraw Create Primary Surface Error!!"，跨度与高度两组都记主表面。
对照：记录量一致；桌面大小改为逻辑 640×480（deviations 315）。

## 0x0043C510 Graphics::ReleaseSurfaces — exact

汇编：设备为空 → 0；依次释放 +0xC、+8、+0x10、+0x14 并清 0，恢复显示模式，返回 1。对照：一致（平台面随之释放）。

## 0x0043C590 Graphics::ReleaseDevice — exact

汇编：设备非空时 Release 并返回其计数。对照：平台替换。

## 0x0043C5A0 Graphics::Present — exact

汇编：+0x34 bit0（全屏）时主表面丢失（0x887601C2）则 Restore，等垂直同步后 Flip，返回 1；否则 Blt 后台缓冲到主表面，丢失时 Restore 重试。
对照：窗口模式先复制后台缓冲，再交显示层呈现（deviations 315）。

## 0x0043C620 Graphics::CreateOffscreen — exact

汇编：建 系统内存离屏面（标志 7、caps 0x2840，宽 = 参数 1、高 = 参数 2），失败 → 0；+0x28 += (位深>>3)×宽×高，返回表面。
对照：显存计数一致；平台面总是建成。

## 0x0043C6C0 FillSurfaceColor — exact

汇编：0x0043C740 取设备色后颜色填充 Blt。对照：平台替换。

## 0x0043C700 Graphics::ClearSurface — exact

汇编：以 +0x88 的清屏色做颜色填充 Blt。对照：一致。

## 0x0043C740 MatchSurfaceColor — exact

汇编：GDI 在 (0,0) 写入该色、锁定表面读回设备色、恢复原像素。对照：平台替换（16 位 565 面上直接换算）。

## 0x0043C840 Graphics::BitDepth — exact

汇编：GetDisplayMode 取 dwRGBBitCount。对照：一致（16）。

## 0x0043C880 Graphics::QueryVideoMemory — exact

汇编：GetAvailableVidMem(caps 0x1000, &+0x2C, &+0x30)，返回结果 ≥0。对照：平台替换（总量固定，剩余 = 总量 − 已用）。

## 0x0043C8B0 Graphics::GetSurface — exact

汇编：0x2711 → +4、0x2712 → +8、0x2713 → +0xC，其他 → 0。对照：一致。

## 0x0043C8F0 Graphics::DetectPixelFormat — exact

汇编：主表面 GetPixelFormat；+0x48 = 0；绿掩码 = 0x7E0 → 2；蓝掩码 = 0x3F → 1；红掩码 = 0xFC00 → 3（后者覆盖前者）；返回绿掩码。对照：一致。

## 0x0043C950 Graphics::PrimarySize — exact

汇编：两个输出各自非空时写 +0x54、+0x58。对照：一致。

## 0x0043C970 Graphics::BackSize — exact

汇编：写 +0x5C、+0x60（不判空），返回值为高。对照：一致。
