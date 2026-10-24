# 第一遍汇编验证：RolePath（0x00405150–0x00406780）

结论均为 exact。偏差见 deviations.md 的 0x00405220 两条（指令 0x24 未初始化偏移、指令 0x18/0x1B/0x1C 角色 -1 与方向越界）。

## 0x00405150 Update

- 角色 1..roleCount−1（无符号）：无旗标 0x8000 跳过；旗标 0x80（随行）调 Follow；旗标位 31 或 talkState==1：act.id 非 0 时 MarkRoleCells；pathNumber 为 0：旗标 0x1000 且不是主角时运行动作（失败报 "Act Err(Path:NoPathCycle)"，格式 0x004AF244），act.id 非 0 时 MarkRoleCells；否则 RunScript。

## 0x00405220 RunScript

- 起点 = path.dat 基址 + 0x200 + dword[0x200+4·pathNumber] + 2·pathStep。跳转表 38 项（0x00405288），各命令对照：
  - 0 Restart：pathStep=0 返回；1 Stop：pathNumber=0、pathStep=0 返回。
  - 2/3：pathStep+=2，写 action（act+8）/direction（act+0x34），运行动作，失败报 ChangQQ/chang_dir，返回。
  - 4 SetCount：pathWait（+0x22）=操作数，前进 2 继续。5：pathWait 非 0 则减 1，否则 pathStep+1；返回。6：同 5 后运行动作（失败报 Active）。
  - 7 FindPath（index, 命令指针）前进 3 继续。8 Step：x、y 有小数部分即返回；Step 返回 2 时前进 1、清旗标 0x4000000 继续，否则返回。
  - 9 WaitClock：操作数 ≤ clock（0x004C655C，无符号）时前进 2；旗标 0x1000 时运行动作（失败报 WTime）；前进过则继续，否则返回。
  - 0xA：操作数 > clock 低 16 位时前进 4，否则跳到 dword 目标。0xB SetClock：clock=操作数，>1000 置 0，只前进 1 字（操作数作为下一条命令执行，原有 BUG）。0xC Jump。
  - 0xD/0xE：GameFlag(操作数) xor (命令==0xE) 为 1 时跳转，否则前进 4。0xF/0x10：旗标表到 0xFFFF，全部（0xF，空表也算）/任一（0x10）成立时跳到表尾之后的 dword，否则前进 表长+4。
  - 0x11/0x12：置/清旗标，前进 2。
  - 0x13..0x17：变量号 ≥0x40 记 0x004AF3F8 日志并返回；值 >1000 记 0x004AF454 日志后照常执行；0x13 赋值、0x14 加（无符号和 >1000 时 1000）、0x15 减（负时 0）各前进 3；0x16（≥）/0x17（≤）成立时跳到 dword，否则前进 5。
  - 0x18 StartPath：查角色（0x0040C4D0）；在 72 个移动记录中找第一个属于它的：Scripted 类时 +8..+0xE 写 0xFFFF；Path 类时，坐标未对齐则清格、按方向表（0x004AA408/0x004AA428）逐步退回到格子边界、RemoveRole(id, 旗标&3, 原行, 0)，然后清空记录；pathStep+=2；对方 pathNumber=操作数 2、pathStep=0；对方就是自己时从新路径开头继续。
  - 0x19：clock > timeMark（0x004D3C78）+操作数（无符号）时跳转，否则前进 4。0x1A：timeMark=clock，前进 1。
  - 0x1B：清旗标 0xC000、ClearRoleCells，前进 2。0x1C：置旗标 0xC000，前进 2。
  - 0x1D PlaceRole：清格，x/y=操作数×16，格子指针重算，运行动作（失败报 MoveRole），MarkRoleCells，pathStep+=3，RemoveRole(id, 旗标&3, 0, 0)，返回。
  - 0x1E：坐标对齐时 Wander，返回。0x1F SetAct：三个操作数非 0xFFFF 时分别写 act.id/action/direction，pathStep+=4，运行动作（失败报 TakeActdata），返回。0x20：PlaySoundAt(操作数, x, y)，pathStep+=2，返回。
  - 0x21/0x24：查角色失败时报 "Path(Follow)"/"角色不在地圖上 GUID[%d]"，pathStep+=2 返回；对方坐标未对齐时返回；0x24 按对方朝向 0..7 组合 ahead/side（有符号）偏移（朝向 1 与 >7 无偏移赋值，见偏差），目标格或其右格是墙则退回对方格；FindPath；前进 2（0x24 再前进 2）；自己与对方到原点距离（各自整数平方和开方，自己的先截断）之差截断取绝对值 >128.0（0x004AA470）时置旗标 0x4000000；继续。
  - 0x22 ShowLabel：释放旧标签并清颜色；颜色=操作数，>10 置 0；从文本开头逐字节找字 0x5125（"%Q"），pathStep += (文本长+6)/2；复制文本为标签；返回。0x23：有标签时释放并清颜色，前进 1。
  - 0x25 JumpIfRoleNear：查角色失败报 "guid Err(Path:DISTANCE_ROLE)"/"guid(%d) not found"，pathStep+=2 返回；距离（dy²+dx² 开方截断）> 操作数×16 时前进 5，否则跳转。
  - 其他：报 "路徑系統"/"無效的指令\nGUID(%d)-(%d)"（RoleId(index), 命令），返回。
