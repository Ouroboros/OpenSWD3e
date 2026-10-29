# 第二遍：爬坡小游戏（minigame）

## 0x0049EFF0 AscentGame::Start — exact

汇编：已有状态 → Stop。0x00579A30 = 0x320；分配 0x90 字节清零状态 → 0x00579A3C：+0 = 参数一、+4 = 0、风 = rand(3)−1、流 = rand(8)、+0x10 = +0x14 = 0、旗标 = 0x80000000。释放旧背景动作（0x00579A34）与领头复本（0x00579A2C），各新建 0xA4 字节清零动作并 0x0040DF70 重置：背景（参数二、三）、复本（参数六、七），方向（+0x34）= 0。玩家（+0x64）：(0x140, 0x320 − 0x14)、速度 0、动作（参数四、五）；领头（+0x68）：(0x140, −0x48)、动作（参数六、七）；同伴（+0x6C）：(0x140, −0x3C)、动作 0x2332／0xB。八个障碍：(−0x400, −0x400)、速度 0、清零重置的动作；障碍动作表（+0x1C 起 8 组）清零。体力（+0x18）= 参数八；按键表 0x004D1638（0x200 字节）清零；镜头上 = 玩家 y − 0xF0；FollowPlayer；返回 1。
对照：Start 一致（CourseHeight 0x320、kStartWinds 3、kStartCurrents 8、kStartX 0x140、kPlayerFromBottom 0x14、kLeaderY −0x48、kPartnerY −0x3C、kPartnerAct 0x2332／kPartnerAction 0xB、Parked −0x400、Running、kCameraOffset 0xF0；随机数次序相同）。
差异：无。

## 0x0049F3D0 AscentGame::Stop — exact

汇编：无状态 → 1。释放玩家、领头、同伴与八个障碍（各先释放动作），再释放状态、置 0；返回 1。
对照：Stop 一致（state.reset() 连同各精灵与动作释放）。
差异：无。

## 0x0049F4D0 AscentGame::SetObstacleAct — exact

汇编：无状态或种类（有符号）不在 0..7 → 0；否则 +0x1C + 种类×8 = 动作、+0x20 + 种类×8 = 动作号，返回 1。
对照：SetObstacleAct 一致（u32 种类使负数同样落到 0）。
差异：无。

## 0x0049F510 AscentGame::Frame — exact

汇编：无状态 → 0。移动量两个清零；ReadControls(&横, &纵)；Update(横, 纵)；Draw；旗标 0x10000000 → Stop；返回状态为空。
对照：Frame 一致（Done 0x10000000）。
差异：无。

## 0x0049F580 AscentGame::ReadControls — exact

汇编：无状态 → 返回。上键（按键 4）press≠0：按住帧（+0xC）≥3 → 纵 = −0x400（帧数非 0 时）；否则体力 >5 → 纵 = −4。下键（6）按住帧 = 1 且体力 >0 → 纵 = 2。左键（3）press 低字节 & 0xF → 横 = −3；右键（5）→ 横 = 3。Esc（0）按住帧 = 1 且 (press & 0xF) 低字节 = 1 → 旗标|=0x40000000、淡出帧（+0x5C）= 0x1E。
对照：ReadControls 一致（Button 编号、ButtonState 的 press +0 与 heldFrames +0xC、kRestHeld 3、kRest −0x400、kStrokeUpStamina 5、kStrokeUp −4、kStrokeBack 2、kSideStep 3、kPressMask 0xF、Ending、kFadeFrames 0x1E）。
差异：无。

## 0x0049F640 AscentGame::Update — exact

汇编：无状态或旗标 0x40000000 → 返回。风 += rand(3)−1，夹在 −16..16；流 += rand(4)−1，夹在 0..16；+0x10 += 风/8、+0x14 += 流/8（有符号向零）。Steer(玩家, 风/8, 流/8, 0)、Steer(玩家, 横, 纵, 1)；玩家位置加速度；FollowPlayer。玩家 x 大于／小于领头 x → 领头与同伴 x 各加／减 1。八个障碍：y ≤ −0x400 → CRT rand() % 0x2EE = 1 且镜头上 ≥0x3C → 种类 = rand(8)、x = rand(0x208)+0x3C、y = 镜头上 − 0x3C、速度 0、动作取障碍动作表；否则 Steer(障碍, 风/8, 流/8, 0)、位置加速度、PushAway(玩家, 障碍)、y > 镜头上 + 0x230 → 停放。玩家 y ≤0 → 旗标|=0x40000000、淡出帧 0x1E。
对照：Update 一致（kWindSteps 3、kCurrentSteps 4、kDriftLimit 0x10、kDriftShift 8、kSpawnChance 0x2EE、kSpawnRoll 1、kSpawnMargin 0x3C、kSpawnColumns 0x208、kObstacleDepth 0x230；先消耗 CRT rand 再判镜头，两种随机数的次序相同）。
差异：无。

