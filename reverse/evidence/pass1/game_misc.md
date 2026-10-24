# 第一遍汇编验证：数据库、新故事、字体、释放函数

## 0x0040F380 GameDatabases::Open — exact

- "%s%s" 安装目录 + maps.dat／path.dat／talk1.dat，以 GENERIC_READ、OPEN_EXISTING 打开。maps.dat 失败报 "SWD3"/"RoleDataBase init Failed."，path.dat 失败报 "PathDataBase init Failed."；path.dat 建映射，视图指针写 0x004C6338；talk1.dat 失败以 MessageBox(0, "SWD3", "StoryDataBase init Failed.", 0)（正文与标题互换）。三种失败都 SendMessage(hWnd, WM_DESTROY)；函数总返回 0，调用者（0x004288B0）不看返回值。实现以返回 false 交调用者 Destroy()，与同步的 WM_DESTROY 等效。

## 0x0040F4A0 Game::StartStory — exact

- 释放 maps 数据，按文件长度 −0x200 分配、移到 0x200 读入（LoadMaps）；NewGame、Menu::InitActs（0x0043E900）、ClearWorld（0x00429B10）。表 4：领队（0x004D0AA0 的角色）eventOffset 0、id 与 partyLeaderId = +0xC 字、eventCursor 0、事件 100、旗标 0；音乐曲字段 6 dword 与 musicCue 清 0。参数为 1 时 currentMap=0x51，LoadCity(+0, +2, +4, +6, +8, +0xA, 0)。返回 1（无调用者使用）。

## 0x0040F680 Game::SetupFont — exact（偏差表地址更正）

- Shutdown、Init(显示对象)、SetTarget(GetSystemMetrics(0), GetSystemMetrics(1))、SetFont(0x004B0BF4 "細明體", 字号)；字号 20 时字距 0x18，16 时 0x12；背景 0xFFFE；阴影 0。屏幕尺寸的平台替换登记于 deviations.md 第 101 行，原写 0x004288B0，改为 0x0040F680（0x004288B0 与激活时调用），来源 asm/0040F680.asm。

## 0x0040F750 Game::FreeItemLists — exact

- 物品链逐节点释放（+0xB0 与节点）；四个技能头：链表释放、头的 +0xB0 与头释放、置空；64 个装备格同样（非空时）。

## 0x0040F840..0x0040F9E0 Scene 释放函数 — exact（注释更正）

- FreeEffects：0x004D3FBC 链（+0x14 下一项），释放 +0xC、+0x10 与节点。FreeSpritesA/B：0x004C6BD4、0x004D3F94 链（+0xBC）。FreeNodes：对话窗链（+0x48，释放 +0x38、+0x44 与节点），0x004C2FF8 &= 0x8000（注释由 swd32 的 0x004A9920 改为 0x004C2FF8）。FreeAnimations：0x004D15F4、0x004D1EBC 链（+0xAC）。FreeMarks：四个标记的链表释放后 0x40 字节清零。ResetLeaves：四片落叶 x=0x7FFFFFFF。FreeObjects：0x004C6150 链（+0x28），析构 0x00435BF0（注释原写 swd32 的 0x00430CF0）后释放。

## 0x0040FBE0 CheatProcedure、0x00410710 AddColumns、0x004107E0 InsertRow、0x00410950 SetRow、0x00410A80 Fill — exact（修正与偏差更正，Windows 专用源）

- 消息：WM_NOTIFY 且 code 为 TCN_SELCHANGE（−0x227）时页签 = TCM_GETCURSEL，删列 3..0，AddColumns、Fill，四个按钮（0x3F4、0x3EB、0x3EE、0x3EC）先启用、页签 ≥5 时再停用，返回 0。WM_INITDIALOG：十个页签 "物品"、四个 "%s法術"、四个 "%s屬性"、"變數"（名字缓冲区为错误数据，已登记），页签 0，AddColumns，LVS_EX_FULLROWSELECT，Fill，三栏 EM_LIMITTEXT 4、8、4，返回 1。WM_COMMAND 按控件号分派：
  - 0x3E8 关闭：0x004E6D08=1，EndDialog(1)，ShowCursor(0)。
  - 0x3EB 补满：页签 <5、有选中行且为物品页；SkipItems 取节点；数量+待加 <90 时待加=90，和 >90 时数量 = 90−待加；SetRow(值 0, 上限 0)。
  - 0x3EC 加入：页签 <5，编号栏非空；技能页编号须在 0x1771..0x1963；数量栏空或技能页取 "1"，附加值栏空或技能页取 "0"；AddItem(…, 模式 0)，空节点报 "CheatDialog"/"物品插入發生錯誤"；物品页按种类设计数（武器 0x0048A6A0 用 +0x50 上限、法术熟练 0x0048A810、计数 0x0048A500，物品 1..0x1F4 再设一次）；Fill，清三栏。
  - 0x3EE 删除：页签 <5、有选中行；AddItem(编号, −0x400, 0) 非空时报 "物品刪除發生錯誤"；Fill。
  - 0x3F6 设定：编号栏非空。变量页：编号 >0x3F 返回，数量栏非空时解析，写变量。页签 <5：始终在物品表找（0x00454D20），找不到返回；数量栏非空时数量 0、待加 = min(数量, 90)；附加值栏非空时解析；物品页设计数。属性页：编号 >0x10 返回，SetRoleStat(编号, 数量, 页签−5)。之后 Fill、清三栏。