- 所有字符串已比对（0x004AF244、0x004AF324..0x004AF468）。

## 0x004061B0 FindPath

- 在 72 个移动记录中找 role==0xFFFF 的空位；没有时返回 (72≠0x20)=真。
- 目标像素 = 格×16；x > 地图宽×16 或 y > 地图高×16（无符号）时报 "路徑系統(piSetpos)"/"路徑終點超出範圍 >> P(%d) RId(%d) Start(%d/%d) Dest(%d/%d)"，返回 0。
- PathFinder::FindPath 成功：act/action/direction=-1、role=index、step=0、x、y、类别=(m&0xF1)|1（等于 SetKind(Path)）。失败：报 "路徑無法產生 >> P(%d) GUID(%d) Start(%d/%d) Dest(%d/%d)"（RoleId），角色直接移到目标。返回 slot≠0x20。

## 0x004063A0 Step

- 找属于角色的记录：Scripted 返回 0；Path 取用；都没有时报 "路徑"/"路徑沒有設定 >> GUID(%d)\n ACT (%d)\nQQ (%d)\nPATH (%d)"，返回 2。
- talkState==1：step|=0x8000，返回 0。当前方向 0xFF：清空记录，返回 2。
- 掩码：blockedTries（+0x1A 字节）>8 时 0x40000000，否则 0x60000000；ProbeCells；step|=0x8000、blockedTries+1、步进清零；BlockedBy（0x004AA3E8）不挡时 step&=0x7FFF、blockedTries=0、步进取 StepX/StepY（0x004AA408/0x004AA428）。返回 1。

## 0x00406530 JumpTo

- 新位置 = 基址+0x200+偏移；pathStep = (偏移 − 本路径起点) >> 1（算术右移）。

## 0x00406590 Wander

- 先调用 CRT rand（结果丢弃），再 Random::Below(0x50)。遍历全部记录（不中断）：Scripted 即返回 1，Path 记下（取最后一个）。没有时找空位（无则以 RoleId 记 "Pth:rndwlk,g(%d)NoFreePath" 日志返回 0），新建 Path 记录、方向[0]=1。
- talkState==1 或随机数 >0x3C：step|=0x8000、action=0，返回 1。否则 action=8；随机数 <8 时作为新方向；方向 0xFF 返回 1；掩码与探测同 Step；blockedTries+1、step=0x8000（整字赋值）、步进清零；不挡时 step=0、blockedTries=0、步进取步长表。返回 1。

## 0x00406780 Follow

- 坐标未对齐返回。在随行表（0x004D1610，序号从 1）找自己，找不到报 "CompanyWalk"/"夥伴紀錄中找不到角色(%d)"。
- 随行移动记录（0x004D2370 起）为空、blockedTries 非 0 或当前方向为 0xFF 时：清空记录，目标取轨迹 trailX[2·序号]（0x004D1380）、trailY（0x004D1300）；旗标 0x4F 时寻路掩码 0；寻路失败记 "CmpyWalk PathFind fail ..." 错误日志；成功则写记录（类别 Path、step 0、x、y、blockedTries 0）、清格，按相机范围（上下左 −0x50 截到 0，右 +0x50，下 +0xA0）把屏幕外的步数直接走完（每步 4×步长），到屏幕内时按 Facing（0x004AA448）转向；step|=0x8000，重算格子，MarkRoleCells，RemoveRole(id, 旗标&3, 0, 0)。
- 然后：掩码 0x40000000（旗标 0x4F 时 0），ProbeCells，step|=0x8000，步进清零；方向 0xFF：action=0、朝向取主角朝向；被挡：action=0；否则 step&=0x7FFF、blockedTries+1、action=8、步进为步长表×2。

## 0x00405220 RolePath::RunScript — 第一遍复核 exact（TileStart 修改后）

