# 第二遍：FieldView（地图画面）

平滑层（smooth_motion、render_scale、宽画面）为已登记的增强（deviations 85、86、87、89、92、93）；以下按关闭平滑时的逻辑与汇编对照。

## 0x004126A0 FieldView::Render — diff → exact

汇编（独立推导）：
- 屏幕 act 0x004D3BCC 起向前 12 个（步长 0xA4），id≠0 时 0x00436C80（不报错）。
- 主角（0x004C4B00）x += (0x004C2B60+0x004D0E80)×0x004C4A58，y += (0x004C2B64+0x004D1288)×步速；镜头四边 += 0x004D0E80／0x004D1288 × 步速。
- 72 个移动记录：role≠0xFFFF、step <u 0x7FFF、角色 flags&0x8000；跳过：id==leader.id 且类型 1，或 flags&0x800 且 talkState==1。act.delay≠0 → 只推进 act。否则 步长 = (i16)记录 +0x16／+0x18，act +0xA0（mask）为 0 时加倍，flags&0x04000000 再加倍；方向 = directions[step]，≠0xFF 时 act.direction = 0x004AA544[方向]（{5,1,6,2,4,0,7,3}）；x、y 加步长；(x|y)&0xF==0 时：0x00411890(id, flags&3, (y shr 4)−1, 0)；x、y 等于记录终点 → speed=0、flags = flags&0xBBFFFFFF|0x02000000、按记录（≠0xFFFF，符号扩展）换 act id/action/direction、mode 无 0x80 → 0x00432630、flags 清 0x80000000、delay=0；id==1 → 清 0x004D1288、按键表 0x200 字节、0x004D0E80、0x004C2B64、0x004C2B60，0x004E5820 无位 2 时 0x0040D410；flags 清 0x40000。随后 0x0040B180；方向≠0xFF 时格指针 += (0x004AA524[方向]×宽+0x004AA504[方向])×4；0x0040B270；step = (step+1)|0x8000；flags&0x100 时按格子 0x800／0xF000 设反光与阴影。leader.id==0xFFFF 且 flags&0x2000 → 0x0040BF50(格, 宽, 高, 0x100) 非 0 → leader 取 eventOffset／eventCursor／event／id／flags。0x00436C80 失败 → 「Act Err(picPaint:PathMove)」。index==主角且 0x004E5820 无位 2 → 0x0040D410。
- 随行者 1..计数−1（u16）：role 为 0xFFFF 跳过；step ≥0x7FFF → 只推进 act；delay≠0 → 推进 act；否则 act.direction = Facing[方向]（不判 0xFF），x、y 加步长（不加倍），落格时 0x00411890(id, flags&3, 0, 0)、0x0040B180、按方向移格、0x0040B270、step+1|0x8000、按格子设旗标；推进 act，失败 → 「Act Err(picPaint:CompanyMove)」。
- 0x00414B70、0x00414EC0；锁离屏面；0x00412F60；0x004357C0(0x190, 8, 0)；0x0049E6A0((float)(u32)x+16.0, (float)(u32)y−64.0)；0x0049E740；每行前两个像素清 0；0x004AF7A0==1 → 0x004145A0(镜头左, 镜头上, 2)；Blt。
- 主角 x、y 低 4 位为 0 且 flags&0x8000：0x00411890(id, flags&3, (y shr 4)−1, 0)、0x0040B180、格 += ((0x004C2B64+0x004D1288)×宽+0x004C2B60+0x004D0E80)×4、0x0040B270、四个步长清 0；trailX[0]≠x 或 trailY[0]≠y → 0x004D1380／0x004D1300／0x004E4DE0 各后移 31 项并写入 x、y、direction；flags&0x100 时设格子旗标。
- 主角 flags 无 0x40000000 且有 0x1000 → 推进 act，失败 → 「Act Err(PicPaint:Render:MRl)」。
- ++0x004E6D44 ≥ 0x004AF714 且层数 0x004D12A8≠1：计时清 0，索引 += 方向，索引 ≥u 层数 → 方向 = (方向≠1 ? 1 : −1)、索引 += 2×方向；0x004D1C84 = 0x004D1400×索引。
- shake[0]≠0xCFCF → 镜头 = (0x004C2AAC, 0x004C3004) 起 640×480。
对照：源码 Render、ArriveMoving、ArriveFollower、SettlePlayer 逐段一致（三张方向表与 exe 一致，鬼魂偏移 16／64 与 0x004AA5E4／0x004AA5E8 一致，kTimerX/Y 0x190／8、kFastFactor 2、kLayerSwing 2）；方向与步数越界、角色号越界为已登记偏差（deviations 262）。
差异：格指针为空与移出格子表的保护未登记。修正：补登 deviations「0x004126A0 空指针、越界」。

## 0x00412F60 FieldView::Paint（含 PaintWorld、ClipToSpotlight）— exact

