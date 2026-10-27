# 第二遍：Talk（对话窗与文字）

## 0x0040B370 Talk::SetupDialog — diff → exact

汇编（独立推导）：
- hasOrigin = fromX|fromY ≠ 0；fixed = flags&0x800；mode = options&0x7FFF；文字起点：mode 0→记录+4，1→+0xC，2→+8，其他→窗口指针本身；options&0x10000 且 fixed→+4。从起点逐字节找字 0x5125（"%Q"），分配 0x200 字节清零，+0x38／+0x3C／+0x40 指向它，复制到 %Q 末尾（不限长）；0x0040BB90 量出 行数<<16|字数。
- options&0x20000：在 0x004E5800 链表尾挂新节点，复制 lstrlenA+8 字节并在第 len+8 字节写 0（新增偏差）。
- 宽高（u16 乘 0x004C30D4）：单行→字数×unit、2×unit；mode 1→记录字 4、5；mode 2→字 2、3；mode 0→行数 ≤3：16×6，=4：18×8，>4：20×10；其他 mode 跳回扫描循环（deviations 420，调用者只传 0～2）。
- 说话者默认 0xFFFD；字 0 ≠0xFFFD 且非 fixed 时 0x0040C4D0，失败→错误框（0x004B0DA4／0x004B0DBC）、释放文字、窗口前 0x4C 字节清零、返回 0。
- 对话框 act：0x004E6880 起 4 个（步长 0xA4）中找 id==字 1，找不到→错误框 "SetupDialog"（0x004B0D88）用第 0 个；frameAct=它，0x00436C80 失败→错误框 "Act Err(Dialog:Frame)"（0x004B0D58，参数 id、+8、+0x34）。名字非空：nameAct=0x004C27B8 同下标，分配 0x20 并 lstrcpyA，0x00436C80 失败→"Act Err(Dialog:Caption)"。
- role=说话者、+0x14=0、+0x26=0、+0x2A=0、speed=2×0x004C3654、color=shade=style=4、next=0；flags&0x80：color=shade=5，说话者有效时 lift = 位移表 Y[面向] ≤0 ? 0x20 : −0x30。role==0：roles[0].x=镜头左+0x40、y=镜头上+0x10。
- x、y = 记录字 2、3；options&0x40000：x −= width>>1。单行且 fixed：x = offsetY−height−0x10（原 BUG），x、y <u 0x10 → 0x10，x+width < 0x260 → 0x260−width，y+height < 0x1D0 → 0x1D0−height，返回 1。mode 不是 0、2 → 返回 1。
- 无 origin：role 0xFFFD → fromX/fromY = leader.x/y，x、y = from − 半宽高 − 镜头；role≠0 → 角色坐标（u16）− 半宽高 − 镜头。
- 非 fixed 且 role≠0xFFFD：options&0x8000 → 加 offsetX/Y，否则加位移表 X/Y[面向]（Y 再加 lift）；否则加表第 1 项（0, −104）。夹紧：(i16)x<0x1E→0x1E，x+w≥0x240→0x240−w，(i16)y<0x28→0x28，y+h≥0x1C8→0x1C8−h。
- 非 0x8000、非 0x80、role 有效：角色屏幕坐标（u32）落在 (x−0x10, x+w+0x30) 且 (y−0x10, y+h+0x40) 内（无符号比较）时再加一次位移表，夹紧到 0x18／0x20／0x240／0x1C8，返回 1。
对照：源码各分支与常量一致；位移表 0x004AA4C4／0x004AA4E4 读出 {0,0,160,−160,80,−80,80,−80}／{96,−104,−64,−56,56,−112,−112,56} 与 Talk::WindowShiftX/Y 一致；fixed 分支的 kAboveSpeaker 0x68 等于表第 1 项；page／typed 为 0 等价于指针指向文字开头；失败时 TalkWindow{} 加 page=Closed 对应原程序清零（文字指针为空）。已登记的越界：deviations 123、124、420。
差异：对话记录的复制越界未登记。修正：补登偏差。复核通过。

