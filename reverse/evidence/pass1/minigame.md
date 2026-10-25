# 第一遍：升天小游戏（minigame）

## AscentGame（第 1467–1479 行）— exact

- 0x0049EFF0 Start：已有状态时 Stop；0x00579A30 = 0x320；分配 0x90 字节状态（HeapBlock::Allocate 清零）存 0x00579A3C；+0 = 参数 0、+4 = 0、wind = Random(3) − 1、current = Random(8)、scrollX／scrollY = 0、flags = 0x80000000；释放旧背景（0x00579A34）与副本（0x00579A2C），各新分配 0xA4 字节动作，ResetPlayback（0x0040DF70），id、action、+0x34 = 0（背景参数 1、2，副本取领头参数 5、6）；玩家（+0x64）x = 0x140、y = 0x320 − 0x14、速度 0、动作参数 3、4；领头（+0x68）y = −0x48、动作参数 5、6；同伴（+0x6C）y = −0x3C、动作 0x2332／0xB；八个障碍（+0x70）x、y = −0x400、速度 0、动作块清零后 ResetPlayback，障碍动作表（+0x1C，8×8 字节）清零；stamina（+0x18）= 参数 7；按键表 0x004D1638 起 0x200 字节清零；cameraTop（0x00579A40）= 玩家 y − 0xF0；FollowPlayer；返回 1。
- 0x0049F3D0 Stop：无状态返回 1；玩家、领头、同伴、八个障碍各释放动作与本体，再释放状态、清零，返回 1（背景与副本不释放）。0x0049F4D0 SetObstacleAct：有状态且种类 0..7（有符号）时写入动作与 action，返回 1，否则 0。
- 0x0049F510 Frame：无状态返回 0；两个移动量清零，ReadControls、Update、Draw；flags 0x10000000 时 Stop；返回状态为空。
- 0x0049F580 ReadControls：按键槽 0x10 字节，按下计数在 +0、按住帧数在 +0xC：Up（0x004D1678）按下非 0 时，按住 < 3 且 stamina > 5 → 纵向 −4，按住 ≥ 3（非 0）→ −0x400；Down（0x004D1698）按住 == 1 且 stamina > 0 → 2；Left（0x004D1668）／Right（0x004D1688）按下低 4 位非 0 → −3／3；Escape（0x004D1638）按住 == 1 且按下低 4 位 == 1 → flags |= 0x40000000、fadeFrames（+0x5C）= 0x1E。
- 0x0049F640 Update：无状态或 0x40000000 时返回；wind += Random(3) − 1，钳到 −16..16；current += Random(4) − 1，钳到 0..16；scroll += 漂移/8（有符号）；Steer(玩家, wind/8, current/8, 0)、Steer(玩家, 横, 纵, 1)；玩家按速度移动；FollowPlayer；领头与同伴按玩家 x 左右移 1；每个障碍：y ≤ −0x400 时 rand()（0x004A14EC）% 750 == 1 且 cameraTop ≥ 0x3C → 种类 = Random(8)、x = Random(0x208) + 0x3C、y = cameraTop − 0x3C、速度 0、动作取障碍动作表；否则 Steer(障碍, 漂移/8, 0)、移动、PushAway(玩家, 障碍)，y > cameraTop + 0x230 时停放；最后玩家 y ≤ 0 → 0x40000000、fadeFrames 0x1E。
- 0x0049F920 Draw：锁定帧表面（0x004C6344）取指针（0x004EDA44）后解锁；0x40000000 时 TintPixels(全帧, 行距 1280 × 高 480 / 2 = 640×480 像素, −1, −1, −1)、fadeFrames 减 1，为 0 时 |= 0x10000000；否则 DrawBackground(scrollX % 64 − 64, (scrollY − 玩家 y) % 64 − 64)（C 余数）、DrawSprites、DrawGauges；最后把帧表面 Blt 到主表面（DDBLT_WAIT，源码 ShowFrame）。0x004B1E8C／0x004B1E94 = 1280／480 已读 exe。
- 0x0049FA20 DrawBackground：Run 背景动作、GetPicture；y 从 top 起 < 480、x 从 left 起 < 640，按图宽高平铺 DrawPicture(…, 0, 0)。空图与宽高为 0 见偏差第 339 行（已引用汇编）。
- 0x0049FAB0 DrawSprites：在用的障碍（y > −0x400）、领头、同伴、玩家依次 Run、GetPicture，画于 (x − act.x, y − act.y − cameraTop)。
- 0x0049FCA0 DrawGauges：行距字节 1280；航程条起点第 64 行第 32 列：上下帽（第 63、264 行，第 33 列起 12 像素，8／−8／−8），200 行各五段（0 列 2 像素 8／−8／−8，2 列 2 像素 4／4／4，4 列 6 像素 0xC，10 列 2 像素 4，12 列 2 像素 8／−8／−8）；标记 DrawPicture(0x232F, 0, 0x16, 玩家 y × 200 / 0x320 + 0x32)；体力条右移 24 列：帽同上；200 行：2×(100 − stamina) > 行 → 2 列 10 像素 −4／−8／0x10，否则 2 列 2 像素 −4／−8／0x10、4 列 6 像素 −4／−4／0x18、10 列 2 像素 −4／−8／0x10；两侧 0、12 列 2 像素 8／−8／−8。源码 kCap、kCourseBands、kStaminaSpent、kStaminaLeft、kStaminaEdges 同此。
- 0x0049FE60 Steer：两速度各向 0 收 1；横移非 0 时加之并以横移为限；纵移 0 → 玩家 stamina + 1（上限 100）；−0x400 → 纵速 0、同上；其它加之并以纵移为限，玩家 stamina − 4（下限 0）；返回 1。
- 0x0049FF60 PushAway：ox、oy 为整数差，fild 后 d = fsqrt(oy² + ox²)（存 float d）；d > 60.0f（0x004AAE38）返回；x += ftol(ox < 0 ? −1.0f（0x004AB258）: 1.0f)；短 = (float)d − 60.0f，sprite.y > 障碍 y 时 y −= ftol(短)，否则 y += ftol(短)。
- 0x004A0010 FollowPlayer：cameraTop = 玩家 y − 0xF0，返回状态。