汇编：剪裁 (0,0,640,480)；0x004E5820&4 → 剪裁到聚光灯 ±0xC0。影片中（0x004E6CD4≠0）→ 0x00415A30。否则：旗标 0xF 或 0x13 → 清屏；0x004E5820&1 → 清屏并只画 animationsB（0x004D1E10 链）；否则 0x004157B0；旗标 0x48 未置且 0x13 置 → 聚光灯剪裁；16 位地图按镜头左或上低 4 位是否非 0 选 0x00413340／0x004131F0(0)，8 位地图以 0x004D0D58 为调色板选 0x00413980／0x00413830(0)，之后调色板清 0；读旗标 0xB（结果未用）；0x00413E80；0x00414DB0(0x004D1548)；0x00415120；旗标 0x48 未置且无屏幕位 1 → 0x00416730、0x00416B00、0x00416D20、0x004160E0、0x00416930、0x00417230（0x00416730 不读参数）；旗标 0x48 未置 → 0x004170A0；两路都再 0x00414DB0(0x004D1E10)。然后剪裁 (0,0,640,480)；0x00414CC0(1)；0x00415410；0x00433C10；0x004152A0；(leader.id==0xFFFF 或 eventCursor <u 8) 且旗标 0x51 未置 → 0x00411B50(0x27C, 0x1CC, 0, 金钱)；0x004172A0；旗标 0xA、9、0x51 都未置 → 0x00414F70；最后 0x00415960。
对照：一致（6 个特效与 DrawSpotlight 的身份、SpotlightRadius 0xC0、GoldX/Y、kLeaderCursorLimit 8、HideHud 0x51、NoPointer 0xA、NoChoicePointer 9）；平滑层的分层截取与宽画面剪裁为已登记增强。

## 0x004131F0 DrawTiles16、0x00413830 DrawTiles8 — diff → exact

汇编：范围 (0,0)–(640,480)，旗标 0x48 未置且 0x13 置时改为聚光灯 ((x+0xF)&~0xF)±0xC0；行尾跳过 = 地图宽−40，左边不为 0 时再加 (左−右+0x280)>>4；首格 = 宽×((镜头上+上)>>4)+((镜头左+左)>>4)；逐格：格子无 0x08000000 时 源 = 像素 + 图号×0x200（8 位：(图号+2)<<8），有 0x04000000 用透明键绘制（16 位 0x00417EF0／0x00417E90，8 位 0x00418010／0x00417F70）；行末格与图号各加跳过量。两段汇编除跳转地址、源偏移与绘制函数外逐条相同。
对照：DrawTileGrid（TileSet indexed 区分）一致；宽画面画布为已登记增强。差异：图块像素越出映射视图时改读视图开头的保护未写进偏差。修正：扩充 deviations 111。

## 0x00413340 DrawTiles16Scrolled、0x00413980 DrawTiles8Scrolled — exact

汇编：镜头左与上低 4 位都为 0 → 转 DrawTiles16(0)。否则 行 = 镜头上 sar 4、列 = 镜头左 sar 4、0x004E6D4C = −左余、0x004E6D50 = −上余、列数 40、行数 30；行 <0 → 行 0、行数 29、tileY += 16；列 <0 → 列 0、列数 39、tileX += 16；左余非 0 → 列数+1，超出地图宽则 = 宽−列；上余非 0 → 行数+1，超出地图高则 = 高−行。旗标 0x48 置或 0x13 未置：首行与末行（y = tileY 与 tileY+16×行数−16）、首列与末列（x = tileX 与 tileX+16×列数−16）各格无隐藏位时以 0x00417A60(x, y, 16, 16, 透明键 ? 4 : 0, 0) 剪裁绘制；范围取 (0,0)–(640,480)。否则范围取聚光灯。内层：tileX、tileY 各 +16 并写回，首格 = 列+1+(左>>4)+宽×((上>>4)+行+1)，y 从 上+tileY、x 从 左+tileX 起，在 x < 右−16、y < 下−16 内整格绘制。8 位版除寄存器分配与 (图号+2)<<8 外相同。
对照：DrawTileGridScrolled 一致（RawCopy 0、RawKeyed 4）。

## 0x00413E80 FieldView::DrawRoles — exact

汇编：层序 2（0x004C318C）、0（0x004C3184）、1（0x004C3188）；每层行号从 (镜头上/16，有符号向零取整)−20 起 70 行，行号 < 地图高+20（有符号）时逐个 0x00413F20，sound(+0x2C)≠0 时 0x004143B0。
对照：一致（ViewRows 30 + 40）；回放标志属平滑层。负行号读到行表之前的保护已登记（deviations 300）。

## 0x00413F20 FieldView::DrawRole — exact

