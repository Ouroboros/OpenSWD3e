# 第一遍汇编验证：全局对象的动态初始化 thunk（89 个）

结论均为 exact。每组形如：一个函数调用构造 thunk 再跳到注册 thunk；构造 thunk 以对象地址为 this 调构造函数（或带参数）；注册 thunk 以析构 thunk 调 atexit（0x004A1664）；析构 thunk 以对象地址调析构函数。实现中这些对象是 Game（或其下级）的成员，随 Game 构造与析构；全程序没有其他代码引用的对象不建立（其构造与析构没有可见效果）。对照依据：tmp 中按汇编解析的地址表（构造/析构函数地址、对象地址、参数），以及 rg 全部 asm 中对象地址的引用。

| thunk 组 | 对象 | 构造／析构 | 实现 |
| --- | --- | --- | --- |
| 0x00409D70..0x00409DA0 | 0x004D0470 MemoryManager | 0x0043D120 / 0x0043D1B0 | 不建立：仅建私有堆、退出时报告未释放块；全程序无其他引用 |
| 0x00409DB0、0x00409DC0 | 0x004D0C10 法宝鬼魂 | 0x0049E7F0（不注册析构） | Game::talismanGhost |
| 0x00409DD0..0x00409E10 | 0x004C6128 脚本对象哨兵 | 0x00435B60(1,1,0,0,1,1) / 0x00435BF0 | Scene::objects（哨兵仅 +0x28 链接被读，实现以 unique_ptr 表头代替） |
| 0x00409E20..0x00409E50 | 0x004C5130 20 像素字体 | 0x00439BA0 / 0x00439BF0 | largeFont |
| 0x00409E60..0x00409E90 | 0x004E5830 16 像素字体 | 同上 | mediumFont |
| 0x00409EA0..0x00409ED0 | 0x004C3658 12 像素字体 | 同上 | smallFont |
| 0x00409EE0..0x00409F10 | 0x004D0E08 maps.dat 文件 | 0x0043CAE0 / 0x0043CB10 | GameDatabases::maps |
| 0x00409F20..0x00409F50 | 0x004D1DB0 HUGE.lmf 文件 | 同上 | MapCache::lmf |
| 0x00409F60..0x00409F90 | 0x004E6B88 文件 | 同上 | 不建立（无其他引用） |
| 0x00409FA0..0x00409FD0 | 0x004D3470 path.dat 文件 | 同上 | GameDatabases::paths |
| 0x00409FE0..0x0040A010 | 0x004C2FA0 剧本文件 | 同上 | GameDatabases::story |
| 0x0040A020..0x0040A050 | 0x004E6830 地图单元映射文件 | 同上 | MapCache::unitFile |
| 0x0040A060..0x0040A090 | 0x004D1438 文件 | 同上 | 不建立（无其他引用） |
| 0x0040A0A0..0x0040A0D0 | 0x004C2A48 地图影片文件 | 同上 | FieldView::file |
| 0x0040A0E0、0x0040A0F0 | 0x004C5008 DirectDraw 封装 | 0x0043C020（不注册析构） | graphics（平台替换） |
| 0x0040A100..0x0040A130 | 0x004D1C90 DirectInput | 0x0043BA30 / 0x0043BA60 | input（平台替换） |
| 0x0042B480..0x0042B4B0 | 0x004F47E8 文件 | 0x0043CAE0 / 0x0043CB10 | 不建立（无其他引用） |
| 0x00459880..0x004598C0 | 0x005136E0 十个战斗角色（0x34D4 字节） | 向量构造/析构迭代器 0x004A1E72/0x004A1EF4 以 0x0047E660/0x0047E6D0 | battleRoles |
| 0x004598E0..0x00459920 | 0x00539590 二十个敌人（0x3078 字节） | 同上以 0x00487B90/0x00487BF0 | battleEnemies |
| 0x00459940..0x00459970 | 0x005761D0 战斗投影器 | 0x00438700 / 0x00438830 | BattleState::projector |
| 0x00459980、0x00459990 | 0x0050F620 battle.ffd 读取器（参数为投影器） | 0x0046B210（不注册析构） | BattleState::file |
| 0x004599A0..0x004599D0 | 0x005373C8 文件 | 0x0043CAE0 / 0x0043CB10 | 不建立（无其他引用） |
| 0x0049C8E0..0x0049C900 | 0x00578628 MIDI 播放器 | 0x0049E090 共用构造 / 0x0049C930 | AudioManager::midi |
| 0x0049D3C0 | 音效 | 0x0049D3D0 | AudioManager::effects |
| 0x0049E050..0x0049E070 | 0x005795C0 音乐流播放器 | 0x0049E090 / 0x0049E0B0 | AudioManager::streams |

- 实现 game.cpp 中原先的对照注释多为 swd32 地址且被格式化挤在一起，已按本表以 swd3e 地址重写。
- 构造顺序（静态初始化顺序与成员声明顺序）对游戏可见行为无影响：有副作用的构造只有平台层对象。
