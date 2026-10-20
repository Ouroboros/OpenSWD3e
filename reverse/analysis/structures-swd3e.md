# swd3e 与 swd32 的结构差异（逐函数还原中确认）

依据：swd3e 汇编与反编译、reverse/analysis/diff_swd32/、reverse/analysis/export/match_swd32/offsets.tsv。已确认的写“确认”，推断的写“推断”，未决的写“待定”。

## 已在代码中按 swd3e 处理

| 结构 | swd32 | swd3e | 处理 |
| --- | --- | --- | --- |
| ActState（动作播放） | 0x98 字节 | 0xA4 字节：+0x5E/+0x60 'CS' 值，+0x8C 循环计数（'PL'），+0x8E/+0x8F 循环区间，其后字段后移 8 字节，+0x94 'CS' 启用字节 | 确认（0x00436C80、0x004594F0 memset 0xA4）；src/res/act_state.h |
| ActScript | 指向脚本的副本 | 指向动作条目内的指针 | 确认（0x00436B80）；ActScript 为条目加偏移 |
| tsw 缓存 | 10 链 | 16 链（id & 0xF），节点 0x20 字节 | 确认；src/res/tsw_archive.* |
| act 缓存 | 10 链（id、sub 为键） | 16 链（只以 id 为键，按 sub 取脚本） | 确认；src/res/act_archive.* |
| SkillScript（mon.dat 技能脚本） | +0x24 turns，操作码 0..0x19 | +0x2A turns，操作码 0..0x1F，新字段 +0x18/+0x24/+0x26 | 确认（0x00489750）；src/org/object_record.h |
| ItemNode（道具链表节点） | 0xB0 字节，next +0xAC | 0xB4 字节，next +0xB0 | 确认（0x00454970 等）；逻辑按字段，未改 |
| MemoryManager | 字段起于 +4 | 多出两个未用字段（+4、+0x18..+0x27），FreeBlock 不清节点 +8 | 确认（0x0043D120、0x0043D350） |
| Controls 按键 | 间隔 150 ms | 间隔 120 ms，每次轮询清 press 的 0x4000 | 确认（0x004050E0） |

## 待在相应模块处理

- ObjectData（mon.dat 物件记录）：swd32 0xA0 字节，swd3e 0xA4 字节、记录 0xA8（描述指针 +0xA4）。确认（0x00489B00）：+0x00..+0x4A 不变；标签 2 写 +0x4A，标签 3 写 +0x4C；标签 1 的块 0x4F 字节复制到 +0x54..+0xA2（swd32 0x4D 字节到 +0x50）；标签 0xE（act）+0x54；标签 0x7D0：dword +0x9C、字节 +0xA0、+0xA1、+0xA2，字 +0x58、+0x56（共 11 字节，swd32 15 字节：抗性 5 字节？）；标签 0x18（新）两个字到 +0x94、+0x96；标签 0x1C 字节 +0xA1，0x1D 字节 +0xA2。另：编号 3000..3400 减 2500、6001 起减 5100 后再查 mon.dat 索引；描述字节计数释放时减 _msize、分配时加长度+1。推断（offsets.tsv）：swd32 +0x52..+0x60 → +4，+0x66..+0x8A → +6。待定：+0x4E..+0x53 与尾部各字节字段的含义（读取者在 battle、menu、altar 模块）。
- RoleRecord（角色记录）：swd32 0x38 字节，swd3e 0x3C 字节。确认（0x00409480）：+0x0A/+0x0C/+0x0E 最大值与 +0x2C 等级不变。待定：+0x2D 之后的抗性与新增 4 字节（party、save 模块）。
- 存档预览：名字区 0x80 字节（swd32 0x40），角色记录 4 个 0x3C 字节（swd32 0x38）。确认（0x00409480）。
- Painter 新绘制模式 0x80、0x84、0x90、0x94、0xA4、0xA8、0xAC、0xC0（0x00423830..0x00426C80，经函数表 0x004EC198[mode] 调用），缩放参数由 0x00423630（'CS'）与 0x00423670 设置：x、y 锚点 0x004EC188、0x004ED9C4，缩放 0x004B1E7C、0x004B1E80（1/1024）。确认（0x00417570、0x00417A60、0x00423830）。
- 错误报告：swd32 发往空调试日志的“Act Err(…)”等约 180 处在 swd3e 调用 0x0040C530（vsprintf 后 MessageBoxA）。确认；src/core/error_box.h（ErrorBox）已建，调用处随各模块改写替换。
- 断言源文件路径：C:\cpp\SWD102e\org.cpp、manrole.cpp、role.cpp（swd32 为 C:\Project\swd102aDVD\…），行号随函数核对。
