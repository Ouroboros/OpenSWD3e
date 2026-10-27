# 第二遍：game（存档画面与存读档）

方法见 reverse/analysis/pass2-method.md。

## 0x00406B50 Game::ResetSaveSlots — exact

汇编：按键状态 0x004D1638 起 0x200 字节清零；0x004C2650=0、0x004C2651=1、0x004C27A4=0、0x004C1610=0；0x0040DF70(&0x004C2700)；释放三张预览，按 3·(最新槽/3)+i（有符号除法）载入；0x00409A30；返回 1。
对照：一致（LoadPage 同顺序）。controls.h 中按键地址注释由 swd32 改为 0x004D1638..0x004D1837。

## 0x00406C20 Game::FreeSavePreviews — exact

汇编：0x00409840 依次释放三张预览。对照一致。

## 0x00406C50 Game::SaveScreenFrame — exact

汇编：0x004C1610 减一，负或 >0x3E8（有符号）时置 0；0x004C27B0 加一；storyState==3 时改 1；指针动作(0x004C64C0)=0xD；0x00406CD0；页 0 走 0x00406D90、页 1 走 0x00406EC0；storyState≠0 时 0x00408B40。
对照：一致。

## 0x00406CD0 Game::SaveScreenKeys — exact

汇编：右键刚按下（press≠0、帧==1）时模拟按下键码 1；F1／F2 且 menuMode(0x0050A404)==0 时 Sleep(500) 后页=0／1；Esc、左右 Ctrl 任一按住时 0x004093C0。
对照：一致。

## 0x00406D90 Game::FlagViewKeys — exact

汇编：空格按下（>0、帧==1）时切换当前旗标；左右上下、PageUp、PageDown（按住>3 帧或帧==1）依次覆盖步长 −1、1、−6、6、−0x78、0x78；光标加步长，负时 0x1FFF，≥0x2000 时 0。
对照：一致。

## 0x00406EC0 Game::SaveSlotsFrame — exact

汇编按段推导（源码拆为 SlotsMouse、SlotsKeys、TakeSlot、SaveGame/WriteSave/PackParty、LoadGame/ReadSave/UnpackParty）：
- 鼠标：左键 press、帧都非 0 时清 0x004C1610 后处理鼠标；否则 0x004C1610 为 0 才处理。浏览中且指针在 (0x10,0x270)×(0x28,0x1CC)（无符号）内：单击选槽（行=(y−0x28)/0x8C，新槽=旧+行−旧%3，变化时音 0x107、0x00409A30）；双击：存档时空槽直接选定、已用槽提示 5 并清按键返回；读档时已用槽选定、空槽提示 0 并音 0xB8；右键双击离开。提示≥5 且指针在答案区（行=0x8C·(槽%3)）内：答案=(x−0x190)/0x2C，提示=答案+5；单击时答案 0 选定、否则提示 0 清按键返回；右键同后者。浏览中 x∈(0,0x280)：y∈[0,0x1D]（有符号）指针动作 0，刚按下时 PageUp 置 (2,1)；y∈(0x1C2,0x1E5) 指针动作 1，刚按下时 PageDown 置 (1,1)。
- 按键：左／上、右／下、PageUp、PageDown（press>0 且 0x004418D0 重复）：0x004C1610=0x78；浏览中槽 −1／+1／−3／+3，越界时指针动作 0xD 并夹到 0／0x62，否则放音（0x107／0xC2）；单步只在未越界时重载预览，翻页总是重载；点亮 0x004C2650（左上、PageDown）或 0x004C2651（右下、PageUp）为 3；提示≥5 且≠7 时改 5／6／5／6。空格或 Enter 刚按下（press≠0）：0x004C1610=0x78；存档：浏览时已用槽提示 5 返回、空槽选定；提示 5 选定；提示 6 改 0 返回；读档：已用槽选定，空槽音 0xB8、提示 0。未选定时 Esc 或右 Ctrl（press==1、帧==1）离开。
- 选定后 0x004C27B0>0xA（无符号）才执行：存档（提示 7、读档旗 0）：0x00409B90；0x0050A82C|=2；提示 0；0x0040D560(主角格 x, y)；开 save\%d.sav（OPEN_ALWAYS，读写）：鬼变量 1..5 时 0x0049E910 存 swdghost.dat；Seek 0、截断；"%04d%02d%02d%02d%02d" 写 12 字节；缩图（锁定、Paint、缩小、反转换 0x4B00 像素）写 0x9600；地图名 lstrcpy 到清零的 0x400 缓冲后写 0x20；压缩块（压缩后大小、原始大小、数据）依次为旗标 0x400、maps 角色表（大小 0x004E4DB4）；0x004D1538、地图号、主角格 x、y、act.id、动作、方向七个 dword；队伍块（时钟、0x40 变量、4×0x3C 角色记录（+0x26、+0x28 清零）、64 个装备 id、4 张技能表（表头后各 id，0 结束）、背包（i16 数量>0 写 id、数量；待领>0 写 id|0x8000、待领；0 结束）、maps 场景音乐记录 8 字节逐条，0 结束）；名字 0x80；time/difftime/ftol 累加游戏秒数后写，起点重置；返回点 0x40（四个 word、两个 dword）；战斗块 0x180；声望（0x0048AA80 得到的长度与数据）；震动 0x84；名字块 0xA8 缓冲中两段各 0x34（字符串块为 0x34 清零分配，复制结果与字符串加零填充相同）压缩写入；关闭。然后重载预览、0x004C27B0=0、storyState=0、0x00408B40、Sleep(600)、menuMode==5 时 0x00451160(1)、menuMode=0、释放预览。
- 读档（读档旗 1）：音乐队列模式==2 时 0x0049D0C0(4,1,0xF)；0x00411C80；提示 0；画面 0x96000 字节清零并呈现；maps.dat 跳过 0x200 重读，角色表=+4 处偏移；开存档（OPEN_EXISTING，读）：0x00459AE0；释放并清空两段文字；映射整个文件；0x962C 处旗标块解包；记下旗标 5、6、7、8、0x13、0x16；置旗标 3、4，0x004C6334&1 时再清；角色表块解包，每 0x18 字节按 (id, act, 动作, 方向, x, y, 事件, 谈话, 设定位, 0, 地图, 变量) 调 0x0040D7C0；读地点七个 dword（主角 x、y 直接取存档值）；已用<大小（无符号）时：队伍块解包到 2 倍缓冲：时钟、变量、角色记录（加成清零）、装备（逐个释放后新建，0xFFDC 为「無」，读物品失败 ErrorBox「載入道具」「位置(%d)裝備了沒有定義的物品號碼(%d)」并改为 0xFFDC，数量 1）、技能（先清空四表，再逐个插到表头后，0xFFDC 跳过，失败 ErrorBox「載入法術」「角色(%d)擁有未定義的法術(%d)」）、背包（先释放，id&0x8000 时以模式 2 否则模式 1 调 0x00454970）、场景音乐（找到同地图记录且其 +6&0x800 为 0 时覆盖 8 字节）；已用<大小时：名字、游戏秒数（起点=time）、返回点（返回地图为 0 时取当前地图与主角格）；已用<大小时战斗块；已用<大小时声望；震动表填 0xCFCF，已用<大小时读震动，已用+0x84<大小时名字块；解除映射、关闭；鬼重置为 0，变量 1..5 时读回；领队记录填 0xFF；0x00409C80；0x0040C570(地图, x, y, act, 动作, 方向, 0)；清六个保留旗标后按记下的值恢复；清旗标 0xA。最后（含打开失败）0x004C27B0=0、storyState=0、menuMode==2 时 0x00452970 并清 0x004D1C88、释放预览。
对照：各段分支、条件位宽、写入与读取的布局、调用顺序、错误框原文（0x004AF5FC、0x004AF608、0x004AF5D0、0x004AF5DC、「無」0x004AF630）一致。party.h 中角色记录与时钟的注释由 swd32 改为 swd3e（0x3C 字节、0x004C4F18；0x004C655C）。AddItem 只按 16 位用数量参数，待领数量寄存器高位残值不影响结果（在 0x00454970 行复核）。

