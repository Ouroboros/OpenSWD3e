# 第一遍汇编验证：map/field_view（地图画面）

增强对照：本模块含用户要求的增强（deviations.md 第 85、86、87、88、91、92 行：两帧之间补画、主角与角色连续移动 smooth_motion、render_scale、地图指针、宽地图）。以下“一致”指关闭这些增强（smooth_motion 关、视野 640×480）时与原程序逐项相同；增强开启时的差别以这些条目为准。

## 0x004126A0 FieldView::Render — exact（修正）

- 12 个画面动作自后向前，act 非 0 时 Run（不报错）。主角 x/y 加 步速×(0x004D0E80+0x004C2B60)、(0x004D1288+0x004C2B64)，镜头四边加 步速×0x004D0E80／0x004D1288。
- 72 个移动记录：角色 0xFFFF 或步号 ≥0x7FFF 跳过；角色不可见跳过；是领队且记录种类为路径（mode&0xF==1）跳过；旗标 0x800 且 talkState==1 跳过。act.delay 为 0 时：步长取 +0x16/+0x18（有符号），**act.mask（角色 +0xE0）为 0 时加倍**，旗标 0x4000000 再加倍；方向字节（+0x1C+步号）不为 0xFF 时 act 方向取 0x004AA544[方向]；坐标加步长，落到格上时 RemoveRole(id, 旗标&3, (y>>4)−1, 0)，到终点（等于 +4/+6）时 act 速度 0、旗标清 0x44000000 置 0x2000000、act/动作/方向取 +0x10/+0x12/+0x14（非 −1）、mode 位 7 未置时 ResumeMove、清旗标 0x80000000、act.delay 0；角色 id 为 1 时按键表 0x004D1638 0x200 字节与四个步长清 0、镜头未固定时 CenterCamera；清旗标 0x40000；然后 ClearRoleCells、方向不为 0xFF 时格子指针移 (0x004AA524[方向]×宽+0x004AA504[方向])、MarkRoleCells、步号 (步号+1)|0x8000、旗标 0x100 时取格子的反射与色调。领队为 0xFFFF 且旗标 0x2000 时 ProbeCells(格子, 宽, 高, 0x100) 非 0 则把角色的 eventOffset、eventCursor、事件、id、旗标写给领队。之后 Run（失败报 "Act Err(picPaint:PathMove)"）；该角色为主角且镜头未固定时 CenterCamera。
- 随行 1..随行数−1（u16）：角色 0xFFFF 跳过；步号 ≥0x7FFF 只 Run；否则 delay 为 0 时方向取表、坐标加步长、落格时 RemoveRole(id, 旗标&3, 0, 0)、格子指针按方向移动（不查 0xFF）、步号置位；Run（失败报 "Act Err(picPaint:CompanyMove)"）。
- PanCamera、ShakeCamera、锁定离屏面、Paint、DrawTimer(0x190, 8, 0)；鬼魂 Follow((float)(u32 x)+16.0f, (float)(u32 y)−64.0f)（0x004AA5E4、0x004AA5E8）与 Draw；480 行各清前两个像素；调试模式 1 时 DrawDebug(镜头左, 镜头上, 2)；呈现。主角在格上且可见时 SettlePlayer（RemoveRole、ClearRoleCells、格子指针加 (行步长和)×宽+列步长和、MarkRoleCells、四个步长清 0、坐标与轨迹首项不同时三条轨迹后移 31 项并写入 x、y、方向、格子效果）。主角旗标无 0x40000000 且有 0x1000 时 Run（失败报 "Act Err(PicPaint:Render:MRl)"）。图层计时加 1，≥ 帧数且层数不为 1 时清 0、序号加方向，越界（无符号）时方向取反并再走两步，偏移 = 序号×每层格数。震动表首字不为 0xCFCF 时镜头取震动前位置。
- 差异与修正：步长加倍的条件原先读 Role::unkD4（沿用 swd32 的 +0xD4，从无写入，等于总是加倍），swd3e 读角色 +0xE0 即 act 的 mask（'MS' 指令置 1）。改为 role.act.mask，删去 Role::unkD4。
- 格子步长表 kCellX/kCellY 移为 FieldView::CellStepX/CellStepY，新增 tests 与 0x004AA504、0x004AA524 逐字节比对；deviations.md 第 244 行的三个表地址由 swd32 改为 swd3e，来源改为 asm/004126A0.asm。注释中的 0x004120B0、0x00412930 改为 0x004126A0、0x00412F60，类注释的地址范围改为 0x004126A0..0x00417290。

