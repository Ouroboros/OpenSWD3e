
## Database::LoadSkill、LoadObject、LoadLevelEntry（第 1169–1171 行）— exact

- 0x00489750 LoadSkill：mon.dat 未开时以安装目录 + "mon.dat" CreateFileA（共享方式见平台替换行）；失败返回 0。读 0x204 处 dword 为基址，读 基址 + 4×(u16)id + 0x200 处 dword 为偏移，从 偏移 + 0x200 读 0x400 字节到清零缓冲；首字非 0 返回 0。逐字读操作码直到 5：跳转表 32 项逐项解码——0：+0xC／+0x10 两 dword；2：+0x2A 字节（占 2）；3／9／0xB：旗标 1／2／4；4：+0x16；6：+0x1A 并旗标 0x80；7：+0x1A；8：+0x14；0xA：+0x20 = 字 | 0x8000；0xC：+0x8 dword，有 0x20000 时旗标 0x20000；0xD：+0x1E；0xE：旗标 8、+0x1C；0xF..0x15：旗标 0x10／0x20／0x40／0x100／0x200／0x400／0x800；0x16：旗标 0x1000、+0x22 字、+0x2A 字节（占 4）；0x17：+0x26、+0x28；0x18／0x19：0x2000／0x4000；0x1A：+0x18；0x1B／0x1C／0x1D／0x1E：0x8000／0x10000／0x40000／0x80000；0x1F：旗标 0x100000、+0x24；1 与 > 0x1F 无操作数。SkillOpcode 取值、SkillScript 偏移与旗标位逐一核对。
- 0x00489B00 LoadObject：记录说明文字（+0xA4）非空时 descriptionBytes −= _msize（0x004A207E，即 长度 + 1）并释放；记录 0xA8 字节清零；打开同上；id 0xBB8..0xD48 时减 0x9C4，再 ≥ 0x1771 时减 0x13EC（字运算）；读 0x204 处 dword（不用），相对移动 4×索引 − 4，读偏移，从 偏移 + 0x200 读 0x400 字节；首字非 0x3E8 返回 0。标签：5 结束；0x64：+0x3E；0x3E8 名字（"$$" 搜索至多 0xFF，找到时复制到记录起点，跳过长度 + 2；未找到跳过 0xFF + 2）；0x7D0：五字节到 +0x9C、+0xA1、+0xA2 字节、+0x58、+0x56 字（占 11）；1..30 跳转表：1 复制 0x4F 字节到 +0x54；2 +0x4A；3 +0x4C；6 +0x40；7 +0x24；8 +0x26；9 +0x2C；0xA +0x32；0xB +0x46；0xC +0x42；0xD +0x44；0xE +0x54；0xF +0x28；0x10 +0x2A；0x11 +0x2E；0x12 +0x30；0x13 +0x34；0x14 +0x36；0x15 +0x38；0x16 +0x48；0x18 +0x94／+0x96；0x19 +0x20 取操作数后第 2 字节起的 dword（占 6）；0x1A +0x3A；0x1B +0x3C；0x1C／0x1D +0xA1／+0xA2 字节（占 1）；0x1E 说明文字（未找到时只跳过 0xFF，找到时分配 长度 + 1、descriptionBytes += 长度 + 1，跳过 长度 + 2）；4、5 以外的 4／0x17 等无操作数。ObjectTag 取值与 ObjectData 偏移逐一核对。偏差行 0x00489B00（名字写过 +0xA4）更正为 0xA4 字节上限并引用汇编。
- 0x0048A010 LoadLevelEntry：level.dat 打开同上；读 (组×100 + 序号)×4 + 0x70 处 dword 为偏移，从 偏移 + 0x200 读 0x400 字节；首字非 0 返回 0；操作码 0：经验 = 块内 +0x16 dword（占 0x1A），1：占 2，5 结束，其余无操作数。
- 共用偏差行（缓冲之外继续读、读失败时偏移取栈残值）改为引用三处汇编。

## LevelUp（第 1172 行）— exact

- 0x0048A180：level.dat 打开与读取同 LoadLevelEntry（(角色×100 + 等级)×4 + 0x70）；首字非 0 返回 0；操作码取每字低字节符号扩展：5 结束；0：0x1A 字节复制到记录 +0xA..+0x23（11 字属性加经验 dword），+0x2C = (u8)等级，生命／灵力／体力 = 三项上限；1：技能 id 为 0 时维护断言（"升級魔法編號為0!!"（0x004BFAC0，CP950 核对）、"C:\cpp\SWD102e\org.cpp"、0x354），以 0x004C2B64[角色]（即 skills[角色 − 1]，0x004C2B68 起）为头逐个比较 id，相同不加；id 为 0x8000 不加；否则分配 0xB4 节点挂到尾部、LoadObject、learned = 1、lstrcpyA 名字到 0x0057747C；其余操作码无操作数。原程序以表项本身作游标并在末尾复原，源码用局部游标，可见结果相同。

