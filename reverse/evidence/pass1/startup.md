# 第一遍：启动、设置与释放（0x004271A0..0x00429B10）

同码比对：reverse/analysis/scripts/asm_identity.py 的输出存于 startup_identity.txt（与 painter_rle_identity.txt 同法；命名全局与栈变量名已归一，含 SEH_ 标签的 EH 处理块单独略去）。

## 0x004271A0..0x004277A0 像素格式（9 个）— exact（与 swd32 同码）

- ColorMasks、InitPixelFormat、ConvertPixels、ConvertPixelsBack、ConvertTo5551、ConvertTo565、ConvertFrom565、ConvertTo6551、MakeColor：与 swd32 0x004233D0..0x004239D0 逐条相同；src/draw/painter.cpp 中对应函数与 swd3-rebuild 已 exact 的版本除命名空间外相同。抽读 MakeColor：((r&31)<<10 | (g&31)<<5 | b&31) 复制到高字后经 ConvertPixels 转 2 像素，返回 32 位。

## 0x004277E0（已移除）、0x004278C0 Config::Update、0x00427BD0 Config::SaveKeys、0x00427D80 Config::ImportEnv — exact（平台替换）

- 与 swd32 0x00423A10/0x00423AF0/0x00423E00/0x00423FB0 逐条相同（swd3e 清单另带 EH 处理块）；swd3e 调用的 0x0043DA70/0x0043DAB0 是清零分配与释放，对应 swd32 的 malloc/free，实现用零初始化容器。源码与 swd3-rebuild 已 exact 的版本除命名空间与正传独有的 autoCombo 外相同。Env.dat 改为设置文件 [game] 见 deviations.md 第 102 行；0x004277E0 默认写 Env.dat 随之移除。导入时缓冲区越界见第 43 行。

## 0x00428080（已移除）— exact（平台替换）

- 分配 0x80 字节、0x004D0E60 起 32 字节填 −1，对 'j'..'z' 逐个 GetDriveTypeA，比较后的跳转已被 nop，计数一律加 1、盘符写入 0x004D0E60（只有已移除的 0x004121C0 读取）。随光盘检测按用户要求移除（deviations.md 第 100 行）。

## 0x00428100 Game::SizeCaches、0x00428160 Config::SetDefaultKeys — exact（与 swd32 同码）

- SizeCaches：GlobalMemoryStatus 的物理内存 /6（无符号）夹在 4 MB..16 MB（实现上限 256 MB，已登记）后设图片快取，脚本快取 0x80000，返回 1（调用者不读）。SetDefaultKeys：0x004D0B90 起 16 个键位写 1、0x39、0x36、0xCB、0xC8、0xCD、0xD0、0xC9、0xD1、0x9D、0xCF、（+0x30）0x1C、（+0x40..）0x13、0x1E、0x22、0x3B。两者源码与 swd3-rebuild 已 exact 的版本相同。

## 0x00428210 Launcher（对话框过程）— exact（平台替换）

- WM_INITDIALOG：对话框居中、背景 0x405 放 (0,0) 640×480，五个 0x70×0x28 控件 0x40E..0x412 放 (0xB8,0x140)、(0xB8,0x168)、(0x160,0x140)、(0x160,0x168)、(0x110,0x1A0)，两组点亮字节 0x004EF5A4..A8、0x004EF5A9..AD 清零，SetFocus，返回 1。WM_CLOSE：EndDialog(6)。
- WM_MOUSEMOVE/WM_LBUTTONDOWN：先把点亮字节存为旧值再清零；坐标取 lParam 的无符号低/高字；x∈[0xB8,0x128) 时 y∈[0x140,0x168) 且游戏旗标 0x12 为真点亮 0、y∈[0x168,0x190) 点亮 1；x∈[0x160,0x1D0) 时同样点亮 2、3；x∈[0x110,0x180)、y∈[0x1A0,0x1C8) 点亮 4。点亮写 5（SW_SHOW），旧值为 0 则记“新点亮”；wParam 恰为 1 时：0 EndDialog(1)、1 EndDialog(2)、2 开官网、3 开安装目录的 Readme.txt、4 EndDialog(6)，EndDialog 后照常判断其余项。随后五个控件按点亮字节 ShowWindow，新点亮时 PlaySoundA 资源 0x77。
- WM_COMMAND（低字 0x3FE..0x402）：0x3FE 先开姓名对话框 0x70 再 EndDialog(2)；0x3FF 官网；0x400 Readme；0x401 EndDialog(6)；0x402 EndDialog(3)。
- 实现 Launcher::Init/OnMouse/OnCommand/OnClose 同此；对话框与位图控件、PlaySoundA、姓名对话框的替换见 deviations.md 第 70、72 行（第 70 行资源来源已更正为由 scripts/make_resources.py 从 swd3e.exe 提取）。