## 0x00412F60 FieldView::Paint — exact

- 裁剪 (0,0,0x280,0x1E0)；0x004E5820 位 2 时裁到聚光灯 ±0xC0。有影片时 UpdateMovie。否则：旗标 0xF 或 0x13 时清屏；0x004E5820 位 0 时清屏后只画 0x004D1E10 动画；不然 DrawOverlays、旗标 0x48 未置且 0x13 置时裁到聚光灯；16 位地图按镜头是否对齐格子 DrawTiles16/16Scrolled，8 位地图先设调色板 0x004D0D58 再 DrawTiles8/8Scrolled，之后调色板清空；读旗标 0xB（结果不用）；DrawRoles、0x004D1548 动画、DrawSprites；旗标 0x48 未置且 0x004E5820 位 0 未置时 DrawLeaves、DrawLightning、DrawFireflies、DrawWeather、DrawHeat、DrawRipples；旗标 0x48 未置时 DrawSpotlight 与 0x004D1E10 动画，否则只画 0x004D1E10 动画。
- 裁剪还原；FadeColors(1)、DrawLines、Talk::Draw、DrawRoleHeads；领队为 0xFFFF 或其 eventCursor <8 且旗标 0x51 未置时 DrawCounter(0x27C, 0x1CC, 0, 金钱)；NameBoxFrame；旗标 0xA、9、0x51 都未置时 RefreshCursor；Notices::Draw。

## 0x004131F0 DrawTiles16、0x00413340 DrawTiles16Scrolled、0x00413830 DrawTiles8、0x00413980 DrawTiles8Scrolled — exact

- 对齐版：范围 (0,0)-(0x280,0x1E0)；旗标 0x48 未置且 0x13 置时取聚光灯中心 ((x+0xF)&~0xF, (y+0xF)&~0xF) ±0xC0；行尾跳过 宽−0x28，左边不为 0 时再加 (左−右+0x280)>>4；首格 = ((镜头上+上)>>4)×宽 + ((镜头左+左)>>4)，图块再加图层偏移；格子无 0x8000000 时以 单元文件 + 图块×0x200（8 位：+0x200 调色板后 ×0x100）为来源，有 0x4000000 时用带透明色的贴法（16 位 0x00417EF0／0x00417E90，8 位 0x00418010／0x00417F70）。
- 滚动版：镜头对齐时调用 16 位对齐版（8 位滚动版也调 0x004131F0，原有 BUG 保留，实际只在不对齐时被调用）。行列 = 镜头>>4，起点 (−x 余数, −y 余数) 写 0x004E6D4C/0x004E6D50，行（列）为负时取 0、少画一行（列）、起点加 16；有余数时行（列）数加 1 并截到地图边；正常时先画上下两行与左右两列的边缘格（DrawPicture 16×16，模式 0/4，8 位也按 16 位读 0x200 字节，已登记），再画内部格；聚光灯时只画聚光灯方框内的内部格。内部格从 ((上>>4)+行+1)×宽+(左>>4)+列+1 起，起点各加 16，范围到 右−16、下−16。
- 实现 DrawTileGrid/DrawTileGridScrolled 与之逐项相同（FillsWindow 为宽地图增强）；deviations.md 第 111、245 行来源改为对应 asm。

