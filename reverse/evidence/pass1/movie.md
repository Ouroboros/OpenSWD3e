# 第一遍：影片（movie_bink）

## Movie::Init、Movie::Movie、Movie::Open、Movie::Close、Movie::Frame、Movie::Waiting（第 1374–1379 行）— exact（平台替换）

- 0x0049C440 Init：第二参数为 0 时 BinkSetSoundSystem(BinkOpenMiles, 第三参数)，否则 BinkOpenDirectSound；拷贝格式 0x00578618 = BinkDDSurfaceType(第一参数)。重建解码器自带声音输出、帧固定 RGB565，函数不做事（偏差行 0x0049C440）。
- 0x0049C480 构造：句柄（+0）与 +4 清零。
- 0x0049C490 Open：路径为空直接返回；BinkOpen(路径, 0x80000)，失败时 lstrcpyA(+0x84, BinkGetError())；+8 起 0x7C 字节清零后 BinkGetSummary；音量钳到 0..0x7F，BinkSetVolume(句柄, (音量 << 15)/0x7F)。源码以 MediaDecoder 打开，错误文字、音量换算相同；打开失败后的空句柄调用见偏差行 0x0049C490。
- 0x0049C520 Close：句柄非 0 时 BinkClose、清零、返回 1，否则 0。
- 0x0049C550 Frame：BinkDoFrame；BinkCopyToBuffer(句柄, 目标, 行距, 高, x, y, 0x00578618)；Frames（+8）> FrameNum（+0xC）（无符号）时 BinkNextFrame、结果 1，否则 −1；BinkService。源码 DecodeFrame、CopyTo(表面, 0, 0)（唯一调用处 0x0049C800 传 x = 0、y = 0x00578620，后者全程序只读不写，恒为 0）、同样的帧数比较。
- 0x0049C5E0 Waiting：句柄非 0 时 BinkWait != 0，否则 0。

## Game::PlayVideo、Game::StopVideo、Game::VideoFrame、Game::VideoPosition、Game::SetVideoMarks（第 1380–1384 行）— exact（平台替换）

- 0x0049C600 PlayVideo：分配 0x400 字节路径；安装类型（0x004E55A4）0x34 时路径 = 安装目录（0x004C2B94）+ "Video\\"，1..2 时先查光盘（0x004121C0，放弃时系统旗标 |= 4 并返回 0），再取光盘路径（0x004D1840）+ "swd3e\\Video\\" 并把首字节换成光驱；其余为空。光盘相关按用户要求删除，一律安装目录 + "Video\\"（偏差行 108）。剧本：命令字后 pc、游标各 +2，逐字节拷贝直到字 0x5125（"%Q"），再各 +2。new Movie（旧影片不关，偏差行 0x0049C600）存入 0x00578624；strstr(路径, "gap")（0x004C03BC，结果不用）；Open(路径, 0, 100, hWnd)；释放路径；系统旗标 |= 0x20；PrimarySize(&0x0057861C, 0)；返回 1。源码另写一条运行日志（偏差表 LogInfo 行）。
- 0x0049C7B0 StopVideo：Close、释放、0x00578624 清零；系统旗标 4 时 SendMessage(hWnd, WM_CLOSE)（源码 Destroy）；清 0x20。
- 0x0049C800 VideoFrame：无影片返回；Waiting 为 1 时 Sleep(0) 返回；取主表面（0x2711）指针 0x004EDA44 后随即解锁，Frame(指针, 行距 0x0057861C, 0x1E0, 0, 0x00578620)，结果 ≤ 0 时 StopVideo。源码写入帧表面后 ShowFrame（偏差行 0x0049C800 平台替换）；打不开的影片立即停止（偏差行 0x0049C800 空指针）。
- 0x0049C890 VideoPosition：0x00578624 为 0 时 −1；否则 chunk 0x0049C5C0（Movie::Position）：句柄为 0 → −1；FrameNum > Frames（无符号）→ −FrameNum，否则 FrameNum。函数表原把 0x0049C890 记为 Movie::Position，改为 Game::VideoPosition，Movie::Position 注释改为 chunk 0x0049C5C0。
- 0x0049C8B0 SetVideoMarks：有影片时 0x004C30E0、0x004C30E8 = 两参数，返回 1，否则 0。
