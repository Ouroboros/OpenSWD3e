# 第二遍：role_path（路径搜索与节点池）

方法见 reverse/analysis/pass2-method.md。

## 0x00401B30 PathNodePool::Init — exact

汇编：分配并清零 0x58 字节的池头（0x004C15C8），头的 +0x28 置 0；空闲数 0x004C15C0 置 0 后 do-while 新建 8 个清零节点，逐个插到头后、空闲数加一；结束后空闲数与峰值 0x004C15C4 置 8，0x004C15BC 置 0。
对照：一致（源码 do-while，storage 只在启动时调用一次，game.cpp:867）。

## 0x00401BC0 PathNodePool::Release — exact

汇编：逐个摘下头后的节点并释放、空闲数减一，释放头、头指针置 0，返回 (空闲数==0)。
对照：一致（源码摘链计数后统一释放；只在退出时调用，game_window.cpp:172）。

## 0x00401C20 PathNodePool::Alloc — exact

汇编：池空时新建清零节点插到头后、空闲数加一，峰值 < 空闲数时峰值=空闲数；摘下头后第一个节点，空闲数减一，整块 0x58 字节清零后返回。
对照：一致（PathNode{} 各字段为 0）。

## 0x00401CA0 PathNodePool::Free — exact

汇编：参数为空直接返回（eax=0）；否则插到头后，空闲数加一，eax 为新空闲数。
对照：一致（源码返回值与 eax 相同）。

## 0x00401CD0 PathFinder::PathFinder — exact

汇编：+0x10 分配 8 字节并清零（栈），+0x14、+0x00、+0x04、+0x08 置 0，+0x28 置 0x40800000（Wall|Unk800000 掩码），返回 this。+0x0C、+0x18..+0x24、+0x2C 起未写。
对照：一致。未写字段在源码中初值为 0；是否在写入前被读，在各读取函数的行核对。

## 0x00401D00 PathFinder::~PathFinder — exact

汇编：调用 0x00401F00（Reset），+0x10 非空时释放并置 0。
对照：一致（vector 自行释放）。

## 0x00401D30 PathFinder::FindPath — exact

汇编：dirs[0]=0xFF；+0x20/+0x24 写宽、高；起点==终点返回 1（不改 found、不 Reset）；终点不可走或两点同格：found=0、Reset、返回 0；否则 Reset、Search(from, to)；IsUsable 为 0 时直接返回 0（不改 found、不 Reset）；从 result 沿 parent 逐步写方向：DirectionByDelta[DeltaY[dy+1]+DeltaX[dx+1]+4]（dx、dy 为当前减父节点的格坐标，sar 4），计数 >0x1FE 时每步 MessageBoxA("!!! 危險 步數超過510 !!!", "路徑計算") 且继续写；写 0xFF，found=1，Reset，返回 1。
对照：一致。文字与 0x004AF0F8、0x004AF114 的 Big5 原文一致；写出 512 字节之外见 deviations.md 0x00401D30 行。

## 0x00401E80 IsUsable、0x00401EB0 TileIndex、0x00401ED0 IsWalkable、0x00401F00 Reset — exact

- IsUsable：result 为空返回 0；found==0 返回 1；否则 (result.parent==0)。
- TileIndex：(y sar 4)·地图宽(0x004D1298) + (x sar 4)，32 位。
- IsWalkable：格值(0x004D12B0 数组)&掩码(+0x28) 为 0 返回 1，否则 0（`neg/sbb/inc`）。地图外见 deviations.md 第 51 行。
- Reset：open 为空返回；逐个摘下 open 链（poolNext）还给池，再还 open，置空。
对照：均一致。

## 0x00401F40 Search — exact

汇编：起点格 = TileIndex(from)；分配 open、closed、目标三个节点并串成 poolNext 链；目标 g=0、poolNext=0，h=f=fsqrt(fild(dy·dy+dx·dx))（32 位整数平方和，按像素），tile=TileIndex(to)，x、y=to；open.listNext=目标，closest=目标；循环 PopBest：空则 result=closest 返回 0；tile==起点格则 result=该节点并返回它；否则 Expand(节点, from)。
对照：一致。

