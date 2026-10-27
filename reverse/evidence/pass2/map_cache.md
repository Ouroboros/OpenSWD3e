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
