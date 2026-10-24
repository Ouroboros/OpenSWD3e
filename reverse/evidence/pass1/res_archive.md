# 第一遍：图片档（TswArchive）、动作档（ActArchive）与相邻 hash 函数

## hash 配对一组（第 400–470 行中的 40 个）— exact

- asm_identity.py 结果见 res_identity.txt：0x004360C0 ShowDeviceError、0x004367E0 FindPicture、0x00436B50 ActArchive::OpenFile、0x00437D10 ActArchive::CacheClear、0x00437FC0 IndexClear、0x00438020 TswArchive::OpenFile、0x00438560 PictureHitTest、0x004386B0 FreeBuffers、Projector 一组（0x00438700..0x00439890）、0x00438A30 ShiftPicture、0x00438E10 LineStep、TextFont 一组（0x00439BA0..0x0043A140）与 swd32 对应函数逐条相同（98 个地址一一对应）。
- 源码函数体与 swd3-rebuild 中已为 assembly_exact 的版本逐行相同，两处例外另对照汇编直接核对：CacheClear 只差 `entry.size` 的转换写法（同为 u32 减法）；FreeBuffers 外传版本另把 0x004F82EC（offsets[0]，缓存载入缓冲的容量）与 0x00509C30（capacityB）清零，正是汇编 0x004386E3／0x004386E9 写的两个全局。

## TswArchive — exact

- 0x004364C0 SetCacheLimit：0x004B705C = 参数。0x004364D0 SetGameDirectory：与空串（0x004E6D40，只读、无写入者）比较不同时复制并接 "\"。
- 0x00436510 OpenArchives：六个档名（all_char/item/magic/sys/map1/map2.tsw）接在目录后由 0x00438020 打开；每档 3000 条，从 0x1C 起每 0x2C 字节读一条，取 +0x18 的 offset 依次写入 0x004F82F0（offsets[1] 起）；读不到时沿用栈上旧值（第 307 行偏差）；0x004F5408 = 1。
- 0x004366B0 CloseAll：六个句柄非 −1 且非 0 时关闭并置 −1；另一个从未打开的句柄 0x004F4FB4 同样处理；目录标记清零，CacheClear、FreeBuffers。重建另清空保留列表（LoadPictureKept 的重建设施）。
- 0x00436720 GetPicture：CacheFind 未中时 已用字节 ≥ 上限（无符号）则 CacheEvict，CacheLoad 失败时当前图清空；返回 0x004F4B80。
- 0x00436770 LoadPictureDirect：按 id/3000 取句柄，LoadPictureB 写入静态 0x004F4BA0（不检查结果），ConvertTo16，复制 0x14 字节返回（第 309、311 行偏差）。0x004367D0 Flush：CacheClear、FreeBuffers。
- 0x00436810 CacheLoad：链 = id 低 4 位（id 0xFFFF 时取 frame 低 4 位），计数 +1，0x20 字节节点清零后挂到链头，记 id、frame、标记（0x00438010）；地图图片（0xFFFF）经 0x0040B060 不判失败，其余 LoadPicture 失败时返回 0（节点留在链上），成功后 ConvertTo16；已用字节 += 大小，当前图 = 节点 +8，返回 1。
- 0x00436950 CacheFind：当前图清空，按 (frame<<16)|id 查链，找到时移到链头并设当前图。0x004369F0 CacheEvict：最长链（相同取前者，无符号）；已用 ≥ 上限时从链尾逐个释放并计数减一，减到 0 或低于上限为止。0x00436AC0 CacheClear：16 条链全部释放、减已用字节与计数。
- 0x00438010 CacheTag：返回 0x004F5404（无写入者）。
- 0x00438050 ReadHeader：读 0x2E0C 字节到 0x004F540C，+4 字不是 0xABCD 时返回 0；格式（+8）写信息 +0x10，格式 8 时 0x200 字节调色板复制到 0x00509C34 并记入信息，帧表起点后移 0x200；复制第 frame 条 0x24 字节帧记录；返回 1（越界帧号第 309 行一组之外由重建读 0）。
- 0x004380F0 LoadPicture：目录已读时 offset = offsets[id]（id 即 档号×3000+条号），否则 Seek(0x2C×id−0x10) 读一条；Seek(offset)，ReadHeader、ReadFrame（0x00438220），成功时复制数据、调色板、宽高到图片；失败时图片不动（CacheLoad 留下的空节点大小为 0）。0x00438320 LoadPictureB 与之逐条相同，只调用 0x00438450。
- 0x00438220／0x00438450 ReadFrame：Seek(基址+帧 offset)；压缩长度 > 容量（无符号）时换缓冲并记容量；读入；按大小分配输出，任一缓冲为空时返回 0；解压（0x0043E500）长度不等于大小时释放压缩缓冲（容量保留）返回 0（0x00438450 另释放输出块，重建由容器释放，行为相同）；成功时 数据、大小、宽（+0x20）、高（+0x22），帧记录 +0xC 非 0 时信息 +8 清零（重建的 Picture 无此字段，无读者）。

