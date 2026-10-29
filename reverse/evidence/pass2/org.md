# 第二遍汇编验证：资料库、等级与熟练度（org）

## 0x00489750 Database::LoadSkill — diff → exact

汇编：0x00578500=0 时以 0x004C2B94 目录拼 "mon.dat"，CreateFileA(GENERIC_READ, 共享读, OPEN_ALWAYS)，失败 → 0，成功置 0x00578500=1、句柄 0x005785E4。移到 0x204 读双字 base；移到 base+id×4+0x200 读双字 offset；移到 offset+0x200；分配 0x400 清零读满。首字≠0 → 释放、返回 0。否则从首字起逐字取操作码（指针 +2），5 结束（释放、返回 1），>0x1F 与 1 不处理：0 → +0xC/+0x10 双字（+8）；2 → +0x2A 字节（+2）；3 → 旗标|1；4 → +0x16；6 → +0x1A、旗标|0x80；7 → +0x1A；8 → +0x14；9 → |2；0xA → +0x20 再 |0x8000；0xB → |4；0xC → +0x8 双字（+4），含 0x20000 → 旗标|0x20000；0xD → +0x1E；0xE → |8、+0x1C；0xF/0x10/0x11 → |0x10/0x20/0x40；0x12..0x15 → |0x100/0x200/0x400/0x800；0x16 → |0x1000、+0x22 字、+0x2A 取下一字低字节（+4）；0x17 → +0x26、+0x28；0x18/0x19 → |0x2000/0x4000；0x1A → +0x18；0x1B/0x1C/0x1D/0x1E → |0x8000/0x10000/0x40000/0x80000；0x1F → |0x100000、+0x24。
对照：LoadSkill、OpenData、ReadScript 一致：SkillOpcode 与 SkillScript 旗标、字段偏移逐项相符（kIndexBase 0x204、kDataBase 0x200、ScriptBytes 0x400）。读出缓冲区之外属偏差 54，读失败偏移属偏差 56，共享方式属偏差 106。
差异：opcode 0xA 置位常量名 Unk1EMark 沿用正传偏移，SkillScript 未锁定大小。修正：改名 Unk20Mark；加 static_assert(sizeof(SkillScript) == 0x2C)。复核：完整函数重新对照一致。

## 0x00489B00 Database::LoadObject — diff → exact

汇编：+0xA4 说明非空 → 0x00578508 −= strlen(说明)（0x004A207E），释放、置 0；记录 0xA8 字节清零；打开 mon.dat 同 LoadSkill，失败 → 0。编号 0xBB8..0xD48 → 16 位加 0xF63C（−0x9C4）；之后 ≥0x1771 → 加 0xEC14（−0x13EC）；移到 0x204 读双字（不用），相对移 编号×4−4，读 offset，移到 offset+0x200，读 0x400 清零缓冲。首字≠0x3E8 → 释放、0。逐字取标签（+2），5 结束（释放、1）：1 → 复制 0x4F 字节到 +0x54；2/3 → +0x4A/+0x4C；4、0x17、0x1F..0x63 及其他未列 → 不处理不前进；6..0x16 → +0x40/+0x24/+0x26/+0x2C/+0x32/+0x46/+0x42/+0x44/+0x54/+0x28/+0x2A/+0x2E/+0x30/+0x34/+0x36/+0x38/+0x48；0x18 → +0x94、+0x96；0x19 → 跳一字取双字到 +0x20（共 6）；0x1A/0x1B → +0x3A/+0x3C；0x1C/0x1D → 字节 +0xA1/+0xA2（+1）；0x1E → 0xFF 内找 "$$"，无 → 前进 0xFF，有 → 分配长度+1、复制、存 +0xA4、前进长度+2、0x00578508 += 长度+1；0x64 → +0x3E；0x3E8 → 找 "$$"，有则复制到记录起点（不写结尾），前进长度+2（无则 0x101）；0x7D0 → 5 字节到 +0x9C、+0xA1、+0xA2 字节、+0x58 字、+0x56 字（共 11）。
对照：ObjectTag 枚举与 ObjectData 字段偏移逐项相符；kSecondRangeFirst/Last/Shift 0xBB8/0xD48/0x9C4、kThirdRangeFirst/Shift 0x1771/0x13EC、kActBlockBytes 0x4F、kTextSearch 0xFF、kTextEndBytes 2、kResistTailBytes 7、kResistLevelAt 7、kResistCategoryAt 9、kResistBytes 11。名称越界属偏差 55，缓冲区之外属偏差 54，读失败属偏差 56，共享方式属偏差 106。
差异：释放旧说明时源码从计数器减去长度+1，原程序只减 strlen（加时却加长度+1，原有 BUG）；descriptionBytes 注释写正传地址。修正：只减去说明到第一个 0 字节为止的长度，注释改为 0x00578508。复核：完整函数重新对照一致。

## 0x0048A010 Database::LoadLevelEntry — exact

