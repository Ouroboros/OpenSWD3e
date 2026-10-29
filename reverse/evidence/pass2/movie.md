# 第二遍：影片（movie_bink）

## 0x0049C440 Movie::Init — exact

汇编：参数二为 0 → BinkSetSoundSystem(BinkOpenMiles, 参数三)，否则 BinkSetSoundSystem(BinkOpenDirectSound, 参数三)；0x00578618 = BinkDDSurfaceType(参数一)。
对照：Movie::Init 为空函数，声音输出与拷贝格式由解码器负责，属偏差 43（平台替换）。
差异：无。

## 0x0049C480 Movie::Movie — exact

汇编：句柄（+0）= 0、+4 = 0。
对照：Movie() = default（解码器未打开、错误文字空）。
差异：无。

## 0x0049C490 Movie::Open — exact

汇编：路径为空 → 返回 0。BinkOpen(路径, 0x80000) 存于 +0；失败 → lstrcpyA(+0x84, BinkGetError())。+8 起 0x7C 字节清零后 BinkGetSummary(句柄, +8)。音量 <0 → 0、>0x7F → 0x7F，BinkSetVolume(句柄, (音量 << 15) / 0x7F)（有符号除法）。返回 0。
对照：Movie::Open 一致（MaxVolume 0x7F、kBinkVolumeShift 15；全部调用处传入非空路径，返回值不用）；空句柄继续调用属偏差 42。
差异：无。

## 0x0049C520 Movie::Close — exact

汇编：句柄非 0 → BinkClose、句柄 = 0，返回 1；否则 0。
对照：Movie::Close 一致。
差异：无。

## 0x0049C550 Movie::Frame — exact

汇编：BinkDoFrame；BinkCopyToBuffer(句柄, 目标, 行距, 高, x, y, 0x00578618)；Frames（+8）> FrameNum（+0xC）（无符号）→ 结果 1、BinkNextFrame，否则 −1；BinkService；返回结果。
对照：Movie::Frame 一致（唯一调用处 x = 0、y = 0x00578620，后者全程序只读、恒为 0）；拷贝与服务属偏差 138（平台替换）。
差异：无。

## 0x0049C5E0 Movie::Waiting — exact

汇编：句柄非 0 → BinkWait ≠ 0；否则 0。
对照：Movie::Waiting 一致。
差异：无。

## 0x0049C600 Game::PlayVideo — diff → exact

汇编：分配 0x400 字节清零的路径；安装类型（0x004E55A4，有符号字节）≤0 → 无前缀；1..2 → 光盘检查 0x004121C0(0x004D1840, 3, &盘符) = 2 → 0x004D1404|=4、释放前返回 0（路径缓冲未释放），否则 0x004D1840 + "swd3e\Video\"、首字节换成盘符；0x34 → 0x004C2B94 + "Video\"；其余无前缀。剧本位置与 pc 各加 2，逐字节复制名字直到字 0x5125（「%Q」，扫描不受限），再各加 2。new 0x484 字节 Movie → 0x00578624（旧的不关闭）；strstr(路径, "gap")（结果不用）；Open(路径, 0, 100, hWnd)；释放路径；0x004D1404|=0x20；PrimarySize(&0x0057861C, 0)；返回 1。
对照：Game::PlayVideo 一致（kVideoVolume 0x64、NameEnd 0x5125、VideoPlaying 0x20）；路径前缀与光盘检查属偏差 103，旧影片先关闭属偏差 452，影片名日志属偏差 139。
差异：读影片名时以剧本缓冲区结尾为界（cursor < script.size()），该保护未在偏差表登记。
修正：偏差表第 132 行（剧本扫描越过缓冲区）的地址栏加入 0x0049C600；代码不变。复核：完整函数重新对照一致。

## 0x0049C7B0 Game::StopVideo — diff → exact

汇编：Close、释放影片对象、0x00578624 = 0；0x004D1404 位 2 → SendMessageA(hWnd, 0x10, 0, 0)；0x004D1404 清 0x20。窗口过程（0x0040A380）不处理 0x10，交 DefWindowProcA 销毁窗口，WM_DESTROY（case 2）释放游戏并 PostQuitMessage。
对照：Game::StopVideo 关闭影片、videoLoaded = false，QuitRequested → Destroy()（WM_DESTROY 的处理），清 VideoPlaying，结果相同。
差异：WM_CLOSE 改为直接调用 Destroy 属平台替换，偏差表未登记。
修正：偏差表末尾新增 0x0049C7B0 平台替换一行；代码不变。复核：完整函数重新对照一致。

## 0x0049C800 Game::VideoFrame — diff → exact

汇编：影片为空 → 返回。Waiting=1 → Sleep(0)，返回。锁定 0x2711 表面取指针 → 0x004EDA44，随即解锁；Frame(0x004EDA44, 0x0057861C, 0x1E0, 0, 0x00578620)；结果 >0 → 返回，否则 StopVideo。
对照：Game::VideoFrame 一致；打不开的影片立即停止属偏差 453，写帧表面、呈现与 Sleep 属偏差 138。
差异：帧表面（offscreen）尚未建立时直接返回的保护未登记。
修正：偏差表第 138 行重建栏改为「等待或帧表面尚未建立时直接返回」；代码不变。复核：完整函数重新对照一致。

## 0x0049C890 Game::VideoPosition — exact

汇编：影片为空 → −1；否则（0x0049C5C0 片段）句柄为空 → −1；FrameNum（+0xC）> Frames（+8）（无符号）→ −FrameNum，否则 FrameNum。
对照：Game::VideoPosition → Movie::Position 一致。
差异：无。

## 0x0049C8B0 Game::SetVideoMarks — exact

汇编：影片为空 → 0；否则 0x004C30E0 = 参数一、0x004C30E8 = 参数二，返回 1。
对照：Game::SetVideoMarks 一致（两值全程序无人读取）。
差异：无。