## 0x00408B40 Game::DrawSaveScreen — exact

汇编：锁定离屏面得到绘制指针（0x004EDA44）并解锁；页 0 时清 0x96000 字节后 0x00408BC0，页 1 时 0x00408CC0；Blt 呈现。
对照：一致（锁定／目标／呈现为平台替换）。

## 0x00408BC0 Game::DrawFlagView — exact

汇编：起始旗标 = 0x78·(光标/0x78)（有符号）；行 y 从 0x20 每次 +0x16 到 <0x1D8，列 x 从 0 每次 +0x64 到 <0x258；光标项先设中号字背景 0xFFFF、颜色 0，"[%5d:  ]" 画在 (x,y)，再恢复背景 0xFFFE；置位时 "@" 黄色 0xFF00，否则 " " 颜色 0，画在 x+0x40；旗标号到 0x2000 停。
对照：一致。

## 0x00408CC0 Game::DrawSlotsPage — exact

汇编：0x004C27B6（u16）加一，>0xD 归 0；颜色 MakeColor(0x1D,0x17,0x10)；背景 act 0x232A，动作 0（存）／0xC（读）；三个槽：底框 FillBox(0xC0,y+0x32,0x1B8,0x7C, 最新槽 Eighth 否则 Quarter)；Run 面板动作；已载入：日期（中号 0xC8,y+0x34）、缩图 160×120 复制到 (0x14,y+0x34)、非最新槽 Gray、地点底 Tint(−12) 与小号字（x=3·(0x20−长度)，颜色主色表第 5 项 0x004AF734）、四人头像（0xE0+0x48·i, y+0x88）与 "%d級"（x=4·(0x12·i−长度)+0xF0）、金钱 0x00411B50(0x26E,y+0x3C,0,金钱)（第三参数不用，alpha 取 0x004E582C）、徽记文件 0x2357+unk24 于 (0x25A,y+0x82)、游戏时间（x=0x26C−9·长度）；未载入：「沒有存檔資訊」于 (0x1FC,y+0x9C)；最新槽两个外框；"%02d" 槽号。存档提示 5／6：外框、「要覆蓋嗎?」、答案高亮 FillBox(0x2C·提示+0xB4, …, Gradient)、「  是  否」。箭头：最新+2≤0x62 画下箭头，指针 y∈(0x1C2,0x1E5) 时画亮箭头（y+(i8)按下帧）；最新−2≥0 画上箭头，y∈[0,0x1D] 时亮箭头（4−(i8)按下帧）；两个按下帧非 0 减一；Run 指针动作（失败「Act Err(RefrashCursor)」），画指针。另分配一块 0x9600 字节随即释放，未使用。
对照：一致（常量、字符串 0x004AF6B8、0x004AF6A4、0x004AF698、0x004AF6C8「%d級」与 exe 相同）。

## 0x00409320 SaveGames::IsFree — exact

汇编：以读写、OPEN_EXISTING 打开存档，返回 (打开失败)。对照一致。

## 0x004093C0 Game::LeaveSaveScreen — exact

汇编：按 menuMode 查 0x00409468 跳转表：1、2：storyState=0、0x00408B40、Sleep(150)、menuMode=0；3：0x00451210、0x004D1C88=0x80000003、menuMode=0、storyState=0；4：storyState=0、0x004D1C88=0、menuMode=0；5：storyState=0、menuMode=0、0x00451160(0)；之后释放预览，返回 1。
对照：一致。

## 0x00409480 SaveSlots::LoadPreview — exact（补登越界）