汇编：0x00578504=0 时打开 level.dat（同上，句柄 0x005785E8），失败 → 0。移到 (组×100+项)×4+0x70 读双字 offset，移到 offset+0x200，读 0x400 清零缓冲；首字≠0 → 释放、0。逐字取操作码：5 结束（释放、1）；0 → 复制 0x1A 字节到栈，输出 = 其中 +0x16 双字，前进 0x1A；1 → 前进 2；其他不前进。
对照：LoadLevelEntry、ReadLevelScript 一致（kLevelsPerGroup 100、kLevelIndex 0x70、kLevelStats 0、kLevelSkip 1、kLevelStatBytes 0x1A、kExperienceAt 0x16）。偏差 54/56/106 同上。
差异：无。

## 0x0048A180 LevelUp — diff → exact

汇编：打开 level.dat（同 LoadLevelEntry），读 (角色×100+等级) 项脚本，首字≠0 → 0。逐字取低字节有符号操作码（+2）：5 结束（释放、1）；0 → 复制 0x1A 字节到角色记录 +0xA，+0x2C = 等级字节，+0x4/+0x6/+0x8 = +0xA/+0xC/+0xE，前进 0x1A；1 → 字 id，为 0 → 0x00436050(hWnd, 0x004BFAC0 "升級魔法編號為0!!", 0, "C:\cpp\SWD102e\org.cpp", 0x354)；在 0x004C2B64[角色]（0x004C2B68 起的技能表）链中从表头起比较 id，相同 → 跳过；到尾且 id≠0x8000 → 分配 0xB4 清零接在尾、id、0x00489B00 载入 +0xC、学会旗标=1、lstrcpyA(0x0057747C, 名称)；表头指针恢复；前进 2；其他不前进。
对照：LevelUp、AppendSkill 一致（kLevelStats 0、kLevelSkill 1、kLevelEnd 5、kStatWords 13、kNoSkill 0x8000、kLevelSkillLine 0x354、kOrgFile 与提示文字与 exe 字节一致；party.skills 0x004C2B68 以 role1−1 取用；RoleRecord +0x4..+0x23、+0x2C 偏移相符）。
差异：缓冲区之外继续读、读失败偏移、CreateFileA 共享方式与名称写入 0x0057747C 越界四类保护未登记本函数。修正：偏差第 54、56、106、220 行加入 0x0048A180 与对应汇编位置，代码不变。复核：完整函数重新对照一致。

## 0x0048A420 Database::CloseFiles — exact

汇编：0x005785E4 不为 −1 与 0 → CloseHandle、置 −1；0x005785E8 同；0x00578500=0、0x00578504=0。
对照：CloseFiles 关闭两个文件并清两个打开旗标，一致。
差异：无。

## 0x0048A470 ClearCounterList — exact

汇编：表头 5 个双字清零（next 置空，原节点不释放）。
对照：ClearCounterList 重置表头；后续节点只经表头链到达，无其他指针，释放与否无可观察差异。
差异：无。

## 0x0048A490 AddCounterCapped — exact

汇编：从表头起按 16 位 id 查找（表头本身也比较）；找到：值 <0x14（无符号）→ 值 += 增量并返回新值，否则返回原值不变；未找到：分配 0x14 清零节点接在尾、值 += 增量、id、表头计数 +1，返回增量。
对照：AddCounterCapped、FindNode、Append、CountOne 一致（Cap 0x14）。
差异：无。

## 0x0048A500 SetCounterCapped — exact

汇编：查找同上；找到：值 = 参数，>0x14 → 0x14；未找到：新节点 id、值同样封顶、表头计数 +1；返回参数值。
对照：SetCounterCapped 一致。
差异：无。

## 0x0048A580 FindCounterValue — exact

汇编：查找同上，返回 +6 值，无则 0。
对照：FindCounterValue 一致。
差异：无。

## 0x0048A5B0 StepCounterProgress — exact

汇编：找到：值 +1，≥最大 → 最大；比 = 值/最大（x87）；+8 = ftol(比×100.0f)；返回 ftol(比×刻度)。未找到：比 = 1.0f/最大；新节点 id、值 1、+8 = ftol(比×100)；表头计数 +1；返回 ftol(比×刻度)。
对照：StepCounterProgress 一致（0x004AAE10 = 100.0f、0x004AA6A0 = 1.0f 与 exe 字节一致，kPercent 100.0）。
差异：无。

## 0x0048A6A0 SetCounterProgress — exact

汇编：查找同上；未找到则新节点（id）；值 = 参数，≥最大 → 最大；+8 = ftol(值/最大×100)；新节点时表头计数 +1。
对照：SetCounterProgress 与 Percent 一致。
差异：无。

## 0x0048A770 FindCounterPercent — exact

汇编：查找同上，返回 +8，无则 0。
对照：FindCounterPercent 一致。
差异：无。

## 0x0048A7A0 FindCounterShare — exact

