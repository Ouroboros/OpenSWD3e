# 第二遍：role（主角操作与碰撞）

方法见 reverse/analysis/pass2-method.md。

## 0x00402AA0 RoleControl::ControlPlayer — exact（补登偏差、注释改为 swd3e 地址）

汇编推导（按出口与分支）：

- 调试键开关：0x004AF7A0==1 且按住 0x9D、0x36、0xC7、0xCF 时置旗标 0x52。
- 旗标 0x52 时（0x0040DFC0 返回 0/1）：
  - F12(0x58) 且领队 id==0xFFFF、0x004C66F8==0：Sleep(500)，storyState(0x004E6CF4)=3，menuMode(0x0050A404)=1，0x004E6D00=0、0x004E6D04=0，0x00406B50，返回。
  - Ctrl(0x1D 或 0x9D) 时依次：F(0x21) 且旗标 0xC：battleRequest=0x0040DDD0(步数,1)，为 0 直接返回；否则停音效、关动作、清图片缓存、开战、inBattle=1、Sleep(300)、解除地图视图（失败 ErrorLog 0x004AF2FC，成功清 0x004D12B8）、关映射、角色 1..数−1 清 0x01000000，返回。F9(0x43)：0x004E6CFC=(+1)%2（有符号取余），Sleep(350)，按结果 DebugLog 0x004AF2EC「啟動快速模式」／0x004AF2DC「關閉快速模式」，返回。Q(0x10)：0x004D3FC4=1，0x0040E0A0（帧间隔清零），0x004D1404|=4，0x0040E020(1)，返回。S(0x1F)／L(0x26) 且空闲：0x004E6D00=1，0x004E6D04=0／1，Sleep(500)，menuMode=1／2，storyState=3，0x00406B50，返回。F1(0x3B)／F2(0x3C)：0x004E6CE0／0x004E6CE4 取反，0x004C15F0=0，Sleep(300)，返回（F1 优先）。F3(0x3D)：旗标 0x14 取反，0x004C15F0=0，按新值 DebugLog「阻擋可通過」／「阻擋不可通過」，Sleep(300)，返回。Tab(0x0F)+小键盘加(0x4E)：0x004AF714 加一上限 0x40，0x004C15F0=0；Tab+减(0x4A)：减一下限 1，0x004C15F0=0；不返回。
  - F4(0x3E)：0x004E6D08=0，ShowCursor(1)，DialogBoxParamA(0x6A)，返回 0 时 0x004E6D08=1；返回。
  - F7(0x41)：0x004D1404|=0x80，Sleep(250)，0x0049EFF0(0x64,0x2332,0,0x2332,1,0x2332,2,0x64)，8 次 0x0049F4D0(k,0x2332,{3,4,5,6,3,4,5,6}[k])；不返回。
  - F6(0x40)：物品 0x65..0x4AF，读出成功且类别（+0x56，u16）为 0..9、0x12、0x13、0x11、0x10、0xF、0x14、0x15、0x18 时 0x00454970(背包,id,10,0)；Sleep(500)。