汇编：(flags&0x8400)≠0x8000 → 0。x−镜头左 <−0x190 或 >0x410 → 1。act.id≠0 时帧 = +0x8A，旗标 0xB 置 → 1；否则帧 0xFFFF。'VW'(+0x98)≠0 → 0x004270B0(VW, x, y) 并清 0。图 = 0x00436720(帧, +0x8C)。模式 = act.flags，缩放 0x400；id≠0xFFFF 且 flags 无 8：缩放 = Scale(LevelAt(x, y))，类型为 2（按行）时 上 = Scale(LevelAt(x, y−16))、缩放 = 上+((缩放−上)×(y&0xF))/16，已缩放=1、模式 |= 0x80。阴影种类 0x004D183C≠0 且 flags&0x100 → 0x00414BF0(角色, 图, x−act.x−镜头左, y−act.y−镜头上, 种类, 0x004AA5A4[(flags>>20)&0xF], 缩放)。0x004ED9AC = act.wait，0x004EDA04 = +0xD1，0x004E6D58 = +0xD2，0x004EF2F0 = 0；缩放≠0 → 0x00423670(act.x+0x10, act.y+8, 缩放, 缩放, 0)；三种着色清 0；以 (x+drawOffsetX−act.x−镜头左, y+drawOffsetY−act.y−镜头上) 画。flags&0x100 → 着色取 0x004C5114／0x004C512C／0x004C633C；flags&0xF00000 → 各加 0x004AA564[(flags>>20)&0xF]；任一非 0 → 以 (模式&0x80000013)|0x10（已缩放再 +0x80）重画。附加 act(+0x3C)：图 = (帧, 图号)，以 (drawOffsetX−附加.x−镜头左+x+0x14, drawOffsetY−(act.y×缩放)/1024−镜头上+y) 按附加.flags 画。0x00423630(0,0,0x400,0x400)；+0xD1 = 0x004EDA04；flags&0x200 且旗标 0x48 未置 → 0x00416450(x, y, id)；leader 空且标签非空 → 大字体在 (x−(长×11)/2−镜头左+0x10, y−act.y−镜头上)，色 0x004AF720[labelColor]、样式 4。返回 1。
对照：一致（kDrawnRoleMargin 0x190、kZoomAnchor 0x10/8、kAttachedShift 0x14、kZoomUnit 1024、kLabel* 11/0x10/4，两张着色表与 exe 一致）；空图片为已登记偏差（deviations 299）；声音与相位的回放跳过属平滑层。
修正（同组）：field_view_internal.h 中 kShadowTint、kRoleTint 的注释由 swd32 地址（0x004995D4、0x00499594）改为 0x004AA5A4、0x004AA564。

## 0x004143B0 FieldView::UpdateRoleSound — exact

汇编：距离 = _ftol(fsqrt(dy²+dx²))（32 位回绕）；到期 = 1；计时 = +0x30：高字 ≠0xFFFF 时 计数 = 低字−1，非 0 → 计时 = 高字+计数、到期 = 0。距离 >0x200 或 flags 无 0x8000：flags&0x01000000 → 0x0049D960(sound) 并清该位，返回。0x0040C500(id, &下标)。flags 无 0x01000000 且到期：计时 = 高字 + (计时>>16)；高字 ==0xFFFF → 置 0x01000000、0x0049D7B0(0, sound, 0, 0, 0, 0)，否则 (0, sound, 0, 0, 1, 0)；0x004E55A8[下标] = (距离<<7)/512，0x004D3C7C[下标] = (i16)(((u16)y−(u16)主角.y)<<6)/512。最后 0x0049DAB0(sound, 0x80−(距离<<7)/512)、0x0049DB00(sound, ((x−主角.x)<<6)/512)。
对照：一致（kSoundRange 0x200、kSoundScale 0x80、kPanScale 0x40、kPlayOnce 0／kPlayLooped 1，Play 的图像与大小参数恒为 0）；下标 −1 为已登记偏差（deviations 110）。

## 0x004145A0 FieldView::DrawDebug — exact

汇编：小字体背景 0xFFFE、阴影 0。0x004E6CE4==1：0x00435130 以 (0x40000000, 蓝+绿+红, 左, 上, 0)、(0x10000000, 蓝+绿, …, 2)、(0xFF, 绿+红, …, 4)、(0x20000000, 红, …, 6)、(0x800000, 蓝+红, …, 8) 标记（掩码 0x004AF70C 红、70E 绿、710 蓝，初值 7C00/03E0/001F）。0x004E6CE0==1：区域 = 格子[((指针y+镜头上) shr 4)×宽 + ((指针x+镜头左) shr 4)]&0xFF；第 0–6 行（y = 0、0x10…0x60，x 8，色 primary[0]）："MAct[%4.1f/%4.1f] LTCor[%4d/%4d]"（(u32)x×0.0625、(u32)y×0.0625、镜头左 sar 4、镜头上 sar 4）；"Mouse :OnMap[%4d/%4d] OnScr[%4d/%4d]"；"FPS[%2d] FUsed(%d),kr.d[%1d] ,kr.p[%4d] _IPA[%4d]"（1000÷0x004AF7A4 无符号、0x004E4C28、左键 0x004D1728、0x004D1734、0x004E6CDC）；"MapID[%d] MapCyc[%d] %d"；"ULck[%x] UIpt[%x] gTm[%d] Fsc[%d] ,BtlM[%d] ,BtnIp[%d]"（0x004C2FF8、0x004C30D0、0x004C655C、0x004E5828&0x7FFFFFFF、0x004D1EC0、0x004E6CC0）；"T.Gd[%d] T.Tid[%d] dbg1[%d] dbg2[%d]"（leader.id、leader.event、0x004E5810、0x004C6614——后两者全程序无写入者）；音乐行 0x004B1510（四个曲号、cue&0xFF、cue&0x800000 ? "劇情" : "場景"、场景 (0x80000 ? (0x40000 ? "Rep1" : "Cyc2") : "Nil")、剧情同理用 0x20000／0x10000）。区域≠0：找不到 → y 0x1B0 "錯誤的地圖劇情號碼 (%d)"；找到 → y 0x1C0 名字、y 0x1D0 0x004B14AC（event&0x7FFF、flag&0xFFFF、可点选与否、blockFlag、可通过与否）。指针下的角色（1..角色数−1，act.id≠0，无符号比较 x−16 < 地图x < x+0x30、y−0x40 < 地图y < y+0x10）：计数 ≥2 → 行 −0x20 处 "角色重疊"；0x00433590(id, 1, &地图x, 1)、(id, 1, &地图y, 1)（读出的变量覆盖指针坐标，原 BUG）；y 0x1A0 色 primary[3] 0x004B1468；当前行 "[GUID %d] [Talk %d] [Path %d] [Argu %4x] [ArrIdx %d]"；行 −0x20。
对照：一致（各格式串、颜色、常量、MouseLeft 15）；除零与格子越界为已登记偏差（deviations 265）。修正：scene.h 中 dbg1／dbg2 成员补注 swd3e 地址 0x004E5810／0x004C6614。