## 0x00402020 PopBest — exact

汇编：open 表空时 ErrorLog("aStar ,Path find fail\n")（0x004AF120，0x00411F10 为空函数）返回 0；否则摘下首个，插到 closed 表头，返回。
对照：差异——源码日志文字缺少原串末尾的 `\n`，已补上（AGENTS.md：日志控制字符照原样保留）。

## 0x00402060 Expand — exact

汇编：UpdateBlocked(node.x, node.y)；按 blocked[7]、[0]、[1]…[6]（+0x48、+0x2C…+0x44）为 0 依次 TryNeighbour，坐标偏移 (−16,−16)、(0,−16)、(16,−16)、(16,0)、(16,16)、(0,16)、(−16,16)、(−16,0)，方向号 0..7。
对照：一致。

## 0x00402180 TryNeighbour — exact

汇编：g = fsqrt(dy²+dx²)+parent.g（dx、dy 为邻格减父节点，像素）；tile；在 open 中：挂为父节点首个空子槽，closest.h > 节点.h（`test ah,41h`）时 closest=节点，g < 节点.g（`test ah,1`）时更新 g、parent、f=g+h，返回；在 closed 中：同样挂接、比较，更新时另调 Propagate，然后 parent.children[0].direction = 0x004AA344[dir]；都不在：新节点挂到 open 的 poolNext 链，g、parent、x、y、tile、direction，h=fsqrt(dx²+dy²)（邻格减 target），f=h+g，InsertOpen，挂接，比较 closest。
对照：一致（运算顺序、比较方向与源码相同）。

## 0x00402460 FindOpen、0x00402480 FindClosed — exact

汇编：沿 listNext 查 tile 相等的节点，无则 0。对照一致。

## 0x004024A0 InsertOpen — exact（比较方向改为与汇编相同）

汇编：open 表空时直接接上；否则从首个起，`fcom` 后 `test ah,41h` 为 0（节点.f 大于当前.f）时后移，小于、等于或无序时停下，插在其前。
对照：源码 `!(node.f <= current->f)` 在 NaN 时继续后移，与汇编在无序时停下不同；改为 `node.f > current->f`（f 不会为 NaN，行为不变，写法与汇编同向）。

## 0x004024E0 Propagate — exact

汇编：先用 node.g 的副本逐子槽（遇空槽即停）计算 g+0x004AF0B0[槽]，小于（或无序）子节点 g 时更新 g、f=g+h、parent 并压栈；然后栈非空时逐个弹出，用其 g 同样处理子节点。
对照：一致（node 不会是自己的子节点，副本与直接读等价）。

## 0x004025B0 Push、0x004025E0 Pop — exact

汇编：Push 分配 8 字节单元 {节点, 栈顶} 作为新栈顶；Pop 取栈顶单元，栈顶改为其 next，释放单元，返回节点。后进先出。对照一致（vector）。

## 0x00402600 UpdateBlocked — exact

汇编：blocked 八项清零；tx=x sar 4，above=(y−16) sar 4；上行从 above·W+tx−1、下行从 ((y sar 4)+高)·W+tx−1 开始。宽≤2（有符号）时：上下各取 3 格，宽==2 再取第 4 格；NW=上0、N=上1、NE=上2、SW=下0、S=下1、SE=下2，宽==2 时各为相邻两格之或。宽>2 时：上下各取宽+2 格，j=0..宽−1 逐项或入 NW←上j、N←上j+1、NE←上j+2，下行同理。左列从 above·W+((x−16) sar 4)、右列从 above·W+tx+宽，高≤2 时各取 3 格（高==2 取 4 格），NW|=左0、W|=左1、SW|=左2、NE|=右0、E|=右1、SE|=右2，高==2 再或入下一格；高>2 时各取高+2 格，逐项或入。
对照：一致（宽、高≤0 按 1 处理，右列按原宽）。8 项栈数组的重叠见 deviations.md 第 52 行；地图外见第 51 行。

