
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

## 0x00457D20 BattleHud::DrawSmallNumber — exact

汇编：0x0050C310 = x、0x0050C314 = y、0x0050C318 = 0；除数 10^9 起 10 次：0x0050C320 = 当前余值，数字 = 余值 / 除数（有符号），余值 −= 数字×除数，数字 ≠0 或已开始 → 已开始、位数+1，除数 /= 10；位数为 0 → 1。从个位起画位数个：0x00436720(编号, 数字)（帧号寄存器只设低 16 位，0x00436950 与载入只用低 16 位），画于 (x, y)，x −= 帧宽；返回最后一次绘制的结果。对照：DrawSmallNumberAdvance（kSmallDigits 10、kSmallTopDivisor 10^9）与 numberX/numberY/numberStarted/numberValue 的写入一致。

## 0x00457E10 BattleHud::DrawNumber — exact

汇编：字 0x0050CB2E（numberFont 高字）= 编号，0x0050C320 = 值，0x0050C310 = x，0x0050C314 = y，0x0050C318 = 0；依次以 10^9、10^8 … 10 调 0x00457F70，每次 0x0050C310 += 返回值（u16）；0x0050C318 = 1 后以 1 调用并累加；返回最后一位的宽度。对照：DrawNumberAdvance 一致（DrawNumberDigit 不改 numberX，源码以参数 x 加首位宽度与原程序读全局相同）。

## 0x00457F70 BattleHud::DrawNumberDigit — exact

汇编：数字 = (u32)0x0050C320 / 除数；(u16)数字 = 0 且未开始 → 返回 0；0x00436720(0x0050CB2E, 数字)；字 0x0050C31C = 0x8000 → 模式 0x20，否则 0；0x00417A60(0x0050C310−16, 0x0050C314, 宽, 高, 模式, 0)；0x0050C320 −= (u16)数字×除数，0x0050C318 = 1；返回 0x0050C328 的宽。对照：kNumberDigitWidth 0x10、kDigitGlow8000、Painter::ScaledGlow（0x20）与写入顺序一致。

## 0x00458040 BattleHud::DrawSlotAnimation — exact

汇编：动作 = 0x0050C3F8 + 槽×0xA4，编号 = 参数、动作 0；0x00436C80 为 0 → 返回 0；0x00436720(+0x4A, +0x4C)，只写 0x004ED9BC（不写 0x0050C328）；0x00417A60(x, y, 宽, 高, 旗标 +0x18, 0)，返回其结果。对照：slotActs（0x0050C3F8）、GetPicture + SetSource（不经 Select、不改 lastPicture）与绘制参数一致；缺帧见 deviations 64。

## 0x004580C0 BattleHud::FillColor — exact

汇编：0x004ED9BC = 栈上颜色参数的地址；0x00417A60(x, y, 宽, 高, 8, 0)，返回其结果。对照：SetSource(&颜色)、Painter::RawGradient（8）、绘后清源一致；颜色低字为 RLE 标记见 deviations 425。

## 0x004580F0 BattleHud::DrawEffectAt — diff → exact

汇编：0x0050C258 编号、动作 = 参数；0x00436C80 为 0 → 返回 0；0x00436720(+0x4A, +0x4C)（不写 0x0050C328）；edx = 锚点 x（+0x10），旗标 +0x18；镜像（参数 = 1）→ 旗标位 0 取反，dx = 帧宽 − 锚点 x（16 位）；0x0045817E movsx ecx, dx，x' = x − ecx；y' = y − 锚点 y（+0x14，32 位）；0x00417A60(x', y', 宽, 高, 旗标, 0)；返回 +0x98（waiting）= 1。
对照：effectAct（0x0050C258）、旗标翻转、y 偏移与返回值一致。
差异：源码镜像时保留锚点 x 的高 16 位、低 16 位按无符号拼成偏移（帧宽小于锚点时得 0xFFxx 之类的正数），不镜像时用 32 位锚点 x；原程序两种情况都用低 16 位符号扩展；注释引用的 0x00450AE2 是正传地址。修正：偏移改为 i16（不镜像取锚点 x 低 16 位，镜像取帧宽 − 锚点 x 的低 16 位），注释改为 0x00458152..0x00458186。复核：完整函数重新对照一致。

## 0x004581B0 BattleHud::ResetEffectAt — exact

汇编：0x0050C258 起 0xA4 字节清零，返回 0。对照：effectAct = ActState{}、返回 0 一致。

## 0x004581D0 BattleHud::DrawCursorAnimation — exact