## 0x00428750 Launcher::RunBrowser、0x00428880 Launcher::RunTextViewer — exact（平台替换）

- RunBrowser 与 swd32 0x00424930 同码：查 HKCR\http 与 http\shell\open\command 的默认值，从尾部截到最后一个空格，接空格与网址后 WinExec(SW_RESTORE)。RunTextViewer："notepad.exe " 加路径后 WinExec。替换为系统默认处理程序（deviations.md 第 71 行，地址与原行为描述已更正）。

## 0x00428730 Game::StartGraphics — exact

- Party::ResetNames（0x00429790）后 InitDrawKernel(hWnd)，返回 1（调用者不读）。

## 0x004288B0 Game::InitDrawKernel — exact

- 记 hWnd，inactive（0x004D3FC4）与帧计数（0x004C2B78）清 0，ShowCursor(0)；InitDevices；输入灵敏度 2.0、指针 (0x140,0xF0)；音乐音量 音量*127/11；Painter::Init(帧面 0x004C6344) 失败时 MessageBox(标题“DrawKernel init Failed.”、正文“SWD3”)、WM_DESTROY、返回。
- 地图快取 Check(1) 向上取整 MB，需要量 = 快取设置 0x004D1434 − 已用 + 0x40，无符号比较 ≥ 磁盘剩余 MB 时 Sleep(0x400)、MessageBeep(0)、错误框“!!! 注意 !!!”/“遊戲過程中,磁碟空間不足\n請確認至少剩餘 %dM磁碟空間.”（0x004B1F0C/0x004B1F1C，CP950 核对）；MarkDirty；锁定帧面写画笔目标 0x004EDA44 后解锁；0x004AF70C 三个通道掩码各转 1 像素；清屏色 0x004C5090 = 填充色对；以屏幕宽高建 0x14/0x10/0x0C 三种字体；对话单位 0x004C30D4 = 0xB；裁剪 (0,0,640,480)。
- 16 组文字颜色（0x004AF720、0x004AF760，exe 字节与 TextColors::Primary/Secondary 一致）由 COLORREF 折成 RGB555（红 &0xF8<<7、绿 &0xF800>>6、蓝 (c>>13)&0x7C0>>6）、转 1 像素、低字复制到高字。再锁定写目标；帧间隔 0x23 设定失败时 MessageBox(标题“SWD3”、正文“mmTimer init Failed.”)、inactive=1、WM_DESTROY、返回。
- GameDatabases::Open（返回值不看，失败时其内已 WM_DESTROY，实现以返回 false 交 Destroy，等效）；脚本缓冲重新分配 0x8000 清零；maps.dat 自 0x200 读入（LoadMaps）；寻路节点初始化（0x00401B30）、NewGame、Menu::InitActs、ClearWorld、menuState = 0x80000003。实现同此（SDL_HideCursor 等平台替换）；TextColors::Convert 注释的 swd32 地址已改为 0x004288B0。

## 0x00428C20 Game::InitDevices — exact（平台替换）

- 设备旗标 0x004C6334 清 0；Miles 启动（安装目录）、音效打开、串流以音效驱动打开；DirectInput 失败：旗标 |= 2、MessageBox(标题“軒轅劍三” 0x004B1F98、正文“DirectInput 初始化錯誤%n” 0x004B1F7C)、WM_DESTROY、返回 0；DirectDraw 640×480×16 失败：系统旗标 0x004D1404 |= 4、MessageBox(标题“DDraw init Failed.”、正文“SWD3”)、WM_DESTROY、返回 0；帧面 0x004C6344 = 640×480 离屏面、active（0x004D1630）= 1、Bink 以 DirectDraw 与 Miles 驱动初始化，返回 1。实现同此。

## 0x00428D60 Game::InitInstall — exact（平台替换、未初始化已登记）

