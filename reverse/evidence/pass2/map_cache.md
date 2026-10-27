# 第二遍：地图快取

## 0x004291B0 MapCache::Check — exact

汇编：以读写打开 env.dat。模式 0：读末字节，为 0 → 关闭后直接取快取大小；非 0 → 末字节写 0 并清快取。模式 1：0x004271A0 取三个显示掩码，与 0x004AF70C/0E/10 的字比较，有不同 → 读 +0x1E 起 6 字节、把三掩码写进去、写回并清快取。清快取：24 个 %sdata\%d.cm 以读写打开、定位 0 后截断；%sdata\mcache.dat 写 24 项 {0xFFFFFFFF, 0, 0, 序号}（0x180 字节）并截断。最后返回 0x004296B0 的快取大小。
对照：平台替换（deviations 103）：会话标记与掩码记在 [game]（session_open、red_mask 等），判断与清快取的步骤一致（Units 0x18、TableBytes 0x180、Slot 默认值、CheckSessionMark 0、CheckColorMasks 1）；EH 注释改为外传地址 0x004A9510、0x004A951B、0x004A9526、0x004A9531。

## 0x00429520 MapCache::MarkClean — exact

汇编：以写打开 env.dat，定位到末字节写 0。对照：平台替换（deviations 103），session_open 置否；EH 注释改为 0x004A9540、0x004A954B。

## 0x004295F0 MapCache::MarkDirty — exact

汇编：同上写 1。对照：平台替换（deviations 103），session_open 置是；EH 注释改为 0x004A9560、0x004A956B。

## 0x004296B0 MapCache::CacheBytes — exact

汇编：24 个 %sdata\%d.cm 逐个以读打开，打开成功则累加文件大小，关闭；返回总和。
对照：一致（Units 0x18、kUnitFormat）；EH 注释改为 0x004A9580、0x004A958B。

## 0x00429BA0 MapCache::Pack — exact

汇编：输出缓冲 = 原长+0x20（0x0043DA70），字典 0x10000 字节（16384 个指针，14 位）清 0；0x0043DC40(输入, 长度, 输出, &输出长)；释放字典；返回输出。
对照：一致（LzoEncoder 14 位字典；清 0 的字典项与“无效项”效果相同）；输出缓冲越界见 deviations 389、59。

## 0x00429C00 MapCache::Unpack — exact

汇编：0x0043E500(输入, 长度, 输出, &输出长, 0)，返回状态。
对照：一致：源码返回写出的字节数，即原程序经指针传回的输出长度；状态值各调用者都不读（调用后立即覆盖 eax）；解压边界见 deviations 10。

## 0x00429C20 MapCache::GetCachedMap — exact

汇编：以 OPEN_ALWAYS 读写打开 %sdata\mcache.dat，失败 → 记「GetCachedMap ,mcache.dat can't open」返回 0。表为空：0x0042A180(0, 起点, 大小, 旧格式, &解压长)，槽 0 = {地图, 解压长, 0, 0}，其余 {−1, 0, 0, 序号}，写 0x180 字节并截断，返回 0x0042A670(0, 旧格式)。否则读入全表到 0x200 字节缓冲，项数 = 长>>4；各项年龄+1；找地图（途中累加已用字节）：命中 → 先 0x0042A670(单元, 旧格式) 再年龄清 0、写回 项数×16 字节（不截断）、返回。未命中：0x0042A620 取所需字节；有空槽则不“满”；当（所需+已用 > 快取上限 MB<<20 且项数≠0）或满：在非空项中取字节最小者，遇到年龄 > 快取 MB/4（有符号除，无符号比）的项立即取它并停；该槽置空（单元号写成槽号），截断其单元文件到 0x10，已用 −= 所取字节；被选槽与单元号跨轮保留。之后找第一个空槽，没有则记「GetCachedMap ,No free slot」；该槽 = {地图, 所需, 0, 槽号}；写回 项数×16 字节并截断；0x0042A180(槽, …)；返回 0x0042A670(槽, 旧格式)。EH unwind 0x004A95A0/AB/B3、handler 0x004A95BE。
对照：一致（kTableSlots 0x20、kKeptHeader 0x10、kAgeShare 4、日志字符串经 exe 字节核对）；读表越界与按项数走出缓冲见 deviations 345，单元号与字节数未初始化见 346；EH 注释改为外传地址。

## 0x0042A620 MapCache::ReadUnitBytes — exact

汇编：第一个参数（地图号）不用；大小为 0 → 0x0040C530("地圖快取製作", "這地圖是舊格式") 后返回（不写输出）；否则在像素文件 0x004D1DB0 定位 起点+大小+(旧格式 ? 0x10 : 8)，读一个 dword 到输出。
对照：一致（kUnpackedAt 0x10、kMsf3UnpackedAt 8、两段文字经 exe 字节核对）；不写输出见 deviations 346。

## 0x0042A670 MapCache::OpenUnit — exact

汇编：第二个参数（旧格式）不用；路径 %sdata\%d.cm；先 UnmapView 旧视图并清指针、关映射、关文件、路径缓冲 0x004C3254 清空；以 OPEN_ALWAYS 只读打开，失败 → 记「GetCacheMapUnit ,can't open %s」返回 0；建映射失败记「GetCacheMapUnit ,can't createmapping:%s ,」（多传的错误码格式里没有占位，不显示）后继续；映射视图，为空记「GetCacheMapUnit ,can't createview:%s ,」；路径存入 0x004C3254；返回视图。
对照：一致（三条日志经 exe 字节核对）；EH 注释改为 0x004A95F0/0x004A95F8。

## 0x0042A180 MapCache::DecompressMap — exact

汇编：大小为 0 → 0x0040C530("地圖快取製作", "這地圖是舊格式") 返回。以 OPEN_ALWAYS 读写打开 %sdata\%d.cm，失败 → 记「DecompressMap ,%s can't open」（再把 cFile 的错误缓冲交给 free）返回；定位单元文件 0，像素文件定位 起点+大小。旧格式：读 0x1A8 字节头（失败 → 关闭返回），总长 = 头+0x10 → 输出，块大小 = 头+0x14，两块缓冲 块大小+(块大小>>10)；块数 = (块大小+总长)/块大小；块表从头+0x1C 起每 8 字节一项：读 块长 字节（失败 → 记「DecompressMap ,read block fail when 」关闭返回），0x00429C00 解压，写出 min(块大小, 剩余)（16 位地图先转换其半数像素），剩余 −= 块大小，进度 = 步/块数+15，步 += 45。MSF3：读 0x400 字节头，总长 = 头+8 → 输出，块大小 = 头+4；总长为 0 结束；块表从头+0x10 起每项 {偏移, 长度}：定位 起点+偏移 读块、解压、16 位时转换产出的半数像素、写出产出字节，已写 += 产出，进度 = 已处理块×45/块数+15，直到已写 == 总长。最后关闭文件。
对照：一致（HeaderBytes 0x1A8、kMsf3HeaderBytes 0x400、kBlockSizeAt 0x14、kBlocksAt 0x1C、kBlockEntryBytes 8、kSlackShift 10、kProgressStart 15、kProgressSpan 45、TrueColorBits 16、日志经 exe 字节核对）；块表越界、块缓冲与除零见 deviations 343，打开失败的无效释放见 344；EH 注释改为 0x004A95D0/0x004A95D8。