## 0x00414B70 FieldView::PanCamera — exact

汇编：0x004C30CC 与 0x004C2F98 都为 0 → 返回；镜头左右 += 0x004C2B54、上下 += 0x004C2AA0；两个剩余量各减步长，为 0 时对应步长清 0。
对照：一致（源码以 “减前差值” 判断纵向，与减后为 0 等价）；平滑层的 StartPan 为已登记增强（deviations 87）。

## 0x00414BF0 FieldView::DrawShadow — exact

汇编：源 = 图像素；模式 = (act.flags & 0x80000093)|0x90；剪切 = 阴影种类(0x004D183C)&1 ? 0x400 : −0x400；纵向缩放 = 缩放 / (种类 <5 ? 2 : 4)（有符号）；0x00423670(act.x+0x10, act.y+8, 缩放, 纵向缩放, 剪切)；三种着色 = 参数 tint；翻转 = (种类−1)%4（有符号）≥2 ? 2 : 0；0x00417A60(x, y, 宽, 高, 模式|翻转, 0)。种类参数未用（读全局）。
对照：一致（ZoomTint 0x90、kShadow* 常量）。

## 0x00414DB0 FieldView::DrawAnimations — exact

汇编：从头节点 +0xAC 起：推进 act（节点 +8），失败 → 「Act Err(PicPaint:PlayAct)」「設定錯誤 \nact[%d] qq[%d] dir[%d]」；图 = (act 帧, 图号)；0x004E6D58 = act.signal；以 ((u16)x−act.x, (u16)y−act.y) 按 act.flags 画；'VW'≠0 → 0x004270B0(VW, x+镜头左, y+镜头上) 并清 0；act.waiting==1 → 摘下并释放该节点，否则前进。
对照：一致；空图片为已登记偏差（deviations 299），回放分支属平滑层。

## 0x00414EC0 FieldView::ShakeCamera — diff → exact

汇编：shake[0]==0xCFCF → 返回；索引 0x004D1838 处为 0xCFCF → 索引 0；dx、dy = shake[索引]、shake[索引+1]（符号扩展）；--0x004C2B7C ≤0 → 重置为 0x004C6880、索引 += 2；0x004C2AAC = 镜头左、镜头左右 += dx；0x004C3004 = 镜头上、上下 += dy。
对照：一致（kShakeEntry 2）。差异：索引越出 0x40 项表的保护未登记。修正：补登 deviations「0x00414EC0 越界」。

## 0x00414F70 Game::RefreshCursor（StepPointer + DrawPointer）— exact

汇编：0x0043BD90(0x2E) 真 → 0x004C64C0（指针动作）= 0xF。menuState==0：(0x004D1288|0x004D0E80)≠0 → ++0x004C6368 >0 则 0；否则 ++0x004C636C >0x10 时 --0x004C6368 <−0x20 则 −0x20，且左键 0x004D1728≠0、指针 x >u 0x262、y <u 0x18：leader.id==0xFFFF → menuState=0x80000001 并直接画按钮。之后 leader.id==0xFFFF 或 eventCursor <u 8 时 0x0040EF60(0x004E4B78, 0x004C6368+0x282, 0)。0x004E6D48≠动作 → 指针 delay(0x004C64FC)=0；0x004E6D48=动作；0x00436C80 失败 → 「Act Err(RefrashCursor)」「設定錯誤 GUID[%d]\nact[%d] qq[%d] dir[%d]」；图 = (0x004C6502, 0x004C6504)，0x004E6D58 = 0x004C654A，以 (指针x−0x004C64C8, 指针y−0x004C64CC) 按 0x004C64D0 绘制，参数为图 +4。
对照：一致（kButtonDelay 0x10、kButtonLeft 0x262、kButtonBottom 0x18、WaitPointer 0xF、MenuEntering|MainMenu）；画在帧内与宽画面、图 +4 参数为已登记偏差（deviations 92、107），空图片见 299。修正：game.cpp 中 StepPointer、DrawPointer 注释的地址由 swd32 的 0x004149B0 改为 0x00414F70。