## 0x00413E80 FieldView::DrawRoles — exact（修正，越界补登）

- 三层依次 2、0、1；起始行 = 镜头上/16（有符号向零）−20，共 70 行；行号 ≥ 高+20（有符号 jge）时跳过该行；行内链表逐个 DrawRole，声音号非 0 时 UpdateRoleSound。
- 差异与修正：实现原先把行号转成无符号比较，镜头上缘不到 320 像素时把地图上方（y<0 的角色所在）的那些行跳过；原程序有符号比较会画这些行。改为有符号；下标为负（镜头在地图上方 320 像素以外）时原程序读表外，补登 deviations.md 新行，实现不画。

## 0x00413F20 FieldView::DrawRole — exact（注释更正）

- (旗标 & 0x8400) != 0x8000 返回 0。x−镜头左 不在 −0x190..0x410 返回 1。act 非 0 时帧取 +0x4A 且旗标 0xB 置则返回 1（否则帧 0xFFFF）。act +0x58 声音非 0 时按 (x, y) 播放并清 0。GetPicture(帧, +0x4C)（空指针已登记于第 280 行）。缩放 0x400；id 非 0xFFFF 且旗标无 8 时取深度表 (x,y) 的缩放，按行种类时与上一行 (y−16) 插值 above+((s−above)×(y&0xF))/16，模式 |0x80。阴影种类非 0 且旗标 0x100 时 DrawShadow(…, 0x004AA5A4[(旗标>>20)&0xF], 缩放)。波形 +0xD0、相位 +0xD1、透明度 +0xD2，缩放非 0 时 SetZoomSheared(act.x+0x10, act.y+8, s, s, 0)，色调清 0，画于 (x+偏移x−act.x−镜头左, y+偏移y−act.y−镜头上)。旗标 0x100 时色调取地图三色（0x004C5114、0x004C512C、0x004C633C），旗标 0xF00000 时三色各加 0x004AA564[…]；任一非 0 时以 (模式&0x80000013)|0x10（缩放时 +0x80）再画一次。附带动作（+0x3C）画于 (偏移x−附带x−镜头左+x+0x14, 偏移y−act.y×缩放/1024−镜头上+y)。SetZoom(0,0,0x400,0x400)；相位写回 +0xD1。旗标 0x200 且旗标 0x48 未置时 DrawBubbles。领队为 0xFFFF 且有标签时以 0x004AF720[标签色] 的低字、样式 4 在 (x−11×长度/2−镜头左+0x10, y−act.y−镜头上) 写标签。返回 1。
- 两张色调表与 exe 逐字节相同（0x004AA564 = kRoleTint，0x004AA5A4 = kShadowTint）；cellTint 成员注释改为 swd3e 地址。

## 0x004143B0 FieldView::UpdateRoleSound — exact（注释更正）

- 距离 = _ftol(sqrt(dx²+dy²))（32 位回绕）。计时高半字不为 0xFFFF 时低半字减 1，结果非 0 则写回并本次不到期。距离 >0x200 或不可见：旗标 0x1000000 时停止该声音并清旗标，返回。FindRole(id)（找不到写 −1，以 −1 写表已登记第 110 行，来源改为 asm）。未在播放一次性声音且到期：计时 = 高半字 + (计时>>16)；高半字为 0xFFFF 时置 0x1000000 并 Play(0, 声音, 0, 0, 0, 0)，否则 Play(0, 声音, 0, 0, 1, 0)；距离表 = 距离×128/512，高差表 = (i16)((u16)(dy)<<6)/512。每次都 SetVolume(声音, 128−距离×128/512)、SetPan(声音, (dx<<6)/512)。注释中的地址改为 0x004143D7..0x004143E8。

## 0x004145A0 FieldView::DrawDebug — exact（修正）

