# 第一遍汇编验证：game_field（进入地图等）

## 0x0040C570 Game::LoadCity — exact（注释更正）

- 清帧间隔；有绘制表面时画载入画面（动作 0x232A、动作号 0x4F 于 (0x78,0xCA)）并 ShowLoading(−1)。screenFlags=0；清旗标 5、6；天气粒子 16 dword 填 0x0F0F0F0F；ResetPlayback(效果动作)；四片落叶 x=0x7FFFFFFF；清 7、8；闪电数组 0x300 字节清零，lightningShown=0、lightningCount=0；清 0x16；闪电数组再清一次；萤火虫计数 0；聚光灯中心与目标 (0x140,0xF0)、步长 0。ActArchive::CloseAll、TswArchive::Flush、StopAllSounds。
- 表 3（每条 0xE 字节，0xFFFF 结束）找地图：阴影种类 byte[+4]&0xF、步行速度 byte[+6]&0xF、layerFrames word[+8]、遇敌组 word[+0xA]、剧情字 word[+0xC]；byte[+4]&0x10 时不可城镇传送；byte[+5] 各位依次控制旗标 0xF、5（0x40；0x20 时另设动作 0x40/8 个，默认 0x60/4 个）、6、7、8、0x13、0x16；三个着色取速度表 0x004B0C20（tests 比对）；步行速度非 1/2/4/8/0x10 时 4；layerFrames<1 时 1；遇敌组（有符号）大于组表首字时报 "載入地圖"/"錯誤的戰鬥機率組[%d]" 并取 1。
- （光驱版本检查 0x0040C8C1 为用户要求的平台替换，见 deviations.md 第 100 行。）MusicZone；法宝鬼魂移除；清 0x19；表 0x19（地图、旗标对，地图 0 结束）中本图且旗标未置者置旗标与 0x19。未找到地图时报 "遊戲初始"/"找不到設定的起始地圖碼 (%d)" 并关闭窗口。
- ShowLoading(2)、currentMap、layerDirection=1、置旗标 地图+0x1388；选项位 0 时 SaveRoleStates(x,y)；遇敌步数 0、声音上升表清零、触发区链表释放、各场景列表与选项释放、标记释放（0x004E6B10 四项）、落叶重置、物件释放、按键清零、ClearRoles、72 个移动记录与 8 个随行记录清空、随行表填 −1、随行数 0、叠加层与提示链表释放。ShowLoading(0xA)、roleCount=1、释放调色板。LoadMap 失败报 "載入地圖"/"過程發生錯誤" 并关闭窗口；8 位地图从画面数据复制 0x200 字节调色板并 ConvertPixels(0x100)；画面指针为空时记 "LoadCity ,gMap.Pixel==NULL"。layerTick=1、layerIndex=0、layerOffset=0、layerTiles=宽×高；地图名（失败报 "讀取地圖名稱"/"MapName.txt中找不到地圖[%d]名稱"（参数为单元号）并用 "不知道"）；ShowLoading(0x55)、(0x5A)。
- 天气旗标 5 时四个粒子依次取 Below(3)+1、Below(动作数)、Below(宽×16)、Below(高×16)、Below(2)−2、Below(2)−2。地图范围 (0,0,宽×16,高×16)；建触发区；ShowLoading(0x5F)；随行数 1。
- 角色表（每条 12 字，0xFFFF 结束）：与此前各条 id 重复时报 "讀取路人"/"重複定義角色(%d)" 并跳过该条；id 为 0、10000、10001 时地图写为本图；id 为领队号时写回地图、act、动作、方向、格坐标、事件/路径/步 0、旗标 0xD100，主角序号与随行[0]=roleCount+已放数，两条轨迹填入坐标×16；地图不是本图则跳过；格坐标超出地图（无符号 >）时报 "地圖(%d)啟動對話(%d)\n路人(%d)位置超出地圖..."（参数顺序 地图、领队事件、id、宽、高、x、y）并改写为 宽−1、高−1；否则填角色（id、act、动作、方向、动作状态三个字段 0、像素坐标、事件、路径、步（有符号）、旗标、talkState/eventOffset/eventCursor 0）、LoadRoleSound、按旗标&3 插入层；id 1 且旗标 0x19 且变量 2 非 0 时以 (变量2+0x5A, 1, x, y, 2.0, 4.0) 生成鬼魂；旗标 0x80 时 FollowLeader、置可见，地图 0x16 或剧情字位 15 时又清可见；旗标 0x200 时登记冒烟标记（超过四个报 "地圖(%d)定義煙囪物件數量超過四個"）。
- roleCount 加已放数；ShowLoading(0x64)；角色 1..n−1 InitRole；镜头居中；两条调试日志；帧间隔 0x23。
- 更正：RoleMark 注释中的数组地址由 swd32 的 0x004CACE0 改为 0x004E6B10（实现布局与 swd3e 一致：链表 +0、x +4、y +6、+8、+0xA、角色 +0xC、+0xE）。