汇编：遍历全部节点（找到后不停止）：id 相同 → 输出百分比 = +8；0x00489B00 载入 id 到 0x00578510、0x0048B010 释放说明；输出份额 = 零扩展 (16 位 +8 × 0x00578552)/100。
对照：FindCounterShare 一致（scratch 记录即 0x00578510，unk42 即 +0x42）。
差异：无。

## 0x0048A810 SetProficiency — exact

汇编：表头计数≠0 → 从第一个节点起，否则从表头起；0x00489B00 载入 id 到 0x00578510、释放说明，最大 = 0x00578554 字（counterMax）。查找 id：找到且 +0xA≠0 → 返回 1；找到 → 值 = 参数，≥最大 → 最大，+8 = ftol(值/最大×100)；未找到且参数为 0 → 返回 1；否则新节点 id、值封顶、百分比、表头计数 +1；均返回 1。
对照：SetProficiency 一致（返回值恒 1，调用者不用，源码为 void）。
差异：无。

## 0x0048A930 GetProficiency — exact

汇编：从表头起查找 id；无论找到与否都载入 id 到 0x00578510、释放说明、输出最大 = counterMax；找到 → 输出值 = +6、返回 1；否则输出值 0、返回 0。
对照：GetProficiency 一致。
差异：无。

## 0x0048A9C0 LearnSkill — diff → exact

汇编：id 为 0 → 0x00436050(hWnd, 0x004BFAEC "魔法編號為0!!", 0, "C:\cpp\SWD102e\org.cpp", 0x56C)。表头 = 0x004C2B68[角色低 16 位]；从表头起比较 id，相同 → 返回 0；到尾：id=0x8000 → 返回 1（不加）；否则分配 0xB4 清零接在尾、id、0x00489B00 载入 +0xC、lstrcpyA(0x0057747C, 名称)，返回 1；表头指针恢复。
对照：LearnSkill 与 AppendSkill 一致（kSkillZero 与 exe 字节一致、kSkillLine 0x56C、kNoSkill 0x8000 时仍返回 1）。
差异：角色号超出技能表时源码越界访问 std::array，原程序读表后数据当指针，未登记；名称写入 0x0057747C 越界未登记本函数。修正：角色号（低 16 位）超出技能表时返回 0，偏差表末尾新增一行；第 220 行加入 0x0048A9C0。复核：完整函数重新对照一致。

## 0x0048AA80 Fame::Save — exact

汇编：大小 = (0x004C634C + 0x004D345C + 0x004D1F5C 三个双字之和的低 16 位，即三表头计数之和)×14 + 0x1E，写出参数；分配清零。依次 0x004C6348、0x004D3458、0x004D1F58：节 = 当前位置，条目从节+0xA 起，每节点复制 +4 起 14 字节；有节点 → 节 [0] = 10+14×n、[4] = n 字；无节点时节仍占 0xA。压缩输出缓冲 大小+大小/64+0x13 清零、工作区 0x20000；0x0043E0A0 压缩；结果 ≥ 大小 → 释放三块，MessageBoxA(0, "save fame compress false !!", "error")，返回 0。否则分配 结果+0xA 清零：[0]=1、[2]=原大小、[6]=结果、+0xA 起复制压缩数据，输出大小 = 结果+0xA，返回 1。
对照：Fame::Save 一致（SavedBytes 0xE、kSlackBytes 0x1E、kHeaderBytes 0xA、BlockVersion 1、UnpackedAt 2、PackedAt 6、BlockHeaderBytes 0xA、DictionaryBits 15 即工作区 0x20000、错误文字与 exe 字节一致）。节点多于计数时的写出属偏差 60；分配失败的提前返回不复现。
差异：无。

## 0x0048AD00 Fame::Load — exact

汇编：原大小 = 块 +2、压缩大小 = 块 +6；分配原大小清零；分配压缩大小并复制块 +0xA 数据（未使用）；0x0043E500 从块 +0xA 解压；结果≠原大小 → MessageBoxA(0, "Load fame decompress false !!", "error")、释放副本、返回 0（表不变）。否则释放副本；三表依次：表头计数字 = 节 +4（只写计数字，值字不变），条目从节+0xA 起；计数>0 → 逐个分配 0x14 清零节点接在表头起的链上（覆盖旧链指针），复制 14 字节；计数为 0 时旧链保留。释放缓冲，返回 1。
对照：Fame::Load 一致（list.id = 计数，计数为 0 时 next 不动；旧链只经表头可达，释放与否无可观察差异）。越界保护属偏差 61。
差异：无。

## 0x0048AF00 Fame::Free — exact

汇编：三表各自从尾逐个释放节点，之后表头 5 个双字清零。
对照：Fame::Free 将三表重置为空表头，一致。
差异：无。

## 0x0048B010 Database::FreeDescription — exact

汇编：记录 +0xA4 非空 → 0x0043DAB0 释放、置 0（不改 0x00578508 计数）。
对照：FreeDescription 只 reset 说明，一致。
差异：无。