- 走跑键（配置键 0x004D0BD0）：DebugLog「切換移動速度」，0x004D3C70=0x004AA38C[值]，Sleep(200)（平台替换见 deviations.md 第 138 行）。
- advance(0x004C30D0)=0；静止 = 四个步长全为 0；0x004E6B58==0 时 0x004E582C−=2，负则 0。
- 0x004D1404&0x40、0x004E6CDC、非静止、0x004D1C88 任一成立时返回。
- 卷动／步进判定：屏幕坐标为无符号差；左卷 = 差≤0x130 且镜头左>0；右卷 = 差≥0x130 且镜头右<16·地图宽（有符号）；上下同理以 0xF0；左步 = x≠0，右步 = 差<0x258，上步 = y≠0，下步 = 差<0x1C2。
- 空格（+0x10 按下、+0xC 帧数==1）且无选项时 advance=1；有对话窗且 Enter 时 advance++；空格或 Enter 且有选项：0x004E4DC8&0x1000 时 0x0040DF30，返回。
- 空格且领队空：以 +0x74 方向直接查 0x004AA398／0x004AA3B8，0x00404920 找前方角色；找到且旗标无 4、+0x26==0 时：DirectionToPlayer(角色中心)，旗标 0x800 时角色转向（存动作、方向，动作 0、延迟 0，Run，失败 ErrorBox「Act Err(KeyCole:Talk1)」0x004AF288 格式）；主角动作 0、方向 (f&0xFFFE)+((f−1)&1)、延迟 0，Run 主角，失败「Act Err(KeyCole:Talk2)」0x004AF244；领队取该角色五项，DebugLog「\nRoleEventBy…」；advance=0，返回。
- Enter 且领队空：0x004D1C88=0x80000001，返回。0x004C2FF8 且无选项：返回。
- 方向键：左先于右、上先于下；无选项时左右设方向 2／3 与 stepX，上下设 0／1（同时左右时 2→4、3→7；2→6、3→5）与 stepY；有选项时帧数==1 或 &7==7 时移动选项（有符号回绕到数−1，无符号 ≥数 回 0），choiceMoved=1。
- 旗标 0x14 为 0 且有步：0x00403DC0 滑行。滑行后有步时 StepEvent；无事件再用原步 StepEvent；找到角色时事件改为角色 +0x1E；旗标 0x18==1 时事件 = 主角格值&0xFF，找到者置 −1。
- 事件非 0：未找到角色时查区域 0x0040DFA0，无则 DebugLog「錯誤的劇情點(%d)」并读空指针 +0x10（deviations.md 第 135 行），旗标(+0x10) 为 1 时两步清零。领队空时：区域事件写领队（id 0xFFFD，坐标为主角减 16·0x004AA34C／0x004AA36C[方向]，方向不掩码），DebugLog「\nMapEvent.Talk[%d]」；角色事件同对话但用「Collision1／2」、0x004AF244 格式，主角处 Run 的是对方的动作，DebugLog「\nRoleEventByCollision…」，advance=0。
- 无步：动作≠上次动作时延迟 0，动作 0（空闲 >0x80 再置 0），0x004E6B58 加一 >0x64 或 0x004E582C&0xF 时 0x004E582C 加一上限 0x10。
- 有步：0x004E6B58=0，按 −1／非 0 设卷动或步进；动作非 0x10 时 8，双击或走跑开时 0x10；方向变延迟 0；动作变时延迟 0 并设步速（跑或双击加倍，快速模式 0x10）；旗标(地图+0x1770) 为 0 时步数加一，未战斗、旗标 0xC、无请求、领队空时掷遇敌，开战时先存音乐队列模式，再同调试开战（另清四个步长，ErrorLog 0x004AF138）；最后 battleRequest=0。
- 尾部：0x004C1604 加一，动作非 0、非 0x34 时清零；0x004C15F0 加一。0x00403DAC 为直接返回。

对照：
- 分支、出口、顺序、位宽（无符号屏幕差、有符号镜头比较、u16 类别、选项回绕）、常量（按键码、旗标号、方向、动作、0x004AA38C／0x004AA398／0x004AA3B8 表与 exe 字节一致）、日志与错误框原文（0x004AF2EC、0x004AF2DC、0x004AF2D0、0x004AF2C0、0x004AF2B0、0x004AF1E4、0x004AF288、0x004AF244、0x004AF2FC、0x004AF138、0x004AF1D0）均一致。
- 差异：源码对方向取低 3 位查表、对空格指针取 0，是未登记的保护。已补登 deviations.md（0x00402AA0 越界、空指针行）。
- 源码对 FindRoleAhead／StepEvent 结果另判 `< Roles`，两者只返回 −1 或有效角色号，与汇编等价。
- 注释中的 swd32 地址改为 swd3e：inBattle 0x004E6CF8、counterWait 0x004E6B58、inactive 0x004D3FC4（读写者与本函数一致）。

## 0x00403DC0 RoleControl::Slide — exact（坐标改为无符号移位，补登偏差，更正注释地址）

汇编：栈上 8 项×6 dword 表（左 {-1,2,1,0x80,0x40,0x20}、右 {宽,2,4,8,0x10,0x20}、上 {-1,0x80,1,2,4,8}、下 {高,0x80,0x40,0x20,0x10,8}、左上 {-1,1,0x80,2}、右下 {1,0x10,8,0x20}、左下 {-1,0x40,0x80,0x20}、右上 {1,4,8,2}）；四周阻挡 = 0x0040BF50(格指针, +0x6C, +0x70, 0x60800000)。纯横步：FreeLanes((x shr 4)+表[0], y shr 4, 高, 地图宽, 地图高, 1)；0 时 stepX=0；按 0x80→表[3] 原样返回、0x40→表[2] stepY=−1、0x100→表[4] stepY=1、0x3F→表[1] 改为 (0,−1)、0xFE00→表[5] 改为 (0,1)，否则两步清零。纯纵步同理：FreeLanes(x shr 4, (y shr 4)+表[0], 宽, 1, 地图宽, 0)，改 stepX。斜步：和≠0 取左上／右下，否则左下／右上；表[1] 不阻挡原样返回；表[2] 不阻挡 stepY=0、stepX=表[0]；表[3] 不阻挡 stepX=0、stepY=表[0]（反对角取负）；否则清零。
对照：表与 BlockingMap::Probe* 一致；差异——源码用有符号 `>>` 取格坐标，汇编为 `shr`，改为 TileOf（无符号移位）；格指针空保护补登 deviations.md（0x00403DC0、0x00404320 行）；表注释的 0x004040B0 改为 0x00403DC0。