汇编：只读打开，失败时状态=1；映射；"____/__/__ __:__" 依次填入表头 12 字节；缩图 0x9600 字节复制后转换；地点从 0x960C 起按 lstrlen 复制；旗标块解包：旗标 0x1E..0x21 未置位的角色记为不在队（0xFFFF），unk24 = 旗标 0x53 置位时 3 否则 2；地点块 +4（当前地图）记入 +0x20；队伍块解包：+4（变量 0）为金钱；在队角色记 +0xA、+0xC、+0xE 与 +0x2C 字节等级；名字 0x80；游戏秒数按 %02d時、%02d分、%02d秒 拼接（无符号除法）；状态=2。
对照：一致；文件短于各块时读出映射之外，并入 deviations.md 0x00406EC0 越界行。

## 0x00409840 SaveSlots::Free — exact

汇编：参数非空时释放五块（缩图、日期、地点、名字、时间）并把 0x2D8 字节清零。对照一致。

## 0x004098C0 SaveSlots::FindNewest — exact（补登未初始化）

汇编：0..0x62 逐个以读写打开，成功时取写入时间（只预先清零低位），高位（有符号）≥ 已记高位时记下该槽与高位；然后释放并载入该页、0x00409A30。
对照：一致；GetFileTime 失败时高位为栈上残值，补登 deviations.md（0x004098C0 未初始化行）。

## 0x00409A30 SaveSlots::ResetActs — exact

汇编：三槽 × 四个动作（0x004C1618 起，每个 0xA4）：ResetPlayback，act={1,2,3,4}[i]，动作 0、方向 6，最新槽%3（有符号）==槽时动作 8。
对照：一致。

## 0x00409AC0 SaveGames::AnyExists — exact

汇编：0..0x62 逐个以读写、OPEN_EXISTING 打开，成功计数，返回 (计数≠0)。对照一致。

## 0x00409B90 Game::SaveFameFile — exact

汇编：0x400 字节清零；1..0x1F4 号（≤500）调 0x0048A580(0x004D3458, id) 取值写到 [2·id]；以 OPEN_ALWAYS、读写打开安装目录 "%sFame.dat"，成功时写 0x400 字节；关闭。
对照：一致（0x004D3458 为 fame.lists[FameList=1]）。

## 0x00409C80 Game::LoadFameFile — exact

汇编：0x400 字节清零；以 OPEN_EXISTING、读写打开 "%sFame.dat"，成功时读 0x400 字节；关闭；1..500 号调 0x0048A500(0x004D3458, id, 字值)（压栈时 ecx 高位为栈地址残值，被调函数只按字读第三参数）。
对照：一致。

## 0x00409D70..0x0040A130 全局初始化片段（60 个，Game::Game 与 Game::talismanGhost）— exact

汇编（逐个读出）：每组为「调用构造」+「atexit(0x004A1664) 注册析构」，或只有构造：
- 0x00409D70／80／90／A0：MemoryManager 构造 0x0043D120（GetSystemInfo、HeapCreate、链表清零）于 0x004D0470，析构 0x0043D1B0（对链表中未释放块逐个 MessageBox「MemMgr report」）。全程序只有这两个片段引用 0x004D0470，0x0043DA70 走 CRT malloc，链表恒空，退出时不弹框——不创建等价。
- 0x00409DB0／C0：TalismanGhost 构造 0x0049E7F0 于 0x004D0C10，不注册析构 → talismanGhost。
- 0x00409DD0／DE0／E00／E10：ScriptObject 构造 0x00435B60(1,1,0,0,1,1) 于 0x004C6128，析构 0x00435BF0；除这两个片段外只有 0x00417230 以它为“前一节点”读写 +0x28（链表头 0x004C6150）→ Scene::objects。
- 0x00409E20..E50、E60..E90、EA0..ED0：TextFont 构造 0x00439BA0／析构 0x00439BF0 于 0x004C5130、0x004E5830、0x004C3658 → largeFont、mediumFont、smallFont。
- 0x00409EE0..0x0040A0D0：File 构造 0x0043CAE0／析构 0x0043CB10 于 0x004D0E08（maps）、0x004D1DB0（HUGE.lmf）、0x004E6B88（无其他引用，不创建）、0x004D3470（path）、0x004C2FA0（story）、0x004E6830（unitFile）、0x004D1438（无其他引用，不创建）、0x004C2A48（FieldView::file）。
- 0x0040A0E0／F0：Graphics 构造 0x0043C020 于 0x004C5008，不注册析构 → graphics。
- 0x0040A100／110／120／130：Input 构造 0x0043BA30 于 0x004D1C90（+0x100、+0x104、+0x108=0，+0x114=0xA），析构 0x0043BA60 → input。
对照：game.cpp 中的初始化说明与上述逐组一致；构造函数本身的初值在各自行核对。构造／析构顺序之间没有相互依赖。

## 0x0040A140 Game::Run — exact

汇编：FindWindowExA("SoftstarSwd3") 已有则返回 0；找 Ghost 窗口("SsSwd3eg"/"Ghost") 发 0x401、Sleep(1000)；0x00411C10(命令行)==1 返回 0；0x00411F10 写日志分隔串（0x004B0B3C，73 个“=”，无换行）；CoInitialize；注册 WNDCLASSEX（图标 0x7D、光标 0x67、黑刷）；CreateWindowExA(0x40000, 640×480, X=-800)，失败返回 0（WM_CREATE 内做开始检查）；ShowWindow／UpdateWindow；SendMessage(0x404)；srand(time(0))、0x0043DAC0(time(0))；0x004E6D08=1；循环：PeekMessage 有消息→GetMessage(为 0 退出返回 wParam)／Translate／Dispatch；无消息时 running(0x004E6D08)≠0：系统旗标 0x20→0x0049C800（视频帧），否则旗标 1 或 inactive(0x004D3FC4)≠0→Sleep(0)，否则 0x0040A7B0；running=0：active(0x004D1630)==1→0x00411F20（暂停横幅），否则 Sleep(0)。
对照：Run／LoopFrame 分支一致；已有实例、Ghost、命令行、消息循环与日志为已登记的平台替换（deviations 283、284、415、103、135）；分隔串逐字节一致。修正：LoopFrame 注释的地址（0x00409EC0→0x0040A140）。

## 0x0040A380 Game::HandleEvent／Game::Start（窗口过程）— exact

