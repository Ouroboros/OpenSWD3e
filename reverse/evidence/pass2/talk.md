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