## 0x0040D130 Game::MusicCue、0x0040D140 Game::AudioPump — exact

- MusicCue 只返回 1。AudioPump 依次更新音乐队列（0x0049D1A0）、音乐流（0x0049E400）、音效（0x0049DB50），返回 1（实现为 AudioManager::Update）。

## 0x0040D170 Game::MusicZone — exact

- 表 2 第二个 dword 指向的偏移再取一次得到场景音乐表（每条 8 字节：地图、曲 1、曲 2、标记，地图 0 结束）。未找到：两首场景曲清零，musicCue 清 0x40000|0x80000。找到且两曲与当前相同：队列模式为 2 且剧情淡出标记为 1 时 Switch(1, 标记&3, 0xF) 并清三个剧情曲字段，返回。否则记录标记与两曲，两曲各以 安装目录+"music\"+曲名 设入队列第一列表的槽 0、1，musicCue=0；队列模式非 2 时 Switch(1, 标记&3, 0xF)，否则剧情淡出标记为 1 时同样切换并清剧情字段。

## 0x0040D390 Game::MusicName — exact

- 表 2 首 dword 为名字表偏移，取第 index 项偏移，名字在该项 +4。

## 0x0040D3B0 Game::LoadRoleSound — exact

- 表 0x15（每条 6 字节：角色、声音、周期，0 结束）中找角色 id：声音写 +0x2C，计时 +0x30 = 周期<<16|周期，返回 1；找不到返回 0。

## 0x0040D410 Field::CenterCamera、0x0040D4C0 Field::CameraAround — exact

- CenterCamera：主角 x−0x130、y−0xF0，负数取 0；右 = 左+0x280，≥ 宽×16 时左 = (宽−0x28)×16、右 = 宽×16；下 = 上+0x1E0，≥ 高×16 时上 = (高−0x1E)×16、下 = 高×16。CameraAround 同样以 (x−0x140, y−0xF0) 写入矩形四项。实现在 640×480 视野下逐项相同；视野更宽时地图小于视野则居中，属用户要求的宽地图增强（deviations.md 第 92 行已列两地址）。

## 0x0040D560 Game::SaveRoleStates — exact

- 角色 1..roleCount−1（roleCount 为 1 时不进入）：复制 0xE4 字节；旗标 &0x18000000、id 0xFFFF、为主角时跳过。旗标位 7：UpdateRoleRecord(id, act, 动作, 方向, x, y, 事件 +0x1E, 路径 +0x1C, 置位=旗标, 清位=0, currentMap, 0x8080)。否则路径号非 0 时 x、y 先清低 4 位，路径当前字为 8（Step）时在 72 个移动记录中找本角色：坐标取终点（无符号），x 或 y 超出 宽×16／高×16（无符号 >）时报 "角色回寫"/"角色(%d)\n終點位置(%d,%d)\n不在地圖(%d,%d)內"；无论是否找到步数加 1；最后 StoreRole(副本)。