## 0x0040BB90 Talk::ExpandText — diff → exact

汇编（独立推导）：
- 局部 0x2A 个双字清零（含帧顶 lpMem，之后只被释放和置 0，是死代码）；consumed=0。首字为 %Q 直接测量。循环：字节 0 停；
  - "%T"：consumed+=2，号码字符写进 0x004D1F6C+2 直到 '.'，consumed+=1，指针到 '.' 之后一字节；长度（含 "%T"）≥7 时补 '.' 并错误框（0x004B0E04 "物品名稱代換"／0x004B0E14 "!! 警告 !!\n號碼字串太長\n%s"）；0x00435AE0(号码, &n)，n 初值为长度；失败→补 '.'、错误框 0x004B0DF0（"錯誤的號碼字串\n%s"），指针与 consumed 再 +1；成功→0x00489B00(局部记录, n)，失败→错误框 0x004B0DE0（"找不到物品 %d"），同样 +1；成功→0x0040BEA0(起点, "%T…."，名字, 0x1FE−consumed)，指针 = 起点+strlen(名字)，consumed += strlen。
  - 其他：3 个角色 × 偏移 {0, 6, 0xC}，0x0040BEA0(指针, 0x004E4AF8+偏移+角色×0x20, 0x004E4C30+…, 0x1FE−consumed) 首个返回 1 者 skip=strlen(名字)−1，否则 0；指针与 consumed 加 skip+1。
  - 下一字为 %Q 结束。
- 测量：chars=0、column=0、lines=1；首字 %Q 返回 0x10000。%N／%L／%K／%P：+2，column=0，lines++；%S／%C／%D：+3；%B／%A：+2；其他：+1，chars++，++column >u 0x14 时 column=0、lines++；字节 0 或 %Q 结束；返回 (lines<<16)+chars。
对照：源码两阶段一致（kItemCodeCounted 3、kSkippedCodeBytes 4、kItemDigits 5、kExpandLimit 0x1FE、名字表 0x004E4AF8／0x004E4C30、各控制码值），4 条错误串与 exe 逐字一致（Big5 转 UTF-8，含 \n）。
差异：扫描与循环在文字末尾停止的保护未登记（原程序可读出文字缓冲区、写出 0x004D1F6C）。修正：补登偏差。复核通过。

## 0x0040BEA0 Talk::ReplacePrefix — diff → exact

汇编：pattern 首字节为 0 → 返回 0；逐字节比较到 pattern 的 NUL，不同返回 0；len = lstrlenA(replacement)（调用 3 次取最后一次）；memmove(text+len, text+patternLen, capacity−len)；复制 len 字节；返回 1。
对照：源码一致（先取尾部再写回等价于 memmove 的重叠语义）。
差异：capacity−len 不为正时不移动、写入止于缓冲区末尾的保护未写进偏差。修正：扩充 deviations「0x0040BEA0 越界」一行。复核通过。

## 0x0040BF20 Talk::AppendWindow — exact

汇编：从哑头 0x004C66B0 沿 +0x48 走到最后一个窗口，last+0x48 = window，window+0x48 = 0。
对照：源码一致（scene.talkWindows 对应哑头的 next）。

## 0x004120A0 Talk::ReadNamePlate — diff → exact

汇编：路径 = 安装目录+"namex.dat"（写进共用缓冲区 0x004D1F6C）；new File，OPEN_ALWAYS、GENERIC_READ；定位 0x200+4×index 读 4 字节偏移；定位偏移+0x200 读 size 字节到缓冲区；strstr(缓冲区, 查找串)，结果处写 0；关闭并释放；返回 结果−缓冲区。唯一调用者 0x0042BAF0 传 (index, 0x20, 0x004F43E0, "%Q" 0x004B2720)。
对照：一致。差异：偏移未初始化与无 NUL 越读两点未写进偏差。修正：扩充 deviations「0x004120A0」一行。