汇编：0x0050C938 编号 = 参数、动作 0；0x00436C80 为 0 → 返回；0x00436720(+0x4A, +0x4C)（只写 0x004ED9BC）；0x00417A60(x, y, 宽, 高, 旗标 +0x18, 0)，返回其结果。对照：cursorAct（0x0050C938）一致。

## 0x00458240 BattleHud::DrawRoleIcon — exact

汇编：0x0050C330 编号 0x23AA、动作 = 参数；0x00436C80 为 0 → 返回；0x00436720(+0x4A, +0x4C)（只写 0x004ED9BC）；0x00417A60(x−(+0x10), y−(+0x14), 宽, 高, 旗标 +0x18, 0)，返回其结果。对照：kRoleIconAct、roleIconAct（0x0050C330）一致。

## 0x004582C0 BattleHud::DrawDigit — exact

汇编：字体参数 = 0 → 0x00436720(0x2370, 数字)，写 0x0050C328 与 0x004ED9BC；否则沿用 0x0050C328（不改源指针）。字 0x0050C31C = 0x4000 → 模式 0x20，否则 0；0x00417A60(x, y, 宽, 高, 模式, 0)；返回 0x0050C328 的宽（u16）。对照：kDigitFont、kDigitGlow4000、lastPicture 的更新与沿用一致。

## 0x00458330 BattleHud::DrawScrollBar — exact

汇编：0x0050C1B0 编号 = (u16)参数；上端：动作 0x1A（样式 1 → 0x1E），不清动作状态即 0x00436C80；0x00436720(+0x4A, +0x4C)（只写 0x004ED9BC）；画于 (x, y)，旗标 +0x18。随后各段先清 0xA4 字节：轨道 0x18，第 i 段画于 (x+2, y+上端高+段高×i)（段数有符号 >0）；轨道长 = 段高×段数；滑块 0x19：长 = _ftol(7.0f / 0x0050C920 × 轨道长)，0x0050C304 = (轨道长−滑块长)/(0x0050C920−7)，上 = 偏移 + 步长×0x0050C924 + 轨道顶（0x0050C308、0x0050C918），下 = 上 + 滑块长，差 <5 → 上+5，0x0050C924+7 ≥ 0x0050C920 → 轨道顶+轨道长；0x0050C91C = 轨道顶，0x0050C3D4 = 下，0x0050C3D8 = 轨道顶+轨道长；裁剪 (x, y, x+0x20, 下)，滑块长 >0 时从 0 起每次画于 (x+5, 0x0050C308+k)、k += 帧高直到 ≥ 滑块长；恢复全屏裁剪；下端 0x1B（样式 2 → 0x1F）画于 (x, y+上端高+段高×实际段数)，返回其结果。任一动作 0x00436C80 为 0 → 返回。
对照：DrawScrollBarAdvance 与 RunScroll 的动作号、首段不清状态、坐标、浮点截断（Ftol，7.0 与 exe 中 0x004AADDC 的 float 相同）、整数除法、写入的六个全局与裁剪一致；调用者只在条数大于可见行数时绘制，0x0050C920−7 不为 0；滑块帧高为 0 的保护见 deviations 67。

## 0x00458670 BattleHud::DrawCoinToss — exact

汇编：帧数 0x0050CDC8 = 0 时：字 0x0050CDCE（胜次）≥1 → 0x0050CDCC 两字清 0；否则 0x0050CDCC = 0x0043DB90(2)。旋转动作 0x0050C1B0：编号 0x239B、动作 0，0x00436C80 为 0 → 返回 0；0x00436720(+0x4A, +0x4C)；缩放 0x0050C3DC < 0x400 → 0x00423630(宽/2, 高/2, 0x400−缩放, 同)；画于 (0x104−锚点 x, 0xC8−锚点 y)，旗标 |0x80。hold（+0x9C）= 0：帧数+1，%25 = 0 → 延迟 = (u16)(2×0x004BEFF4)，+0x46 +1，0x004BEFF4 两字 = 延迟，+0x44 = +0x46；否则延迟 = 低字；延迟 ≥0x40（无符号）→ 0x004BEFF4 = 0x00010001、缩放 0，结果 = 1 → 胜次+1，hold = 1，音效 0x117，延迟 = 1；倒数（高字）−1，为 0 → = 延迟；+0x44 = 0 → 音效 0x2E；返回 0。hold ≠0：0x0050CA88 编号 0x239B；帧数 ≠1 → 动作 = 结果+1，画于 (0x104−x, 0xC8−y)，倒数 +1 >0x1E → 音效 0x197、清 0x0050CA88、帧数 1、倒数 1；返回 0。帧数 = 1 → 动作 3，0x00423630(宽/2, 高/2, 5×缩放+0x400, 同)，三通道色调 0，旗标 0x84；缩放 += 0x3C，<0x400 → 返回 0；否则帧数、缩放 = 0，倒数 1，清两个动作，返回 1。
对照：DrawCoinTossAdvance 的分支、16 位运算、常量（kCoinAct 0x239B、kCoinX/Y、kCoinSlowFrames 25、kCoinStopDelay 0x40、各音效、kCoinFaceFrames 0x1E、kCoinEndAction 3、kCoinEndGrowth 5、kCoinFullZoom 0x400、kCoinZoomStep 0x3C）、Painter::ZoomCopy 0x80 与 Additive 4、随机数（random.Below(2)，0x0043DB90）一致；coinDelay 初值 0x00010001 与 exe 中 0x004BEFF4 相同；ActState 的 hold/delay/frameDelay 偏移一致；缺帧见 deviations 64。

