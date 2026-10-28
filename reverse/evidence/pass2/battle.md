
## 0x00456D80 TotalItemValue — exact

汇编：比率 = 0x0050C0EC 位 0 置时 60，否则 100；从表头 0x0050C0F8 逐节点累加 (u16 价格 +0x52 × u16 工作数量 +6 × 比率) / 100（每项有符号除法取整，32 位和）。对照：一致；工作数量至多 198（卖出：数量+待入）或 99，价格 ≤0xFFFF，乘积不超过 2^31，int 运算不溢出。

## 0x00456DE0 CountListB — exact

汇编：0x0050BF18 = 0；0x0050C034 为空 → 返回；否则逐节点计数，到 13 为止。对照：一致。

## 0x00456E10 SortBattleItems — exact

汇编：表头 0x0050C0F8 后继清 0、字 0x0050C100（表头数量）清 0；对源表每个节点：工作数量清 0；(0x0050C0EC & 3) = 1 且（价格 +0x52 = 0 或种类字节 +0x2C 位 6）→ 跳过、留在源表；否则在已排好的表中从表头起找：当前节点优先级（0x004575E0）< 本节点 → 前进；否则前一节点（起初是表头本身）优先级 < 本节点 → 插在当前节点前；否则前进；到表尾 → 接在最后；节点从源表摘下（源链接不动，指向下一个）。对照：ListModes = 3、SellMode = 1、kNoPriceKind = 0x40、插入条件与位置一致；源码先 reset 表头后继（原程序只清指针），唯一调用者 0x00455B20 在调用前都已 0x00454C40 释放表头链，二者等价；ItemPriority 为纯函数，源码缓存前一节点优先级与原程序每次重算结果相同。

## 0x00456EF0 FreeBattleItems — exact

汇编：表头后继非空时循环：表头后继 = 节点后继，释放说明与节点。对照：FreeItemList(itemsHead.next) 一致。

## 0x00457050 CountItemTotal — diff → exact

汇编：累加背包 0x004C3018 中 id（+4，零扩展）等于参数的节点的数量+待入（各有符号扩展）；0x0050C0EC & 3 ≠0 → 返回；否则对角色 0..3：旗标 0x1E+角色置 → 该角色 16 个装备栏（0x004E4CB0 + 角色×0x40）中 id 相等者加数量（有符号）；返回总和。装备栏不判空。
对照：ListModes = 3、Party::FirstRoleFlag = 0x1E、SlotsPerRole = 16、累加顺序一致。
差异：源码跳过空装备栏，此前只登记于 0x00454CF0（deviations 57）。修正：deviations 57 补列本地址，源码不改。复核：完整函数重新对照一致。

## 0x004575E0 ItemPriority — exact

汇编：优先级 = 种类 << 16；位 8 → 1；位 9 → 种类 >> 24；位 10 → 返回 0x800。对照：kKindShift 16、kFirstKind 0x100、kTopByteKind 0x200、kLastKind 0x400 与覆盖顺序一致。

## 0x00457610 SetGaugeFull — exact

汇编：0x004BEFF8 = (20 − 速度)×100，并返回该值。对照：kGaugePerSpeed 100、kSpeeds 20、gaugeFull（0x004BEFF8）一致；初值 InitialGaugeFull = 900 与 exe 中 0x004BEFF8 的 dword 相同。
