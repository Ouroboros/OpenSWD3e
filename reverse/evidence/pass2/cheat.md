# 第二遍：作弊对话框（cheat_dialog）

Win32 外观（资源 0x6A、ANSI API、ShowCursor）为已登记平台替换（deviations 292），以下逐条对照逻辑。

## 0x00410710 AddColumns — exact

汇编：页（0x004E6D24）≥5 时 3 栏，否则 4 栏；每栏 LVCOLUMN {mask 0xF, fmt 0, cx = {0x78,0x40,0x40,0x60}[i], 文字 = {"物品","數量","號碼","附加值"}[i]（0x004B1238/128C/1284/127C）, cchTextMax 4, iSubItem i}，LVM_INSERTCOLUMN(i)。
对照：一致。

## 0x004107E0 InsertRow、0x00410950 SetRow — exact

汇编：InsertRow 先 LVM_INSERTITEM {mask 0xD, iItem row}，其余同 SetRow：0x40 字节缓冲区写名字（lstrcpyA）设第 0 栏；"%d" 数量设第 1 栏；"%d" id 设第 2 栏；值栏：值 −1 → 空；上限 0 → 先 "%d%%" 再跳到 "%d" 覆盖（只剩数值）；上限 >1 → "%d/%d"；上限 1 → 不写（留着 id 文字）；上限 <1 且 ≠0 → "%d"。
对照：ValueText 与上述一致（中途曾误读跳转、改动后已撤回，源码未变）。修正：cheat_dialog.h 中 ValueText 注释的地址由 swd32 的 0x00410490／0x00410600 改为 0x004107E0／0x00410950。

## 0x00410A80 Fill — exact

汇编：LVM_DELETEALLITEMS。页 <5：页 0 背包（0x004C3018 的 next），页 1–4 技能头 0x004C2B68+4k 的 next；逐个节点：值与上限初为 −1，页 0 时依次：武器类（SlotKinds[0]，比较前与结果清第 15 位）→ 值 = (u16)0x0048A770(0x004C6348, id)、上限 0；法术类（[7]）→ 0x0048A930(0x004D1F58, id, &上限, &值)；计数类（[9]）→ 值 = (u16)0x0048A580(0x004D3458, id)、上限 −1；id 在 1..0x1F4 → 同上（后者覆盖前者）；InsertRow(行, 名字 +0xC, (i16)+8 + (i16)+0xA, id, 值, 上限)。页 5–8：17 项属性（名字表 0x004B0C60 每 8 字节）InsertRow(i, 名字, 0x004115D0(i, 页−5), i, 0, 0)。页 9：栈上 64×8 字节标签表（第 0 项 "錢"、第 2 项 "符鬼"、第 11／12 项 "Cmpy1"／"Cmpy2"，其余首字节取 0x004E6D40——.data 未初始化区、无写入者，恒为 0）；InsertRow(i, 标签, variables[i], i, −1, 0)。
对照：一致（SlotKinds 与 exe 逐项一致、Fame 三表 0x004C6348／0x004D3458／0x004D1F58、AttributeNames 17 项与 exe 一致、标签串一致）。修正：cheat_dialog.h 中 ObjectCounter 注释地址由 swd32 的 0x00410730 改为 0x00410A80。

## 0x0040FBE0 CheatProcedure — exact

汇编（独立推导）：
- WM_NOTIFY：code == TCN_SELCHANGE(0xFFFFFDD9) → 页 = TCM_GETCURSEL；LVM_DELETECOLUMN 3、2、1、0；AddColumns；Fill；0x3F4、0x3EB、0x3EE、0x3EC 先全部启用，页 ≥5 时再全部停用；返回 0。
- WM_INITDIALOG：插入 10 个页签（"物品"、4 个 "%s法術"、4 个 "%s屬性"、"變數"；名字取 0x004E4DB0 起的无关字节，deviations 426）；页 = 0；AddColumns；LVM_SETEXTENDEDLISTVIEWSTYLE(0, 0x20)；Fill；EM_LIMITTEXT 4／8／4；返回 1。
- WM_COMMAND（LOWORD−1000 经字节表分派）：1000 关闭：0x004E6D08=1、EndDialog(1)、ShowCursor(0)。1003 补满：页 ≥5 返回；选中行 <0 返回；页 1–4 返回（跳转表 0x004106CC：{0x0040FC9F, 0x00410698×4}）；页 0：SkipItems 取节点，count+pending <0x5A → pending=0x5A，再超过 0x5A → count = 0x5A−pending；SetRow(…, 0, 0)。1004 加入：页 ≥5 返回；id 栏为空返回；id 解析失败时为 Msg 槽中的 0x111；页 ≠0 时 id 须在 [0x1771, 0x1964)；数量栏空或技能页 → "1"，附加值栏空或技能页 → "0"；AddItem(列表, id, 数量, 0)，空 → 错误框「CheatDialog」「物品插入發生錯誤」；页 0 时按武器（0x0048A6A0(0x004C6348, id, 节点 +0x50, 值)）、法术（0x0048A810(0x004D1F58, id, 值)）、计数类与节点 id 1..0x1F4（0x0048A500(0x004D3458, id, 值)）设计数器；Fill；清三栏。1006 删除：页 ≥5 返回；选中行 <0 返回；SkipItems 后 AddItem(列表, 节点 id, −0x400, 0) 非空 → 「物品刪除發生錯誤」；Fill。1014 设置：页 9：id 栏 → 解析，>0x3F 返回，数量栏非空时解析，variables[id] = 数量；页 <5：在背包按种类查 0x00454D20(背包, id)，无则返回；数量栏非空 → count=0、pending=min(数量, 0x5A)；附加值栏非空时解析；页 0 时设计数器（同上）；页 5–8：id >0x10 返回，数量栏非空时解析，0x00411390(id, 数量, 页−5)；随后 Fill、清三栏。
对照：源码各分支一致（按钮与栏位编号、kFieldBytes 0xA、kFullStock 0x5A、kDeleteAll −0x400、技能 id 范围、kLastVariable 0x3F、kLastAttribute 0x10、kUnparsedId WM_COMMAND、ItemCountMode::Pending 0、8 条字符串）；空节点、负变量号、空技能表与未初始化输入值为已登记偏差（deviations 293、294）。修正：ClearFields 与 SetObjectCounter 注释中的 swd32 地址 0x0040F890 改为 0x0040FBE0。