- 小字体背景 0xFFFE、阴影 0。0x004E6CE4 为 1 时 MarkCells 五次：0x40000000 用 红+绿+蓝、0x10000000 用 绿+蓝（内缩 2）、0xFF 用 红+绿（4）、0x20000000 用 红（6）、0x800000 用 红+蓝（8）（颜色掩码 0x004AF70C 红、0x004AF70E 绿、0x004AF710 蓝）。
- 0x004E6CE0 为 1 时，在 (8, 16n) 写各行（白色 0x004AF720）：主角 x、y ×0.0625（0x004AA5EC）与镜头 >>4；指针格（无符号 >>4）与指针坐标；FPS = 1000 / 0x004AF7A4（无符号 div）、主帧耗时、左键两个计数、0x004E6CDC；地图号、层帧数、步速；playerHold、0x004C30D0、0x004C655C、遇敌步数 &0x7FFFFFFF、遇敌组、0x004E6CC0；领队 id、事件、0x004E5810、0x004C6614；音乐行（两首场景曲、两首剧情曲、musicCue&0xFF、位 23 选 "劇情"/"場景"、场景 0x80000/0x40000 与剧情 0x20000/0x10000 各取 "Nil"/"Cyc2"/"Rep1"）。指针格区域号非 0 时：FindRegion 找不到在 0x1B0 行写 "錯誤的地圖劇情號碼 (%d)"，找到在 0x1C0 行写名字、0x1D0 行写事件&0x7FFF、点选旗标与是否可点选、阻挡旗标与是否可通过。
- 角色 1..roleCount−1：act 非 0 且指针地图坐标（无符号）在 (x−16, x+0x30)、(y−0x40, y+0x10) 内时计数加 1，第二个起在当前行上方 0x20 写 "角色重疊"；角色变量 1 读两次分别写进比较用的 x、y（之后的角色以此比较，原有 BUG 保留）；在 0x1A0 行以颜色 0x004AF72C 写角色行，在当前行写故事行，当前行上移 0x20。
- 差异与修正：(1) 当前行每次上移 0x20（0x00414B45），实现原为 0x10，改为两行（kRoleLineRows）；(2) FPS 除数为 0 时原程序整数除零崩溃，实现原先显示 0，改为 Fault；(3) frameTime（0x004AF7A4）初值为 0x400，实现原为 0，改为 Game::FirstFrameTime；debugText、debugCells、mainFrameTime、frameTime 的注释改为 swd3e 地址。

## 0x00414B70 FieldView::PanCamera — exact（注释更正）

- 剩余平移（0x004C30CC 横、0x004C2F98 纵）都为 0 时返回；镜头四边加步长（0x004C2B54、0x004C2AA0）；剩余减步长，减到 0 的方向步长清 0。注释中的 swd32 地址改为 swd3e。smooth_motion 时由 StartPan 在帧内移动（增强，第 87 行）。

## 0x00414BF0 FieldView::DrawShadow — exact

- 来源为图片；模式 (act 旗标 & 0x80000093)|0x90；SetZoomSheared(act.x+0x10, act.y+8, 缩放, 缩放/(种类<5 ? 2 : 4), 种类为奇数 0x400 否则 −0x400)（种类取 0x004D183C，忽略参数）；三色 = tint；(种类−1)%4（有符号）≥2 时加 Flip；DrawPicture(x, y, 宽, 高, 模式, 0)。

## 0x00414CC0 FieldView::FadeColors — exact

- 三个步长都等于 0.0（0x004AA5F0）返回。参数非 0 时：帧数 <0 则恢复，否则减 1；帧数 ≥0 时三个色阶各加步长（float），否则步长取初值（0x004C2AA4、0x004D046C、0x004E580C）；以 _ftol(三色阶) 对 0x4B000 个像素着色（0x00420E50）。float 加法经双精度再舍入与直接 float 加法结果相同。

## 0x00414DB0 FieldView::DrawAnimations — exact