## 0x00402A90 SetMask — exact

汇编：+0x28 = 参数，返回参数。对照一致。

## 构造函数未写字段

+0x0C closest 在 Search 中先写后读；+0x20/+0x24 在 FindPath 中先写，UpdateBlocked 才读；+0x2C 起的 blocked 由 UpdateBlocked 先清零；+0x18/+0x1C 在本组函数中不被读。

## 0x00405150 RolePath::Update — exact

汇编：角色 1..数−1（无符号）：旗标无 0x8000 跳过；有 0x80 调 0x00406780(下标)；旗标位 31 或 +0x26==1 时 act.id 非 0 则 0x0040B270；+0x1C 非 0 调 0x00405220(下标)；否则旗标有 0x1000 且不是主角(0x004C4B00) 时 Run(act)，失败 ErrorBox「Act Err(Path:NoPathCycle)」0x004AF244 格式（id、act、动作、方向），act.id 非 0 时 0x0040B270。
对照：一致。

## 0x00405220 RolePath::RunScript — 差异已修正，exact

汇编：pc = 0x004C6338+0x200+dword[0x004C6338+0x200+4·路径号(+0x1C)]+2·步(+0x18)；循环取码，按 0x004060E4 跳转表（38 项）分派，>0x25 走默认。
- 0 Restart：步=0 返回。1 Stop：路径号=0、步=0 返回。2／3：动作(+0x48)／方向(+0x74)=操作数，步+2，Run，失败 ErrorBox「Act Err(Path:ChangQQ)」／「Act Err(Path:chang_dir)」，返回。
- 4 SetCount：+0x22=操作数，pc+4、步+2，继续。5：+0x22>0（无符号）时减一否则步+1，返回。6：同 5（为 0 时步+1、pc+2），然后 Run，失败「Act Err(Path:Active)」，返回。
- 7 FindPath：0x004061B0(下标, pc)，pc+6、步+3，继续。8 Step：x 或 y 低 4 位非 0 返回；0x004063A0 返回 2 时 pc+2、步+1、清 0x04000000 继续，否则返回。
- 9 WaitClock：操作数≤时钟(0x004C655C，无符号) 时 pc+4、步+2；旗标 0x1000 时 Run（失败「Act Err(Path:WTime)」）；通过则继续，否则返回。
- 0xA：操作数>时钟低 16 位 时 pc+8、步+4，否则 JumpTo(dword[pc+4])。0xB SetClock：时钟=操作数，>0x3E8 时 0；只前进 1 字（操作数作下一条指令）。0xC：JumpTo(dword[pc+2])。0xD／0xE：(旗标 ^ (码==0xE))==1 时跳 dword[pc+4]，否则 pc+8、步+4。
- 0xF：数旗标列表（0xFFFF 结束，用 0x004AA468 位表，与 GameFlag 的 0x004AA4BC 内容相同），全部置位（含空列表）跳结束符后的 dword，否则 pc+2·n+8、步+n+4。0x10：有置位时跳，否则同样前进。
- 0x11／0x12：置位／清位，pc+4、步+2。
- 0x13..0x17：槽≥0x40 时 DebugLog「變數不合法(%d)(%d)」（0x004AF3F8，原串无换行）返回；值>0x3E8（u16）时 DebugLog「數值不合法(%d)(%d)」（0x004AF454，无换行）；设／加（无符号和>0x3E8 取 0x3E8）／减（结果为负取 0）前进 3；0x16 变量<值（无符号）前进 5 否则跳 dword[pc+6]；0x17 变量>值 前进 5 否则跳。
- 0x18 StartPath：查角色（0x0040C4D0），在 72 个移动记录中第一个属于它的：Scripted 类 +8..+0xE 写 0xFFFF；Path 类坐标未对齐时清格、按方向表退回格边界、RemoveRole(id, 旗标&3, 原行, 0)，然后清空记录；本角色 pc+4、步+2；对方路径号 = [pc+2]（即同一个操作数），步=0；对方是自己时从新路径开头继续。
- 0x19：时钟>标记(0x004D3C78)+操作数（32 位，无符号比较）时跳 dword[pc+4]，否则前进 4。0x1A：标记=时钟，前进 1。
- 0x1B／0x1C：查角色，旗标清／置 0xC000（清时另 ClearRoleCells），pc+4、步+2，继续。
- 0x1D PlaceRole：ClearRoleCells，x、y=操作数·16，格指针 = 格网+4·((y shr 4)·宽+(x shr 4))，Run（失败「Act Err(Path:MoveRole)」），MarkRoleCells，步+3，RemoveRole(id, 旗标&3, 0, 0)，返回。
- 0x1E Wander：坐标对齐时 0x00406590(下标)，返回。0x1F SetAct：三项各自非 0xFFFF 时写 act.id／动作／方向，步+4，Run（失败「Act Err(Path:TakeActdata)」），返回。0x20 PlaySound：0x004270B0(音, x, y)，步+2，返回。
- 0x21／0x24：查角色失败 ErrorBox「Path(Follow)」「角色不在地圖上 GUID[%d]」步+2 返回；对方未对齐返回；0x21 取对方格；0x24 按对方方向用 (i16)操作数 2、3 算偏移（方向 1 与 >7 取栈上残值，见第 119 行），目标格或其右格有 0x40000000 时退回对方格；FindPath(下标, 格)，前进 2（0x24 再前进 2）；|ftol(ftol(√(本 y²+本 x²)) − √(对方 x²+对方 y²))| > 128.0（0x004AA470）时置 0x04000000；继续。
- 0x22 ShowLabel：释放旧标签；颜色=操作数，>0xA 取 0；从 pc+4 逐字节找 0x5125，步 += ((结束−pc+2)/2)&0xFFFF；复制文字；返回。0x23 ClearLabel：有标签时释放并清颜色，前进 1，继续。
- 0x25：查角色失败 ErrorBox「guid Err(Path:DISTANCE_ROLE)」「guid(%d) not found」步+2 返回；距离 = ftol(√(dy²+dx²))（本减对方），>操作数·16（有符号）时前进 5，否则跳 dword[pc+6]。
- 默认：ErrorBox「路徑系統」「無效的指令\nGUID(%d)-(%d)」(RoleId(下标), 码)，返回。
对照：
- 差异 1：指令 0x18 源码按两个操作数前进 3 字并取第 2 个字作路径号；汇编只前进 2 字（步+2、pc+4），路径号取 [pc+2]（与查角色的 guid 是同一字）。已改为一个操作数。（第一遍证据写作“操作数 2”，与汇编不符。）
- 差异 2：0x13..0x17 两条 DebugLog 源码多了末尾 `\n`，原串（0x004AF3F8、0x004AF454）没有，已删去。
- 差异 3：0x1D 坐标在地图外时源码把格指针置空，未登记；已补登 deviations.md（0x00405220 指令 0x1D 行）。
- 其余指令的分支、出口、前进字数、位宽、调用顺序与错误框原文一致。

