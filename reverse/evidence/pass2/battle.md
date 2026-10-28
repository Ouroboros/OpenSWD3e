
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

## 0x00457630 BattleHud::DrawTileBox — exact

汇编：每取一帧都经 0x00436720(编号, 帧)，帧指针写 0x0050C328、帧首 dword 写 0x004ED9BC。帧 4 → 0x004580C0(x, y, (列+2)×16, (行+2)×16, 填色)；帧 0 画于 (x, y)，x += 帧 0 宽；帧 1 取一次，列数（有符号 >0）次画于 (x, y)、x += 宽；帧 2 画于 (x, y)，y += 帧 2 高；每行：帧 3 画于 (x0, y)，右 = x0 + 帧 3 宽×(列+1)，帧 5 画于 (右, y)，y += 帧 5 高；帧 6 画于 (x0, y)，x = x0 + 帧 6 宽；帧 7 取一次，列数次画；帧 8 画于 (x, y)。画法 0x00417A60(x, y, 宽 +0xC, 高 +0xE, 0, +4)。
对照：HudFrame::DrawTileBoxAdvance 的帧号（kTileFill 4 等）、kTileSize 0x10、取帧次数与时机（列、行为 0 时帧 1/7 仍取、帧 3/5 不取）、坐标推进与 Select（lastPicture 0x0050C328、SetSource）一致；Picture 的 data/palette/width/height 对应原结构 +0/+4/+0xC/+0xE；缺帧见 deviations 219；绘制经 Scene 见 deviations 97。

## 0x004578C0 BattleHud::DrawPictureFrame0 — exact

汇编：0x00436720(编号, 0) 写 0x0050C328、0x004ED9BC；0x00417A60(x, y, 宽, 高, 0, 0)，返回其结果。对照：DrawPictureFrame0Advance 一致；模式 0 的绘制（BlitCopy、BlitRawCopy）不读第 6 参数；缺帧见 deviations 64。

## 0x00457900 BattleHud::DrawPortrait — diff → exact

汇编：栈上 0xA4 字节动作清零，编号 0x2390、动作 = 下标（u16）；0x00436C80 为 0 → 返回 0。0x00436720(+0x4A, +0x4C)；高亮表 0x00537480[下标] = 1 → 0x004179D0(x−(+0x10), y−(+0x14), 宽, 高, 0, &0x07E007E0)（绿色轮廓）；画头像 0x00417A60(同位置, 宽, 高, 0, 调色板)；参数 dim ≠1 → 返回 dim；= 1 → 色调 绿 0x004EC180、蓝 0x004EC178 = −10、红 0x004ED9A4 = 0，以 0x0050C328 的帧 0x00417A60(同位置, 宽, 高, 旗标 +0x18 | 0x10, 调色板) 并返回其结果。色调全局按 0x004271D0 的像素格式掩码（0xF800、0x7E0、0x1F）对应红、绿、蓝。
对照：kPortraitAct、kOutlineGreen、kDimTint、Painter::TintUnder（0x10）、返回值（调用者 0x00464240 不读）一致；模式 0 绘制不读第 6 参数。
差异：高亮表在原程序只有 4 字节（其后是 0x00537484 hitFading），符鬼头像的下标 4..8 读到 hitFading；源码数组为 8 项，下标 8 越界（未定义行为）且 4..7 恒为 0。修正：portraitHighlight 改为 4 字节，下标不小于 4 不描边，deviations 新增 0x00457900 行；同一数组在 0x0046BFB0 以 hoverUnit（可达 19）为下标的清零写入加边界判断并登记（deviations 0x0046BFB0 行，该函数第二遍时复核，见 pass2-method.md）。复核：完整函数重新对照一致。

## 0x00457A50 BattleHud::DrawArrow — diff → exact

汇编：动作 = 0x0050CB30 + 槽×0xA4，编号 0x2392、动作 0；0x00436C80 为 0 → 返回 0；0x00436720(+0x4A, +0x4C)；三通道色调 = −4；0x00417A60(x−(+0x10), y−(+0x14), 宽, 高, 旗标 +0x18, 调色板)，返回其结果。
对照：kArrowAct、kArrowTint、arrowActs 一致。
差异：arrowActs 的注释写的是正传地址 0x004FDD10。修正：改为 0x0050CB30。复核：完整函数重新对照一致。

## 0x00457B00 BattleHud::DrawBar — exact

汇编：0x00436720(编号, 帧)；宽 >0（有符号）→ 0x00417A60(x, y, 宽, 帧高, 0, 调色板) 并返回其结果；否则返回帧指针。对照：DrawBarAdvance 一致；宽 ≤0 时源码返回 1（原程序返回帧指针），唯一调用者 0x0048BA60 不读返回值；模式 0 绘制不读第 6 参数。

## 0x00457B50 BattleHud::DrawPictureFrame — exact

汇编：0x00436720(编号, 帧)；0x00417A60(x, y, 宽, 高, 0, 调色板)，返回其结果。对照：DrawPictureFrameAdvance 一致（模式 0 不读第 6 参数）；缺帧见 deviations 64。

## 0x00457BA0 BattleHud::DrawGauge — exact

汇编：帧 0 画于 (x, y, 宽, 高)；帧 1：宽度参数 ≠0（负数也画）→ 0x00417A60(x, y, 宽度, 帧 1 高, 0, 调色板) 并返回其结果，否则返回帧 1 指针。对照：kGaugeBack 0、kGaugeFill 1、`fill != 0` 一致；宽度为 0 时源码返回 1，调用者不读返回值。

## 0x00457C20 BattleHud::DrawGaugeAlt — exact

汇编：同 DrawGauge，填充用帧 2，恒返回 1。对照：kGaugeAltFill 2 一致。

## 0x00457CA0 BattleHud::DrawGaugeWide — exact

汇编：帧 0 画于 (x, y)；帧 1 以宽度 (u16 参数)+2 画于 (x, y)（参数为 0 也画），返回其结果。对照：u16 参数与 kWideFillExtra 2 一致。