## 0x00458A00 BattleHud::DrawTimingBar — exact

汇编：0x00436720(0x2380, 0) 写 0x0050C328/0x004ED9BC，画于 (x, y)；字 0x0050C934 = 0；对 0x004BEFFC 的 3 个刻度（20、40、55）：裁剪 (x+刻度, y, x+刻度+1, y+0x0050C328 高)，0x00436720(0x2380, 1) 画于 (x, y)；指针 = (u16)0x0050C2FC >> 1 再 +1，刻度 ≤ 指针 ≤ 刻度+2（有符号）→ 0x0050C934 = 序号+1，(u16)0x0050CB2C = 序号+1 → 0x0050C934 = 0，否则字 0x0050CB2C = 0。之后裁剪 (x+指针+1, y, x+指针+2, y+高)，再画帧 1，恢复全屏裁剪；字 0x0050C2FC +1；((u16)计数 >> 1)+1 = 0x3E → 字 0x0050C2FC = 0x8000、返回 1；否则返回 0。
对照：DrawTimingBarAdvance 的裁剪（随 lastPicture 的高）、TimingMarks（与 exe 字节相同）、kNeedleShift 1、kNeedleReach 2、kNeedleEnd 0x3D（等价于原程序的 +1 = 0x3E）、kTimingDone 0x8000 与 numberFont 低字的清零一致；缺帧见 deviations 64。

## 0x00458BB0 BattleHud::DrawTallGauge — exact

汇编：0x00436720(0x237A, 2) 只写 0x0050C328；每级高 = 帧高/6（级数参数 = 4 时 (帧高−0x20)/6），显示高 = 级 × 每级高。帧 0 画于 (x, y)；身顶 = y + 帧 0 高 − 2；级 <6 → 裁剪 (x, 身顶, x+帧 0 宽, 身顶+显示高)；帧 2 画于 (x+4, 身顶)，高为帧高（级数 = 6）或帧高−0x20；alpha 0x004E6D58 = 8，帧 3 以模式 0x14 画于 (x+0xE, 身顶+0x1F)，高为帧高或帧高−0x1E；恢复全屏裁剪；帧 1 画于 (x, 身顶+显示高−3)，返回其结果。
对照：DrawTallGaugeAdvance 的帧号、除法、级数比较（4 与 6）、裁剪、alpha 与 Painter::Translucent（0x14）一致；DrawOptions 的 alpha 与三通道色调补注地址 0x004E6D58、0x004ED9A4、0x004EC180、0x004EC178（按 0x00417A60 与 0x004271D0 的读写确认）；缺帧见 deviations 64。

## 0x00458D80 BattleHud::SetupRoulette — exact

汇编：字节 0x004BF004..0x004BF007 = 5、5、0、0，0x004BF008 = 2；步数（有符号）>0 时调 0x00458DC0 步数次。对照：Roulette::InitialTops、BaseDigits 与 exe 初值（05 05 00 00 02）一致。

## 0x00458DC0 BattleHud::RaiseRoulette — exact

汇编：依次找第一个 <9（有符号字节）的位：+1 后返回；第 3、4 位时先把 0x004BF008 设为 3、4。全 9 时不变。对照：TopDigit 9、digits = 位序号+1（≥2 时）一致。

## 0x00458FD0 BattleHud::ResetRoulette — exact

汇编：字节 0x0050CDD8 = 0，dword 0x0050CDDC、0x0050C324 = 0；对 0..0x004BF008−1：字节 0x0050CDE0[i] = 0x0043DB90(10)。对照：stopped、result（4 字节）、spins（4 字节）清零与 random.Below(10) 的次数一致。

## 0x00458E30 BattleHud::DrawRoulette — diff → exact