## 0x0040D720 Game::StoreRole — exact（越界查找已登记 deviations.md 第 115 行，来源改为 asm/0040D720.asm）

- 每条 0x18 字节按 +2 的 id 查找；旗标 2 置且到 0xFFFF 时报 "!!! 警告 !!!"/"角色編號(%d)\n資料歸鍵時\n找不到符合的位置\n可能是定義錯誤或是角色不存在了"。找到：+4 act、+6 动作、+8 方向、+0xA x>>4（16 位）、+0xC y>>4、+0xE 事件、+0x10 路径、+0x12 步、+0x14 旗标（各取低 16 位）。

## 0x0040D7C0 Game::UpdateRoleRecord — exact

- 地图字 0xFFFF 结束；未找到时 wsprintf "未定義角色[%d]" 送调试输出（nullsub）返回 0。找到返回 1：各参数非 0xFFFF 时写 act/动作/方向/x/y/事件；路径非 0xFFFF 时写路径且步 0；清位参数（第 10 个）先 AND，置位参数（第 9 个）后 OR；地图；变量字非 0x8080 时写 +0x16。

## 0x0040D8D0 Game::LoadRoleRecord — exact

- 地图字 0xFFFF 结束，按 id 找到：id、act、动作、方向（零扩展）、x、y = 格×16、事件 +0xE、路径 +0x10、步 0、旗标 +0x14，返回地图字（零扩展，0xFFFF 时为 0x0000FFFF，不会与 −1 相等故不记日志）。未找到时记 "未定義角色[%d]" 返回 0xFFFFFFFF。

## 0x0040D980 Game::FollowLeader — exact（修正 TileStart；越界补登记）

- 路径号非 0 时在 72 个移动记录中找本角色；找到后若 x 或 y 低 4 位非 0：ClearRoleCells，取当前步方向（步号 &0x7FFF），x、y 低 4 位非 0 时反复减步长表 0x004AA47C／0x004AA49C（与 StepX/StepY 逐字节相同）直到低 4 位为 0，RemoveRole(id, 旗标&3, (y>>4)−1, 0)；随后 UpdateRoleRecord(id, 七个 0xFFFF, 置位 0x80, 0xFFFF, 0xFFFF, 0x8080)、移动记录清空。无论是否在路径上：随行表[随行数]=index、对应随行移动记录清空、事件 0、旗标清 0x4000 置 0x80、随行数加 1。
- 差异与修正：原循环只有在余数是步长（±4）的倍数时才结束，否则永不结束；RolePath::TileStart 原先直接给出格子起点。现改为余数不能被步长整除时与步长为 0 一样 Hang()（src/role/role_path.cpp），测试 "game state: a role between tiles goes back to its tile as the original's loops take it" 去掉了按格子起点断言的三项。
- 随行数已达 8 时原程序写到随行表与随行移动记录之外：补入 deviations.md 第 327 行（实现不写、照样加 1）。成员注释改为 swd3e 地址 0x004D1610、0x004D2370。

## 0x0040DB00 Game::LeaveParty — exact（偏差表更正）

- FindRoleOrPlayer 失败：按 0x16 字节（swd32 的 11 字记录）步进比较 +2，命中时该处 +0x14 字 &0xFF7F 返回，到 0xFFFF 报 "解除夥伴"/"角色(%d)不存在"。找到：在 8 项随行表中找下标，找不到报 "角色(%d)不是夥伴"。随行移动记录步号 <0x7FFF 且坐标不在格上时 ClearRoleCells；方向不为 0xFF 时按步长加到格上（TileStart(−步长)）；RemoveRole。移动记录清空；act/动作/方向/事件/路径/步 0/旗标（低 16 位）写入；UpdateRoleRecord(id, 五个 0xFFFF, 事件, 路径, 置位 0, 清位 0xFF7F, 0xFFFF, 0x8080)；MarkRoleCells；随行表与随行移动记录后项前移（memmove，末项保留）；随行数减 1。
- 原程序在 0x0040DB98 检查方向 0xFF 并跳过对齐：deviations.md 第 131 行误列 0x0040DB00，已删去该地址。

