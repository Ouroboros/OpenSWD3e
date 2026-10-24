# 第一遍汇编验证：GameState 等小函数

## 0x0040C420 GameState::FindRole(id) — exact

- 角色 0..roleCount−1（无符号），id（+0x24）相等且旗标无 0x10000000 时返回序号，否则 −1。

## 0x0040C460 RolePath::RoleId — exact

- 序号 ≥ roleCount（无符号）时以 "%d"（0x004B0E40）格式化该角色 id，弹出标题 "Illegal Role id"（0x004B0E30）的消息框；返回该序号角色的 id。序号超出角色表时实现夹到最后一项读取（原程序读表外）。

## 0x0040C4D0 GameState::FindRoleOrPlayer、0x0040C500 GameState::FindRole(id, &index) — exact

- 先把输出置 0；id 0xFFFE 时输出主角序号（0x004C4B00）并返回 1；否则调 0x0040C500：输出 FindRole 的结果（找不到为 −1），找到返回 1，否则 0。

## 0x0040C530 ErrorBox — exact

- vsprintf 到 0x004C4658，再 MessageBoxA(主窗口, 该文字, 标题, 0)。实现另写入日志警告级。

## 0x0040C560 DebugLog — exact

- 空函数（retn）。实现把消息写入重建程序自己的日志，对游戏无可见影响。
