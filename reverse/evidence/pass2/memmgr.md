# 第二遍：内存管理器（MemoryManager）

私有堆与错误文字为平台替换（deviations 28）。

## 0x0043D120 MemoryManager::MemoryManager — exact

汇编：HeapCreate(1, 页大小, 0) 写 +0；+8/+0xC/+0x10/+0x14 清 0；32 个桶头（+0x1C、+0x28 起每 0x10）清 0；32 次 0x0043D2D0，成功则挂到备用链 +0x228 链首，失败 MessageBoxA("MemMgrObject node Create Failed", "MemMgr", 0x10)。
对照：一致（不建私有堆）。

## 0x0043D1B0 MemoryManager::~MemoryManager — exact

汇编：32 个桶逐个从链首取节点：wsprintf("Resource not release ,Size[%d]", 大小) 到 +0x22C 并以 "MemMgr report" 提示，0x0043D350 失败时再以 "MemMgr" 提示 +0x22C，节点挂回备用链；再逐个取备用链首 0x0043D310，失败提示 +0x22C；节点计数 +0x14 ≠ 0 → "Resource node not match"；HeapDestroy 失败 → 0x0043D470 并提示。
对照：一致；源码中没有任何代码把节点放入桶，未释放块的报告循环不可达（取节点的先后不影响结果）。字串与 exe 字节相同。

## 0x0043D2D0 MemoryManager::NewNode — exact

汇编：new 0x10 字节清零，失败 → 0；计数 +0x14 加 1，峰值 +0x10 < 计数时更新；返回节点。对照：一致。

## 0x0043D310 MemoryManager::FreeNode — exact

汇编：节点为空 → 0；0x0043D350 失败 → 0（节点不释放）；释放节点，计数减 1，返回 1。
对照：一致；失败时源码随所有权释放节点：不可达，只用于没有块的备用节点，0x0043D350 对空块总返回 1。

## 0x0043D350 MemoryManager::FreeBlock — exact

汇编：节点为空 → 复制 "Internal error : NodeContainFree" 到 +0x22C，返回 0；块为空 → 返回 1（字段不动）；块首两 dword 或尾部 (+8+大小) 两 dword 不是 0xABCDDCBA → wsprintf("block[0x%x][%d] overwrited! [%2X …×16]", 用户区地址, 大小, 前 16 字节) 到 +0x22C，结果 0；释放块，已分配 +0xC −= 大小，节点 +4、+0、+0xC 清 0（+8 保留）；返回结果。
对照：一致；地址印作 0 见 deviations 28。

## 0x0043D470 MemoryManager::FormatLastError — exact

汇编：FormatMessageA(FROM_SYSTEM, 0, GetLastError(), 0, +0x22C, 0x400, 0)。对照：平台替换（deviations 28）。

## 0x0043DA70 HeapBlock::Allocate — diff → exact

汇编：尺寸 ≤0（有符号）→ NULL；否则 operator new((尺寸+0xF)&~3) 后整块清零（不查结果），返回块。
对照：取整与清零一致。
差异：分配失败时原程序清零写入空指针，源码在请求达到 1 GB 或分配失败时返回失败，此前未登记。修正：登记 deviations（0x0043DA70 空指针），源码不改。复核：完整函数重新对照一致。

## 0x0043DAB0 HeapBlock::Free — exact

汇编：非空时 free。对照：一致。