- 差异与修正：编号栏不是数字时 0x00435AE0 不写结果，编号保留 WM_COMMAND 的 uMsg 参数槽的值 0x111；实现原先取 0（变量页会改写金钱）。改为 kUnparsedId = WM_COMMAND（src/game/cheat_dialog_win.cpp）。
- deviations.md：第 276 行改写为数量取栈残值、附加值取 lParam（按钮窗口句柄）的准确条件；第 274 行改为 swd3e.exe 资源模板（launcher/106.dlg），删去不属于外传的姓名对话框；第 275、276 行来源改为 asm/0040FBE0.asm。
- AddColumns：页签 ≥5 时 3 列，否则 4 列（"物品"、"數量"、"號碼"、"附加值"，0x004B1238、0x004B128C、0x004B1284、0x004B127C；宽 0x78、0x40、0x40、0x60；mask 0xF，cchTextMax 4，iSubItem=列号），返回 1。注释地址由 swd32 改为 swd3e。
- InsertRow：LVIF_TEXT|LVIF_PARAM|LVIF_STATE 插入第 row 行后与 SetRow 相同。SetRow：第 0 列名字，第 1、2 列 "%d" 数量与编号，第 3 列：值 −1 为空；上限 0 先写 "%d%%" 又被 "%d" 覆盖；上限 >1 "%d/%d"；上限 1 保留第 2 列的编号文字；其他 "%d"。实现 ValueText 与此一致。
- Fill：清空列表。页签 9：64 个变量，标签 0 "錢"、2 "符鬼"、11 "Cmpy1"、12 "Cmpy2"，其余首字节取 0x004E6D40（全程序只读、恒为 0，即空串），InsertRow(值 −1, 上限 0)。页签 5..8：17 项属性名（0x004B0C60 每 8 字节，tests 比对）与 RoleStat(项, 页签−5)，值 0、上限 0。页签 0..4：物品链或技能头链；物品页按种类取计数（武器 0x0048A770 百分比、上限 0；法术 0x0048A930 取上限与值；计数 0x0048A580、上限 −1；物品 1..0x1F4 再取计数），InsertRow(名字 +0xC, 数量+待加, 编号, 值, 上限)。

## 0x00411390 Party::SetRoleStat、0x004115D0 Party::RoleStat — exact

- 队伍记录 0x004C4F18 每 0x3C：项 0..13 为 +4..+0x1E 的字（写入低 16 位；读取 0..5 有符号、6..13 无符号），14 为 +0x20 dword，15 为 +0 dword，16 为等级字节 +0x2C（设定时再以 0x0048A010(角色+1, 等级+1, &经验) 成功时写 +0x20）；项号无符号 >0x10 时不做、读为 0；返回 0。

## 0x00411790 Field::InsertRole、0x00411830 UnlinkRole、0x00411890 RemoveRole、0x00411970 ResetRows — exact（越界补登）

- 行号 = y/16（有符号向零取整）+20。InsertRole：空行直接放入；只有一项时该项 y ≤ y（无符号）且该项 id ≥ 新 id 则接在后面，否则放在前面；多项时首项 y ≤ y 且新 id ≥ 首项 id 则放在最前（原有次序规则照搬），否则从首项起找"前项 y ≤ y、前项 id ≥ 新 id、后项 id ≤ 新 id"处插入，找不到接在末尾。UnlinkRole：找到则摘下、next 清空并返回，否则返回空。RemoveRole：从给定行到地图高−1 逐行找 id，摘下后 next 清空；keep 为 1 返回该角色，否则按旗标&3 InsertRole 后返回空；都没有返回空。ResetRows：三层各释放后分配 (高×4+0xA0) 字节并清零。
- 层号 3 与行号出界的越界访问：补登 deviations.md（0x00411790、0x00411890）。

## 0x00411A50 Game::DrawTextInput、0x00411B50 ActDrawer::DrawCounter — exact（空指针补登）

- DrawTextInput：先调用共用代码 0x0049D3B0（结果不用）；有图片参数时画 0x234E 于 (x−0x62, y−0x36)；CopyText 到 0x40 字节缓冲，CharNextA 遍历，单字节且为负（未配对的前导字节）改为 '@'；MakeColor(0x15, 0xF, 8)、样式 4 画字；光标框 FillBox(x+11×光标, y, 0xB, 0x16, 当前框 (0x14,0xD,0) 否则 (0xA,3,0), 模式 5)。DrawCounter：值逐位（do-while，无符号 /10）画 0x2377 的数字图（x 先减宽，y+1，透明度 0x004E582C，模式 0x14），再画 0x2355 于 (x−0x18, y)。两者取不到图片时原程序经空指针读取：并入 deviations.md 的空图片指针条目。