## 0x00415120 FieldView::DrawSprites — exact

汇编：从 0x004C6BD4 起（前驱哑节点 0x004C6B18）：x′ = _ftol(+0xB4)−镜头左、y′ = _ftol(+0xB8)−镜头上；推进 act（节点 +0），失败 → 「Act Err(PicPaint:DrawTObj)」；−0x48 < x′ < 0x2C8 且 −0x48 < y′ < 0x228 → 图 = (+0x4A, +0x4C)、0x004E6D58 = +0x92、以 (x′−act.x, y′−act.y) 按 act.flags 画（参数图 +4）。act.delay≠0 → 下一个。否则 x = stepX(+0xAC)+x 存回（取整用未舍入的和），y = stepY(+0xB0)+y 存回（取整用存回的 float）；取整后的 x、y 都在目标 (i16)+0xA8／+0xAA 的 ±0x20 之内（开区间）→ 摘下并释放，否则前进。
对照：一致（kSpriteMargin 0x48、kSpriteArrival 0x20；x 以 double 求和取整、y 以 float 存回后取整，与 x87 的两种取值相同）；空图片见 299，平滑层的 EffectAt／MoveEffect 为已登记增强。

## 0x00414CC0 FieldView::FadeColors — exact

汇编：三个步长（0x004C30DC、0x004C2B90、0x004E55A0）都等于 0.0 → 返回。参数≠0：帧数 0x004C300C <0 → 恢复，否则帧数−1；之后帧数 <0 → 恢复，否则当前值 0x004C2AA8／0x004D0468／0x004E5808 各加步长。恢复：步长 = 起始值（0x004C2AA4／0x004D046C／0x004E580C；当前值不变，原 BUG）。最后 0x00420E50(0x004EDA44, 0x4B000, _ftol(红), _ftol(绿), _ftol(蓝))。
对照：一致（DrawScreenTint 对 640×480 画布着色）。

## 0x004152A0 FieldView::DrawRoleHeads — exact

汇编：spritesB（0x004D3F94，前驱哑节点 0x004D3ED8）逐个：推进 act，失败 → 「Act Err(PicPaint:drawRoleHead)」；图 = (+0x4A, +0x4C)，0x004E6D58 = +0x92，以 ((i16)+0xA4−act.x, (i16)+0xAA−act.y) 按 act.flags 画；速度(+0xA6)&0x7FFF≠0：x += 速度、速度 ×3，(i16)x ≤−0x78 或 ≥0x2F8 → 摘除释放；否则 slide = 2×((i16)+0xA8−(i16)x)/3（有符号），x += slide，slide 在 [−1, 1] 时 x = 目标。
对照：一致（HeadSpeedMask 0x7FFF、kHeadAcceleration 3、HeadMargin 0x78、kSlideShare 2／kSlideParts 3）；空图片见 299，平滑层为已登记增强。

## 0x00415410 FieldView::DrawLines — exact

汇编：effects（0x004D3FBC，前驱 0x004D3FA8），pitch = 0x004B1E8C sar 1，颜色 = 0x004AF720[(i16)+0xA]。模式&0xFF00：0x800 静止 → 各行 0x004187A0(屏幕+(x+(y+行)×pitch)×2, 颜色, 上限 +4)，下一个。0x8000 下落：自末项到首项 长度 += 2×Below(6)+0x30，≥上限则计数并取上限，以 (x+偏移[i]+(y+i)×pitch) 画长度；全部到达 → 模式 = 低字节|0x800。0x4000 上升：偏移 −= 0x30+2×Below(0x18)，≤0 计数并取 0，长度 = 上限−偏移，画；全部 → 静止。0x2000 展开：偏移 += 2×Below(0x30)+0x30，≥上限计数并取上限，长度 = 上限−偏移且 ≤1 时为 2，画；全部 → 释放偏移、长度与节点。0x1000 收拢：长度 −= 0x30+2×Below(0x30)，≤2 计数并取 2，画；全部 → 释放。各模式依次判断（下落／上升完成后同帧不再画静止）。
对照：一致（随机数调用次序与次数、kLineStep 0x30、kFallJitter 6、kRiseJitter 0x18、kOpenJitter 0x30、kJitterStep 2、kMinLineLength 2、模式值）；颜色号取低 4 位、画面外像素跳过与计数不足一对为已登记偏差（deviations 113、112、20）。

## 0x004157B0 FieldView::DrawOverlays — exact