## 0x004061B0 RolePath::FindPath — exact（错误框坐标改为无符号移位）

汇编：在 72 个移动记录中找角色字为 0xFFFF 的空位，没有时返回 (72≠0x20)=1；目标 x、y = 操作数·16（u32），超出 16·地图宽／高（无符号）时 ErrorBox「路徑系統(piSetpos)」「路徑終點超出範圍 >> P(%d) RId(%d) Start(%d/%d) Dest(%d/%d)」(槽, 下标, x shr 4…) 返回 0；PathFinder::FindPath(角色 x、y, 目标, 宽, 高, 记录方向)：为 1 时记录 act／动作／方向 = −1、角色=下标、步=0、x、y=目标低 16 位、类型 (b&0xF1)|1；否则 ErrorBox「路徑無法產生 >> P(%d) GUID(%d) Start(%d/%d) Dest(%d/%d)」(槽, RoleId, …) 并把角色放到目标；返回 槽≠0x20。
对照：一致（SetKind 掩码 0xF 与 &0xF1|1 结果相同）；差异——错误框中角色坐标源码用有符号移位，汇编为 shr，已改为无符号。

## 0x004063A0 RolePath::Step — exact

汇编：在 72 个记录中找属于角色的：Scripted 类返回 0，Path 类取用；没有时 ErrorBox「路徑」「路徑沒有設定 >> GUID(%d)\n ACT (%d)\nQQ (%d)\nPATH (%d)」返回 2。+0x26==1 时步|=0x8000 返回 0；方向 0xFF 时清空记录返回 2；尝试次数(u8)>8 时掩码 0x40000000 否则 0x60000000，探查四周；步|=0x8000、尝试加一、两步长清零；0x004AA3E8[方向] 不阻挡时步&=0x7FFF、尝试清零、步长取 0x004AA408／0x004AA428；返回 1。
对照：一致（BlockedBy 与 0x004AA3E8 相同）；方向越界见第 122 行，格指针空并入空指针行。

