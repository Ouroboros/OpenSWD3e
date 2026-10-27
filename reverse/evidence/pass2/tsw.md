# 第二遍：图片档（TswArchive）

## 0x004364C0 TswArchive::SetCacheLimit — exact

汇编：0x004B705C = 参数。对照：一致；初值 0x600000 与 exe 字节一致。

## 0x004364D0 TswArchive::SetGameDirectory — exact

汇编：lstrcmpA(参数, 0x004E6D40) 不等时 0x004F4BB4 = 参数 + "\"。0x004E6D40 在未初始化区且全程序无写入，即空串。
对照：一致（判空）。

## 0x00436510 TswArchive::OpenArchives — exact

汇编：以空串起的 0x400 字节路径缓冲，依次拼「目录 + all_char.tsw／all_item.tsw／all_magic.tsw／all_sys.tsw／all_map1.tsw／all_map2.tsw」，0x00438020 打开，句柄存 0x004F4B84..0x004F4B98；每档 3000 条：从 0x1C 起每 0x2C 字节定位、ReadFile 0x2C 字节（不查结果），取记录 +0x18 的偏移依次写入 0x004F82F0 起（偏移表 0x004F82EC 的第 1 项起）；0x004F5408 = 1。
对照：一致；档名与 exe 串一致；读不到时沿用上一条见 deviations 325。

## 0x004366B0 TswArchive::CloseAll — exact

汇编：六个句柄非 −1 且非 0 时 CloseHandle，置 −1；0x004F4FB4 同样处理（全程序无打开者）；0x004F5408 = 0；0x00436AC0(−1)；0x004386B0。返回值（残值）唯一调用者 0x0042917C 不读。
对照：一致；源码另清保留列表属 deviations 82 的增强。

## 0x00436720 TswArchive::GetPicture — exact

汇编：0x00436950 命中 → 返回 0x004F4B80；否则缓存字节 0x004F4B7C ≥ 上限（无符号）时 0x004369F0；0x00436810 失败 → 0x004F4B80 = 0；返回 0x004F4B80。
对照：一致。

## 0x00436770 TswArchive::LoadPictureDirect — exact

汇编：0x00438320(句柄[(id&0xFFFF)/3000], id&0xFFFF, 帧&0xFFFF, 0x004F4BA0)（结果不查），0x00401780 转 16 位，复制 0x14 字节图片头给调用者。
对照：一致；档号越界见 deviations 327，载入失败返回上一次的块见 329，交给调用方的副本与保留见 82。

## 0x004367D0 TswArchive::Flush — exact

汇编：0x00436AC0(−1)，跳 0x004386B0。对照：一致。

## 0x004367E0 TswArchive::FindPicture — exact

汇编：0x004F4B80 = 0，0x00436950(id, 帧)，返回 0x004F4B80。对照：一致。

## 0x00436810 TswArchive::CacheLoad — exact

汇编：链号 = id&0xF（id 低字为 0xFFFF 时取帧&0xF），链计数（字）+1；新 0x20 字节节点插在链头哨兵之后：+0 id 字、+2 帧字、+4 next、+0x1E = 0x00438010()；id 低字 0xFFFF → 0x0040B060(帧, 节点+8)（结果不查）；否则 0x004380F0(句柄[(id&0xFFFF)/3000], id&0xFFFF, 帧&0xFFFF, 节点+8)，失败返回 0（节点与计数留下），成功 0x00401780 转 16 位；缓存字节 += 图片 +0x10 的大小，0x004F4B80 = 节点+8，返回 1。
对照：一致；源码对地图图片提供者判空不可达（Field 总会设置）；档号越界见 deviations 327。

## 0x00436950 TswArchive::CacheFind — exact

汇编：0x004F4B80 = 0；在链中找 dword (帧<<16|id) 相同的节点，找不到返回 0；不是第一个时移到链首；0x004F4B80 = 节点+8 并返回。
对照：一致。

## 0x004369F0 TswArchive::CacheEvict — exact