汇编：0x00436720(0x23B0, stop = 1 ? 1 : 0) 写 0x0050C328/0x004ED9BC，画于 (x−宽+0x34, y−高/2+2)，模式 0。周期 bl = 3，对 i = 0..0x004BF008−1（x 每位 −0xE）：i ≥2 → 周期−1；i < 已停位数（有符号字节 0x0050CDD8）→ 0x00457E10(0x2377, 结果[i], x, y)；否则取滚动值 0x0050CDE0[i]，stop = 1 → 结果 0x0050CDDC[i] = 该值，0x00457E10(0x2377, 该值, x, y)，计帧 0x0050C324[i] = 周期 → 计帧 0、滚动值 = (滚动值+1) % (上限 0x004BF004[i]+1)；计帧+1。之后计帧[已停位数] = 1 → 音效 0x125。stop = 1 → 已停位数+1，≥ 位数 → *参数 = 结果[0] + 10×结果[1] + 100×结果[2] + 1000×结果[3]（有符号字节），返回 1。其余返回 0。
对照：帧与坐标、周期、DrawNumber（kMeterNumbers 0x2377、kRouletteDigitWidth 0xE）、取模、音效 0x125（battle_hud_constants.h）与合成公式一致；计帧下标越出 4 位见 deviations 431。
差异：源码在 stop ≠1 时也比较已停位数，全部停下后以 stop = 0 调用（battle_action.cpp 每帧）会重写伤害值并返回 1；原程序 stop ≠1 一律返回 0、不写。修正：stop ≠1 直接返回 0，只在停下一位时计数并在最后一位写出数值。复核：完整函数重新对照一致。

## 0x00459010 BattleHud::RouletteBonus — exact

汇编：首位 0x0050CDDC[0] = 0：字 0x00577220 = 10，其后各位全为 0 时每位 +10，最后写回并 wsprintf(0x0050CDE4, “此場戰鬥經驗值加百分之%d”) 后 0x00479220(0x118, 0xA, 0x3C, 文字, 0x40000002)，返回 0；某位非 0 → 0x00577220 = 0、返回 0。首位 = 7：字 0x0057721E 从原值起其后各位全为 7 时每位 +2，写回，非 0 时以“此場戰鬥金錢變%d倍”提示，返回 0；某位不为 7 → 0x0057721E = 0、返回 0。其他：其后各位都等于首位时每位 +10，非 0 时以“龍颿法寶加%d點額外經驗”提示并返回该值；位数 ≤1 或某位不同 → 返回 0。
对照：experienceBonus（0x00577220）、moneyTimes（0x0057721E）、kRouletteStep 10、kLuckyDigit 7、kLuckyStep 2、提示的位置与旗标（BlackBar | Centered = 0x40000002）、三段文字（按 exe 字节解码一致）与返回值一致；位数 ≤1 时 moneyTimes 不写回与原程序跳过写入相同。

## 0x00459170 BattleHud::SetMeterLimit — exact

汇编：字 0x004BF002 = 参数（返回 ax = 参数）。对照：meterLimit（初值 100 与 exe 相同）一致。

## 0x00459180 BattleHud::AddToMeter — exact

汇编：参数 = 0xFFFF → 0x0050CDD6 = 0、0x0050CDD4 = 0；否则 0x0050CDD6 = 参数（负 → 0），0x0050CDD4 += 参数（16 位），结果为负 → 0；0x0050CDD4 ≤ 0x004BF002（有符号）→ 返回 1；否则两者清 0、返回 0。对照：一致。

## 0x004591E0 BattleHud::ResetMeter — exact

汇编：0x0050CDD0 = 0、字 0x0050CDD4 = 0、0x00577368 = 0（不动 0x0050CDD6），返回 0。对照：一致。

## 0x00459200 BattleHud::DrawMeter — exact

汇编：0x00577368 = 0 → 返回。0x00436720(0x2397, 0) 画于 (x, y)；帧 1 取出后写 0x0050C328；比率 = (i16)0x0050CDD4 / (i16)0x004BF002（x87），fcom 1.0（0x004AA6A0）后 C0/C3 均未置（大于 1）时取 1.0；目标 = _ftol(帧 1 高 × 比率)；0x0050CDD0 ≠ 目标 → 大于时 −1，否则 +1；裁剪 (x, y+高−当前高, x+宽, y+高)，设源后画帧 1（模式 0）；目标 ≠ 当前高 → 三通道色调 10、模式 0x10 再画；恢复全屏裁剪；0x00457E10(0x2377, 值, x+宽/2, y+高)。
对照：DrawMeter 的比较（`share > 1.0`，NaN 保留，与 fcom 结果一致）、Ftol 截断、步进、裁剪、kMeterTint 10、Painter::TintUnder 与数值位置一致；缺帧见 deviations 64。