## 0x00406530 RolePath::JumpTo — exact（差值改为 32 位）

汇编：pc = 数据+0x200+偏移；步 = (偏移 − 路径起点) 按 32 位相减后 sar 1；返回 1。
对照：源码以 64 位 ptrdiff 相减，偏移小于起点 2^31 以上时与 32 位结果不同；改为 u32 相减转 i32 再算术右移。

## 0x00406590 RolePath::Wander — exact

汇编：先 rand()（0x004A14EC，结果丢弃），再 0x0043DB90(0x50)；在记录中找属于角色的：Scripted 返回 1，Path 取最后一个；没有时找空位，没有空位 DebugLog「Pth:rndwlk,g(%d)NoFreePath」(RoleId) 返回 0，有则角色=下标、步=0、x=y=0、类型 Path、方向[0]=1（不动尝试次数）；+0x26==1 或掷值>0x3C（有符号）时步|=0x8000、动作=0 返回 1；动作=8，掷值<8 时方向[0]=掷值；方向 0xFF 返回 1；同 Step 的掩码、探查，尝试加一、步=0x8000、步长清零；不阻挡时步=0、尝试=0、取步长；返回 1。
对照：一致（随机数消耗两次、顺序相同）。

## 0x00406780 RolePath::Follow — exact（注释改为 swd3e 地址、日志坐标改为无符号移位，补登格指针保护）

汇编：未对齐返回；在跟随者表(0x004D1614 起) 1..数(0x004D3FC0)−1 找下标，找到末尾（=数）时 ErrorBox「CompanyWalk」「夥伴紀錄中找不到角色(%d)」返回（数为 0 时用槽 1）；记录 = 0x004D2370+槽·0x21C；角色字为空、尝试≠0 或当前方向为 0xFF 时：清空记录，目标 = 0x004D1380[2·槽]（x）、0x004D1300[2·槽]（y），旗标 0x4F 时掩码 0，寻路失败 ErrorLog「CmpyWalk PathFind fail ,G(%d) sx(%d/%d) sy(%d/%d) dx(%d/%d) dy(%d/%d)」（shr 4）；成功则类型 Path、角色、步 0、目标、尝试 0，ClearRoleCells，以镜头外扩 0x50（上、左，负取 0）／0xA0（下）／0x50（右）为界，屏幕外时逐步前进（步加一，坐标加 4·步长），进入屏幕时按 0x004AA448 设朝向；步|=0x8000，按坐标设格指针，MarkRoleCells，RemoveRole(id, 旗标&3, 0, 0)。之后：方向、掩码（旗标 0x4F 时 0 否则 0x40000000）探查，步|=0x8000，两步长清零；方向 0xFF 时动作 0、朝向取主角；阻挡时动作 0；否则步&=0x7FFF、尝试加一、动作 8、步长 2·StepX／2·StepY。
对照：一致；scene.h 中 trailY、trailX、followerCount 注释改为 0x004D1300、0x004D1380、0x004D3FC0；ErrorLog 中的移位改为无符号；按坐标设格指针的保护并入 deviations.md 0x1D 行，格指针空并入空指针行。