汇编（独立推导）：
- 旗标 0x50 时先调输入框 0x0043D620；它只在 WM_IME_CHAR 时返回 0（窗口过程返回 1），其余消息照常往下处理——WM_KEYUP 不会被吞，F8、P、Esc 与源码顺序等价。
- WM_CREATE：0x00429780 返回 0 则 SendMessage(WM_DESTROY)。WM_DESTROY：旗标 1 或系统旗标 4 时 0x00428EC0；CoUninitialize；PostQuitMessage(0)。
- WM_SIZE（旗标 1）：wParam 4／3／1→Activate(0)，wParam 2／0→Activate(1)（新增偏差行）。WM_ACTIVATEAPP（旗标 1）：低字≠0→Activate(1) 否则 Activate(0)（deviations 287）。WM_SYSKEYDOWN 回车、WM_SYSCOMMAND 吞掉 0xF130／F140／F170／F090（deviations 285）。
- WM_KEYUP：0x77 翻转 0x004E6D08；系统旗标 0x20 且 Esc→0x0049C7B0；旗标 1 且系统旗标 1 清且 'P'：0x00411D10()>0x40 时 MessageBeep(0)，在 0..0x1869D 中找第一个不存在的 "%sScrnShot\%05d.bmp"（0x004B0BBC，全部存在时用最后一个），0x004352D0(0x004EDA44, 640, 480, 名字)。
- 0x404：PlaySound(0x78)；0x00428160；0x00428D60；清旗标 0x12，0x00409AC0 非 0 则置旗标 0x12；DialogBoxParamA(0x6B)：1→0x00428730，0x004D1C88=0、0x0050A404=3、0x004E6CF4=3、0x004E6D00=1、0x004E6D04=1，0x00406B50、0x004098C0；2→0x00428730；6→SendMessage(WM_DESTROY)；其他不处理。
对照：Start 中各全局与 menuState=NoMenu、menuMode=TitleLoadMode(3)、storyState=SaveSlotsState(3)、saveScreenPage=1、slotsLoad=1 一一对应，Choice 值 1／2／6 一致；启动音随启动器（deviations 74）；截图、F8、视频 Esc、WM_DESTROY 的释放条件一致。
修正：game.h 中 running、active、slotsLoad 的注释由 swd32 地址改为 0x004E6D08、0x004D1630、0x004E6D04；kScreenshotName 注释改为 0x004B0BBC；补登 WM_SIZE 映射偏差。

## 0x0040A7B0 Game::MainFrame（含 RunTimers、EnterBattle、BattleMainFrame、EndMainFrame）— exact

汇编（独立推导）：
- 系统旗标 0x10→返回；active(0x004D1630)=0→Sleep(0) 返回；0x004C30D8=time(0)；0x004C4654=timeGetTime；(now−0x004E6D0C) <u 0x004D1540 → 返回；0x004E6D0C=now（固定步长为 deviations 84）。
- 0x004C6334 位 1→清旗标 3、4。menuState(0x004D1C88)=0 时：旗标 0x10 且 --0x004E6B50<0 且 leader.id(0x004D0AC4)==0xFFFF 且 menuState=0 且 storyState(0x004E6CF4)=0：0x004E6B50=-1，清 0x10、清 0x12、置 0x11，0x0040F930／F840／F880／F8B0／0x0040DF30／0x0040F8E0，1..roleCount−1 调 0x0040FA20 后 roles[i]+0x26=0（0x004D40D2 起，步长 0xE4），leader +0x14=0、+0x20=0、+0x1E=0x004C2A9C、+0x24=0x004D1538、+0x10=0、+4=0、+8=0；旗标 0x4A 且 --0x004C2FF0<0：=-1，清 0x4A、置 0x4B。
- 0x004E4C2C=now；0x004AF7A4=now−0x004E4C24；0x004E4C24=now；0x0043BD70；0x00404E00；0x004C64C0（pointerRole.act.action）=0xD。
- 系统旗标 0x80：0x0049F510()==1 → 清 0x80；→尾。storyState≠0：0x004E6B58=0x400、0x004E582C=0x10、0x00406C50；→尾。
- inBattle(0x004E6CF8)=0 且旗标 0xD 清且 0x004C2B84 符号位：去符号位，非 0 时：0x0049D3B0(0x00578A70，同码函数读 +0x0C 即 MusicQueue::mode)→0x004C15D0；0x00427080；0x00437D80；0x004367D0；UnmapView 失败→日志 0x004B0BFC（"into story fight CloseView fail ,%s"，无换行）；view=0；CloseMapping；0x00459BD0(低字)；inBattle=1；roles[1..count−1].flags &= ~0x01000000（0x004D40BC 起）。随后 0x004E581C=0、0x004C2B84=0。
- inBattle≠0：r=0x00479860()；r=0：请求、inBattle 清 0，三种字体 0x0040F680(20／16／12)，0x004C30D4=0xB，重建映射并映射，失败日志 0x004B0BD0（"After fight ,Remapping map fail!"），0x0049D0C0(队列, 0x004C15D0, 1, 1)；r=2：清 0x10、0x12，置 0x11，清 inBattle 与请求，字体与 unit 同上，重新映射（不检查），leader +0x14=0、+0x20=0、+0x1E=0x63、+0x24=0x004D1538、+0x10=0；r=3：清两者、清旗标 0x11、重新映射并检查；均以 0x0040D140 结束并直接返回（不检查系统旗标 4）。
- 否则 0x0040D130；menuState≠0：计数器 0x400／0x10，字体间距 0x16／0x12／0x10，(menuState&0x0FFFFFFF)：2→0x004560E0，1、3..6→0x0043EB10（跳转表 0x0040AE9C 读出）；→尾。
- 0x004C6378（variables[2]）≠0 且 ++0x004E4C1C ≥ 900（有符号）：清 0；++day(0x004D0CD8) ≥ cycleDays(0x004D0CD4) 则 0；satiety(0x004D0D20) += −3−0x0043DB90(5)，为负则 0。
- 0x0042B4C0；0x00402AA0；running、inactive、menuState、inBattle、storyState 任一不为 0 → 0x004E4C28=now−0x004E4C2C，→尾。否则 ++0x004C2B78 >u 0x14 → 置 0 并时钟递增，否则为 0 时也递增（++0x004C655C >u 1000 → 0）；0x0042BAF0(0x004D0AA0)；系统旗标 0x25 全清时 0x00405150、字体间距、0x004126A0；0x004E4C28 同上。
- 尾：0x0040D140；系统旗标 4→SendMessage(WM_CLOSE)。
对照：源码 MainFrame 与 RunTimers、EnterBattle、BattleMainFrame、EndMainFrame 逐条一致：常量（Timer 0x10／TimerExpired 0x11／TimerEvent 0x12、SecondTimer 0x4A／0x4B、SoundOnce 0x01000000、ClockFrames 0x14、ClockLimit 1000、900／3／5、0x63、0xB、结果 0／2／3、RepeatList 1）、Role 偏移（x +4、y +8、flags +0x10、eventOffset +0x14、event +0x1E、eventCursor +0x20、id +0x24、talkState +0x26；roles 0x004D3FC8）与有无符号比较均一致；平滑层与帧时钟为已登记的偏差（84、86）。
修正：game.h 中 lastFrame、mainFrameStart、lastFrameStart、deviceFlags 注释由 swd32 地址改为 0x004E6D0C、0x004E4C2C、0x004E4C24、0x004C6334。

