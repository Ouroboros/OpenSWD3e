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

## 0x00408B40 DrawSaveScreen — exact

- 锁定离屏表面得到绘制指针（0x004EDA44）并解锁（0x00417790 即表面 Unlock；实现另把同一指针交给画笔，原程序各绘制函数直接读该全局）；第 0 页清屏为黑并画旗标页，第 1 页画存档页；随后以 DDBLT_WAIT 把离屏表面送上屏幕（平台替换为 ShowFrame）。

## 0x00408BC0 DrawFlagView — exact

- 起始旗标 = (cursor/120)·120（有符号）；行 y 从 0x20 每行 +0x16 到 0x1D8 之前，列 x 从 0 每列 +0x64 到 0x258 之前；当前旗标以背景 0xFFFF、黑字画 "[%5d:  ]"（0x004AF674），其余白字（0xFFFF），之后背景恢复 0xFFFE；x+0x40 处已置旗标画黄色（0xFF00）"@"（0x004AF670），否则黑色 " "（0x004AF66C）；到 0x2000 即止。

## 0x00408CC0 DrawSlotsPage — exact（修正后）

- 0x004C27B6 自增，>13 归零（无读取者，成员由带 swd32 地址的 `unk4CC2AA` 改名 `slotsPageFrames`）；文字色 MakeColor(0x1D,0x17,0x10)；背景动作 0x232A，存档页动作 0、读档页 0xC，画于 (0,0)。
- 每格（行高 0x8C）：底框 FillBox(0xC0, y+0x32, 0x1B8, 0x7C, 黑, 当前槽模式 4 否则 2)；运行面板动作（0x0050AF70）；预览已载入时：日期（中字体，0xC8, y+0x34，样式 4）、缩略图 160×120 逐行复制到 (0x14, y+0x34)，非当前槽再以模式 3 变灰；地名底 FillBox(0x14, y+0x9C, 0xA0, 0xE, −12×3, 模式 1)，地名小字体右对齐 x=(0x20−字节数)·3、y+0x9D、颜色为主文字色 5 的低字（0x004AF734）；四个角色（maxHitPoints≠0xFFFF）在 x=0xE0+0x48·n、y+0x88 画头像动作，等级 "%d級"（0x004AF6C8）x=(0x12·n−字节数)·4+0xF0、y+0x9C；金钱计数（0x00411B50，第三参数未用）在 0x26E、y+0x3C；徽章图 0x2357+unk24 于 0x25A、y+0x82；游戏时间 x=0x26C−字节数·9；未载入时 "沒有存檔資訊"（0x004AF6B8）于 0x1FC。当前槽画两个窗框（0x16..0xB2、0xC4..0x270，y+0x37..y+0xA9，边框 0x80000008）。编号 "%02d"（0x004AF6B0）小字体于 (0x14, y+0x34)。
- 存档模式提示 5/6：窗框 0x190..0x1F4、row+0x46..row+0x78；大字体 "要覆蓋嗎?"（0x004AF6A4）；答案高亮 FillBox(0xB4+0x2C·提示, row+0x5E, 0x2C, 0x18, 0x14, 0xD, 0, 模式 5)；"  是  否"（0x004AF698）。
- 翻页箭头：newest+2≤0x62 时画图 0x235B 帧 0 于 (0x130,0x1C0)，指针 y 在 (0x1C2,0x1E5) 内时帧 1 于 y 0x1C0+downArrowPress；newest−2≥0 时帧 2 于 (0x130,4)，指针 y 在 [0,0x1E) 内时帧 3 于 y 4−upArrowPress；两个按下计数非 0 时各减 1。
- 指针：运行指针动作，失败时报 "Act Err(RefrashCursor)"，参数为 GUID、act.id、动作、方向（0x004092AB 起四个压栈）；在指针坐标减动作偏移处画当前帧。
- 差异（已修正）：实现只传 GUID、其余三项为 0（并在 deviations.md 记为“取栈残值”），与 swd3e 汇编不符；改为传四项并删除该偏差条目。字符串常量注释由 swd32 地址改为 swd3e 地址。

## 0x00409320 SaveGames::IsFree — exact

- 以 OPEN_EXISTING 读写打开 "%ssave\%d.sav"，打开失败即为空槽（返回 1）；总是 Close。

## 0x004093C0 LeaveSaveScreen — exact

- menuMode 1/2：storyState=0、DrawSaveScreen、Sleep 150、menuMode=0；3：SystemEnter、menuState=0x80000003、menuMode=0、storyState=0；4：storyState=0、menuState=0、menuMode=0；5：storyState=0、menuMode=0、ItemsAfterSave(0)；其他不动。最后释放三个预览，返回 1。

## 0x00409480 SaveSlots::LoadPreview — exact

- 以 OPEN_EXISTING 只读打开，失败时状态 1。成功：映射文件；日期模板 "____/__/__ __:__"（0x004AF6E8）的 12 个下划线依次填入文件头 12 字节；缩略图 0x9600 字节经 ConvertPixels；地名（lstrcpy 到首个 0）；旗标块按“压缩长度、字数”解压（缓冲 2×字数），旗标 0x1E..0x21（位表第二份拷贝 0x004AA474 的掩码）未置时该角色 maxHitPoints=0xFFFF，unk24 = 旗标 0x53 ? 3 : 2；unk20 取位置块的地图号；队伍块同样按 2× 解压，金钱取 +4，四条角色记录（+0x104 起每条 0x3C）在角色存在时取 maxHitPoints（+0xA）、maxSpirit（+0xC）、maxStamina（+0xE）、等级字节（+0x2C）；名字 0x80 字节；秒数格式化为 "%02d時"、"%02d分"、"%02d秒"（0x004AF6E0/D8/D0，无符号除法）；关闭后状态 2。

## 0x00409840 SaveSlots::Free — exact

- 空指针直接返回；释放缩略图、日期、地名、名字、时间五个指针，再把整个 0x2D8 字节结构清零。

## 0x004098C0 SaveSlots::FindNewest — exact

- 槽 0..0x62 依次以 OPEN_EXISTING 读写打开，成功时取最后写入时间，高 dword（有符号）≥ 当前最大值时 newest=该槽并更新最大值（平手取后者）；每槽 Close；然后释放并读入 newest 所在页的三个预览、ResetActs。

## 0x00409A30 SaveSlots::ResetActs — exact

- 三格 × 四个动作（0x004C1618 起每个 0xA4）：ResetPlayback，id=1..4，action=0，direction=6；newest%3 等于该格时 action=8。

## 0x00409AC0 SaveGames::AnyExists — exact

- 一个 File 对象循环槽 0..0x62，以 OPEN_EXISTING 读写打开成功则计数，每槽 Close；计数非 0 返回 1。

## 0x00409B90 Game::SaveFameFile — exact

- 0x400 字节缓冲清零；名声计数 1..500 各以 FindCounterValue(列表 0x004D3458, n) 的低 16 位写入第 n 个字；以 OPEN_ALWAYS 读写打开 "%sFame.dat"（0x004AF6FC，安装目录），成功则写 0x400 字节；Close。

## 0x00409C80 Game::LoadFameFile — exact

- 0x400 字节缓冲清零；以 OPEN_EXISTING 读写打开 "%sFame.dat"，成功则读 0x400 字节；Close；名声计数 1..500 以 SetCounterCapped(列表 0x004D3458, n, 第 n 个字) 写回。