- 光驱字节 2、音效与音乐 6、快取 0x3C、文字速度 2、战斗速度 0xB；光驱检测结果写 0x004C66AC（随光盘检测移除）；ImportEnv 读 Env.dat（第 7 个输出为 unknown14 的 dword，第 8 个为关闭特效，只写低字节）；SizeCaches（推入的 "Born.avi" 不被读取）；地图快取 Check(0)；建 Save、Data、Music、ScrnShot、Video 目录；图片档案设目录并打开；设游戏旗标 0x48，关闭特效为 0 时清除。实现以设置文件 [game] 代替 Env.dat（第 102 行），`running` 为 R 键走跑状态的保存（第 138 行）。第 288 行（关闭特效与 unknown14 未初始化）已改为 swd3e 地址，并补上有 Env.dat 时高 3 字节仍为栈残留。

## 0x00428E60 Game::MakeDirectory — exact（与 swd32 同码）

## 0x00428EC0 Game::ReleaseGame — exact（平台替换）

- 次序：GhostFollower::RemoveUnused、三种字体关闭、FreeSavePreviews、ReleaseBattle、Fame::Free、触发链表（0x004C3198 起 +0x14 的链表，节点 +0x14 链接）、Scene 的 FreeAnimations/FreeEffects/FreeSpritesA/FreeSpritesB、ClickRegions::Free、FreeNodes、FreeMarks、ResetLeaves、叠层链表（0x004C4A7C，+0x1C 链接，先释放 +0 的数据）、角落通知链表（0x004D1F4C，+0x84 链接，头节点 0x004D1EC8）、寻路节点释放、ClearRoles、三行表、FreeItemLists、动作档案关闭、图片快取写回、空的调试函数、StopAllSounds、遇敌组表 0x004E6CBC、画笔调色板指针 0x004EDA34（只有 Paint 在画地块时临时指向 0x004D0D58 并随即清零，这里为空）、帧面 Release、DirectDraw 面与设备、DirectInput；八个文件关闭失败各记“ReleaseGame : …”错误日志（exe 字节核对）；释放 0x004D0D58、0x004D12B4、0x004D12B0、0x004E5818、0x004D1DA8、0x004C6470（全库无写入，恒为空）与区域链表 0x004C6258；MarkClean、ShowCursor(1)、图片档案关闭、串流关闭、音效关闭、AIL_shutdown，返回 1。
- 实现同此；`mapBehind` 属宽地图增强。deviations.md 原第 274 行（调色板释放两次）在 swd3e 不成立（0x004EDA34 在释放时为空），已删除；注释里的 swd32 地址（调色板、触发表、叠层、通知头）已改为 swd3e 地址。

## 0x004291B0 MapCache::Check、0x00429520 MarkClean、0x004295F0 MarkDirty、0x004296B0 CacheBytes、0x00429780 Game::CheckStart — exact（与 swd32 同码）

- 与 swd32 0x00425570/0x004258E0/0x004259B0/0x00425A70/0x00425B40 逐条相同（swd3e 清单另带 EH 处理块，清零分配对应 malloc）。源码与 swd3-rebuild 已 exact 的版本对应；平台替换见 deviations.md 第 102 行。

## 0x00429790 Party::ResetNames — exact

- 0x004E4C30 与 0x004E4AF8 两张 4×0x20 字节表先清零，每个角色在 +0、+6、+0xC 依次 strcpy 三段。默认名：陳/靖仇/阿仇、于/小雪/阿雪、拓跋/玉兒/阿玉、張/烈/阿烈（0x004B2188、0x004B2180..0x004B213C）；对话用名首段为 ▲▲（0x004B2134）与 陳美（0x004B212C），其余同默认名。CP950 解码与 Party::DefaultNames/NameTags 一致，各段 ≤4 字节加结尾 0，不越出 6 字节。

## 0x00429B10 GameState::ClearWorld — exact

- 与 swd32 0x00425B50 只差角色步长（0xE4 对 0xD8）。释放地块与区块缓冲、地图头 0x004D1298 的 0x19 个 dword 清零、角色数清零；300 个角色（0x004D3FC8 起步长 0xE4）各释放 +0x38 的缓冲并清零 0x39 个 dword；领队 0x004D0AA0 的 0x39 个 dword 填 0xFF；72 条移动记录（0x004C6C88 起步长 0x21C）各调 0x0040E0B0。实现同此。