## 0x0040DD50 Game::BuildTriggers — exact

- 表 7 每条 0xE 字节（地图、左、上、右、下、dword 表偏移），0xFFFF 结束；本图条目分配 0x18 字节节点插在链表头之后。

## 0x0040DDD0 Game::RollEncounter — exact（修正）

- 遇敌组 0 返回 0；先 Below(100)；组表 (组−1)×16+2 字节起 4 对（门槛累加、几率，均有符号）：steps < 累计门槛或 force==1 时取该对，4 对都不满足返回 0；几率 ≤ 掷值且 force≠1 返回 0。主角 x、y **逻辑右移** 4 位（shr）后与触发区有符号比较（左≤列≤右、上≤行≤下），无触发区返回 0；命中时 Below(战斗数)，遇敌步数清 0，返回所选战斗号。
- 修正：格坐标原先用算术右移（注释误称 sar），改为无符号右移。

## 0x0040DEB0 ClickRegions::Count、0x0040DED0 Find、0x0040DF30 Free — exact

- Count 数链表；Find 返回首个严格包含指针（无符号 left<x<right、top<y<bottom，+8/+0xC/+0xA/+0xE）的序号，写出该节点或空；Free 释放链表并清 0x004E4DC8..0x004E4DD8 五个 dword。注释中的地址由 swd32 的 0x004C8BE8/0x004C8BEC 改为 swd3e。

## 0x0040DF70 ResetPlayback、0x0040DFA0 Field::FindRegion — exact

- +0x1C、+0x20、+0x3C 置 −1，+0x48、+0x46、+0x44、+0x42 字清 0，+0x9C 清 0。FindRegion 从 0x004C6258 链表头找 +4 等于 id 的节点。

## 0x0040DFC0..0x0040E0B0 游戏旗标、步速、帧间隔、MoveRecord::Clear — exact

- 旗标字节 0x004C4B0C[flag>>3]（sar）与位表 0x004AA4BC（01 02 04 … 80，tests 比对）；Set 置位、Clear 清位、SetTo 值为 1 时置位否则清位。SetWalkSpeed 写 0x004C4A58；SetFrameInterval 写 0x004D1540 返回 1；ClearFrameInterval 清 0 返回 1。MoveRecord::Clear 以 0xFF 填满 0x21C 字节。

## 0x0040E0D0 RoleRecord::Load — exact

- 0x34 字节：dword → +0；记录 +4/+6/+8 → +0xA/+0xC/+0xE（上限），+0xA/+0xC/+0xE → +4/+6/+8，+0x10..+0x1E 原位，+0x20 → +0x2A，+0x22 字零扩展 → dword +0x20，+0x24..+0x28 原位，字节 +0x2A → +0x2C 等级，+0x2B..+0x33 → +0x2D..+0x35，再把 +0x33 复制到 +0x36、+0x37。

## 0x0040E1C0 MarchingRect::Draw — exact

- x<0、y<0、x+宽≥640、y+高≥480 不画。颜色 (0x3F−相位)&0x1F 三通道灰，经 0x00427680 转两像素，每画一点相位+1（&0x1F）；上边自左向右、右边自上向下、下边自右向左（起于 x+宽）、左边自下向上；结束时全局相位 0x004E6D30 加 1。

## 0x0040E3A0 GameState::DirectionToPlayer、0x0040E3F0 ShrinkScreenQuarter — exact

- 主角 (x+宽×8, y+高×8) 到 (x, y) 的角度（0x00411DA0）转方向（0x00411E80）。ShrinkScreenQuarter 120 行 × 160 列，每 4 像素取 1、每 4 行取 1，就地写入连续的 160 像素行。