## 0x00411C10（已移除）— exact（平台替换）

- 命令行给出安装目录时：0x00428160、安装目录 = 参数+1、0x00428080、光驱号 = 参数首字符−'0'，以默认值 0x004277E0 写 Env.dat，返回 1；参数为空返回 0。整个功能按用户要求移除（deviations.md 第 100、102 行已列 0x00411C10），无对应实现。

## 0x00411C80 Scene::ResetEffects — exact（注释更正）

- 依次 FreeEffects、FreeSpritesA、FreeSpritesB、FreeNodes、FreeAnimations、FreeMarks、ResetLeaves；淡入淡出三个步长（0x004E55A0、0x004C2B90、0x004C30DC）清 0；震动表填 0xCFCF；热浪行数 64 字填 1、每行字节 64 字填 4；0x004C6154 起清 0x100 字节；0x004E6CD8 字清 0。热浪偏移成员只有 0x30 项（0xC0 字节），其后 0x40 字节在全部汇编中无引用，清零无可见效果；注释中的 swd32 地址 0x004AC9BC 改为 0x004C6154。

## 0x00411D10 Game::DiskFreeMegabytes — exact（修正）

- LoadLibrary("KERNEL32")、GetProcAddress("GetDiskFreeSpaceExA")，取不到时返回 −1；否则分配两块清零的 8 字节缓冲，GetDiskFreeSpaceExA(NULL, 可用, 总数, NULL)（返回值不查），结果 (低>>20)+(高<<12)（32 位）。修正：取不到剩余空间时实现原先返回 0xFFFFFFFF，改为 0（与调用失败时读到清零的缓冲区一致；KERNEL32 总有此函数）。deviations.md 第 78 行补充说明，来源改为 asm/00411D10.asm。

## 0x00411DA0 AngleBetween、0x00411E80 AngleToDirection — exact（注释更正）

- 差值与平方和按 32 位回绕，fild/fsqrt/_ftol 得距离写出，为 0 返回 0；正弦 |(dy<<13)/距离|；象限：dy<0 时 dx≥0 取 (0, 下标 0, +5)，否则 (90, 89, −5)；dy≥0 时 dx<0 取 (180, 0, +5)，否则 (270, 89, −5)；18 个候选中差 ≤ 最佳者取后者（0x004B086C 正弦表，tests 比对）。AngleToDirection：栈上 17 项 {3,7,7,0,0,4,4,2,2,6,6,1,1,5,5,3,0}，下标 角度×16/360（有符号）。注释中的地址改为 0x00411DB4..0x00411DC2。

## 0x00411F10 ErrorLog — exact

- 空函数返回 0；实现写入重建程序日志（与 DebugLog 相同的处理，见 game_state.md），对游戏无可见影响。

## 0x00411F20 Game::DrawPauseBanner — exact（注释更正）

- 0x40 字节缓冲复制 "遊戲暫停  按F8繼續遊戲"（0x004B12E0），半长 = 长度/2（有符号）；x = 0x140 − 11×半长；DrawBox(x, 0xE5, 22×半长, 0x16, 0,0,0,0)；MakeColor(0x19, 0x17, 0x11)、样式 4 画字；呈现。字符串注释由 swd32 地址改为 0x004B12E0。

## 0x00411FD0、0x004121C0（已移除）— exact（平台替换，偏差表补地址）

- 0x00411FD0：SetErrorMode 包住 GetVolumeInformationA，比较卷标（调用者给 "SWD3E_1"，0x004B0CE8）。0x004121C0：最小安装等光盘的画面循环（"c:\\" 起逐盘、timeGetTime、提示文字 0x004B132C..0x004B1374、按键放弃）。按用户要求不找光盘：deviations.md 第 100 行原只写 swd32 的 0x004118B0、0x00412050，补上 swd3e 的 0x004121C0、0x00411FD0；game_field.cpp 中的说明注释改为 swd3e 地址。

## 0x004120A0 Talk::ReadNamePlate — exact

- 安装目录 + "namex.dat" 写入 0x004D1F6C 路径缓冲（共用暂存），new 0x50 字节文件对象，GENERIC_READ、OPEN_ALWAYS 打开；移到 0x200+4×序号读 4 字节偏移，移到 偏移+0x200 读调用者给的字节数（唯一调用者 0x0042FA14 给 0x20 与 "%Q" 0x004B2720）；strstr 找到处写 0，关闭并删除文件对象，返回长度。找不到时写地址 0 已登记（deviations.md 第 321 行）。