- 链表（+0xAC 下一项）逐个 Run（失败报 "Act Err(PicPaint:PlayAct)"）；图片（帧 +0x52、图 +0x54；空指针已登记第 280 行），透明度 +0x9A，画于 ((u16)x−act.x, (u16)y−act.y)，模式 act 旗标；声音非 0 时于 (x+镜头左, y+镜头上) 播放并清 0；act 的 waiting 为 1 时摘下释放。

## 0x00414EC0 FieldView::ShakeCamera — exact（注释更正）

- 震动表首字为 0xCFCF 返回；当前项为 0xCFCF 时回到 0；取一对偏移；帧数减 1，≤0 时重置为保持帧数（0x004C6880）并前进一对；记下镜头左上（0x004C2AAC、0x004C3004）后四边加偏移。成员注释改为 swd3e 地址（0x004D1838、0x004C2B7C、0x004C6880、0x004C2AAC、0x004C3004、0x004C6624）。

## 0x00414F70 Game::RefreshCursor（StepPointer、DrawPointer）— exact

- 键 0x2E 按下时指针动作 0xF。无菜单时：有滚动步长则滑入量加 1（>0 取 0）；否则等待加 1，>0x10 时滑入量减 1（< −0x20 取 −0x20），且左键按下、指针 x >0x262、y <0x18 时，领队为 0xFFFF 则菜单状态 0x80000001 并直接画按钮；领队存在且其 eventCursor ≥8 时不画，否则画系统按钮于 (滑入量+0x282, 0)。动作与上次不同时 act.delay 清 0；记录动作；Run（失败报 "Act Err(RefrashCursor)"）；画指针（帧 +0x8A、图 +0x8C、透明度 +0xD2，于 指针−act 锚点，模式 act 旗标，参数图片 +4）。宽地图下按钮位置与指针比较为增强（第 92 行）。

## 0x00415120 FieldView::DrawSprites — exact

- 只处理链表 0x004C6BD4（+0xBC 下一项）。屏幕位置 = _ftol(x/y) − 镜头，先算后 Run（失败报 "Act Err(PicPaint:DrawTObj)"）；屏幕位置在 (−0x48, 0x2C8)×(−0x48, 0x228) 内时画图（透明度 +0x92、参数图片 +4）。act.delay 为 0 时 x += 步长x（x87 寄存器中的未舍入和用于 _ftol）、y += 步长y（先存 float 再 _ftol），两者都在目标 ±0x20 以内（开区间）时摘下释放。

## 0x004152A0 FieldView::DrawRoleHeads — exact

- 链表 0x004D3F94（+0xBC 下一项）：Run（失败报 "Act Err(PicPaint:drawRoleHead)"）；画于 ((i16)headX−act.x, (i16)targetY−act.y)（空指针已登记第 280 行）。速度 &0x7FFF 非 0：headX += 速度、速度 ×3（16 位），headX ≤ −0x78 或 ≥ 0x2F8 时摘下释放；否则 slide = 2×(targetX−headX)/3（有符号），headX += slide，slide 在 −1..1 时 headX = targetX。

## 0x00415410 FieldView::DrawLines — exact（注释更正，偏差补登）

- 行宽 0x004B1E8C>>1；链表 0x004D3FBC（+0x14）。颜色 0x004AF720[(i16)+0xA]（越界补登新行，实现取低 4 位）。种类 +8&0xFF00：0x800 整行画 count 行 limit 像素；0x8000 自下而上长度 += 2×Below(6)+0x30，≥limit 计完成；0x4000 偏移 −= 0x30+2×Below(0x18)，≤0 计完成，长度 = limit−偏移；完成数等于行数时变为 0x800；0x2000 偏移 += 2×Below(0x30)+0x30，≥limit 计完成，长度 = limit−偏移（≤1 取 2），全部完成时释放；0x1000 长度 −= 0x30+2×Below(0x30)，≤2 计完成并取 2，全部完成时释放。混合 0x004187A0（像素数>>1 对，0 对时按 2^32 处理已登记）。注释中的 0x00417DE0 改为 0x004187A0。