## 0x00411FD0、0x004121C0（按用户要求去除）— exact

光盘卷标检查与等待光盘：已登记的平台替换（deviations 101），无对应源码。

## 0x0042B4C0 Talk::HandlePointer — diff → exact

汇编：取控制角色（0x004C4B00）与镜头左上（0x004C5118/0x004C511C），找到 = −1、允许区域 = 1。领头 id = 0xFFFF 且控制角色动作为 0 或 0x34 时，从角色 1 起（角色数无符号 ≤1 时找到 = 1）逐个检查：旗标有 0x8000、无 4、事件 +0x1E ≠ 0、+0x26 = 0；图片 = 0x004367E0(+0x8A, +0x8C)，非空且指针（有符号）落在 (x−动作x−镜头左, y−动作y−镜头上) 起的宽高之内（两端不含）→ 命中：旗标无 0x20 时指针 = 9，有 0x40 时指针 = 0xA 且不允许区域。随后旗标 9（0x0040DFC0）为 0、选项区非空（0x004E4DCC）、控制角色动作 0 或 0x34 → 0x0040DED0 取指针下选项，指针 = 0xD；有选项 → 选项号 0x004E6CC0 = 下标，左键按下且持续 1 帧 → 0x0040DF30 释放选项、左键四个 dword 清 0、日志「\nButton[%d]Pressed」；返回。找到 ≠ −1、≠ 角色数、≠ 控制角色、领头 id = 0xFFFF、+0x26 = 0、左键按下且 1 帧 → 方向 = 0x0040E3A0(x+宽×8, y+高×8)；对方旗标 0x800 → 保存动作与方向、动作 0、延时 0、方向 = 方向，0x00436C80 失败报「Act Err(Talk:RoleTurn)」；控制角色动作 0、方向 = (方向 & 0xFFFE) + ((方向−1) & 1)、延时 0（不保存），失败报「Act Err(Talk:MRoleTFace)」；领头复制对方 +0x14、+0x20、+0x1E、id、旗标、x、y；左键清 0；日志「\nRoleEvent.GUID[%d].Talk[%d].Ridx[%d].RArgu[%x]」(id, 事件, 0x0040C420(id), 旗标)。否则：领头 id = 0xFFFF、允许区域、控制角色站立时，格子 = ((指针y+镜头上)>>4)×宽 + ((指针x+镜头左)>>4)（32 位 imul/add，无符号移位），区域号 = 区块表[格子]&0xFF ≠ 0，区域 = 0x0040DFA0(号)，旗标 0x0040DFC0(区域+0xC 低字) 为真 → 指针 = 0xB；左键按下且 1 帧 → 领头 +0x14 = 0、+0x20 = 0、旗标 0、事件 = 区域+8 & 0x7FFF、x/y = 指针 &~0xF + 镜头、id = 0xFFFD；方向 = 0x0040E3A0(指针+镜头)；控制角色保存方向与动作、动作 0、方向取反向、延时 0，失败报「Act Err(Talk:MouseActiveStory)」；0x004C2FF8 = 0x8000；日志「\nMapEvent.Talk[%d]」；返回。其余：0x00411DA0(指针x, 指针y, 控制角色 x+宽×8−镜头左, y+高×8−镜头上, &距离) → 0x00411E80 → 方向；指针 = 0xD 时指针 = 方向；控制角色 (x|y)&0xF = 0、0x004C2FF8 = 0、右键按下计数低 4 位非 0、距离 ≥ 16 → 方向 {0,4,7} 上键、{1,5,6} 下键、{2,4,6} 左键、{3,5,7} 右键的按下计数 = 右键计数；对话窗口存在（0x004C66F8）且空格按下为 0 → 空格四个 dword = 左键。
对照：分支顺序、有符号比较、按键槽（基址 0x004D1638，每槽 0x10）、方向枚举、日志与错误字符串（exe 0x004B23C8、0x004B2364、0x004AF1D0、0x004B23B0、0x004B2394、0x00004B2344）一致；0x00411E80 只返回 0–7，(方向 & 0xFFFE) 与 & ~1 等价；区域为空与格子越表见 deviations 142。
差异/修正：格子下标源码用 64 位 size_t 计算，原程序为 32 位乘加会回绕；改为 u32。复核：完整函数重新对照一致。