## 0x0040C460 RolePath::RoleId — diff → exact

汇编：index ≥u 角色数时 wsprintfA("%d", roles[index].id) 并 MessageBoxA(hWnd, 文字, "Illegal Role id", 0)；返回 roles[index].id（0x004D3FEC，步长 0xE4）。
对照：一致（先取 id 再报告，报告内容相同）。差异：下标越出数组时取最后一个角色的保护未登记。修正：补登 deviations「0x0040C420、0x0040C460 越界」。复核通过。

## 0x00405220 RolePath::RunScript — 第二遍复核：diff → exact

核对 0x0040DB00 时发现：移动记录为路径类的角色在格间时，原程序在 0x004057AA 以 `shr ebx, 4` 求行号（无符号），源码写成 `other.y >> TileShift`（有符号）。修正为无符号右移。复核：该分支（0x00405784..0x0040583F：类型 1、低 4 位判断、0x0040B180、方向 = 记录 +0x1C+(step&0x7FFF)、x/y 按 0x004AA408／0x004AA428 减到对齐、0x00411890(id, flags&3, 行, 0)）其余与源码一致；函数其余部分沿用本遍前次逐段核对的结论，未改动。

## 0x00432630 RolePath::ResumeMove — exact

汇编：角色旗标位 31 为 0 → 返回 1。找角色号相同且种类 >1 的移动记录，找不到 → 返回 0。下一条的角色字 +8 为 0xFFFF → 种类不为 1 时 0x0040E0B0 清除（循环已保证种类 >1）→ 返回 1。否则构造寻路器（0x00401CD0），+0 = +8、+4 = +0xC、+6 = +0xE、步号 +2 = 0、模式 &0xF1|1；0x00401D30(x, y, 目的 x, 目的 y, 宽 +0x6C, 高 +0x70, 方向串 +0x1C) ≠ 1 → 「對話系統(unlock)」「路徑無法還原 >> RId(%d)\n Start(%d/%d) Dest(%d/%d)」(序号, x>>4, y>>4, 目的x/16, 目的y/16)；析构，返回 1。
对照：一致（SetKind(Path) 与 &0xF1|1 结果相同）；角色 −1 见 deviations 128；EH 注释为 swd3e 地址 0x004A9650、0x004A9658。

## 0x00432800 RolePath::MoveRoleTo — exact