汇编：overlays（0x004C4A7C，next +0x1C）：IntersectRect(镜头, (u16 +0xA, +0xC, +0xE+16, +0x10+16))，为空跳过；剪裁 = 交集减镜头左上；视差 X(+0x16)==8 → 偏移 = 镜头左 ≥区左 ? 镜头左−区左 : 0，否则 偏移 = (镜头右−区左)×视差/16（有符号），镜头左 <区左 时加剪裁左；Y 同理用 +0x18；以 (剪裁左−偏移X, 剪裁上−偏移Y, 宽 +6, 高 +8, 模式 0) 画 +0 的像素；剪裁复原 640×480。
对照：一致（FixedToMap 8、ParallaxUnit 16、Copy 0）。

## 0x00415960 Notices::Draw — exact

汇编：小字体背景 0xFFFE、阴影 0（各两次）；从 0x004D1F4C（前驱 0x004D1EC8）起，y 初值 8：旗标 0xE 未置时 0x0043B590(目标, 0x280−11×lstrlenA(+4), y, +4, primary[3], 0x10)；y += 0x18；帧数(+0)−1 为 0 → 摘除释放，否则前进。
对照：一致（kTop 8、kRight 0x280、kAdvance 11、kLineHeight 0x18、kStyle 0x10、kNoticeColor 3、HiddenFlag 0xE，step 默认为真）。

## 0x00415A30 FieldView::UpdateMovie — exact

汇编：0x004E6CD4=0 → 0。对话窗链、effects、spritesB 任一非空或 0x004D1408&0x10：0x004D1E00==0 → 屏幕存入 0x004D128C、置 1；否则恢复；返回 0。0x004D1E00==1 → 恢复、清 0、返回 0。帧号 0x004D1430 ≤0：0x00415D50；黑带 = 4×帧号+0x34 行，清上下各黑带；帧号+1；返回 0。帧号 ≥10000：0x004D1408&1 → 0x00420E50(屏幕, 0x4B000, 2×(10000−帧号) ×3)；否则 active 暂置 0、清屏、0x00412F60、0x004E5820 |= 1、恢复 active，清上下各 2×pitch字节×(10030−帧号) 字节；帧号+1；变暗时 >10015、合拢时 >10030 → 释放块链（next +0x20）、0x004D1C80、0x004D128C、0x004D1F50、0x004C4B04，块头清零、当前块指回头、0x004E5590=0、active=0、帧号=0、关闭 0x004C2A48、0x004D1404 清 2、帧间隔 0x23；返回 1。其余：0x00415E40 非 0 → 帧号=10000，否则 0x00415D50 并帧号+1；返回 0。
对照：一致（FadeInStart −13、BarStep 4、FadeOutStart 10000、DarkenFrames 15、CloseFrames 30、kDarkenStep 2、kCloseRowsPerFrame 2、Paused 0x10、DarkenOut 1、MovieOpened 2）。

## 0x00415D50 FieldView::DrawMovieFrame — exact

汇编：基址 = 屏幕 + ((480−高 0x004E5590) sar 1)×pitch×2；行数 = 当前块（0x004E4DBC）+8（有符号，≤0 不画）。调色板 0x004D1F50 为空：每行 0x00427680(像素, (长−8) sar 1)，再复制 长−8 字节到 基址+行+4 的偏移；否则每行 长−8 个字节（有符号 ≤0 跳过）经调色板写成 16 位像素；行 += 长。返回 0（调用者不用）。
对照：一致（kRowHeaderBytes 8、kRowTargetAt 4）；负长度停画、帧与画面外不读写见 deviations 114。

## 0x00415E40 FieldView::ReadMovieFrame — diff → exact

汇编：在块链（0x004E6B60，next +0x20）找帧号 +0x1C == 0x004D1430：找到 → 定位 +0 + 0xC，长 +4 非 0 时读入 0x004D128C 并以实际读到的字节 0x00429C00 解包到 0x004D1C80；当前块 = 它；返回 0。否则从末块：偏移 = +0 + +4，帧 = +0x1C+1，定位偏移+0xC；循环：帧 > 0x004D0A9C（有符号）→ 屏幕 0x96000 字节复制到帧缓冲、返回 1；新块（0x24）偏移、+0xC←前+0x18、+8←前+0x14、+4←前+0x10、帧；读 +4 字节；调色板数 0x004D12FC≠0 且 表[序号] == 0x004D1430（当前帧号，非循环帧）→ 序号+1，定位文件尾 −((数−旧序号)×0x300+0x400)，读 0x300 字节，256 色 ((r&0xF8)<<7)|((g&0xF8)<<2)|(b>>3) 写入调色板后经 0x00427680 转换；新块 +0x18/+0x14/+0x10 = 块尾 −0xC/−8/−4 处的字，挂到链尾；偏移 += 长；帧 == 当前帧号 → 解包 长−0xC（为 0 跳过），当前块 = 它，返回 0；否则帧+1 继续。
对照：分支、次序与常量一致（FileHeaderBytes 0xC、TrailerBytes 0xC、PaletteBytes 0x300、PaletteTailBytes 0x400、Rgb555）。
差异/修正：块尾不足 12 字节的尾字取 0、调色板序号越界不换色两处保护未登记；解包长度回绕时源码把块缓冲区扩到约 4 GB。解包改为不超出块缓冲区，三处与块缓冲区增长一并登记为 deviations「0x00415E40 越界」行。复核：完整函数重新对照一致。