## 0x0040AEC0 Game::Activate — diff → exact

汇编：先 0x0040E0A0（frameInterval=0）。state=0：nullsub、0x00427080、inBattle==1 时 0x00459BA0、三种字体 0x00439CB0、active(0x004D1630)=0、inactive(0x004D3FC4)=1、ShowWindow(6)，返回 0。state≠0：ShowWindow(9)、SetWindowPos(640×480)、active=1、主表面与离屏表面 Restore（vtbl+0x6C）、0x00417510(离屏表面)、三种字体 0x0040F680(20／16／12)、inactive=0、inBattle==1 时 0x00459B50、等待主表面翻页链、0x0040E090(0x23)。函数开头没有任何表面判断。
差异：源码开头在主表面为空时整个返回（继承自 swd3-rebuild，未登记），停用分支在原程序中根本不访问表面。修正：删去该判断；离屏表面为空时跳过 Painter::Init 的保护补登记为空指针偏差。窗口与表面恢复为 deviations 286。复核：两分支的调用顺序、赋值与 FrameInterval 0x23 一致。

## 0x0040C420 GameState::FindRole(id)、0x0040C500 FindRole(id, &index)、0x0040C4D0 FindRoleOrPlayer — 0x0040C420 diff → exact，其余 exact

汇编：0x0040C420：角色数 0x004AF71C 为 0 → −1；i=0.. 按无符号小于角色数，roles[i].id（0x004D3FC8+0x24）== id 且 flags 无 0x10000000 → i；否则 −1。0x0040C500：*index = 0x0040C420(id)，返回 (≠−1)。0x0040C4D0：*index=0；(u16)id==0xFFFE → *index=0x004C4B00，返回 1；否则 0x0040C500。
对照：一致（Role::Removed 0x10000000、PlayerId 0xFFFE、playerRole 0x004C4B00）。差异：比较止于数组末尾的保护未登记。修正：补登 deviations「0x0040C420、0x0040C460 越界」。

## 0x0040C530 ErrorBox — diff → exact

汇编：vsprintf(0x004C4658, format, 参数)；MessageBoxA(hWnd, 0x004C4658, caption, 0)。
对照：源码格式化后经错误报告接口按 (caption, 正文) 显示，日志为 “标题 正文”（一个空格连接）。差异：不限长度写进 0x400 字节共用缓冲区未登记。修正：补登偏差。

## 0x0040C560 DebugLog — exact

汇编：只有 retn。源码写到重建日志的 debug 级别（平台替换，deviations 135 类）。0x00411F10（ErrorLog）同为空函数（xor eax,eax／retn），注释无误。

## 0x0040C570 Game::LoadCity — diff → exact

