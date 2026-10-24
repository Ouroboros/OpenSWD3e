# 第一遍汇编验证：存档画面（0x00406B50 起）

## 0x00406B50 Game::ResetSaveSlots — exact（顺带改名）

- 0x004D1638 起 0x80 dword（32 个按键状态）清零；downArrowPress（0x004C2650）=0、upArrowPress（0x004C2651）=1、slotPrompt（0x004C27A4）=0、slotMouseBlock（0x004C1610）=0；ResetPlayback(0x004C2700)；释放三个预览、按 (newest/3)·3（有符号除法）起读三个预览、ResetActs（即 SaveSlots::LoadPage）；返回 1。
- 0x004C2700 起 0xA4 字节的 ActState 全程序只有此处引用（IDA 中仅 unk_4C2700 一处），成员名由带 swd32 地址的 `unk4CC200` 改为 `saveScreenAct` 并注明无读取者。

## 0x00406C20 FreeSavePreviews — exact

- 三个预览（0x004C1DC8、0x004C20A0、0x004C2378）各调 0x00409840。

## 0x00406C50 SaveScreenFrame — exact

- slotMouseBlock 减 1，负或 >1000 时置 0；saveFrames（0x004C27B0）+1；storyState==3 时置 1；指针动作（0x004C64C0）=0xD；SaveScreenKeys；页 0 FlagViewKeys、页 1 SaveSlotsFrame；storyState 非 0 时 DrawSaveScreen（尾调用）。

## 0x00406CD0 SaveScreenKeys — exact

- 右键（槽 14）press≠0 且 heldFrames==1 时注入 Esc 按下（0x0043BDC0(1)）；F1/F2（0x3B/0x3C）且 menuMode==0 时 Sleep 500 并切到第 0/1 页；Esc、左右 Ctrl 任一按下时 LeaveSaveScreen（尾调用）。

## 0x00406D90 FlagViewKeys — exact

- Space（槽 1）press>0 且 heldFrames==1 时翻转当前旗标（0x004C27A8）。左右上下、PageUp/PageDown 依次按 press>0 且（heldFrames>3 或 ==1）取步长 −1、+1、−6、+6、−0x78、+0x78（后者覆盖前者）；相加后负则 0x1FFF，≥0x2000 则 0。

## 0x00406EC0 SaveSlotsFrame — exact（修正后）

- 鼠标：左键 press 与 heldFrames 都非 0 时 slotMouseBlock=0 并处理鼠标；否则 slotMouseBlock 非 0 时跳过鼠标。浏览状态且指针在槽区（0x10<x<0x270、0x28<y<0x1CC，无符号）：单击选槽（行=(y−0x28)/0x8C，与 newest%3 的差加到 newest，变化时音效 0x107 与 ResetActs）；双击：存档模式下已用槽进入覆盖提示（5，按键清零，本帧结束），空槽直接取用；读档模式下已用槽取用，空槽音效 0xB8；右键双击离开。提示状态（≥5）时答案区（0x190<x<0x1E6，行偏移 +0x5E..+0x76）按 (x−0x190)/0x2C 取答案 5/6，左键点击：答案 5 取用，否则回到浏览并清按键；右键：同样回到浏览。浏览状态时上箭头（0≤y<0x1E）动作 0、点击（heldFrames==1）注入 PageUp（press 2、held 1）；下箭头（0x1C2<y<0x1E5）动作 1、点击注入 PageDown（press 1、held 1）。
- 按键：左或上、右或下、PageUp、PageDown（press>0 且 Repeating，0x004418D0）各使 slotMouseBlock=0x78；浏览时 newest 改变 −1/+1/−3/+3（下限 0、上限 0x62，越界时指针动作 0xD 并夹住），在界内时音效 0x107（±1）或 0xC2（±3）；±1 只在界内重载页面，±3 总是重载；方向闪烁 downArrowPress/upArrowPress（左上、PageDown 用前者，右下、PageUp 用后者）=3；提示状态（≥5 且 ≠7）时改为答案 5（左上、PageUp）或 6（右下、PageDown）。
- Space 或 Enter（press≠0 且 heldFrames==1）：slotMouseBlock=0x78；读档模式：空槽音效 0xB8 并回到浏览，否则取用；存档模式：浏览时已用槽进入提示（本帧结束）、空槽取用，提示 5 取用，提示 6 回到浏览（本帧结束）。未取用时 Esc 或右 Ctrl（press==1 且 heldFrames==1）离开。saveFrames≤10 时本帧结束。
- 存档（取用、提示 7、存档模式）：SaveFameFile（0x00409B90）、Menu::flags（0x0050A82C）|=2、提示 0、SaveRoleStates(主角格)、以 OPEN_ALWAYS 读写打开 "%ssave\%d.sav"；成功时变量 2 在 1..5 内先存 swdghost.dat，SeekBegin(0)、Truncate，依次写：日期 12 字节、地图画面缩小一半的 160×120 缩略图（ConvertPixelsBack 0x4B00 像素，0x9600 字节）、地名 0x20 字节、旗标压缩块（压缩长度、原长度 0x400）、maps.dat 角色表压缩块、领队号、地图号、主角格 x/y、act、动作、方向、队伍资料压缩块、名字 0x80 字节、累计游戏秒数（time/difftime/_ftol）、返回点块 0x40、战斗块 0x180、名声块（长度+内容）、震动块 0x84、两段地名文本 0x68 的压缩块；关闭文件。无论成败：重载页面、saveFrames=0、storyState=0、DrawSaveScreen、Sleep 600、menuMode==5 时 ItemsAfterSave(1)、menuMode=0、释放预览。
- 读档（取用、读档模式）：音乐队列模式为 2 时 Switch(4,1,0xF)；ResetEffects；提示 0；画面清零并显示；maps.dat 跳过 0x200 重读、角色表指针=基址+dword[4]；以 OPEN_EXISTING 只读打开；成功时 ResetBattle、释放两段地名、映射整个文件，旗标解压后记下旗标 5..8、0x13、0x16，置 3、4（设备旗标位 0 时清），角色表逐条 UpdateRoleRecord，位置块，之后各段在已用长度小于文件长度时依次读：队伍资料（缓冲为原长度 ×2）、名字与秒数、返回点（地图为 0 时以当前地图与主角格补上）、战斗块、名声；震动先填 0xCFCF，再读震动块与地名文本块（0xA8 缓冲）；解除映射并关闭；法宝鬼魂重置并按变量 2 读回；领队记录填 0xFF；LoadFameFile；LoadCity；恢复保留旗标，清旗标 0xA。无论成败：saveFrames=0、storyState=0、menuMode==2 时 OptionsLeave 与 menuState=0、释放预览。
- 队伍资料打包/解包与汇编逐段一致（时钟、0x40 个变量、4 条 0x3C 字节角色记录并清 +0x26/+0x28、0x40 个装备号、4 个法术表、背包（数量与待定数量，后者 |0x8000）、场景音乐记录 8 字节，各以 0 结束；读回时装备号 0xFFDC 为“無”，未定义的装备与法术报 "載入道具"/"載入法術"，背包以模式 1/2 加回，音乐记录 cue 位 0x800 时不覆盖）。
- 差异（已修正）：旗标块的原长度字段原程序写 0x400（0x004079A7 `push 400h`），实现写 0（save-format.md 依据反编译误记）。改为写旗标数组大小；save-format.md 同步更正。游戏目录的四个存档在该处均为 0x400，与汇编一致。
- 偏差：deviations.md 0x00406EC0 四条（读出映射之外、0x2000 缓冲、空装备指针、音乐偏移数组无结尾）。
