# 第一遍汇编验证：新游戏

## 0x0040E420 Game::NewGame — exact（注释整理）

- ResetBattle（0x00459AE0）、SetUseCache(1)；旗标 0x400 字节清零后置 1、3、4、0x46、0x1E，清 0、2、5..0x14、0x16..0x19、0x48..0x53、0x5A、0x15（各位互不相同，顺序不影响结果）。金钱 100；返回格 x/y（0x004C3008/0x004C3010）、两个计时器帧数 −1；returnMap、warpLeaderAct、timerEvent 0；三层行表释放置空；音乐曲字段 6 dword 清零；淡出模式 1、淡出长度 0、0x004D0B84 与 musicCue 0、Switch(1, 1, 0xA)；0x004C2B88=0x40000000；震动表 0x20 dword 填 0xCFCF；0x004C6564=3；对话窗链表逐个释放（+0x38、+0x44、节点）并清零链表哨兵 0x004C66B0（0x4C 字节，+0x48 即链表头 0x004C66F8）；SetWalkSpeed(0x10)。
- 0x004C4F10 写 1：全部汇编中只有此处引用，无代码读取；它位于队伍记录 0x004C4F18 之前，只有角色 −1 的越界访问（deviations.md 第 333、339 行，不复现）会碰到，故不建成员。
- 三个指针角色：清 0xE4 字节，act 0x2329/动作 0、0x2329/0xC、0x2391/0，旗标 0x9000，ResetPlayback；依次 Run，失败报 "Act Err(Misc:Init:Mouse:Norm/Btn/nPage)"/"設定錯誤 GUID[%d]\nact[%d] qq[%d] dir[%d]"（0x004AF244），第三个用按钮指针的值（原有 BUG 保留）。原程序先设好按钮与翻页两个角色再依次 Run，实现交错执行，两者互不依赖，结果相同。
- 计时数字动作清零并 ResetPlayback；两个传送提示文本指针 0；12 个画面动作清零、ResetPlayback，act 0x232E、动作 0..11；系统按钮 ResetPlayback、0x2329/8；视野 0x280×0x1E0；按钮滑出 2、等待 0；聚光灯与载入画面动作 ResetPlayback；对话框框架 0x232D/0x232F/0x2330/0x2331 与名牌 0x2337×4（清零、ResetPlayback、动作 0）；效果动作 ResetPlayback、天气 16 dword 填 0x0F0F0F0F、效果 0x232B/0x60；四片落叶动作 ResetPlayback、act 0x232B。
- 表 0x12 数字数 n（到 0xFFFF），分配 (8n+0x80)/8 个字（清零），首字 n/8；n 不是 8 的倍数时报 "戰鬥機率讀取"/"設定數質量必須是 8 的倍數" 不复制，否则复制到首字之后。表 1 为角色表，rolesTableBytes 按每条 0x18 累加。表 6 四条 0x34 字节 RoleRecord::Load 到 0x004C4F18 起每 0x3C。四个战斗块 0x60 字节清零；物品链表释放（各节点 +0xB0 与节点）、0xB4 字节头清零；recipesTable=表 0x18；表 5（物品、数量，物品 0 结束）AddItem(物品, 数量, 1)。技能：四个头链释放后 NewEmptyItem，表 9..0xC（0 结束）各 AddItem(…, 1, 1)。装备 64 格释放（节点链与头的 +0xB0）后 NewEmptyItem；表 0xD..0x10 每名角色 11 格：非 0 时 AddItem 到局部链头取得节点，0 时 NewEmptyItem，替换原格（释放旧头）。
- 整理：报错字面量注明地址（翻页指针的串因风格检查拆成两段）；scene.h 中 returnColumn、returnRow、loadingPicture、viewWidth、viewHeight、pointerRole、leafActs、musicTunes、warpLeaderAct、rolesTableBytes、pointerButtonRole、encounterGroups、returnMap、两个提示文本的注释改为 swd3e 地址。