## 0x004593C0 BattleHud::DrawGrowing — exact

汇编：0x00436720(编号, 0) 写 0x0050C328/0x004ED9BC；0x00423630(宽/2, 高/2, 0x0050C3E0[槽]+0x400, 0x0050C3EC[槽]+0x400)；三通道色调 0；0x00417A60(x, y, 宽, 高, 0x84, 0)；0x0050C3EC[槽] += 0x50、0x0050C3E0[槽] += 0x3C、0x0050C928[槽] −1；0x0050C3E0[槽] ≥0x4B0 → 三者清 0、返回 1（al），否则返回 0。对照：growWidth/growHeight/growFrames、kGrowUnit 0x400、kGrowWidthStep 0x3C、kGrowHeightStep 0x50、kGrowEnd 0x4B0、Painter::ZoomAddTinted 0x84 一致；槽 ≥3 见 deviations 418，缺帧见 deviations 64。

## 0x004594B0 BattleHud::DrawPicturePalette — exact

汇编：0x00436720(编号, 0) 写 0x0050C328/0x004ED9BC；0x00417A60(x, y, 宽, 高, 0, 调色板)，返回其结果。对照：一致；缺帧见 deviations 64。

## 0x004594F0 BattleHud::LoadAnimation — exact

汇编：thiscall（ecx = 动画结构），5 个栈参数（retn 0x14）。局部 6 个字的已载表 = 0xFFFF；+0x12C = id、+0x120 = x、+0x124 = y；动作 +0 = id（u16）、+8 = 0；0x00436C80 为 0 → 返回 0。循环：字节 +0x90 ≠0 → +0x128 = 该值；帧 = 字 +0x4C，已载表[帧] = 0xFFFF → 0x00436770(+0x4A, +0x4C) 的 5 dword 帧头复制到 +0xA8 + 帧×20，已载表[帧] = 帧，0x00438A30(帧数据, 3, 0x280 / 除数（有符号）)；字 +0x42 = 0 → 动作 0xA4 字节清零、返回 0；否则动作编号 = 字 +0x12C、动作 0 再 0x00436C80，为 0 → 返回 0。
对照：BattleAnimation（act +0、wave +0x128、id/x/y）、kNotLoaded、LoadPictureDirect、ShiftPicture（ShiftMode::Right = 3、Display::Width / 除数）、循环与退出条件一致；第一个栈参数（图形对象）不用；帧号 ≥6 见 deviations 68；源码在载入结果为空（档案缺失）时不移位，属 0x00436770 的载入失败处理。

## 0x00459640 BattleHud::DrawAnimation — exact

汇编：thiscall。字 +0x12C = 0 → 返回 0；动作编号 = id、动作 0，0x00436C80（结果不看）；0x004ED9BC = 帧[+0x4C] 的数据，0x004ED9AC = +0x128；0x00417A60(+0x120−锚点 x, +0x124−锚点 y, 帧宽, 帧高, 旗标 +0x18, 0)；0x004ED9AC = 0；返回 +0x98。对照：DrawAnimationAdvance（SetWave 前后、忽略 Run 结果、waiting）一致；Painter::waveMode 补注地址 0x004ED9AC（0x00419200 以它选波纹行）；帧号 ≥6 见 deviations 68。

## 0x004596E0 BattleHud::PlayAnimationShifted — exact

汇编：thiscall，2 个栈参数（retn 8）。局部 6 个字 = 0xFFFF；id = 0 → 返回 0；动作 0xA4 字节清零后编号 = id、动作 0，0x00436C80 为 0 → 返回 0。循环：帧 = +0x4C，首次出现 → 记下，位移量 ≠0 时 0x00438A30(帧数据, 位移 >0 ? 3 : 2, |位移|)（thiscall，被调方清栈），再画于 (+0x120−x, +0x124−y)（不设波纹）；已出现过的帧不画；+0x44、+0x46 清 0；+0x42 = 0 → 动作清零、返回 1；否则重新 0x00436C80，为 0 → 返回 0。对照：PlayAnimationShiftedAdvance、ShiftMode（Left 2、Right 3）一致；帧号 ≥6 见 deviations 68。

## 0x00459830 BattleHud::FreeAnimation — exact

汇编：thiscall。6 个帧头的数据指针非空则释放并清 0；字 +0x12C = 0、+0x128 = 0；动作 0xA4 字节清零（返回 0）。对照：pictures 逐个 reset、id、wave、act 清零一致。