## 0x004160E0 FieldView::DrawWeather — exact

汇编：旗标 5 未置 → 返回。两个粒子（步长 0x10）：先 roll = Below(1000)；x ≤−0x280、x ≥ 16×宽+0x280、y ≤−0x140、y ≥ 16×高+0x140 时为出界：roll < 0x3DE 跳过；否则 色调目标=0、速度目标 = roll/200+1、动作 = 0x004C66A4 + roll×0x004E55A6/1000，按 0x004D183C 0..3 分派：0/1 x=Below(16×宽)、y=16×高+0x76，2/3 x 同、y=−0x12C；步长 0→Below(1)−2 两次，1/3→Below(2) 两次，2→Below(2)−2 两次；不画。界内：计数+1 > 速度 → 计数 0、x += 步长X + (步行速度×0x004D0E80 sar 2)、y += 步长Y + (步行速度×0x004D1288 sar 2)；roll<0x19 → 速度目标 = Below(5)+1；速度向目标走 1；roll<5 → 色调目标 = Below(4)−5；色调向目标走 1。之后 act 0x232A、动作，取图（空图片见 deviations 300），三色调 = 色调，透明 = act +0x92，模式 0x004E55A6==4 ? 4 : 0x2C，位置 x−act.x−镜头左、y−act.y−镜头上。
对照：一致（随机数次序、常量、模式）；平滑插值为已登记增强（deviations 90）。

## 0x00416450 FieldView::DrawBubbles — exact

汇编：x、y 在镜头外 0x40 以上 → 返回。sway = Below(5)−3（存入 y 参数位）；act 0x232B 动作 0x3B、取图。四个标记（0x004E6B10，步长 0x10）：(i16)角色 == 参数（零扩展）才处理：0x0040C4D0 查序号（先写 0，故未找到时序号为 0），旗标清 1，x/y 取角色表（步长 0xE4）；Below(1000) > 0xFA → 旗标置 1；计数 +8 < 0x18 且旗标 1 → 新泡（0x10 清零）插到链头：y = 标记 y、x = (Below(16) + 标记x−8)<<4、寿命 = 地图 0x32 ? Below(16)+16 : Below(0x1E)+0x1E、步长X = Below(16)−8、步长Y = −1−Below(1)。逐泡：roll = Below(1000)；y += 步长Y、x += 步长X + sway（16 位）；roll > 0x32 → 寿命−1；寿命 > 0 → 地图 0x32 时红/绿/蓝 = 寿命−0x1E/−0x26/−0x32，否则三色 = (寿命 sar 1)−0x1E，画于 (x/16 −act.x−镜头左, y−act.y−镜头上)、模式 act +0x18；寿命 ≤0 → 有后继则把后继 16 字节搬入本泡、释放后继并重新处理本泡，否则摘除释放本泡、结束本标记。
对照：一致；源码角色序号的 min 截断不可达（0x0040C4D0 返回的序号总在表内）；平滑重放分支与插值为已登记增强（deviations 90），空图片见 300。

## 0x00416730 FieldView::DrawLeaves — exact

汇编：栈上动作表 '4','4','4','6','8','8','8','6'；旗标 6 未置 → 返回。四片叶（0x004D1C40，步长 0x10；act 0x004C6888，步长 0xA4）：x ≤−0x78、x ≥ 16×宽+0x78、y ≤−0x78、y ≥ 16×高+0x78 → x = 0x7FFFFFFF。x == 0x7FFFFFFF：x=−0x40，y=Below(16×高)，步长Y=Below(5)−2，步长X=Below(7)−3，为 0 时取 2，<0 时 x=16×宽+0x40。否则 Below(1000) > 0xFA：turn=Below(3)−1，步长Y += Below(3)−1 并限于 ±3，步长X += turn 并限于 ±5，为 0 时 = −turn。动作 = 表[步长Y+3]，步长X>0 加 1；推进 act、取图（空图片见 deviations 300），画于 (x−act.x−镜头左, y−act.y−镜头上)、模式 act +0x18；x += 步长X，y += 步长Y。
对照：一致（随机数次序、常量）；动作表下标截断不可达（步长Y 总在 ±3）；平滑插值为已登记增强（deviations 90）。

## 0x00416930 FieldView::DrawHeat — exact

汇编：旗标 7 未置 → 返回。帧号 0x004E6CD8（u16；0x0040DFC0 返回 0/1，eax 高位为 0）为 0：列 0..0x280 步 0x50、行 0..0x4B000 步 0xC800 共 48 项，依次 长度=2×Below(0x26)+0x4C、偏移=Below(3)+行+列+0x27F、行数=Below(0x4E)+1。否则帧号&3==0 时按 (帧号>>2)&3：0 → 长度表全部 64 项重取；1 → 48 个偏移按上式重取；2 → 行数表全部 64 项重取；3 无。帧号+1。48 项逐项：行数（有符号）>0 时从 偏移×2 起每行把下一行的 长度 字节前向复制到本行，共 行数 行。
对照：一致（表长 0x40/0x40/0x30、随机数次序、常量）；画面外跳过见 deviations 112，满窗平滑画布见 93。