汇编（独立推导，按段）：
- frameInterval 清 0；绘图表面非空时载入画面 act（0x004C2AB0：id 0x232A、action 0x4F、方向/x/y 0）以 0x0040EF60 画在 (0x78, 0xCA)，0x0040F0D0(−1)。
- 0x004E5820=0；清旗标 5、6；0x004E5550 起 0x10 个双字填 0x0F0F0F0F；0x0040DF70(0x004C6290)；0x004D1C40/50/60/70=0x7FFFFFFF；清旗标 7、8；0x004D0E88 起 0x300 字节清零，0x004D1E04、0x004C4650 字清零；清旗标 0x16；再清 0x004D0E88；0x004D0B88=0；0x004D3450=0x004C2B80=0x140，0x004D3FA4=0，0x004D3454=0x004E6CC4=0xF0，0x004C6608=0x004C660C=0；0x00437D80、0x004367D0、0x00427080。
- 设置表（maps.dat 表 3，每项 0xE 字节，0xFFFF 结束）中找地图号：单元 +2；0x004D183C=字节4&0xF；0x004E4DB8=字节6&0xF；0x004AF714=+8；0x004D1EC0=+0xA；0x004D1E08=+0xC；0x004C6560 = 字节4&0x10 ? 0 : 1；旗标 0xF 依字节5&0x80；0x004C66A4=0x60、0x004E55A6=4，旗标 5 依 &0x40，&0x20 时置 5 并改为 0x40／8；旗标 6／7／8／0x13／0x16 依 &0x10／8／4／2／1；0x004C5114／0x004C512C／0x004C633C = 表 0x004B0C20[字6>>12]、[(字6>>8)&0xF]、[(字节6>>4)&0xF]；步速不是 1、2、4、8、0x10 → 4；层帧 <1 → 1；(i16)遭遇组 > (i16)*0x004E6CBC → 错误框「載入地圖」「錯誤的戰鬥機率組[%d]」并置 1。
- 0x004E55A4==1 时等光盘（deviations 101 已去除）；0x0040D170(map)；0x0049E660；清旗标 0x19；表 0x19（4 字节一对，0 结束）中地图号相符且旗标未置 → 置该旗标与 0x19。
- 单元为 0xFFFF → 错误框「遊戲初始」「找不到設定的起始地圖碼 (%d)」，WM_DESTROY，返回。
- 0x0040F0D0(2)；0x004C3190=map；0x004AF718=1；置旗标 map+0x1388；选项位 0 → 0x0040D560(x, y)。0x004E5828=0；0x004D3C7C 起 0x258 字节清零；释放 0x004C31AC 链（next +0x14）；0x0040F930／F840／F880／F8B0／0x0040DF30／0x0040F8E0／F980／F9C0／F9E0；0x004D1638 起 0x200 字节清零；0x0040F6F0；0x004C6C88..0x004D0468、0x004D2370..0x004D3450 每 0x21C 调 0x0040E0B0；0x004D3FC0=0；0x004D1610 起 8 个 −1；释放 0x004C4A7C 链（next +0x1C，先释放 +0）与 0x004D1F4C 链（next +0x84）；0x0040F0D0(0xA)；角色数=1；释放 0x004D0D58。
- 0x0042A7D0(单元, 地图头) 失败 → 错误框「載入地圖」「過程發生錯誤」，WM_DESTROY。8 位地图：分配 0x200，从像素指针复制 0x200 字节，0x00427680(…, 0x100)。像素为空 → 日志 "LoadCity ,gMap.Pixel==NULL"。
- 0x004C4658 清零 0x400；0x004E6D44=1、0x004D12AC=0、0x004D1C84=0、0x004D1400=宽×高；0x0040F310(map, 缓冲区)==1 → 复制为地图名，否则错误框「讀取地圖名稱」「MapName.txt中找不到地圖[%d]名稱」（参数是单元号，原 BUG）并用 "不知道"（0x004B0EF0）。0x0040F0D0(0x55)、(0x5A)。
- 旗标 5：4 个粒子依次 Below(3)+1、Below(0x004E55A6)(+0x004C66A4)、Below(宽<<4)、Below(高<<4)、Below(2)−2、Below(2)−2；另一处清 0。
- 地图边界 0/0/宽<<4/高<<4；0x0040DD50(0x004C3198)；角色表 0x004C65F0；0x0040F0D0(0x5F)；0x004D3FC0=1。
- 角色表（每项 0xC 字，0xFFFF 结束）：与前面各项 id 比较，重复 → 错误框「讀取路人」「重複定義角色(%d)」并跳到下一项（不再检查结束标记，边界随下标重算）；id 为 0、10000、10001 → 地图字 = map；id == 0x004D1538 → 写入地图、act、动作、方向、x、y，事件/路径/步 0，旗标 0xD100，0x004C4B00 = 0x004D1610 = 角色数+已放，trailY／trailX 填 y<<4／x<<4；地图字 == map 时：x >u 宽或 y >u 高 → 错误框（map、leader.event、id、宽、高、x、y），改为宽−1、高−1，跳过；否则填角色（id、act id/action/direction、wait/drawLevel/signal 0、x/y <<4、事件、路径号、步号符号扩展、旗标零扩展、talkState/eventOffset/eventCursor 0），0x0040D3B0，0x00411790(角色, 旗标&3)；id==1 且旗标 0x19 且 variables[2]≠0 → 0x0049E5D0(var+0x5A, 1, (float)x, (float)y, 2.0, 4.0)；旗标 0x80 → 0x0040D980(槽位)、置 0x8000，map==0x16 或 0x004D1E08&0x8000 时再清；旗标 0x200 → 烟囱表（0x004E6B10 起 4 项）满时错误框「地圖(%d)定義煙囪物件數量超過四個」，否则写 role +0xC、x +4、y +6，+8／+0xA／+0xE 清 0；已放+1。
- 角色数 += 已放；0x0040F0D0(0x64)；1..角色数−1 调 0x0040F5C0；0x0040D410；两条空日志；0x0040E090(0x23)。
对照：源码逐段一致（常量、Role／ActState／RoleMark 偏移、随机数顺序、所有 13 条字符串与 exe 逐字一致）。超过 300 个角色的保护已登记（deviations 350）。
差异：遭遇组表为空、像素为空、地图名截断、maps.dat 表越界四处保护未登记。修正：补登偏差；game.h 中 UnknownPlace 注释由 swd32 地址改为 0x004B0EF0。复核通过。

## 0x0040D130 Game::MusicCue、0x0040D140 Game::AudioPump — exact

汇编：0x0040D130 只返回 1。0x0040D140：0x0049D1A0(0x00578A70 音乐队列)、0x0049E400(0x005795C0 流)、0x0049DB50(0x00578F40 音效)，返回 1。
对照：AudioManager::Update 依次调 queue、streams、effects 的 Update，一致（返回值无人使用）。

## 0x0040D170 Game::MusicZone — diff → exact

汇编：表 2 第 1 项的偏移再取一次得场景音乐表（每项 8 字节，地图号 0 结束）。找不到：0x004D15FC=0x004D1600=0，0x004C6568 &= 0xFFF3FFFF。找到：marks=+6、first=+2、second=+4；与 0x004D15FC／0x004D1600 都相同时，若队列模式 0x0049D3B0()==2 且 0x004D1604==1 → 0x0049D0C0(1, marks&3, 0xF)，0x004D1604/08/0C=0；返回。否则 0x004D15F8/FC/1600 = marks/first/second；两段各以 安装目录+"music\\"（0x004B0FA0）+0x0040D390(曲号) 调 0x0049CF20(路径, 曲号, 段, 1)；0x004C6568=0；模式 ≠2 → 0x0049D0C0(1, marks&3, 0xF)；模式 2 且 0x004D1604==1 → 同上并清故事三项。
对照：一致（musicTunes 0x004D15F8 六项、SceneRepeatFirst 0x40000、SceneLoops 0x80000、FirstList 1、SecondList 2、15 步）。差异：maps.dat 越界保护未登记。修正：补登 deviations「0x0040D170、0x0040D390、0x0040D3B0 越界」。