## 0x0042BAF0 Talk::RunEvent — diff → exact

汇编（外层）：事件角色 id 0xFFFF → 返回 1。事件偏移 +0x14 为 0 时：控制角色 (x|y)&0xF ≠ 0 → 返回；id ≠ 0xFFFD → 0x0040C4D0 找角色并置旗标 0x80000（找不到为角色 −1，见 deviations 128）；Talk<事件/2000+1>.dat（wsprintf 到 0x004C4658）打不开 → 「SWD3-Talk」「找不到 Talk%d」、SendMessage(WM_DESTROY)、返回 0；读 4×(事件%2000)+0x200 处偏移到 +0x14，+0x20 = 0，定位到偏移+0x200，剧本缓冲 0x2000 个 dword 填 0x0C0C0C0C 后读 0x8000 字节，0x004C2FF8 |= 0x8000，控制角色动作 0x00436C80 失败报「Act Err(Talk:LoadTalk)」。循环（0x0042BD21）：命令 = 字 & 0x3FFF（写 0x004F4840，全程序只写不读）；>0x6D 时 0x400 RunOn（var_28 = 1，前进 2）、0x401 RunOff（var_28 = 0）、0x402 SkipQuick（继续）、0x3FFF 结束、0x6E–0xD6 次表 0x0043235C，其余（含 0）默认分支 MessageBeep 后「對話系統」「沒有指令(%d,%x),(%d,%x)」（命令、0x004F47E0）且不前进；循环尾 0x004F47E0 = 命令，系统旗标 4 → 返回 1，var_28 与本条继续标志皆 0 → 返回 1，否则继续。返回值仅 0x0040A7B0 调用且不读。跨指令的栈局部量：var_50（找到的角色，每条指令先写）、var_48／var_44（源码 valueA／valueB）。
对照（逐指令，长度均与 kCommandBytes 一致）：
- 1–6、0x59、0x5A 对话：说话人 0xFFF0 → 事件角色并写回剧本；0xFFFD 用事件角色坐标（不是剧情点启动时记日志）；原点 0x004B2338 低字 ≠ 0x8000 时按镜头加；布局 3–6 为 1、0x59/0x5A 为 2；旗标 5/6 0x40、选项位 0x80000000→0x20、计时 0x004F4844→8＋超时、0x40000000→0x400、0x20000000→0x80、0x4000000→2、0x10000000→0x8000、0x8000000→0x10000、0x2000000→0x20000、0x004F4848→0x40000；0x0040B370(窗, 游标+2, 名牌, 选项, 0x004F4838, 0x004F483C)；对话状态 2，奇数命令置 0x10、状态 1、0x004C2FF8+1；0x0040BF20 挂链；逐字节找 "%Q"；设置复位（0x80008000、−1、0x3C、0…）。一致。
- 7、8、9、0x50、0x68、0x69、0x79、0x7C、0xA0 对话选项位与偏移/居中：一致。
- 0xA SetAction、0xB SetDirection、0x2D SetAct、0x78 SetActData、0x36 SeekAct、0x2C SetSpeed、0x2E–0x31、0x4D/0x4E、0x4C TurnToFace、0x2A/0x2B：字段（+8 动作、+0x34 方向、+0 动作号、+0x48 速度、+0x44 延时、+0x42 位置、+0x58、+0x1C/+0x20/+0x3C 保存值）、0x00433440 检查后续命令、错误串与「找不到」时 0x0040D7C0 的 12 个参数（末项 0x8080）一致；TurnToFace 两次查找都用第一个角色报错。
- 0xC PutRoleOnTile、0x28 PutRole、0x14/0xA9 MoveRoles、0x74 PutRoles、0x0D StepRole、0x6D StepRoles、0x10/0x11、0x12/0x13、0x4B、0x1C SetRoleTalk（标签释放、移动记录清理、路径中途对齐与 0x00411890）、0x26 RemoveRole（&0x7FFF）、0x27（置 0x8000）、0x3E PlaceRoleOnMap（命令号改 0xFFFF、气泡标记 4 个全部填）、0x41/0x42、0x44/0x45、0x47/0x48、0x37–0x39、0x64、0x65、0x66/0x67/0x75/0x88/0x8C/0x91/0x92/0xAE 子表（0xFFF0 取控制角色槽号）、0x7E/0x7F、0x8A、0xB1、0xB2、0xC1、0xCC、0xCE、0xCF、0xD3/0xD4：一致。
- 0x0E、0x76、0x77/0x8B 对话等待与关闭（关闭时 0x004C2FF8 计数减 1 不低于 0、valueA/valueB 为两部分）：一致。
- 0xF、0x15–0x18、0x29、0x57、0x6E/0x6F、0x81/0x82/0xA7/0xA8、0xA3/0xA4、0xA5/0xA6、0xBA/0xBB（跳转后返回 0）、0x1D–0x21/0xB5–0xB9 变量（有符号序号 ≥0x40 或负报错停住、无符号比较、金钱负值清零、变量 2 载入 swdghost.dat）、0x22–0x25 时钟（0x23 跳转后不重置游标的 BUG 保留）、0xC5–0xCB、0xD5 角色变量（0x00433590 写入只取一个字节）：一致。
- 0x19/0x1A、0x5C/0x5D、0x93–0x95、0x8F 旗标；0x32/0x46/0x49 移镜头（步长取字、整除检查、取负、四边夹取、8 个参数的错误框）、0x33、0x3F/0x40 地震（0xCFCF）、0x34/0x35/0x4A 淡色（±31 浮点夹取、_ftol 后相减再除帧数）、0x3A/0x99、0x6A/0x9A、0x3B/0x71、0x3C/0x3D、0x4F（0xC0 字节节点、开方求步长）、0x51/0x52/0x56 头像、0x53/0x54 线条效果、0x55、0x58、0x5E/0x5F、0x60 地图动画（"%"/"*" 选项、名字复制 0x20 字节、头 0x24 字节、首块 0x18、两缓冲 0x9C400、8 位调色板 RGB555 与尾部 0x400）、0x61–0x63、0x6B、0x6C、0x70、0x72/0x89/0x8D 音乐、0x73、0x7A、0x7B、0x7D、0x80、0x83、0x84、0x85、0x86、0x87、0x8E、0x90、0x96–0x98、0x9B/0x9C、0x9D、0x9E/0x9F/0xCD（跳转表直达循环尾，停在该指令）、0xA1、0x5B/0xA2、0xAA–0xAD、0xAF/0xB0、0xB3、0xB4、0xBC–0xBF、0xC0、0xC2–0xC4、0xD0、0xD1、0xD2、0xD6：一致。
- 0x3FFF 结束：恢复移动、清持有、0x800 时恢复保存的动作/方向、保存值清 −1、0x00436C80、0x80000 时再恢复；所有角色保存值清 −1；种类 >1 的移动记录清除并清路径；日志「TalkEnd [%d]」、0x004C2FF8 清 0x8000、剧本首字 0xFFFF、领头 0xE4 字节填 0xFF、0x004E6B58/0x004E582C 清 0。一致。
- 字符串：tmp 脚本按源码注释地址逐个比对 84 处报错/日志字面量与 exe（CP950），全部一致；asm 引用的 ASCII 串均在源码中；0x004AF244、0x004B23EC、0x004B26F0、0x004B2538 另行比对一致。
- 0x3D 黑屏源码对 0x004EDA44 判空：显示初始化失败时游戏已结束（deviations 291），运行中不为空，分支不可达。结束指令清路径时源码按 Roles 限制下标：移动记录的角色号只由有效槽号写入，不可达。
差异/修正：
1. 0x3E PlaceRoleOnMap：原程序只在放置的角色有气泡旗标 0x200 时把 var_48 写成 4（0x0042DD30），源码却写入角色号与地图号；改为只在气泡分支写标记数。
2. 0x60 MapMovie：8 位影片转换调色板时 var_48 = 0x100（0x0042F023），源码未写；补上。
3. 0x4F、0x8A、0xCC：距离平方是 32 位 imul/add 回绕后按有符号开方，源码 i32 乘加会有符号溢出；改为 u32 回绕再转 i32。
4. 命名：0x27 置 Visible，HideRole 改名 ShowRole；0x7E 相等时跳转、0x7F 不等时跳转，名字互换为 JumpIfAction／JumpIfActionNot。
5. scene.h 中 TalkWindow、ScriptSprite、ScriptAnimation、ScriptEffect、ScriptObject 的大小、偏移与链表地址是正传的，改为 swd3e（0x004C66F8；0xC0 字节、+0xA4 起、0x004C6BD4／0x004D3F94；0xB0 字节、+0xAC、0x004D15F4／0x004D1EBC；0x004D3FBC；0x004C6150）。
6. 偏差登记：128 补对话指令；135 补 0x1C 步号越过 0x200；93 地址列补本函数；101 删去 swd3e 不存在的 0x9E／0x9F 复制删除光盘文件；新增 0x32/0x46/0x49/0x57 除零、0x53 释放后读取、0x83 与 0x84 空指针、0xCB 未初始化读取。
复核：完整函数重新对照一致。