## 0x004157B0 FieldView::DrawOverlays — exact

- 叠加层链表 0x004C4A7C（+0x1C）：区域 (+0xA,+0xC)-(+0xE+16,+0x10+16) 与镜头 IntersectRect，空则跳过；裁剪为交集减镜头；视差 +0x16/+0x18 为 8 时偏移 = 镜头≥区域 ? 镜头−区域 : 0，否则 视差×(镜头右/下−区域左/上)/16（有符号），镜头在区域前时再加裁剪左/上；DrawPicture(裁剪左−偏移x, 裁剪上−偏移y, +6, +8, 0, 0)，裁剪还原 (0,0,0x280,0x1E0)。

## 0x00415960 Notices::Draw — exact

- 小字体两次设背景 0xFFFE、阴影 0；链表 0x004D1F4C（+0x84）从 y=8 起每条 0x18：旗标 0xE 未置时以颜色 0x004AF72C 的低字、样式 0x10 在 (0x280−11×长度, y) 写文字；帧数减 1，到 0 摘下释放。

## 0x00415A30 FieldView::UpdateMovie — exact

- 未播放返回 0。有对话窗、画线或角色头像，或选项位 4 时：首次存画面到 0x004D128C 并置 0x004D1E00，之后每帧恢复，返回 0；不再暂停后先恢复一次并清标记。帧号 ≤0：画帧，上下各 (4×帧号+0x34) 行清黑，帧号加 1。帧号 ≥10000：选项位 0 时以 (10000−帧号)×2 着色 0x4B000 像素，否则暂停播放后清屏、Paint、置 0x004E5820 位 0、恢复播放，上下各清 (10030−帧号)×2 行；帧号加 1，超过 10015（变暗）或 10030（关幕）时释放块链与缓冲、清块链字段、current 指回头、高度 0、停止、帧号 0、关闭文件、清 0x004D1404 位 1、SetFrameInterval(0x23)；返回 1。其余：ReadMovieFrame 非 0 时帧号置 10000，否则画帧、帧号加 1；返回 0。

## 0x00415D50 FieldView::DrawMovieFrame — exact（修正，偏差补登）

- 起点 = 画面 + ((480−高)>>1)×行宽；块的行数有符号比较；每行 长度、目标偏移、像素：8 位帧按调色板逐字节写（长度−8 ≤0 不画），16 位帧以 (长度−8)>>1 个字原地转换（转换器 do-while，0 对时也转换 1 个字）再复制 长度−8 字节；下一行 += 长度。
- 差异与修正：(1) 行数原先按无符号循环，改为有符号（负数时不画）；(2) 16 位行无像素对时原程序仍转换紧随其后的一个字，实现原先不转换，已改；(3) 8 位行短于 8 字节时原程序不画并按长度前进，实现原先停止整帧，已改；(4) 目标偏移按有符号加。16 位行短于 8 字节与帧外读写补登 deviations.md。

## 0x00415E40 FieldView::ReadMovieFrame — exact（修正）

- 块链中有当前帧：移到 偏移+0xC，大小非 0 时读入并以**实际读到的字节数**解压，current 指向该块，返回 0。否则从末块之后（偏移+大小、帧号+1）继续：帧号 > 末帧（有符号）时把画面复制进帧缓冲返回 1；新块取上一块尾部记录的大小、行数、字，读入；有调色板且当前目标帧号等于下一个调色板帧时从文件尾 (个数−序号)×0x300+0x400 处读 0x300 字节转为 15 位色再 ConvertPixels（之后的块从文件当前位置接着读，原有 BUG 保留）；尾部 12 字节写入新块的下一块信息；挂到链尾；到达目标帧时以 大小−0xC 解压，current 指向它，返回 0。
- 修正：已有块的解压字节数改为实际读到的字节数（0x004160A1）；删去三处 swd32 遗留的 audioDevice.Serve()（swd3e 此函数不调用 AIL_serve，且实现中为空操作）。