## 0x0040D390 Game::MusicName — diff → exact

汇编：返回 data + dword[data + dword[data + 表2偏移] + 4×index] + 4（C 字符串）。对照：一致；越界保护补登同上。

## 0x0040D3B0 Game::LoadRoleSound — diff → exact

汇编：表 0x15（每项 6 字节，id 0 结束）中 id == role.id(+0x24) → role+0x2C = +2，role+0x30 = (+4)<<16 | (+4)，返回 1；找不到返回 0。对照：一致；越界保护补登同上。

## 0x0040D560 Game::SaveRoleStates — diff → exact

汇编：角色数为 1 时返回；i=1 起 do-while 到 i == 角色数：复制 0xE4 字节；flags&0x18000000、id==0xFFFF 或 i==0x004C4B00 → 跳过。flags&0x80：0x0040D7C0(id, act.id, action, direction, x, y, event, pathNumber, flags, 0, 0x004C3190, 0x8080)。否则 pathNumber≠0 时：x、y &= ~0xF；path.dat（0x004C6338）中 dword[0x200+4×pathNumber] 起第 pathStep 字 == 8 → 在 0x004C6C88..0x004D0468（步长 0x21C）找 role==i 的移动记录，取其 x、y（u16），x >u 宽<<4 或 y >u 高<<4 → 错误框「角色回寫」「角色(%d)\n終點位置(%d,%d)\n不在地圖(%d,%d)內」(id, x, y, 宽<<4, 高<<4)；找到与否 pathStep 都 +1；0x0040D720(&副本)。
对照：一致（第 12 个参数为 UpdateRoleRecord 的默认值 KeepVariables 0x8080；TileFraction 0xF；MoveRecord x/y 为 u16）。差异：path.dat 空指针与越界保护未登记。修正：补登偏差。复核通过。

## 0x0040D720 Game::StoreRole — exact

汇编：从角色表 0x004C65F0 起每项 0x18 字节：旗标 2 置位且地图字为 0xFFFF → 错误框「!!! 警告 !!!」「角色編號(%d)\n資料歸鍵時\n找不到符合的位置\n可能是定義錯誤或是角色不存在了」(id)，返回；id 相同 → +4 act.id、+6 action、+8 direction、+0xA (u16)x>>4、+0xC (u16)y>>4、+0xE event、+0x10 pathNumber、+0x12 pathStep、+0x14 flags（均取低 16 位）。旗标 2 未置时不检查结束标记。
对照：一致；越过表尾为已登记偏差（deviations 117），两条串逐字一致。

## 0x0040D7C0 Game::UpdateRoleRecord — diff → exact

汇编：找 id（地图字 0xFFFF 结束）；各参数不为 0xFFFF 时写 +4、+6、+8、+0xA、+0xC、+0xE；路径号写 +0x10 并清 +0x12；清位 &=、置位 |= 到 +0x14（先清后置）；地图写 +0；变量不为 0x8080 写 +0x16；返回 1。找不到：wsprintfA(0x004C4658, "未定義角色[%d]") 交给空的调试日志，返回 0。
对照：一致。差异：越出 maps.dat 的保护未登记。修正：并入 deviations「0x0040D170、0x0040D390、0x0040D3B0、0x0040D7C0、0x0040D8D0 越界」。

## 0x0040D8D0 Game::LoadRoleRecord — diff → exact

汇编：找 id；找到：role.id=id，act.id/action/direction = +4/+6/+8（零扩展），x、y = (+0xA)<<4、(+0xC)<<4，event=+0xE，pathNumber=+0x10，pathStep=0，flags=+0x14（零扩展），返回地图字；找不到：调试日志 "未定義角色[%d]"，返回 −1。
对照：一致。差异与修正同上。

## 0x0040D980 Game::FollowLeader — diff → exact

汇编：role = roles[index]；pathNumber≠0 时在 72 个移动记录中找 role==index：x 或 y 低 4 位非 0 → 0x0040B180(role)；方向 = 记录 +0x1C+(step&0x7FFF) 字节；x 低 4 位非 0 时循环 x −= 0x004AA47C[方向] 直到对齐，y 同理用 0x004AA49C；0x00411890(id, flags&3, (y 无符号>>4)−1, 0)。随后 0x0040D7C0(id, 0xFFFF×7, 置位 0x80, 0xFFFF, 0xFFFF, 0x8080)，0x0040E0B0(记录)。最后 followers[0x004D3FC0] = index，0x0040E0B0(0x004D2370+计数×0x21C)，event=0，flags = (flags&~0x4000)|0x80，计数+1。
对照：步长表 {4,0,−4,−4,−4,0,4,4}／{4,4,4,0,−4,−4,−4,0} 与 RolePath::StepX/StepY 一致；步长只有 0 与 ±4，TileStart 的“余数是步长倍数才结束，否则挂起”与原循环等价；UpdateRoleRecord 参数一致；方向无效与随行表满的保护已登记（deviations 354）。
差异：移出格子的行号源码用有符号右移（role.y >> 4），原程序为 shr。修正：改为无符号右移。复核通过。

## 0x0040DB00 Game::LeaveParty — diff → exact