## 0x00404220 RoleControl::FreeLanes — exact

汇编：沿轴位置 = 纵向时 y 否则 x；起点 = 地图宽·y+x；k=0..2：后移 k 格（位置−k ≥0，有符号）整条（长度个、步长 stride）无 0x60000000 时或入 0x80>>k；位置+k+长度 < 上限（有符号）时前移 k 格同样或入 0x80<<k；长度≤0 时整条视为空。
对照：一致（格网外见 deviations.md 第 118 行）。

## 0x00404320 RoleControl::StepEvent — exact

汇编：*found=−1、结果=−1；x、y 为 shr 4；按 stepX+3·stepY+4 查 0x004048F0 跳转表：0:(−1,−1) 1:(0,−1) 2:(1,−1) 3:(−1,0) 4:(0,0) 直接返回 −1 5:(1,0) 6:(−1,1) 7:(0,1) 8:(1,1)。行段：逐格（无符号计数 < 宽）读格值&0xFF，非 0 返回；否则 found=0x00404DD0(碰触坐标)，找到返回 0。列段：逐格（< 高）同样，格指针每步加角色宽（不是地图宽）。各方向起点与碰触坐标：左上 行 cell−宽−1/(x−1+i,y−1) 列 同起点/(x−1,y−1+i)；上 行 cell−W/(x+i,y−1)；右上 行 cell−W+1/(x+1+i,y−1) 列 cell+宽−W/(x+宽+1,y−1+i)；左 列 cell−1/(x−1,y+i)；右 列 cell+宽/(x+宽,y+i)；左下 行 cell+高·W−1/(x−1+i,y+高) 列 cell+W−1/(x−1,y+1+i)；下 行 cell+高·W/(x+i,y+高)；右下 行 cell+高·W+1/(x+1+i,y+高) 列 cell+宽/(x+宽,y+1+i)。循环不执行时返回最后读到的值（初值 −1）。
对照：一致。格网外区域号见第 118 行，格指针空见新增行。

## 0x00404920 RoleControl::FindRoleAhead — exact

汇编：x、y shr 4；前视 nearX=nearY=5，x≤5 时 nearX=x，地图宽−x≤5 时 nearX=地图宽−x（无符号），y 同理；按 stepX+3·stepY+4 查 0x00404CC4 跳转表。左上：far 1..≤nearY、side 1..≤nearX 查 (x−side,y−far)；上：nearY≥1 时 far 1..≤nearY、side 0..<宽+2 查 (x−1+side,y−far)；右上：far 1..≤nearY、side 1..<nearX 查 (x+side,y−far)；左：nearX≥1 时 far 1..≤nearX、side 0..<高+2 查 (x−far,y−1+side)；右：nearX≠0 时 far 0..<nearX 查 (x+宽+far,y−1+side)；左下：nearY>1 时 far 1..<nearY、side 1..≤nearX 查 (x−side,y+far)；下：nearY≠0 时 far 0..<nearY 查 (x−1+side,y+高+far)；右下：nearY>1 时 far 1..<nearY、side 1..<nearX 查 (x+side,y+far)；之后（含 (0,0)）查自身 宽×高 范围 (x+side,y+line)；都没有返回 −1。
对照：一致（循环边界 jb／jbe 与源码 < ／≤ 逐一对应）。

## 0x00404CF0 RoleControl::FindRoleAt — exact

汇编：角色数≤1（无符号）返回 −1；角色 1..数−1：事件字(+0x1E)≠0 且旗标&0x8000 时，左=x shr 4、上=y shr 4，左≤tx、左+宽>tx、上≤ty、上+高>ty（全部无符号）返回下标；否则 −1。
对照：一致。

## 0x00404DD0 RoleControl::FindTouchRoleAt — exact（更正注释地址）

汇编：0x00404CF0(x,y)；找到且旗标无 0x2000 时返回 −1。对照一致；注释中的 0x00404FD0 改为 0x00404CF0。