汇编：找计数最大的链（相等取先者，无符号比较）；缓存字节 < 上限则返回；否则反复取链尾节点：字节 −= 大小，释放数据、调色板与节点，前一个的 next = 0，计数 −1，为 0 则返回，字节仍 ≥ 上限时继续。
对照：一致；上限由 0x00428100 夹在 0x400000..0x1000000，淘汰时被选中的链计数 ≥1 且与链长一致，源码的空链判断不可达。

## 0x00436AC0 TswArchive::CacheClear — exact

汇编：参数不用；16 条链逐个节点：缓存字节 −= 大小，释放数据、调色板与节点，链计数 −1；链头清 0。
对照：一致。

## 0x00438010 TswArchive::CacheTag — exact

汇编：返回字节 0x004F5404。对照：一致。

## 0x00438020 TswArchive::OpenFile — exact

汇编：CreateFileA(名, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0x80, 0)，失败（−1）→ 0，否则句柄。
对照：一致（共享方式见 deviations 104）。

## 0x00438050 TswArchive::ReadHeader — exact

汇编：ReadFile 0x2E0C 字节到 0x004F540C（不查结果）；+4 的字 ≠ 0xABCD → 0；格式 = +8 的字写 info+0x10；格式为 8 时 +0xC 起 0x200 字节复制到共用调色板 0x00509C34，info+4 指向它，帧表从 +0x20C 起，否则从 +0xC 起；复制第 帧号 个 0x24 字节记录，返回 1。
对照：一致；帧号越过缓冲区见 deviations 328。

## 0x004380F0 TswArchive::LoadPicture — exact

汇编：0x18 字节帧信息清零；目录已读（0x004F5408=1）时 offset = 0x004F82EC 表[(id/3000)×3000 + id%3000]（即完整编号，表首项兼作缓存载入的缓冲容量），否则定位到 id×0x2C−0x10 读 0x2C 字节，offset 取记录 +0x18（指令字节确认：缓冲在 E−0x2C，offset 在 E−0x14），再定位到 offset；0x00438050 失败 → 0；0x00438220(句柄, offset, 帧信息, 记录) 失败 → 0；输出：+0 像素、+4 帧信息+8（恒为 0）、+8 调色板、+0xC 宽、+0xE 高、+0x10 长度，返回 1。
对照：一致（DirectoryOffset 的表首项与容量、读不到时的未初始化值见 deviations 326、327；+4 恒 0、长度即像素块大小）。

## 0x00438220 TswArchive::ReadFrame — diff → exact

汇编：定位到 base+记录.offset；压缩长度 > 容量 0x004F82EC（无符号）时释放旧缓冲 0x00509E78、按压缩长度新分配、容量 = 压缩长度；ReadFile 压缩长度字节（缓冲为空也调用）；按记录长度分配输出块；缓冲或输出块为空 → 0；0x0043E500 解压，写出长度 ≠ 记录长度 → 释放缓冲并清 0（容量保留），返回 0；否则帧信息 +0xC = 长度，记录 +0xC 非 0 时 +8 = 0，+0x12/+0x14 = 记录宽高，+0 = 输出块，返回 1。
对照：流程一致，缓冲释放后容量保留、之后不超过容量的帧都因缓冲为空而失败也一致。
差异：缓冲为空与解压长度不符两条失败路径上原程序不释放已分配的输出块，源码由容器释放，此前未登记。修正：补登记 deviations（0x00438220、0x00438450 内存泄漏），源码不改。复核：完整函数重新对照一致。

## 0x00438320 TswArchive::LoadPictureB — exact

汇编：与 0x004380F0 逐条相同（同一目录表），只把读帧换成 0x00438450。
对照：一致。

## 0x00438450 TswArchive::ReadFrame（直接载入缓冲） — diff → exact

汇编：与 0x00438220 相同，缓冲为 0x00509E74、容量为 0x00509C30（目录表之后的 dword）；解压长度不符时先释放输出块再释放缓冲。
对照：一致（源码同一函数传入 packedB、capacityB）。
差异：缓冲为空时输出块不释放，未登记。修正：同上补登记，源码不改。复核：完整函数重新对照一致。

## 0x004386B0 TswArchive::FreeBuffers — exact

汇编：两个压缩缓冲 0x00509E78、0x00509E74 非空时释放并清 0；容量 0x004F82EC（目录表首项）与 0x00509C30 清 0。
对照：一致。