汇编：0x0040C4D0(id, &idx) 失败 → 角色表按 0x16 字节步长（原 BUG，记录实为 0x18）找 +2 == id，找到则 +0x14 &= 0xFF7F 返回，找不到错误框「解除夥伴」「角色(%d)不存在」。找到 idx：在 followers[0..7] 中找；找不到 → 错误框「角色(%d)不是夥伴」。找到 k：移动记录 0x004D2370+k×0x21C 的 step <u 0x7FFF 且 x 或 y 低 4 位非 0 → 0x0040B180；方向 = 记录 +0x1C+(step&0x7FFF)；方向 ≠0xFF 时 x += StepX[方向]、y += StepY[方向] 直到对齐；0x00411890(id, flags&3, (y shr 4)−1, 0)。随后清该移动记录；act.id/action/direction = 参数，event、pathNumber = 参数低字，pathStep=0，flags = 参数&0xFFFF；0x0040D7C0(id, 0xFFFF×5, event, path, 0, 0xFF7F, 0xFFFF, 0x8080)；0x0040B270；followers 与移动记录各前移一项（memmove 0x1C−4k、0xEC4−0x21C×k 字节，最后一项保留）；计数−1。
对照：一致（0x16 步长保留，TileStart(x, −StepX) 即加步长，三条串逐字一致）。
差异：移出格子的行号用了有符号右移；方向越界与 maps.dat 越界保护未登记。修正：改为无符号右移；补登 deviations「0x0040DB00 越界」。复核通过。

## 0x0040DD50 Game::BuildTriggers — diff → exact

汇编：表 7（每项 0xE 字节，0xFFFF 结束）中地图号（零扩展）== 0x004C3190 的项建 0x18 字节节点：+0/+4/+8/+0xC = +2/+4/+6/+8（零扩展），+0x10 = +0xA 起的双字，插到链表头（head+0x14）。
对照：一致（AreaBytes 0xE、各偏移、MapTrigger 字段顺序）。差异：越出 maps.dat 的保护未登记。修正：并入 maps.dat 越界一行。

## 0x0040DDD0 Game::RollEncounter — diff → exact

汇编：遭遇组号 0 → 0。r = Below(100)。从遭遇组表 2+16×(组−1) 字节起，最多 4 对：累计 += (i16)门槛，steps < 累计（有符号）或 force==1 → 取该对；(i16)几率 > r 或 force==1 才继续，否则 0。列 = x shr 4、行 = y shr 4（领队 0x004C4B00），在触发区链表中找 left ≤ 列 ≤ right 且 top ≤ 行 ≤ bottom（有符号），找不到 0；找到则 n = 表首字，k = Below(n)，0x004E5828 = 0，返回表第 k+1 字。
对照：一致（随机数先后：Below(100) 在前、Below(n) 在找到区域后；GroupsAt 1、GroupWords 8、kSetPairs 4、kForcedEncounter 1）。差异：表外读取保护未登记。修正：补登 deviations「0x0040DDD0 越界」。

## 0x0040DEB0 ClickRegions::Count、0x0040DED0 ClickRegions::Find、0x0040DF30 ClickRegions::Free — exact

汇编：Count：从 0x004E4DCC 沿 +4 数节点。Find：鼠标 (0x004C2FFC, 0x004C3000) 与各节点 left +8、top +0xA、right +0xC、bottom +0xE（u16）无符号比较，left < x < right 且 top < y < bottom 时停；*out = 节点（找不到为 0），返回下标（找不到为节点数）。Free：逐个释放，0x004E4DC8..0x004E4DD8 五个双字清 0。
对照：一致（ClickRegion 偏移、flags 0x004E4DC8、state 0x004E4DD0 三项；鼠标坐标由调用者传入）。

## 0x0040DF70 ResetPlayback — exact

汇编：+0x1C、+0x20、+0x3C = −1；+0x48、+0x46、+0x44、+0x42 字 = 0；+0x9C = 0。
对照：unknown1C、savedAction、savedDirection（NotSaved −1）、speed、frameDelay、delay、position、hold 偏移一致。

## 0x0040DFC0 GameState::GameFlag、0x0040DFF0 SetGameFlag、0x0040E020 ClearGameFlag、0x0040E050 SetGameFlagTo — exact

汇编：字节下标 = flag sar 3（算术右移），位 = 表 0x004AA4BC[flag&7]（读出 01 02 04 08 10 20 40 80）；GameFlag 返回 (字节&位)≠0；Set 或上该位并写回（返回值为新字节）；Clear 与上 0xFF−位；SetGameFlagTo：value==1 → Set，否则 Clear。
对照：一致（FlagByteShift 3、BitMasks、flags 0x004C4B0C 共 0x400 字节）。旗标号越出 0x2000 的保护在剧本指令处（deviations 128）；LoadCity 首访表 0x19 的旗标号来自 maps.dat，实际 4 对、最大 0x19A，在范围内。

## 0x0040E080 SetWalkSpeed、0x0040E090 SetFrameInterval、0x0040E0A0 ClearFrameInterval — exact

汇编：0x004C4A58 = 参数；0x004D1540 = 参数并返回 1；0x004D1540 = 0 并返回 1（返回值无人使用）。对照：一致。

## 0x0040E0B0 MoveRecord::Clear — exact

汇编：0x87 个双字（0x21C 字节）填 0xFFFFFFFF。对照：Clear 把所有字段置为全 1，字段恰好覆盖 0x21C 字节；新增 static_assert 锁定 sizeof 0x21C 与 act +0x10、blockedTries +0x1A、directions +0x1C。

## 0x0040E0D0 RoleRecord::Load — exact

汇编：目标 +0 = 源双字 +0；+0xA/+0xC/+0xE = 源 +4/+6/+8；+4/+6/+8 = 源 +0xA/+0xC/+0xE；+0x10..+0x1E = 源同位置 8 字；+0x2A = 源 +0x20；+0x20 = 源字 +0x22 零扩展；+0x24/+0x26/+0x28 = 源同位置；+0x2C = 源字节 +0x2A；+0x2D..+0x35 = 源 +0x2B..+0x33；+0x36、+0x37 = 源 +0x33。
对照：源码按顺序读出并赋给同名字段，RoleRecord 偏移与上述一一对应（sizeof 0x3C 有断言）；后两项抗性取第 9 项。

## 0x0040E3A0 GameState::DirectionToPlayer — exact

汇编：领队 = roles[0x004C4B00]；0x00411DA0(x+宽×8, y+高×8, 参数 x, 参数 y, &距离)，结果交给 0x00411E80 返回。
对照：一致（宽高为 act +0x2C／+0x30，即 role +0x6C／+0x70）。