汇编：构造寻路器；清旗标 0x2000000。控制角色：行 = (y>>4)−1，x、y 各按 (0x004C2B60+0x004D0E80)、(0x004C2B64+0x004D1288) 递减到对齐，四个步长与按键 0x004D1638（0x80 dword）清 0，0x00411890(id, 层, 行, 0)。找角色号相同的移动记录：种类 1 时写结束动作/动作/方向、把目标存为下一条（+8/+0xC/+0xE），选项 0x8000 清模式位 7；角色不在格上时 0x0040B180，非控制角色时按当前步方向逐步退到格首（步长表 0x004AA614/0x004AA634）并 0x00411890(行=(y>>4)−1)。选项低 4 位为 0 且无记录 → 找空记录（角色字 0xFFFF）。仍无 → 旗标 2 且选项低 4 位为 0 时「警告:對話系統(isetpos:mode0)」(选项, 0x0040C460(序号))；0x0040B180、x/y 取参数低字、按目的算格指针（不查地图范围）、0x0040B270、0x00411890(…,0,0)；控制角色时清步长与按键、镜头未固定（0x004E5820&2）时 0x0040D410；返回 0。有记录：写动作/动作/方向、角色、步号 0、目的、模式 &0xF2|2，0x8000 清位 7；x > 宽×16 或 y > 高×16（无符号）→ 返回 0。选项低 4 位非 0 → 步长清 0、速度 +0x88 清 0、x/y 置目的、0x0040B180、格指针、0x0040B270、控制角色且镜头未固定时 0x0040D410、0x00411890(…,0,0)、置持有、返回 1。否则：穿墙旗标 0x40000 时 0x00402A90(0)；寻路失败 → 「對話系統(isetpos)」「路徑無法產生 >> P(%d) crId(%d)\n Start(%d/%d) Dest(%d/%d)」(记录号, 0x0040C460(序号), x>>4, y>>4, 目的x>>4, 目的y>>4)，按放置同样收尾返回 1；成功 → 0x0040B180，视野 = 镜头左−0xA0（负取 0）、上−0x50（负取 0）、右+0xA0、下+0xA0；逐步：方向 = 方向串[步号]，角色在视野内（无符号开区间）→ 方向非 0xFF 时面向 = 0x004AA654[方向]，否则取记录的动作/动作/方向（−1 不改）；在视野外且方向为 0xFF → 同上取记录值；否则步号+1、x/y += 步长×4；步号置位 15、格指针、0x0040B270、0x00411890(…,0,0)、置持有、返回 1。
对照：AlignPlayer（两个步长之和，步长为 0 时 Hang）、stopKeys、各分支顺序、错误框参数（含 mode0 把选项填进 GUID 的原有 BUG）、边距 kFarMargin 0xA0／kNearMargin 0x50、kStepsPerTile 4、Facing 表 {5,1,6,2,4,0,7,3}（exe 0x004AA654 比对一致）一致。方向 8 以上或 0xFF、步号越过 0x200 见 deviations 135；无记录时目的在地图外的格指针见 342（本次补入本函数）。EH 注释为 swd3e 地址 0x004A9670、0x004A9678。

## 0x00432F90 RolePath::ScriptStep — exact

汇编：找角色号相同、种类 2 的移动记录，找不到 → 0。方向 = 方向串[步号&0x7FFF]：0xFF → 坐标与目的（字）不同时「對話系統(iMove)」「沒有到達終點\nGUID(%d)\nrx%d,ry%d\ndx%d,dy%d」(id, x, y, 目的x, 目的y)；速度 +0x88 = 0、旗标 &0xBBFFFFFF；控制角色时清步长与按键、镜头未固定时 0x0040D410；返回 2。否则：无穿墙旗标时 0x0040BF50(格指针 +0xC, 宽, 高, 0x40000000)；步号置位 15、受阻次数 +0x1A 加 1；BlockedBy 表 0x004AA5F4[方向] 与探查结果不相交 → 清位 15、步长 = 0x004AA614/0x004AA634[方向]、受阻次数 0；置 0x40000000，返回 1。
对照：一致；BlockedBy 表值 {0x10,0x20,0x40,0x80,1,2,4,8}（exe 比对）都在低字节，源码把探查结果截为 u8 不影响相与结果。方向 8 以上读表外与步号越过 0x200 见 deviations 135（补入本函数），格指针为空见 140（补入本函数）。

## 0x004332A0 RolePath::HoldRole — exact

汇编：控制角色：行 = (y>>4)−1，x/y 各按两个步长之和递减到对齐，四个步长清 0（按键不清），0x00411890(id, 层, 行, 0)。找角色号相同的第一条移动记录：种类 1 时步号置位 15、把角色/目的存为下一条；角色不在格上时 0x0040B180，非控制角色时按当前步方向逐步退到格首并 0x00411890(行=(y>>4)−1)。置持有，返回 1。
对照：一致；方向 8 以上或 0xFF 见 deviations 135。