## 0x00433140 Talk::SeekScript — exact

汇编：事件偏移 +0x14 = 偏移，游标 +0x20 = 0；剧情档定位到 偏移+0x200，读 0x8000 字节到剧本缓冲；返回 1。
对照：一致（不先填 0x0C，与 RunEvent 开头不同，源码相同）。

## 0x00433190 Talk::OpenScript — exact

汇编：关闭剧情档；文件号 = 序号/2000（无符号 mul 0x10624DD3>>7），wsprintf「%sTalk%d.dat」(安装目录, 文件号+1)；打不开 → 0x004B2D94「劇情資料(1)無法讀取/%s」复制到 0x004C4658，0x004C4661 = 文件号低字节+'1'，0x0040C530(「檔案開啟」, 缓冲区)，SendMessage(WM_DESTROY)，返回 0。否则定位 4×(序号%2000)+0x200，读偏移到 +0x14，+0x20 = 0，定位偏移+0x200，读 0x8000 字节，返回 1。
对照：一致；替换的字节位置（Big5「劇情資料(」后第 9 字节）对应源码替换 '1'；%s 无参数见 deviations 371。

## 0x00433440 Talk::CommandsRole — exact

汇编：命令字（整字，不掩码）为 0xA、0xB 或 0x2D 时，以第二个字（不替换 0xFFF0）0x0040C4D0 找角色，找到且下标等于参数 → 1，否则 0。
对照：一致。

## 0x00433590 Talk::RoleVariable — exact

汇编：0x00433620 找记录，找不到 → 0；变量字在记录 +0x16。模式 1（读）：变量 1 取低字节、变量 2 取 (字>>8)，写入 *输出（dword）；模式 2（写）：变量 1 低字节 = *输入 的低字节、变量 2 高字节 = *输入 的低字节；其他变量或模式不动；找到时返回 id。
对照：一致（MapsRecord::RoleVariables = 11 → +0x16）；截断记录见 deviations 355，未写输出的读取见 442。

## 0x00433620 Talk::FindRoleRecord — exact

汇编：从 0x004C65F0 的角色表起，首 dword 为 −1 → 0；逐条（0x18 字节）比较 +2 的字与 id，相同返回该记录，遇到首 dword −1 → 0。
对照：一致；没有结束标记见 deviations 354。

## 0x00433C10 Talk::Draw — exact（实现在 smooth::TalkFrame::DrawAdvance／DrawWindowAdvance）

汇编：窗口链 0x004C66F8 为空 → 返回。建 40×单元 × 11×单元 离屏面（0x0043C620）并清空，大字体目标同尺寸（0x0043A070），锁定取像素、解锁，保存 advance。逐窗口：颜色 +0x2C、阴影 +0x2E、样式 +0x34 低字节、x 1、列 0、行 0、首行 0、选项号 −1、opening 1。
- Fade 0x40：opening 0；未 Typed 时 openStep+1（u16，>4 取 4）、alpha = openStep×4；Typed 时 counter <8 则 alpha = counter×2；alpha &= 0xF；框 = (x, y, x+宽+2×单元, 高+y)。否则 openStep+1，>4 时取 4 且 opening 0：FixedPlace 0x800 时 x0 = fromX + step×((x−fromX)/4)、偏移 = step×((y−fromY)/4)、起 y = fromY；有说话者时 x0 = 角色x + step×(x−角色x+镜头左)/4 − 镜头左，偏移同理（有符号截断），起 y = 角色y；0xFFFD 时以 fromX/fromY 同样换算；上 = 起y+偏移−4，右 = x0 + step×((宽+2×单元)/4)，下 = step×(高>>2)+上+8。
- Overlay 0x80：opening 0，框 = (x, y, 宽+2×单元+x, 高+y)，edge = x；否则 0x00433720(frameAct+0x4A, 框, alpha, 0x10)，edge = 左。裁剪 (edge, 上, 右+edge, 下+上)。
- 每帧：delay−1，advance 取回；全部跳过 0x004E6CFC 时 delay 0x8FFF、advance 1、空格计数 1；Enter 计数 1 时空格计数 1；Skippable 且空格 1 时先置 0x80000000、再次置 0x40000000；Skipping 时 speed 0xFFFF，首次 delay 0xFFFF 并置 0x20000000。
- PageFull|Typed：NoAdvance 且未全跳过 → advance 0。Typed：保存的 advance 非 0 置 Read，advance 非 0 置 0x200，空格不为 1 清 advance 与 0x200，有超时且 (0x004C4654−shownAt)/100 > timeout 且 Timed → 0x200、advance 1。PageFull：advance 非 0 置 0x200、空格不为 1 清、advance 非 0 置 Skipping；timeout 0xFFFF 时 NoAdvance 则 advance 取回，否则超时（同上条件）清 shownAt、0x200、advance 1。Fade：advance 0，timeout 0xFFFF 时经过 <0x10018 则 counter 0x10，否则经过 > timeout 时 counter−1（负取 0），为 0 时 0x200、advance 1。两者都不是：counter 0x24，Skippable → advance 0，否则 advance 非 0 置 Skipping。Choices 0x2000：counter 0x20，没有点击区时 counter 1、advance 1、0x200。
- 打字循环（opening 时不进入）：全跳过 → advance 1、本次打字标记清 0；advance 非 0：非 Skippable 时 delay 0x8FFF，没有点击区时 PageFull 且 advance ≠ 2 → 翻页（page = typed，typed 处字节非负且非 '%' 时 +1，清 0x4300，颜色/阴影/样式取页存值），否则 0x200 → 关窗（非 FixedPlace：说话者 0xFFFD 时领头对话状态清 0，否则说话者有 0x800 时跑动作「Act Err(DlgMessage:Fr)」；Blocking 时 0x004E6CDC = 0xC、锁定计数减 1 不低于 0 保留 0x8000；page 0、空格两计数 0）。delay ≥ 0 时到达 typed 画出结束；delay <0 时越过 typed 且本帧打过字 → delay = speed、typed = 到达处、画出结束。"%Q" 画出，未 Typed 时记 shownAt、Fade 时 counter 0x10，置 Typed，有点击区时 0x004E4DC8 |= 0x1000。(x+列)×单元 > 宽（有符号）或 "%N"：画出，(行+1)×2×单元 ≥ 高 → PageFull、点击区旗标、"%N" 时越过它并 typed、shownAt、全跳过或 Instant 时 typed = 当前、存页；否则列 0、x 1、行+1，"%N" 时越过，首行记第一次的 consumed。"%L"、"%P"、"%S"（(数字+0x004C3654)×2，Skipping 时 0）、"%C"／"D%"／"%G"（越过 3 字节、画出、x = x+列、0→4、1→0x10）、"%K"、"%A"／"%a"（读到 '.'，consumed 不计 "%A" 两字节；ParseNumber；"%A" 时播放 0xC0 并改写为 "%a"）、"%B"／"%b"（选项号+1，选中项以 primary[2] 0x84 绘制并画标记，否则普通字色；背景 0xFFFE；"%B" 时追加点击区 0x14 字节节点 (1, x, y, x+宽, y+2×单元)；改写为 "%b"）、普通字符（负字节再取一个）。
- 收尾：0x004ED9BC = 像素，首像素 0xFFFF 改 0xFFFE，0x004E6D58 = alpha，0x00417A60(x, y, 40×单元, 11×单元, alpha ? 0x14 : 4)，清面。PageFull|Typed 且（ShowMark 且未 Read，或非 NoAdvance）：Typed 用结束标记 0x004E54A8（帧 0x004E54F2、图 0x004E54F4），否则下页标记 0x004E6C18，位置 右−动作x−4、下−动作y−8，方式 0。非 opening、有名牌且 page 未关：15 行 0x004187A0(屏幕+(上+行−16)×640+edge+16, 0, 8×长度+0x1C)，名牌框 (nameAct+0x4A, edge+6, 上−0x1B, 长度+edge+8×长度+0x10, 上−8, alpha, 4)，中字体阴影 secondary[4]、primary[4] 写名字于 (edge+0xC, 上−0x1A)，阴影清 0。
- 全部窗口后：释放离屏面，裁剪 (0,0,0x27F,0x1DF)，advance 0；page 为 0 的窗口摘链，说话者非 0xFFFD 时对话状态清 0，释放。跑结束标记与下页标记动作，失败都以结束按钮角色的 id/动作/动作/方向报「Act Err(DlgMessage:EndBtn)」「…:NextPage」。大字体目标恢复 0x004B1E90×0x004B1E94。
对照：常量（kOpenSteps/kOpenStep 4、kFadeStep 2、kFadeFrames 8、kSkipDelay 0x8FFF、kUntimedHold 0x10018、计数 0x10/0x24/0x20、kMouseRest 0xC、框与名牌偏移）、旗标值（scene.h）、按键槽（空格 1、Enter 12）、字体旗标（GradientUp|BoldShadow 0x84）、颜色表下标一致。源码对窗口角色号的范围保护、frameAct/nameAct 判空不可达：角色号只由 SetupDialog 写为有效槽号、0（失败时整体重置）或 0xFFFD，frameAct 总会设置、nameAct 仅在有名字时使用且此时已设置。页标图片缺失与像素为空见 deviations 新增 0x00433C10 一行。

## 0x00435250 Talk::DrawColoredText — exact

汇编：lstrlenA（结果不用）；大字体背景 0xFFFE、阴影 = 0x004AF760[阴影号]；0x0043B590(像素, x, y, 文字, 0x004AF720[颜色号], 样式)；背景 0xFFFE、阴影 0。
对照：一致；序号越界见 deviations 310。

## 0x00435AE0 Talk::ParseNumber — exact

汇编：首字符 '+' → 正号并越过，'-' → 负号并越过，其他 → 正号不越过；逐字节到 NUL：减 '0' 不在 0..9 → 返回 0；位数 >9 → 返回 0；值 = 0，从末位往前 值 += 数字×权、权 ×10（权初值为符号），返回 1。
对照：一致；空数字时往前无界读取见 deviations 108。