## 0x004160E0 FieldView::DrawWeather — exact

- 旗标 5 未置返回。两颗粒子各先 Below(1000)：坐标在 (−0x280, 16×宽+0x280)×(−0x140, 16×高+0x140) 外时，掷值 ≥990 才重置：色调目标 0、速度目标 掷值/200+1、动作 = 天气起始动作 + 动作数×掷值/1000，按阴影种类 0..3 放到下方 16×高+0x76 或上方 −0x12C，x = Below(16×宽)，步长 Below(1)−2 / Below(2) / Below(2)−2 / Below(2)，不画。在图内：计数加 1，超过速度时清 0 并移动（步长 + (步速×滚动步长)>>2），掷值 <25 时速度目标 Below(5)+1，速度向目标走 1；掷值 <5 时色调目标 Below(4)−5，色调向目标走 1；效果动作 0x232A 取粒子动作 Run，三色 = 色调，透明度 +0x92，模式 动作数为 4 时 Additive(4) 否则 Subtractive(0x2C)，画于 坐标−锚点−镜头。

## 0x00416450 FieldView::DrawBubbles — exact（注释更正，偏差补登）

- 不在镜头 ±0x40 内返回。sway = Below(5)−3；效果动作 0x232B/0x3B Run 取图。四个标记中角色（movsx）等于参数者：FindRoleOrPlayer（失败时下标 −1 越界读，补登新行），清发射位，标记坐标取角色 x/y 低字，Below(1000)>250 置发射位；+8 计数 <0x18 且发射时新建气泡（x = 16×(标记x−8+Below(16))，y = 标记y，寿命 地图 0x32 时 Below(16)+16 否则 Below(30)+30，步长x Below(16)−8，步长y −1−Below(1)）插在链首。逐个气泡：Below(1000)；y += 步长y，x += 步长x+sway；掷值 >50 寿命减 1；寿命 ≤0 时有下一个则把它整个复制过来并释放它、再处理同一位置，否则释放并结束；否则着色（地图 0x32 寿命−0x1E/−0x26/−0x32，否则 (寿命>>1)−0x1E）画于 (x/16−锚点x−镜头左, y−锚点y−镜头上)，模式效果动作旗标。注释中的 0x00415F7D 改为 0x004164ED。

## 0x00416730 FieldView::DrawLeaves — exact

- 旗标 6 未置返回。栈上动作表 {0x34,0x34,0x34,0x36,0x38,0x38,0x38,0x36} 以纵向步长+3 取。四片落叶：坐标在 (−0x78, 16×宽+0x78)×(−0x78, 16×高+0x78) 外时置 x=0x7FFFFFFF；未放置时 x=−0x40、y=Below(16×高)、纵步 Below(5)−2、横步 Below(7)−3（0 取 2，负时 x=16×宽+0x40）；否则 Below(1000)>250 时 转向 = Below(3)−1、纵步 += Below(3)−1（夹在 ±3）、横步 += 转向（夹在 ±5，为 0 时取 −转向）。动作取表，横步 >0 时加 1；Run；画于 坐标−锚点−镜头（模式 act 旗标）；坐标加步长。

## 0x00416930 FieldView::DrawHeat — exact

- 旗标 7 未置返回。帧计数为 0 时 8 列（每 0x50）×6 行（每 0xC800）共 48 项依次取 长度 2×Below(0x26)+0x4C、偏移 Below(3)+行+列+0x27F、行数 Below(0x4E)+1；否则计数为 4 的倍数时按 (计数>>2)&3：0 重取 64 个长度，1 重取 48 个偏移，2 重取 64 个行数。计数加 1。48 条各把 (偏移×2) 起 行数 次、每次 长度 字节从下一行（+0x500）复制上来（越界跳过已登记）。热浪偏移只有 48 项，与 ResetEffects 的 0x100 字节清零之后的无引用字节一致。