## 0x0049F920 AscentGame::Draw — exact

汇编：无状态 → 返回。锁定画面（0x004C6344）取指针 → 0x004EDA44 后解锁。旗标 0x40000000：TintPixels(画面, 0x004B1E8C（每行字节 1280）× 0x004B1E94（480）/ 2 = 640×480 像素, −1, −1, −1)，淡出帧减 1，为 0 → 旗标|=0x10000000；否则 DrawBackground(+0x10 % 64 − 64, (+0x14 − 玩家 y) % 64 − 64)（C 余数）、DrawSprites、DrawGauges。主表面 Blt 呈现。
对照：Draw 一致（kFadeStep −1、kTileSize 0x40；TintPixels 每次处理一个 16 位像素，计数即整屏像素数；呈现属平台替换）。
差异：无。

## 0x0049FA20 AscentGame::DrawBackground — exact

汇编：背景动作前进；取图片；y 自参数二起 <0x1E0、每行 x 自参数一起 <0x280，以模式 0 画图，x 加图宽、y 加图高。
对照：DrawBackground 一致；取不到图片与宽高为 0 属偏差 334。
差异：无。

## 0x0049FAB0 AscentGame::DrawSprites — exact

汇编：y > −0x400 的障碍、领头、同伴、玩家依次：动作前进，取图片，以模式 0 画于 (x − 动作.x, y − 动作.y − 镜头上)。
对照：DrawSprites／DrawSprite 一致；取不到图片属偏差 334。
差异：无。

## 0x0049FCA0 AscentGame::DrawGauges — exact

汇编：行距 = 0x004B1E8C/2 像素；路程条起点 (32, 64)：第 63 行与第 264 行自第 33 列 12 像素 (8,−8,−8)；200 行各五段：列 32 宽 2 (8,−8,−8)、34 宽 2 (4,4,4)、36 宽 6 (12,12,12)、42 宽 2 (4,4,4)、44 宽 2 (8,−8,−8)。标记：DrawPicture(0x232F, 0, 0x16, 玩家 y × 200 / 0x00579A30 + 0x32)（有符号除法）。体力条起点列 56：同样上下两端；200 行：(100 − 体力)×2 > 行 → 列 58 宽 10 (−4,−8,0x10)，否则列 58 宽 2 (−4,−8,0x10)、60 宽 6 (−4,−4,0x18)、66 宽 2 (−4,−8,0x10)；两边列 56 与 68 宽 2 (8,−8,−8)。
对照：DrawGauges 一致（kGaugeTop 0x40、kGaugeLeft 0x20、kGaugeLines 0xC8、kStaminaGaugeLeft、kCap、kCourseBands、kStaminaSpent、kStaminaLeft、kStaminaEdges、kMarkerFile 0x232F、kMarkerX 0x16、kMarkerTop 0x32、kStaminaMax 0x64）。
差异：无。

## 0x0049FE60 AscentGame::Steer — exact

汇编：两速度非 0 时向 0 退 1。横≠0 → 速度 += 横，超过横（同号方向）→ 横。纵 = 0 → 玩家体力 +1（>100 → 100），返回 1；纵 = −0x400 → 纵速 = 0、同样加体力，返回 1；否则纵速 += 纵并同样夹住，玩家体力 −4（≤0 → 0），返回 1。
对照：Steer 一致（kRest、kStaminaMax、kStrokeCost 4）。
差异：无。

## 0x0049FF60 AscentGame::PushAway — exact

汇编：dx、dy 为整数差；x87：距离 = sqrt(dy² + dx²)，另存 float；距离 > 60.0f（0x004AAE38；ah&0x41 为 0）→ 返回。dx < 0 → −1.0f（0x004AB258），否则 1.0f，ftol 加到 x；精灵 y > 障碍 y → y −= ftol(float距离 − 60)，否则 y += ftol(float距离 − 60)。
对照：PushAway 一致（kPushRadius 60.0 与 exe 的 60.0f 相等；距离非负，NaN 不会出现）。
差异：无。

## 0x004A0010 AscentGame::FollowPlayer — exact

汇编：镜头上（0x00579A40）= 玩家 y − 0xF0；返回状态指针。
对照：FollowPlayer 一致。
差异：无。