## CloseFiles 与计数表、熟练度、LearnSkill（第 1173–1184 行）— exact

- 0x0048A420 CloseFiles：两个句柄非 −1 且非 0 时 CloseHandle 并置 −1；两个已开旗标清零。
- 0x0048A470 ClearCounterList：头 0x14 字节清零（原程序不释放后续节点，源码释放；可见状态相同）。
- 计数表查找（0x0048A490／0x0048A500／0x0048A580／0x0048A5B0／0x0048A6A0／0x0048A770）：先比头的 +0x4（头中存个数），再沿 next 比较；新节点 0x14 字节清零挂尾，头 +0x4 加 1。
- 0x0048A490 AddCounterCapped：找到且值 < 0x14 时值 += 量并返回新值，≥ 0x14 返回原值；新建时值 = 量、返回量。0x0048A500 SetCounterCapped：值 = 参数（> 0x14 时 0x14），返回参数。0x0048A580 FindCounterValue：值或 0（零扩展；源码返回 i16，调用处全部转 u16／u8，等价）。0x0048A770 FindCounterPercent：+0x8 或 0。
- 0x0048A5B0 StepCounterProgress：找到时值加 1，≥ 上限（u16）时置上限；比 = 值/上限（fild／fidiv），+0x8 = ftol(比×100.0f)，返回 ftol(比×(u16)倍数)；新建时值 1、比 = 1.0f/上限，同上。0x0048A6A0 SetCounterProgress：值 = min(参数, 上限)，+0x8 = ftol(值/上限×100)。精度：原程序启动时 _setdefaultprecision（0x004A21E6）以 _controlfp(0x10000, 0x30000) 设 53 位精度，x87 逐步运算与 double 相同，源码以 double 计算一致；100.0f（0x004AAE10）、1.0f（0x004AA6A0）以 od 核对。
- 0x0048A7A0 FindCounterShare：遍历全部节点（含头，不中断），id 相同时出参一 = +0x8，LoadObject(0x00578510)、FreeDescription，出参二 = (u16)(+0x8 × unk42（0x00578552）) / 100。
- 0x0048A810 SetProficiency：头 +0x4 非 0 时从 next 开始；先 LoadObject(0x00578510)、FreeDescription；找到时 done（+0xA）非 0 不改，否则值 = min(参数, counterMax（0x00578554 低字）)、+0x8 = 百分比；未找到且参数非 0 时新建同上并头 +0x4 加 1；返回 1（调用处不用）。0x0048A930 GetProficiency：找到时出参上限 = counterMax、值 = 节点值，返回 1；否则上限同、值 0，返回 0；两路都先 LoadObject、FreeDescription。
- 0x0048A9C0 LearnSkill：id 为 0 时维护断言（"魔法編號為0!!"（0x004BFAEC，CP950 核对）、org.cpp、0x56C）；skills[(u16)角色]（0x004C2B68）已有该 id 返回 0；id 0x8000 不加；否则分配 0xB4 节点、LoadObject、lstrcpyA 名字到 0x0057747C；返回 1。

## Fame::Save、Load、Free、Database::FreeDescription（第 1185–1188 行）— exact

- 0x0048AA80 Save：个数和 = 三个表头 +0x4 的低字之和（dword 相加后取低字），大小 = 个数×14 + 0x1E，出参 = 大小；清零缓冲；三表依次为 0x004C6348、0x004D3458、0x004D1F58（lists[0..2]）：每表头 0xA 字节，节点 14 字节（节点 +0x4..+0x11）从头 +0xA 起顺写，有节点时表头 dword = 0xA + 14×个数、+0x4 字 = 个数；下一表从本表末尾起（空表也占 0xA）。压缩缓冲 = 大小 + 大小>>6 + 0x13、工作区 0x20000，LZO 压缩（0x0043E0A0）；结果 ≥ 大小时 MessageBoxA 标题 "error"、正文 "save fame compress false !!" 并返回 0；否则块 = 结果 + 0xA，[0] = 1、+2 = 大小、+6 = 结果大小、+0xA 起压缩数据，出参 = 结果 + 0xA，返回 1。偏差行 0x0048AA80 引用汇编。
- 0x0048AD00 Load：读 +2 大小、+6 压缩大小，复制压缩数据后解压（0x0043E500）；输出长度 ≠ 大小时 MessageBoxA "Load fame decompress false !!"／"error" 返回 0；否则三表依次：头 +0x4 = 表头个数，个数 > 0 时以 头.next 为起点逐个新建 0x14 字节节点并复制 14 字节（个数为 0 时 头.next 不动，源码同）。补登记偏差行 0x0048AD00（压缩数据与解压缓冲不检查长度）。
- 0x0048AF00 Free：三表各自反复释放末节点直到 头.next 为空，再把头 0x14 字节清零。0x0048B010 FreeDescription：+0xA4 非空时释放并清空（不改 descriptionBytes）。