## ActArchive — exact

- 0x00436B40 SetCacheLimit：0x004B70C0。0x00437E00 SetUseCache：0x004F516C。0x00437E10 SetEffectEnabled：0x004F5400。
- 0x00436B80 Extract：子号 ≥ 个数（有符号）返回 0；offset 为 0 返回 0；否则 条目+offset（不是最后一个子号时原程序再向后扫描非 0 offset 而不用其结果，新增偏差行）。
- 0x00436BD0 LoadEntry：IndexLookup，offset 为 0 时 IndexInsert，按长度分配，写出长度，Seek(offset) 读入，读取计数 +2，返回块。
- 0x00436C80 Run：hold 为 1 且帧非 0、或 id 为 0 时返回 1；方向（保留 0x80000000）、动作、id（保留 0x80000003）改变时重置 30 余个字段并置重启标记 0x004F5160；useCache 为 1 时 Get 否则 LoadDirect（子号 = 动作+方向），取不到返回 0；重启标记清零；delay 非 0 时减一返回 1；actionCode 清零后逐字执行：DE／VO／2O 按 PL 轮次（起始轮结束、结束轮减计数或永远、超过起始轮结束、之前清 view 与 target 继续），无 PL 时 DE（hold 时退回）、VO（hold 时退回否则回 0）、2O（delay 为 0 时置 waiting，退回）；其余指令先 delay = frameDelay（speed 负时取低 15 位），YX、HW、GC、RC、BC 读第二个值不前进；CS 在 +0x94 为 1 或 0x004F5400 为 1 时置 0x80 并调用 0x00423630(x, y, 有符号两值)，再写 0x004F5120／0x004F517C／0x004F517E；AP 轮次 +1；PL 只在计数无 0x8000 时写入。偏差第 29 行改为汇编出处。重建的 Peek（平滑层预读）以 peeking 跳过 CS 的副作用，正常执行与汇编相同。
- 0x004377B0 Get：首次打开六个 .act（安装目录 0x004C2B94 + 档名，0x00436B50）并置 0x004F5170；CacheFind 为 1 时返回当前；已用 ≥ 上限（有符号）时 CacheEvict；CacheLoad 成功返回当前，否则 0。
- 0x00437920 LoadDirect：同样首次打开；LoadEntry(句柄[id/3000], id/3000, id%3000) 后 Extract，结果记入 0x004F5174 并返回（原程序每次分配不释放，重建由共享指针管理）。
- 0x00437AA0 CacheLoad：LoadEntry，空时返回 0；链 = id 低 4 位（id 0xFFFF 时子号），计数 +1，0x18 字节节点挂到链头（id、大小、数据），已用 += 大小，Extract 的结果为当前，返回其非空。0x00437BB0 CacheFind：按完整 id 查链，找到时移到链头、当前 = Extract，返回 1（即使 Extract 为空）。0x00437C50 CacheEvict：与 TswArchive 相同，比较为有符号。
- 0x00437D80 CloseAll：六个句柄非 −1 时关闭并置 −1，CacheClear、IndexClear，打开标记、读取计数、0x004F5120、0x004F517C、0x004F517E 清零；已用字节不为 0 时以 "ERR"（0x004B713C）"ACT記憶體管理有誤!!"（0x004B7128）报。
- 0x00437E20 IndexInsert：链 = 条号低 4 位，计数 +1，0x14 字节节点挂到链头；Seek(0x2C×条号−0x10) 读 0x2C 字节，读取计数 +2，记 档号、条号、offset（+0x18）、长度（+0x14）并写出。0x00437F30 IndexLookup：按 档号与条号 查链，找到时移到链头并写出两值，未找到时不写。