## 0x00416B00 FieldView::DrawLightning — exact

汇编：roll = Below(1000) > 900 时：旗标 8 置 → 计数（0x004C4650，i16）+1，>8 则取 8 并直接画；未置 → 计数−1，<0 则取 0 并在 0x004D1E04==0 时返回、否则画。其余情况计数与 0x004D1E04 都为 0 时返回。画：0x004D1E04=0，pitch = 0x004B1E8C sar 1，画面末 = 屏幕+0x96000；0x40 道（0x004D0E88，步长 0x10）：未激活且已起 < 计数 → x=Below(0x280)<<4、y=0、slope=Below(3)（(i16)x>0x1400 取负）、速度=Below(0x1E)+0x10、长度=Below(0x10)+8、寿命 速度>15 ? 0x20 : 0x10、相位 0、激活 1，已起+1，本帧不画。激活：长度 ≥0 时逐段 段 0..长度：行地址 > 屏幕 → 行地址 < 末 时 0x004277A0(级,级,级)（结果不用）后 0x00420E50(行+x/16, 1, 级,级,级)，否则停；行地址 ≥ 末 → 停；x += slope、行 += pitch、级+1（寿命 0x20 再 +1）。之后 x += 速度×slope、y += 速度（16 位），寿命−1 为 0 → 停，否则 0x004D1E04+1。
对照：一致（kTenthChance 900、kMaxBolts 8、kSlopes 3、速度/长度/寿命常量、随机数次序）；0x004277A0 只把颜色经转换函数算出后丢弃、无副作用，源码不调用；像素越界跳过见 deviations 112，满窗画布见 93。

## 0x00416D20 FieldView::DrawFireflies — exact

汇编：栈上摆动表 0,1,1,1,0,−1,−1,−1。roll = Below(1000) > 900 时：旗标 0x16 置 → 计数（0x004D3FA4）+1，>1 取 1 并画；未置 → −1，<0 取 0、0x004D0B88==0 时返回。其余情况两者都为 0 时返回。画：0x004D0B88=0，首行界 = 屏幕+0x500、末行界 = 屏幕+0x95B00；0x60 只（0x004E4E60，步长 0x10）：未激活且已起 < 计数 → x=Below(0x280)<<4、y=0、slope=Below(3)（x>0x1400 取负）、速度=Below(3)+1、长度 1、寿命 = 速度×0xA0−Below(0xA0)（16 位）、相位 0、激活 1。激活：亮度 = 0x1F−(0x1E0−寿命)/0x18（有符号），无符号 >1000 取 0；摆动 = 表[相位/4]；长度 >0 时逐段：行 > 首行界 → 行 < 末行界 时 0x004277A0（结果不用）后染 中心=亮度、左右上下=亮度>>1、四角=亮度>>2（中心 = 行 + x/16 + 摆动），否则停；行 ≤ 首行界 且 ≥ 末行界 → 停；x += slope、行 += pitch、亮度+1（寿命 0x20 再 +1）；段+1 < 长度 继续。之后 x += slope×速度、y += 速度、相位+1 >0x1C 归 0、寿命 −= 速度，>0 → 0x004D0B88+1，否则停。
对照：一致（常量、九点染色次序、随机数次序）；像素越界跳过见 deviations 112，满窗画布见 93。

## 0x004170A0 FieldView::DrawSpotlight — exact

汇编：旗标 0x13 未置 → 返回。act 0x004C6BD8 id 0x232B 动作 0x4E，推进、取图：裁剪 (X−宽/2, Y−宽/2, X+高/2, 高/2+Y)（原程序上边用宽、右边用高，保留），画于 (X−宽/2, Y−高/2)、模式 0；动作 0x4F，推进、取图，裁剪 X/Y ±0xC0，画于中心、模式 0x2C。X(0x004D3450)≠目标(0x004C2B80) 或 Y(0x004D3454)≠目标(0x004E6CC4)：X += 0x004C6608、Y += 0x004C660C，到达的一维步长清 0。
对照：一致（kSpotlightRimAction 0x4E、kSpotlightDarkAction 0x4F、SpotlightRadius 0xC0、Subtractive 0x2C）；空图片见 deviations 300（边缘图缺失时不设裁剪区），画布偏移见 93，平滑见 85。

## 0x00417230 FieldView::DrawRipples — diff → exact

汇编：水波链（0x004C6150，前驱 0x004C6128，next +0x28）逐个：0x00435C50(屏幕) 复制、0x00435C80(屏幕) 折射、0x00435DB0 推进，返回非 0 → 摘除、0x00435BF0 析构并释放；否则前进。
对照：一致。
差异/修正：源码在画布行距改变后不复制、折射旧涟漪，重放（PaintBetween）时不推进；两处增强原已在 deviations 85、93 的说明中，地址列缺 0x00417230，行距一条未写明。补登记地址与行为，代码不改。复核：完整函数重新对照一致。