- FollowLeader 验证时修改了共用的 RolePath::TileStart（余数不能被步长整除时 Hang，原循环 0x004057E6／0x00405806 同样永不结束），RunScript 指令 0x18 的对齐调用受影响，恢复为待复核后重新核对：0x00405796..0x00405830 先判低 4 位、ClearRoleCells、取方向字节、按 0x004AA408／0x004AA428（与 StepX/StepY 相同）逐次减到格上、RemoveRole，与实现一致；函数其余部分源码未改动，结论维持。

## 0x00432630 RolePath::ResumeMove — exact

- 角色无 0x80000000 时返回 1。找 role == 序号且种类（+0x1B 低 4 位）> 1 的移动记录（72 项），找不到返回 0。
- 记录的下一个角色（+8）为 0xFFFF 时：种类不为 1 即 Clear（0x0040E0B0），返回 1。否则 role/x/y = next 三字，step = 0，种类 = 1（&0xF1|1 与 SetKind 相同），FindPath(角色 x, y, 目标 x, y, act 宽 +0x2C, 高 +0x30, 方向表 +0x1C)；不为 1 时以 “對話系統(unlock)”（0x004B2C5C）、“路徑無法還原 >> RId(%d)\n Start(%d/%d) Dest(%d/%d)”（0x004B2C70）报（序号、x>>4、y>>4、目标/16），返回 1。EH 尾 0x004A9650／0x004A9658 为寻路对象析构，注释改为 swd3e 地址。

## 0x00432800 RolePath::MoveRoleTo — exact

- 去 0x2000000。控制角色：行 = (y>>4)−1，按两组步长对齐（AlignPlayer），清 0x004D1288、键表 0x200 字节、0x004D0E80、0x004C2B64、0x004C2B60，RemoveRole(id, 层, 行, 0)。
- 找 role == 序号的记录（任意种类）：种类 1 时写 act/action/direction、next = 当前三字，选项 0x8000 时去 0x80；角色不在格点时 ClearRoleCells，非控制角色按当前方向退回格点并 RemoveRole（第 133 行偏差）。
- 选项低 4 位为 0 且没有记录时找空记录；仍没有：旗标 2 且低 4 位为 0 时报 “警告:對話系統(isetpos:mode0)”（0x004B2CF4／0x004B2D14，参数为选项与 RoleId），直接放到目的地、重标格子、RemoveRole(…, 0, 0)，控制角色停键、镜头未锁（0x004E5820 位 2）时居中，返回 0。
- 记录写入 act/action/direction、role、step 0、x、y，种类 2（&0xF2|2），0x8000 时去 0x80；x > 宽×16 或 y > 高×16（无符号）返回 0。低 4 位非 0：步长与速度清零、放到目的地后标格子、居中、RemoveRole，置 0x80000000 返回 1。否则 0x40000 时 SetMask(0)，FindPath 失败报 “對話系統(isetpos)”（0x004B2CA4／0x004B2CB8，参数 记录号、RoleId、起点、终点格）后同样直接放置。
- 成功：ClearRoleCells，可见范围 左−0xA0、上−0x50（负数取 0）、右+0xA0、下+0xA0，严格在范围内或方向 0xFF 为止跳过步（每步 4×步长），范围内时朝向取 Facing，到终点时套用 act/action/direction（−1 不改）；step |= 0x8000，重算格子、标格子、RemoveRole，置 0x80000000 返回 1。
- 只改注释：错误串地址、EH 尾地址、walkStepX（0x004C2B60）与 screenFlags（0x004E5820）的成员注释。

## 0x00432F90 RolePath::ScriptStep — exact

- 找 role == 序号且种类 2 的记录，没有返回 0。当前方向（step & 0x7FFF）为 0xFF：位置与目标不同时报 “對話系統(iMove)”（0x004B2D4C）“沒有到達終點\nGUID(%d)\nrx%d,ry%d\ndx%d,dy%d”（0x004B2D5C；id、x、y、目标 x、y），速度清零、去 0x44000000，控制角色停键并按镜头锁定居中，返回 2。
- 否则无 0x40000 时 ProbeCells(格子, 宽, 高, 0x40000000)，step |= 0x8000、尝试次数 +1，方向未被挡时去 0x8000、写两步长、次数清零；置 0x40000000 返回 1。注释地址改为 swd3e。

## 0x004332A0 RolePath::HoldRole — exact

- 控制角色：行 = (y>>4)−1、对齐、清四个步长（不清键表）、RemoveRole(id, 层, 行, 0)。找 role == 序号的记录：种类 1 时 step |= 0x8000、next = 当前三字；不在格点时 ClearRoleCells，非控制角色退回格点并 RemoveRole。置 0x80000000 返回 1。
