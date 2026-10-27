# 第二遍：Party（队伍数据）

## 0x00411390 Party::SetRoleStat — exact

汇编：记录 = 0x004C4F18 + 角色×0x3C；字段 0..13 写 +4..+0x1E 的字（取参数低 16 位）；14 写 +0x20 双字；15 写 +0 双字；16：+0x2C 字节 = 值，0x0048A010(角色+1, 值+1, &输出) 成功则 +0x20 = 输出；其他字段不写；返回 0。
对照：一致（WordStats 0xE、ExperienceStat 0xE、TotalExperienceStat 0xF、LevelStat 0x10，RoleRecord 偏移见 0x0040E0D0）。

## 0x004115D0 Party::RoleStat — exact

汇编：字段 >u 0x10 → 0；0..5 读 +4..+0xE 符号扩展；6..13 读 +0x10..+0x1E 零扩展；14 读 +0x20、15 读 +0 双字；16 读 +0x2C 字节。
对照：一致（SignedStats 6）。
