# 第一遍验证：战斗模块

## 与正传相同的 43 个函数（第 830–985 行之间）— exact

asm_identity 判为与正传相同（battle_identity.txt），且源码与正传重建中已为 assembly_exact 的版本相同：ItemPriority 0x004575E0、SetGaugeFull 0x00457610、BattleHud::DrawTileBox 0x00457630、DrawPictureFrame0 0x004578C0、DrawBar 0x00457B00、DrawPictureFrame 0x00457B50、DrawGauge 0x00457BA0、DrawGaugeAlt 0x00457C20、DrawGaugeWide 0x00457CA0、DrawNumber 0x00457E10、DrawNumberDigit 0x00457F70、FillColor 0x004580C0、DrawEffectAt 0x004580F0、DrawCursorAnimation 0x004581D0、CreateBattleSurfaces 0x00459B50、ReleaseBattleSurfaces 0x00459BA0、Game::FadeFieldOut 0x00466290、SortItems 0x00467AB0、SortSkills 0x00467B00、Game::FieldFadeFrame 0x00467B50、Game::QuakeFrame 0x004695B0、Game::ReturnBattleItems 0x004697A0、BattleState::SetReturnedItem 0x00469BA0、BattleFile::ReadHeader 0x0046B260、Game::SelectPreviousRole 0x0046F380、Game::SelectNextRole 0x0046F400、Controls::PressConfirm 0x00472C60、Game::NextSelectableRole 0x00472C90、Game::PreviousSelectableRole 0x00472D00；以及 14 个全局初始化 thunk（0x00459880、0x004598B0、0x004598E0、0x00459910、0x00459940..0x004599D0，构造／析构登记，与正传逐条相同，映射到 Game::Game）。

源码差别：FillColor 多一个守卫（颜色低字等于 RLE 标记时不绘制，已登记 deviations.md 0x004580C0）。

## 战斗 HUD、转盘、计量条与动画（第 834–889 行中的 32 个）— exact

与正传配对且只差布局或常数者（汇编规范化后对照，源码与正传重建一致或只差对应常数）：
- 0x00457900 DrawPortrait、0x00458040 DrawSlotAnimation、0x004581B0 ResetEffectAt、0x00458330 DrawScrollBar：只差 ActState 0xA4 字节（正传 0x98）与栈偏移；滚动条常数 7.0（0x004AADDC，正传 0x00499CF0 同值）。
- 0x00457A50 DrawArrow：箭头动作 0x2392、外传另把三色 tint 设为 −4（0x004EC178／0x004EC180／0x004ED9A4），smooth 层 HudFrame::DrawArrowAdvance 相同。
- 0x00457D20 DrawSmallNumber：外传 10 位（首除数 1000000000，正传 4 位、1000）。
- 0x004582C0 DrawDigit：字体 0 用图 0x2370（正传字体 0／1 分用 0x2358／0x2359），其余字体沿用上一张图；模式 0x4000 时加 0x20 发光；返回图宽。
- 0x00458A00 DrawTimingBar：图 0x2380（正传 0x234F）；帧图取不到时读空指针高度（已登记）。
- 0x00459890／0x004598C0／0x004598F0／0x00459920：10 个 0x34D4 字节的我方单位（0x005136E0）与 20 个 0x3078 字节的敌方单位（0x00539590）的数组构造／析构登记（正传 0x2F34／0x2B28、8 个）。
- 0x00459640 DrawAnimation、0x004596E0 PlayAnimationShifted、0x00459830 FreeAnimation：外传 BattleAnimation 为 0x130 字节（动作 +0、六个 0x14 字节帧图头内联于 +0xA8、x +0x120、y +0x124、波形 +0x128、id +0x12C），正传存指针；DrawAnimation：id 为 0 返回 0，否则动作 id = id、动作 0、推进（不看结果），以帧图头的像素与 wave（0x004ED9AC）画于 (x−[+0x10], y−[+0x14])，返回动作 +0x98；PlayAnimationShifted：动作清零后推进，每个未画过的帧号移位（amount > 0 右移 3、< 0 左移 2）并画，[+0x44]／[+0x46] 清零，位置 [+0x42] 为 0 时动作清零返回 1，推进失败返回 0；FreeAnimation：六帧像素释放，id、波形与动作清零。
- 0x00459AE0 ResetBattle：只差单位大小。

外传新增或无配对者（直接对照汇编）：
- 0x00458240 DrawRoleIcon：动作 0x23AA、动作号 = 参数，推进成功时画于 (x−[+0x10], y−[+0x14])。
- 0x00458670 DrawCoinToss：coinFrames（0x0050CDC8）为 0 时 coinState（0x0050CDCC）高字 ≥ 1 则清零，否则低字 = Below(2)；旋转动作（0x0050C1B0）0x239B 动作 0：推进失败返回 0；coinZoom（0x0050C3DC）< 0x400 时以 (w/2, h/2, 0x400−zoom, 0x400−zoom) 缩放，画于 (0x104−x, 0xC8−y)，flags|0x80；未停住（[+0x9C] 为 0）时 coinFrames+1，每 25 帧 coinDelay（0x004BEFF4）两半 = 2×低字、[+0x46]+1 并抄到 [+0x44]，延迟 ≥ 0x40 时 coinDelay = 0x00010001、zoom 0、低字为 1 时高字（胜场）+1、停住、音效 0x117；倒数（高字）减到 0 时重装；[+0x44] 为 0 时音效 0x2E；返回 0；停住后 coinFrames ≠ 1：面（0x0050CA88）0x239B 动作 = 低字+1，画于同处，高字+1，> 0x1E 时音效 0x197、面清零、coinFrames 1、高字 1，返回 0；coinFrames 1：面动作 3，以 zoom×5+0x400 缩放、tint 0、0x84 画，zoom += 0x3C，到 0x400 时 coinFrames、zoom 清零、旋转与面清零、高字 1，返回 1。初值 0x00010001 与 exe 相符。
- 0x00458BB0 DrawTallGauge：图 0x237A：步长 = 图 2 高 /6（共 4 级时 (高−0x20)/6），shown = 步长 × level；图 0 于 (x,y)；level < 6 时裁剪 (x, top, x+宽, top+shown)（top = y+图 0 高−2）；图 2 于 (x+4, top)（非 6 级高减 0x20）；alpha 8 以 0x14 画图 3 于 (x+0xE, top+0x1F)（非 6 级高减 0x1E）；全屏裁剪；图 1 于 (x, top+shown−3)，返回其结果。
- 0x00458D80 SetupRoulette、0x00458DC0 RaiseRoulette、0x00458E30 DrawRoulette、0x00458FD0 ResetRoulette、0x00459010 RouletteBonus：tops（0x004BF004，初值 5,5,0,0）、digits（0x004BF008，初值 2）、stopped（0x0050CDD8）、result（0x0050CDDC）、spins（0x0050C324）、spinning（0x0050CDE0）；Setup 置初值后 Raise steps 次；Raise 依次把未到 9 的位加一（第 3、4 位时 digits = 3／4）；Draw：图 0x23B0（stop 1 时 1）于 (x−宽+0x34, y−高/2+2)；各位（x 每位 −0xE，周期 3，第 3 位起每位 −1）：已停者画 result，否则 stop 1 时 result = spinning，画 spinning，spins == 周期时清零且 spinning = (spinning+1) % (tops+1)，spins+1；spins[stopped] 为 1 时音效 0x125（全停时读表外，已登记）；stop 1 时 stopped+1；全停时 value = r0+10r1+100r2+1000r3 返回 1；Reset：stopped、result、spins 清零，spinning = Below(10)；Bonus：首位 0 时其余全 0 则经验加成（0x00577220）= 10×digits 并以 "此場戰鬥經驗值加百分之%d" 公告，否则 0；首位 7 时其余全 7 则金钱倍数（0x0057721E）+2×(digits−1)（非 0 时以 "此場戰鬥金錢變%d倍" 公告），否则 0；其余全等时返回 10×(digits−1) 并以 "龍颿法寶加%d點額外經驗" 公告；公告 (0x118, 0xA, 0x3C 帧, 0x40000002)。
- 0x00459170 SetMeterLimit、0x00459180 AddToMeter、0x004591E0 ResetMeter、0x00459200 DrawMeter：limit 0x004BF002、value 0x0050CDD4、lastAdd 0x0050CDD6、level 0x0050CDD0、shown 0x00577368；Add：−1 时清零，否则 lastAdd = max(amount,0)、value += amount（< 0 归 0）；value > limit 时两者清零返回 0，否则 1；Draw：shown 为 0 返回；图 0x2397 的 0 以调色板画于 (x,y)；share = value/limit（> 1 取 1），target = ftol(图 1 高 × share)，level 向 target 走 1；裁剪 (x, y+高−level, x+宽, y+高) 画图 1；level ≠ target 时 tint 0xA 以 0x10 再画；全屏裁剪；DrawNumber(0x2377, value, x+宽/2, y+高)。
- 0x004593C0 DrawGrowing：growWidth 0x0050C3E0、growHeight 0x0050C3EC、growFrames 0x0050C928；以 (w/2, h/2, width+0x400, height+0x400) 缩放、tint 0、0x84 画；height += 0x50、width += 0x3C、frames−1；width ≥ 0x4B0 时三者清零返回 1，否则 0。0x004594B0 DrawPicturePalette：图 id 的 0 以调色板画于 (x,y)。
- 0x004594F0 LoadAnimation：id 写入、x／y、动作 id = id、动作 0，推进失败返回 0；循环：[+0x90] 非 0 时 wave = 它；帧号未载过时直接载入（0x00436770）帧图头并右移 640/divisor；位置 [+0x42] 为 0 时动作清零返回 0，否则动作 0、id 后再推进。
- 0x004599E0 LoadBattleBackground：释放旧背景，载入图 背景+0x3A98 的 0 并右移 640/scene；animationCount（0x005770B8）个背景动画先 Free 再 Load(x 0x0050EE38[i], y 0x0050EE58[i], id 0x0050EE08[i], scene)；三个暗色复制 tint（0x004BF1D6..DA）= −1。

## 开战、开场、滑入与战斗帧（第 892–895 行）— exact（含修正）

- 0x00459BD0 StartBattle：前段清零逐地址对到成员：0x005394E8 ActState（0xA4）、0x0050ED68（0x50）、ClearActing 的 0x00575F18..0x00575F33（8 个成员共 0x1C 字节）、0x005377FC（0xF0）、0x0050D88C（0x28）、actionQueue 0x0053477C（0x50）、commandNames 0x0050EBF0..F8、commandKinds 0x0050D870（8 字节）、消息 0x00537388（0x28）、0x005376B8（0x78，两次）、commandEnabled 0x005378EC（8 字节）、pendingOrders 0x005369C8（0xC8）、enemyPendingOrders 0x00536EE0（0x190）、0x0050EBFC／slotDamage 0x00537758（各 0x78）、−1：0x00513664、slotTargets 0x005378F8、0x00575F38（各 0x78）、0x00537A20（0x348）、roleListScrolls 0x005394B4..C0、0x00537A10／14、inParty 0x00536EDC、enemyHits 0x005376A4（20 字节）、0x00577224／0x00577228、itemUses 0x005134A0（0xFF 字节）、ailment400From 0x005761C8（6 字节）、skillNamePending 0x0050D878（20 字节）、roleNames 0x0050ECB4（0x80）、ghostLockTarget、0x0050CE6C（0x50）、stolenItems 0x0050F41C（0x200）、0x005774E8／0x00576FC4／coinTossing／0x00576FD0／0x00576FC8／0x00576FCC、ResetMeter、0x0057734C、actingUnit −1、listedCommands、0x00577994。0x0050CEBC／0x0050CEC0 只被本函数清零，无读者，不建模。
  名字收集（4 × 10 字节，u16 计数）、槽记录 30 个（+0 −1，+0x18／+0x0C／+0x10／+0x14 清零，+0x0A 字清零）、roleListRows 全 1、结束动作 0x2329／0xC、清屏色、旗标 0x1E.. 等于 1 时入队并置 partyMask 位、partyRoles 依次取在队角色、timingWindow 0x3C、中字体间距 0x10、三色 (0x1F,0x1D,0x17)／(0xF,0xE,0xB)／(0x19,0x19,0x1D)、投影器、指针 (0x140, 0xC8)、离屏面、战斗对话、battle.ffd 头与阵形；敌人数低字为 0 时报错返回；scene = Below(4)+1、背景；敌人：Reset、记录清零、动作／x／y／种类 0、镜像时朝向与 0x280−x、Setup、守备旗标 1 时 SetGuarding(1)；角色动作取 0x004BF17C（1..4，与 exe 相符）、种类 1；站位与 RoleCycle（0x004BF57C..88）三种组合、面板偏移 (+0x14,−0x96)(+5,−0x9E)(+0x14,−0xA0)(−5,−0xA3)（movsx）；角色 Reset、镜像（0x280−x、面板 0x270−x）、Setup、倒下计数；SortItems／SortSkills；装备、技能、物品、显示名；三条计量（double 除法 × 48，_ftol）与原值、防御值；开场姿势角色；符鬼（旗标 0x5A）：角色 0 SelectEntry(1)、+0x10C = 0xA8、+0x108 = 变量 + 0x5A、(0x1C2, 0xAA)、Reset、SetupGhost、ghostOrderRow、0x005770D6 = roleCount、0x005770D4 = 1、roleCount+1、角色 0 倒下时 0x00577370 = 1 并 SetStatus(0x5000)；MeasureUnits、SortDrawOrder、ListEnemyOrder；Below(10) ≥ 7 时各敌人 Below(10) 步推进，否则同序号角色 RandomizeGauge(0)；角色 RandomizeGauge(开场姿势者 1)；倒下数 ≥ roleCount − escort − guest − ghost（无符号，jb）时 result = 0x67。
  修正：① 指针初值原为 (0x140, 0xC8)，源码误为屏幕中心 (320, 240)；② itemUses 原清零全部 0xFF 字节（rep stosd + stosw + stosb），源码只清前 0xFC，余下 3 字节误作 unk51359C／unk51359E（已删）；③ 末尾人数比较改为无符号；④ guestUsed（按正传地址误认，外传无此量、无读者）删除；⑤ unk4FF0BC、slotDamage、unk502984、slotTargets、unk53AE90 改为 30 项（原为正传 18 项，按敌人与单位槽下标访问会写出）；⑥ 敌人数超过 20 时只建前 20 个，其后的敌人仍各消耗一次开局随机步数（偏差已登记）。
- 0x0045AAD0 BattleIntro：音效 0xC0、0x005772BC = 1、主画面存入 surfaces[1] 并编码（0x96000）、开场画面（背景、排序、更新、底栏 0x2375 (0,0x180)、面板、存入 surfaces[0]）、style ≠ 0 时编码；样式 0：红绿 tint −2（0x004ED9A4／0x004EC180）、蓝 0（0x004EC178）、剪切缩放 (left, 0xF0, left+0x2C0, 2(0x200−frame), frame<<6)、wave 0xA、0x84；样式 1：sin(n·3.14f/33)×256、锚 (0x140,0xF0)、左右分开；样式 2：sin(n·3.14f/30)×256、SetZoom(0x140,0xC8)、上下分开；缩放 0x10×(0x40 − s/10)，s 从 9 起每帧 +0x12，left 从 0x140 每帧 +2 到 0x17C；之后样式 0：滑入（Below(3) 未用）；样式 2：0..0x672 步 0x32 加法缩放后滑入；样式 1：0x21 帧截断后左移 8 的正弦分开（0x225 起每帧 −0x12）；其他样式不做。0x005772BC = 0、全屏裁剪；未给音乐（panelFlags 0x400）时 music\Battle01.mp3（id 1、列表 3、Switch(3,1,0)、音量 0x7F×v/11 有符号）；musicLevel、清 0x400；重复上限非 0 且 byte_576DE7[battle] ≤ 上限（u16）时加一；同动作敌人互相 EnrageCheck(0x1D,0)（先 first 后 second）；旗标 0x1F 与 0x20 时站着的敌人 EnrageCheck(8)，0x1E 与 0x21 时 (9)；TargetFrame。敌人循环限制在前 20 个（偏差并入 0x00459BD0 行）。
- 0x0045B490 SlideBattleIn：临时离屏面（屏幕大小，平台替换已登记）；每轮先抄出旧画面，480 行各加 2×Below(0x14)+0x1E 宽，满 640 计到达；显示；到达数 > 480 结束；释放。
- 0x0045B5C0 BattleFrame：frameResult 1、timeGetTime 计时；MouseHover、BattleKeys、AutoBattleTurn、MeasureUnits、SortDrawOrder；BattleDebugKeys 为 0 时 AudioPump 返回 0；槽记录 0 非 −1 且 0x00577270、battleRuns 1、result、0x005772C8 全为 0 时 TakeSlotRecord，取到单位时 0x00577202 = 0、0x00577270 = 1、0x00578428 = 单位；enemyGaugeBoost = 无行动单位且无当前单位；SlideUnits(0x00577800, 0x004BF1E0, 0x00577804)；背景；0x00577260 或 hitsAllRoles 时 SortActingDrawOrder；UpdateUnits；掷硬币结束时 grayBandState 低字 2；CheckStoneSides、ApplyHits、HitFrame 为 1 时 spellStage|1；底栏；当前单位：0x239F 于 (0x004BF10C[unit], 0x180)（表值 0xEF,0x4F,0x18F,0x18F 与 exe 相符；当前单位只由 RoleGaugeFull 设为非自动、非客、非符鬼的角色，不超出 4 项），未灰带且 Flag26B8 为 0 时光标 0x2391 于面板位置；面板、菜单、结束帧、消息、线、头像、对话、抗性面板、计时器 (0xA,8,1)、调试、CheckBattleEnd、提示；fadingField 且帧数 ≤ 0 时 StartFieldFade(0x0050CEC8, 0x00536ECC, 0x005394B0, 0,0,0, 8)（原参数为 float，调用方都由整数转来，源码以 int 传递取值相同）；FieldFadeFrame(1)；地震或 panelFlags 0x100 时 QuakeFrame 否则显示；AudioPump；截图（增强已登记）。
- 0x0045B8D0 DrawBattleBackground（主体在 smooth 层 BattleFrame::DrawBattleBackgroundAdvance）：源图 0x00513638、全屏裁剪；未变暗（0x00577290、0x00577294 皆 0）：shake ≠ 0 时投影器移位背景（> 0 右移 3、< 0 左移 2）、画背景、各动画先移位再画；shake 0 时画背景与动画，灰带（0x00577218）为 1 时 band（0x00577216）< 0xC0 则 < 0x14 翻倍否则 +0x16，裁剪 (0, 0xC0−band, 0x280, 0xC0) 与 (0, 0xC0, 0x280, band+0xC0) 各以 0x28 再画；shake（0x0057702C）清零；闪光（0x005772CC）：三色 tint = (i8)0x004BF1A2，对 0x3C000 像素依次调红、绿、蓝，等级 −4，到 0 时回 0x10 并停；全屏裁剪；变暗且未恢复（0x005772EC 为 0）：按色调（0x005777F4）时 shake 移位动画（不画）并清 shake、画背景与动画、三色显示值向目标逐一减（i16 有符号）、调色；否则等级（0x0057723A）≥ 1 时以 surfaces[等级−1] 复制屏幕；帧数（0x005369C4）> 1 时归零并升级（上限 2），帧数 +1；恢复中且 0x00577334、0x005772C8 皆 0：shake 移位动画、画背景（不画动画）；三色显示值皆非 0 时等级 −1，darkenedAt（0x004BF108）为 −1 或按色调时向 0 逐一加并调色，否则以 0x005379F0[等级] 复制返回（0x005379EC..0x005379F0 无任何读写，恒为空指针，复制失败不画，源码只在等级 ≥ 1 时复制，结果相同）；否则清三色目标与显示值、等级、darkenUnit −1、0x00577290／94、按色调与恢复旗标。flashLevel 注释改为外传地址 0x004BF1A2。

## RoleActionFrame（第 897 行）— exact（含修正）

- 0x0045BEA0 RoleActionFrame（源码分为 RoleActionFrame、RoleCommandFrame、RoleSpecialFrame 及各命令辅助函数）：先取命令 A，倒下返回 1；0x00575F24 与 0x00575F2C 皆 0 且 target ≥ 敌人数（有符号）返回 1；命令 A 为 0 时命令 B 为 0 返回 1，否则（target 改为命令 B）落入 switch 的 default 返回 0；0xC8 走向目标（slideUnit = target）返回 0；> 0xC8 为特殊攻击。
  跳转表 byte_45ED3C／jpt_45BF75 解出 34 个命令号：1、2、3、4、5、6、9、11、12、13、14、15、17、22、23、24、25、26、27、28、29、31、32、33、34、35、36、37、38、39、40、41、80、99、100，其余 default 返回 0，与源码 switch 相同；特殊表 byte_45EDB4／jpt_45E521：0x12C、0x190、0x192、0x194，另 0x195、0x196、0x199、0x1F4 直接比较。
  逐 case 对照：1 攻击（敌：命中、slideMode 2、伤害、吸血戒指 0x38 比例与 0x237D／延迟 8／计数步 5、ApplyDrain；角色目标：伤害、ApplyDrain 用同序号敌人、吸血按目标算）；2 法术（SpellCast、sweepGap、slotTargets −1、sweptUnits 1、slotDamage 0、ActFlag13 时 rippleHits 与 0x2C 字节 ScriptObject(640,480,0,0,0xC8,0xC8)、两侧 sweepTotal 上限、CountEnrageEvent(2)、enemyCommandTurns、DefeatAllByCast、音乐降到 5、SpellOver 后恢复、对所有单位调空函数、清伤害、ResistancePanel 返回、释放水波、undarkening、SetCommand(0x12C)、TryRetreat）；3 物品（同上，UseSelectedItem(listRole) 的第二参数被调函数不读）；4 法宝攻击（法术点 +2／+5 带 −1 检查）；5 防御；6／80 收化与封印（pushFrames 0x5770CC：0x28 等待、0x1E 显示、2 时公告「收化成功」「收化失敗」、0x117／0x116、失败 EnrageCheck(0xF)；收化另以名声表计数 ≥ 0x14 视为成功；封印用 UseSelectedItem(6)，敌人全灭时 autoBattle 0、result 0x63）；9 倒下；11／12 淡入淡出（SetGuarding 0／1、+0x2A0A = 8、enemyFaded）；13 投掷（actionQueue 第 role 行 8 格中第一个空位 = target+1）；14 捕捉（result 0x62）；15 召唤护卫（escortCalled 0x00536EA8[0x14+role]、护卫上限 2、0x5770DB = 0x80）；17 撤退（等级比较、「撤退失敗!!」「撤退成功!!」、0x8C、护卫撤退的站位清除、角色 0 撤退时符鬼 0x5008、result 0x68）；22 掷硬币；23 偷窃（「成功偷到[%s]!!」0x32 帧、0x61A8 时「敵人身上無任何物品!!」、「偷竊失敗!!」）；24 姿势横扫（0x8000 位、伤害上限 0x270F、enemiesHitAct = dword & 0x7FFF、2 时结束）；25／99 控制与解除；26 时机攻击；27 直接攻击（UseSelectedItem(4)）；28／29／32 饰品增益（施法 0x1965／0x1791／0x17B7，饰品 0x1C／0x1D／0x20，位 0x10000000／0x8000000／0x2000000，回合 4×熟练度/100+2）；31 计时攻击；33 必杀（0x1C87、物件 kind 位 0x20 免疫、熟练度 0x21 机率、姿势 7）；34–36 自伤三种；37 龙颿法寶转盘（0x40000000／0x20000000 位、0x168,0x64、0xBA、15 帧、0x2389、加成 +5 经验、「龍颿法寶升級」）；38／39 施法 0x17BB／0x17BF；40 计量条（0x1776／0x1C9D）；41 物件化身；100 完成。特殊：0x12C 回位（ReturnHome(−1,0)）；0x190 冲锋、0x195 吸取法术、0x196 光束（命中、特技点 +2 不查 −1、伤害、+5）；0x192 群攻（变暗、darkenByTint，不记命中）；0x194 连击（灰带 1→2、他人与敌人状态 4／0x40）；0x199 跳击（SetCommand 后 AddFury、返回 1）；0x1F4 魔法。
  修正：① 0x004873C0 ZoomEntranceFrame 的第一参数不是未用参数：两处取绘制位置都对它（命令 41 的目标角色）调用 0x0048B230，源码误用自身，现改为 anchor 参数并由命令 41 传入目标角色；② 0x00488BB0 LoadSkillAction 把技能动作写入整个 dword（高字清零），两个调用者（命令 25 的控制、战斗对话 0x00479860）原先保留 enemyHitActs 的高字，现改为整 dword 写入。escortCalled 注释改为外传地址 0x00536EA8。

## EnemyActionFrame（第 898 行）— exact

- 0x0045EE20 EnemyActionFrame：先取命令 A，倒下返回 1；A 与 B 皆 0 返回 1（A 为 0、B 非 0 时 switch 落到 default 返回 0）；跳转表 jpt_45EED6（17 项）：1、2、4、6、9、10、11、12、15、17 有分支，3、5、7、8、13、14、16 default；> 100 时 0xC8 走向 battleRoles[target]、0x12C 回位（ReturnHome(−1,0) 成功时 darkenUnit −1 返回 1），100 返回 1，其余 0。
  1 攻击：0x00575F28 为 0 时 slideUnit = 敌人、slideX 0x140、EnemyAttackFrame(角色)，成功后 slideX 0x140、slideMode 2；角色有效果位 0x400 且 0x005761C8[target] 为 0 时记为 敌人+1；偷窃（0x00577255）：noticeText 填空格、在 0x0050F41C 该敌人列表找第一个空位、TakeRandomItem 写入、非 0 时载入物件并以「敵人成功偷走了[%s]」0x117、(0x118,0xA,0x32, 0x80000002) 公告并清偷窃旗标，为 0 时「偷竊失敗」0x40000002（不清旗标）；命中、unk53BFC0 0、lastRoleHit、ApplyDrain；伤害（hit, spirit, stamina）倒下时 0x004BF1C8 = target、闪白；清 spirit／stamina（hitDamage 不清）、darkenUnit −1、角色 0x0049B7D0、命令 0x12C；0x00577360 为 1 时对角色以 0x005770C4 再伤一次（0x2389、计数 1）并清二者返回 1，否则返回 0。对敌人目标：命中、伤害倒下时 0x00513664 与 0x004BF1C8、闪白；hitDamage 0、darkenUnit −1、返回 1。
  2 法术：SpellCast、sweepGap、slotTargets −1、sweptUnits 1、slotDamage 0、ActFlag13 水波；enemyCommandTurns[0]；hitActs[0] 以 enemyHitActs[enemy]（整 dword）、动作 0 推进，0x00575FD0 = (计数低字节 >> 1) & 0x7F；全体时 sweepTargets 计数（0x00575F28 为 0 数站着、未防御、非符鬼的角色，否则数站着的敌人），每个角色 enemyCommandTurns；单体且目标角色倒下时改瞄第一个可打的角色；清 hitActs[0]；音乐降到 5；SpellOver 后恢复音量、清 enemyCommandTurns、伤害、水波，undarkening 1、返回 1。
  4 技能：EnemySkillFrame(角色) 成功时命令 0x12C，返回 0。6 吸走角色：pushFrames 0 时 StartDrainEffect、角色 StartDefeat(1)、SetGuarding(1)、移除槽记录 0x14+target、变暗 −12、0x00577290、DarkenByTint、pushFrames 1、sweptUnits 1；DrainEffectFrame 成功时 sweptUnits 1、pushFrames 0、0x00577290 0、darkenUnit −1、undarkening 1、返回 1。9 角色倒下：DefeatFrame、SetCommand(0)、移除 target+4（原样）、0..3 时 defeatedRoles+1、currentUnit／actingUnit 等于 target+4 时清除（后者另清 0x00577270）。10 淡出：0x004956B0（参数不读），返回 0。11 淡入：清 0x1000、SetGuarding(0)、+0x2A0A 8、enemyFaded。12 隐身：SetGuarding(1)、置 0x1000、+0x2A0A 8、enemyFaded，enemiesGone ≥ 敌人数时 autoBattle 0、result 0x63。15 召唤：pushFrames = 0x8019、CommandValues(0x00577210, 0x00577212)，每个召唤者 Reset、清 0x00537D68、站位 (0xF0, 第 2 个 0x154 否则 0xC8)、镜像时朝向与 0x280−x、载入物件、Setup、置 0x400、敌人数+1、MeasureUnits、SortDrawOrder、ListEnemyOrder；每帧 pushFrames−1，低 15 位为 0 时 darkenUnit −1、清 pushFrames 与召唤数、返回 1（敌人满 20 时停止召唤，偏差改为外传地址）。17 逃走：RunAway、SetGuarding(1)、+0x2A0A 8、StartDefeat(1)、姿势 0、escapedEnemies[enemy] 1、enemyFaded。

## RoleFrame（第 899 行）— exact

- 0x0045FA80 RoleFrame（源码分为 RoleFrame、RoleGaugeFull、RoleAutoCommand、RoleTakeTurn、RoleOrderFrame、RoleGatherSweep、RoleTakeOrder、RoleActFrame、RoleAfterAction、RoleRetreatFrame）：开场姿势角色只跑 Pose36Frame（0x005772BC 1，结束时 |0x80、清 0x005772BC）返回 1；IdleFrame(+0x2A74 非 0 且 0x00577290／94 为 1，或 0x00577310 为 1)；battleRuns 1、无行动单位、0x005772BC／0x005772C0 为 0 时 TickEffects、StepGauge(0x00577348)；满条：PartyMember 时 panelGrown 0、Regenerate；非自动／客／符鬼：0x2C、0x005772C0 1、currentUnit、0x005772BC 1，autoBattle 0 时 0x00577258 1 并 ConfirmKey(0)；否则数倒下或带效果位 4 的敌人，全数时 ResetTurn，否则 RoleAutoCommand 后 0x005772C0／0x005772BC 置 1。
  RoleAutoCommand：SetTarget(1)；aim（0x004BF0E4）= ghostLockTarget，无或已倒下时随机取站着、无效果位 4 的敌人；指令 5／6 以第一个站着敌人的强度为基准取更弱／更强者；0x0057725C 1；技能（aim 1、找 LowHitPoints(0) 的可瞄角色，位 0x80000000）、攻击（找无该命令效果的敌人）、治疗（aim 1、找可瞄角色）、指令 7（首个可瞄角色的 HitPointShare(100) 为基准，LowHitPoints(1) 者比较，选中后 ChooseGhostSkillOf(1) 为 0 时以目标 HP 判 SpendGhostSpirit(…, 0x46)，成功则 0x0050D88C[role] = 0x17B7、order.unk00 1，否则 aim 0；aim 非负时取 MostHitEnemy、倒下则随机，Below(100) > 0x32 时 ChooseGhostSkillOf(3)）；符鬼：GhostSkillCommand，ActFlag1／0 记入 order +8／+4，种类 1 时 order.unk00 1 并随机瞄可瞄角色；aim 去掉高位写入 aimTarget；非符鬼：敌人等级（物件 +0x58）平均（有符号除法）LevelGapRoll 成功则命令 0x11；ChooseCommand 成功时 ActFlag1／0、CommandSupports 时 aim = role+1、order.unk00 1。
  RoleTakeTurn：0x005376B8[role]、IsHeld、+0xAB4、行动单位、result、currentUnit、battleRuns、DrainSecondGauge 依次检查；倒下单位记录且剩一个以下敌人时 ResetTurn；result 0x63 且 0x00577206／0x00577208 为 0 时 ResetTurn；0x005772BC／0x005772C0 1、0x005376B8[role] 1、turnUnit；turnTaken 不为 2 时 InsertSlotRecord(1, self, −1)，槽记录 0 已占时 (…, 0)，否则直接行动并取出 pendingOrders。
  RoleOrderFrame／RoleGatherSweep／RoleTakeOrder：order +4 为 1 时聚气横扫（0x005772D0 非 0 对角色，否则对敌人；目标数达标时找最后一个站着的角色或第一个站着的敌人，0x0057727C 1、SetAA8、Retarget），+0xAA8 为 1 时取命令（自动者只清旗标）；其余按 order.unk00 与自动与否三条路径（SetAC0、SetAD4、SetCommand(turnTaken)、Retarget、PrepareCommand、SetTarget 的次序、menuCommand／pageListRow／specialRow／aiTarget 的设定）与汇编相同。
  RoleActFrame／RoleAfterAction：0x00577314 非 0 时 0x00577260、0x005772BC 1、darkenUnit、MarkAC8、RoleActionFrame(role, (i16)+0x29A2)，返回 1 时善后（符鬼回位状态 0x4000／8／0x1000 清、置 0x200；各旗标清零；目标非 −1 时 RetargetRoles、敌人全倒时角色 ExpireEffects(1)、panelFlags 0x80 时站着的敌人 SetFacingIfAble(0)；ResetTurn；ClearActing、各表清零、actingUnit −1、spellStage 0、enemyFaded 1；对话倒数；SpreadPoison(0)；灵力／体力低时 EnrageEnemies(0x19／0x1A)；清 +0x29A2）；hitsAllRoles 非 0 时跳过；目标敌人 +0xAAC 非 0 时等待；否则 0x00577314 1、darkenUnit，按 AA8 与目标倒下改瞄第一个站着的敌人（无则复位），技能名（+0x2B38）非空时 (0x118, 0, 0x23, 名, 0x40) 并清 0x005774D4..E4，FinishCommand。
  RoleRetreatFrame：0x00577204 位 0x4000 时 StepBack(0) 且 sweptUnits+1 ≥ 站着人数（无符号）时取站着敌人最高等级（该角色有倒下记录时 0xC8），0 时复位，角色 0 BeatsLevel 成功则 SetValueAAC(1)、位 0x8000、sweptUnits = 站着人数（低 16 位），否则全员 SetValueAAC(0)、无消息时「撤退失敗!!」、turnUnit 的角色 ResetTurn、0x8C、复位、EnrageEnemies(0x16)；位 0x8000 时各旗标、角色 0 时符鬼 SetAF8(1)／SetGuarding(1)，该角色位未置且 StepBack(1) 成功时置位、defeatedRoles+1，全员撤退时「撤退成功!!」、清 roleOrder 与 30 个槽记录、result 0x68、行动单位 −1。之后 SpecialActFrame、RemoveDefeated(role, 1) 为 1 时 0x005772D8 0、0x004BF1C8 −1，返回 1。damageShown、aiTarget 注释改为外传地址。

## EnemyFrame（第 900 行）— exact（含修正）

- 0x004612D0 EnemyFrame（源码分为 EnemyFrame、EnemyAimSide、EnemyPickTarget、EnemyMayAim、EnemyChooseCommand、EnemyAimHeal、AimedDown、EnemyActFrame、EnemyAfterAction、ReturnHeldEscort）：battleRuns 为 1 时：无行动单位、未倒下、0x005772BC／0x005772C0 为 0 时 TickEffects，+0x2FF0 为 0 且 AdvanceGauge(0x00577348) 成功时 AddSlotRecord(2, 敌人)、两旗标置 1；0x00577260 与 0x00577204 皆 0 时：行动单位是自己时，IsHeld 或倒下则清旗标、行动单位 −1（被定住另记 0x00577028 = 敌人+1）并返回 ResetTurn 的值；全体攻击（0x00575F2C）时 Retarget(0)、0x00577280 1，打敌人时数站着的敌人，否则 0x005772BC 1；单体时未选目标（0x00577318）则 EnemyPickTarget（状态位 5：除将死者外只剩自己时随机瞄可瞄角色，否则随机瞄站着、非位 13、无效果位 4 的其他敌人，置 26B8 位 28、0x00575F28 1；有待命命令时直接；PickRoleTarget(EnrageElement)；嘲讽（效果位 8）计数、0x0057736C；不可瞄（效果位 4）计数；循环随机角色，倒下人数 ≥ 战斗人数时行动单位 −1 并返回目标值；标记角色（0x00576FC8／0x00576FD0／0x00576FCC，倒计 2、26B8 位 0x1000）；同目标连续 3 次且其他可瞄者多于 1 个时重抽；嘲讽、倒下、+0x3038、符鬼、位 13、效果位 4 时重抽；同目标计数 0x005774E8）；+0xAB4 为 0 时 EnemyChooseCommand（命令高字节为目标；CatchRoll 1 逃走（Retarget(0)、0x11、+0x2A0A 2）、0 回位（0x12C）；命令带 0x8000／0x4000／0x2000 时按下令执行：0x005772D4 转为打敌人、0x8000 PrepareCommand、0x4000 法术 2 并等待第二计量、0x2000 命令 6、ActFlag1／0、0x00577018 0、目标倒下时向后找（到 0x14，越界已登记）再 Retarget；否则非状态位 5 且能治疗时 EnemyAimHeal（自身为治疗型时只治自身，否则找站着且虚弱者中治疗型者或 HP 最低者），ChooseCommand 成功时等待第二计量并记技能名，CommandSupports 时打自己一方、ActFlag0、目标未倒下时 Retarget）。第二计量满（0x0050CE6C）时 AddSlotRecord(2)；行动单位是自己、第二计量已过、+0xAB4 为 1 时 EnemyActFrame（hitsAllRoles 1、0x005772BC 1、MarkAC8、darkenUnit、技能名 (0x118,0,0x23, 名, 0x40)、EnemyActionFrame 完成时善后：0x00577298 0；符鬼听令、目标为角色 0、打角色且角色 0 非低血时符鬼反击（turnUnit、aimTarget、aiTarget、Retarget、0x00486E10、InsertSlotRecord(1, 符鬼, 0)）；目标角色有伤害转移（0x20000）且伤害 > 0 时转给 +0x34C0 所指（倒下则随机）的敌人（0x2389）；AA8 时全体、否则单体的被定住护卫归还；ResetTurn；panelFlags 0x80 时站着的非客角色 SetFacingIfAble(0)；各旗标清零、行动单位为敌人时 ClearActing；对话倒数；SpreadPoison(1)；EnrageCheck(0,0)，无可选时 (0x1E)；具名敌人在倒计为 0 时找 HP ≤ 5 成的在队角色标记 2 回合；未完成时 hitsAllRoles 1）。之后 IdleFrame、0x00575F38[敌人] 非 −1 时 PlayHitAct(0x00577098)，成功则清；EnrageFrame；RemoveDefeated(敌人, 0) 为 1 时清 0x00513664、slotDamage、slotTargets、0x004BF1CA −1、enemyFaded 1、0x005772DC 0，返回其值。
  修正：0x004616F9 以 `cmp byte_5774E8, 3; jge` 有符号比较同目标连续次数，源码原为无符号；计数到 0x80 以上时二者结果不同（影响重抽与随机数消耗），现按有符号字节比较。

## HitTargetFrame、HitAllFrame（第 901–902 行）— exact

- 0x004626D0 HitTargetFrame（主体在 smooth 层 HitFrame::HitTargetFrameAdvance、DrawHitPictureAdvance、RippleHitFrameAdvance、AddAndDrawHitPictures 与 Game::TakeHitBits）：hitActs[index] 的 +0x98 为 0 时：水波（0x00577340）：攻击者 CommandTarget 时取双方位置，帧数（0x00575FD4）< 0x12C 时 WalkLine，到达时 0x142、Strikes、帧数 0x12C；先取屏幕副本（0x00435C50），帧数 0 时 MoveTo(x−0x50, y−0x96) 与 Splash(0x64,0x32,0xC,0xC8)；帧数 +1，≥ 0x12C 时停在 (目标x−0x14, 目标y)；MoveTo(x−0x50, y−0x78)、Refract、Step；≥ 0x154 时 waiting 与帧数 0；否则每 9 帧（≤ 0x1E）MoveTo(目标x−0x64, 目标y−0x96)，第 0 帧 0x142，Splash(Below(0x50)+0x3C, Below(0x64)+0x32, 0xC, 0xC8)（先抽 Below(100)），帧数 +1 后折射，≥ 0x64 时 Strikes、waiting、帧数 0。非水波：动作 id = act 推进，失败时清 followActs[index] 与 0x0050EBFC[index] 并返回 1；+0x5E／+0x60 时 flags|0x80 并 SetZoom；未走到时载入帧图；攻击者 kind 0 与镜像各翻转 Mirror 并按图宽调整 x 与锚点（镜像时锚点用原值）；有偏移且有锚点时用原点+偏移减锚点，否则用目标位置减原点；音效 +0x58 与声像（x ≥ 0x140 时 +0x30 否则 −0x30）；ClearMoveAxes；CommandTarget 时 WalkLine，到达时清 0x00577250、effectA 与 0x00535B38 动作取 FollowAct、Strikes、走到旗标、enemyCommandTurns 有符号 ≤ 0 时 waiting 否则减一并重走，effectA 非 0 时 AddsPicture、不画并停在目标位置减锚点；记 0x00537A04／08 后画图。之后暗色取 +0x68／6A／6C、DarkenByTint、0x00577290／94 1。加图（AddsPicture：0x0046AD90，攻击者 kind 0 时 1，0x0050EBFC 1）、DrawHitPictures、HitBits 并入；TakeHitBits：0x8000 地震、FollowsUp 下 0x1000／0x2000／0x4000 各 SetNudge(0x1E, 1／2／3)、0x400 以 +0x7E.. 六个 float 与帧数 StartFieldFade 并置 fadingField、4 时 RaiseFlag26B8 并清、Strikes 或 0x10 时（ActFlag14 抗性面板：panelFlags|0x20、0x00577A44 = 目标物件、result 0）伤害（kind 1 HitDamage 否则 EnemyHitDamage，上限 0x270F，−1 时 0 不显示）、灵力 0x237E／体力 0x237F 数字依次延 8、记 lastSpirit／lastStamina、slotDamage 累加、hitDamage、hitFading 0、target 0；hitFading 为 1 时 FadeHitAct 成功则清；slideBackPicture > 0 时 PushUnits 为 0 返回 0；waiting 且 HitPicturesDone 时清位置、followActs、0x00535B38、帧数与走到旗标返回 1。
- 0x004632B0 HitAllFrame（smooth 层 HitAllFrameAdvance 与 Game::PlaceAllHit、NudgeAllHit、RaiseAllHit、StrikeAllHit、ShowAllHitDigits）：lastDamage 0；waiting 为 0 时推进动作（失败返回 0）、载入帧图、音效、Mirror 翻转与原点／锚点调整；有偏移且有锚点时原点+偏移减锚点，否则固定位置（镜像时 0x280−宽；原点非 0 时按 kind 与镜像取 0xA0／0x1E0 减原点、y = 0xEB − y，kind 0 且镜像时 x = 0xA0 − x）；CommandTarget 时从攻击者位置 WalkLine 到 x（y 0），到达时 Strikes 与 waiting；0x005772F8 时居中 (0x190−x, 0xC8−y)；画图、0x0050D868+1、暗色、DarkenByTint；加图同上；地震；FollowsUp 下 0x1000 时对全体（攻击者 kind 1 则敌人否则角色，非全体时目标）SetNudge(0x1E, 1)，0x400 场景淡化；4 时全体（hitsAllRoles 时站着的非客非符鬼角色，否则站着的敌人）RaiseFlag26B8；Strikes 或 0x10 时：非全体对目标（Strikes 时 Raise 并清 0x00577030），全体时 (0x00577260 且 0x00575F24) 或 hitsAllRoles 对非客、非符鬼、非位 13 的角色（hitsAllRoles 时跳过倒下者并以 EnemyHitDamage，否则 HitDamage；每个角色延迟从 0 起；前者条件下再对角色 Damage），否则对站着的敌人（延迟接着 PlaceAllHit 的输出累加，不清零）；灵力／体力显示后清零，lastSpirit／lastStamina 记为 0；target 0；slideBackPicture > 0 时 darkenByTint 1、PushUnits；0x005772F8 时敌人全倒前返回 0，否则 waiting 且 HitPicturesDone，slideBackPicture > 0 时再 PushUnits；结束时清 0x0050D868、followActs、0x0050EBFC 返回 1。
- 0x00463ED0 PlayHitAct（smooth 层 PlayHitActAdvance）：地震位 0x8000 时 0x00577030 0、quaking 1；+0x98 为 0 时动作 id/动作 0/hold 0 推进（失败清 followActs[index] 与 0x0050EBFC[index] 返回 1）；载入帧图；镜像时翻转 Mirror 且原点 = 图宽 − x；偏移两者皆非 0 时原点+偏移，否则绘制位置；减 (原点, y)；音效与声像（原点 + x ≥ 0x140 时 +0x30）；画图并释放；waiting 为 1 时清 hitActs[index] 返回 1，否则 0。
- 0x00464110 FadeHitAct（smooth 层 FadeHitActAdvance）：act 0 返回 1；0x00536EAC[index] 有符号 ≤ −0x20 时清零返回 1；取绘制位置；fadeActs[index]（0x00538160）以 act／action 推进，失败返回 0；载入帧图（不释放）；三色 tint = 淡出值；画于 (x − +0x10, y − +0x14)；淡出值 −4，返回 0。

## 角色面板、倒下结算、目标重选与排序（第 905–914 行）— exact（含修正）

- 0x00464240 DrawRolePanels（smooth 层 DrawRolePanelsAdvance、StepPanelGaugeAdvance、RollNumber）：中字体阴影 0、无底色 0xFFFE；跳过自动、客、防御中者；符鬼：头像 变量+3 于 (0x258,0x1AE)、灵力条 0x236E 于 (0x23F,0x1B8)（(i16)值 ÷ (i16)最大 × 48.0f，_ftol）、数字 0x2377 于 (0x265,0x1AB)、计量 GaugeLength+1 宽 3 高蓝 0x18 于 (0x23F,0x1BB)、命令名 0x004BF62C[ghostOrderRow] 于 (0x238,0x1C2) 亮色；角色：当前单位且已长出时高亮框 (0x004BF15C[i]+4, 0x184, 0x9C, 0x5C, 亮度 0x004BF1A3&0x7F) 并每 3 帧呼吸（0x80 位下降至 0、上升超过 8 时转降）；第二计量（0x00537A18）宽度非 0 时绿 0x18 于 (nameX,0x177)；未倒下且 +0x2FF0 为 0 时计量 0x2380 于 (nameX,0x18B)；倒下时名字 0xF000；+0x2FF0 为 1 且已长出时名字 0xFFFF，当前单位再按 nameFlashes%4（有符号）以 0xF3F0／0xFFFF 重画并计数；头像（倒下 1）于 (portraitX,0x1CC)、效果图标于 (gaugeX,0x1CE)；防御值条 0x2385 于 (portraitX,0x1D0)（相等时取 *roleBlock，下降差 >10 时 5 否则 2，上升 3／1）；生命值（float 比例、滚动差 bit 27 时取反、步长差/10+1、值 ≤0 归零）、框 0x239E 帧 0 于 (gaugeX−0x12,0x19F)、条 0x236D 于 0x1A7（下降 3／1）、数字 ≤10 用 0x2379、≤ 最大/3 用 0x2378 否则 0x2377 于 (gaugeX+0x26,0x19A)；灵力 帧 1 于 0x1B0、条 0x236E 于 0x1B8、数字 0x1AB；体力 帧 2 于 0x1C1、条 0x236F 于 0x1C9、数字 0x1BC，滚动下降低于 1 时归零；+0x2FF0 为 1、在队、未长出时 DrawGrowing(i, 0x2380, nameX, 0x18B)；*roleBlock 为 0x30 且未倒下时箭头于 (portraitX,0x1D0)；计量 (0x14,0x186)；自动战斗时框 x 每帧走 (0x44−x)/3，DrawMessageBox(x−0x3A, 0x162, 0x46, 0x18, 0, 0, 0x004BF3B8)。修正：符鬼灵力条的分母原程序按 movsx（0x00464307）取 i16，源码原为 u16。
- 0x00464E70 DamageNumbersFrame：role 1 时角色否则敌人 DrawDamageNumbers，为 1 返回 1 否则 0（开头读 0x0057727C 的比较结果未用）。
- 0x00464ED0 RemoveDefeated：角色：DefeatFrame 为 1；护卫倒下的站位清除同撤退，MeasureUnits／SortDrawOrder／ListEnemyOrder；否则 defeatedRoles+1；chargingRole 清除；CancelCommand、移除槽记录、0x005376B8 清零；当前单位是它时全员 SetAC0(0)、currentUnit／turnCommand 0、aimTarget 1、result 1、三旗标 0；行动单位是它时清除；roleOrder 中移除并前移（最后一项 0）；StartSpecialAct 为 1 时 defeatedRoles−1；有符鬼且为角色 0 时 0x00577370 1、符鬼 0x5008、移除其槽记录；倒下数 ≥ 战斗人数（无符号）时 result 0x67，否则 EnrageEnemies(0x17)，返回 1。敌人：DefeatFrame 为 1；result 0x64 时返回 1；奖励（0x00487F80 的两值，第三值物件 kind 0x20 时 0x14 否则 3）；逃走者清零全部奖励，否则名声计数 +1（上限）；收化成功时第一值 0；否则（含逃走者）归还被偷物品（0x117、「取回被偷的物品!!」）；超过重复上限时前两值减半；累加 0x005771FC、（非 0x005772F8 时）0x005771FA、0x005771FE；CureHeldBy、enemyHits 0、移除槽记录；0..敌人数时 enemiesGone+1；全倒时 result 0x63 等；只剩一个时 EnrageEnemies(2)；召唤者只剩首个敌人时敌人数重置为 1；0x0050CE6C 清零，返回 1。修正：角色路径末尾的倒下人数比较原程序为无符号（0x004651C7）。
- 0x00465460 RetargetRoles：0x004BF1CA 不是该敌人时返回；其他角色瞄准它时改瞄第一个站着的其他敌人并置 0x00577314；无可改且只剩一个敌人时 SetCommand(0)、SetTarget(0)、ResetTurn、清 0x00577260、roleOrder、turnUnit、currentUnit、行动单位 −1、30 个槽记录全 0xFF。
- 0x004655A0 DarkenByTint：0x005777F4 = 1。0x004655B0 MeasureUnits：清 unitY 与 drawOrder（各 30），记敌人与角色的绘制 y（movsx 字）。
- 0x00465660 SortDrawOrder、0x00465750 SortActingDrawOrder（BattleState::SortDrawOrderAround）、0x00465A70 ListEnemyOrder、0x00465AB0 UpdateUnits：按 y 由小到大排序（首选跳过 y 0 与已取，读到第 30 项时取 darkFrames 0x005369C4），行动者与目标成对排在一起（0x00577A50、0x00575F24、0x00575F28 任一为 1 或无行动者时不排），排完 0x00577344 = 1，有 30 时重新普通排序；敌人顺序取前 enemyCount 个小于 0x14 的项；UpdateUnits 按顺序调 RoleFrame／EnemyFrame。0x00465660、0x00465750 的两条越界偏差改为外传地址（第 30 项写到水波对象指针 0x00536880）；ListEnemyOrder 敌人数超过 20 时的越界写入新增偏差并加界检查。

## EndBattle（第 915 行）— exact（含修正）

- 0x00465B00 EndBattle：敌人与角色 Reset（有符号计数）、ReleaseBattleSurfaces、背景动画 FreeAnimation（前 8 个，偏差已登记）、投影器 Release、ResetMeter、释放背景；逐地址清零与汇编相符：roleOrder 0x28、0x005375C8 0x50、turnTaken[10..19] 0x0057604C、0x0053742C 字、0x00537A20 −1 0x348、enemyOrders 0x00513434 0x28、enemyPlaces 0x00537070 0x280、rolePlaces 0x140、槽记录 0x348 全 0、ClearActing 0x1C、hitActs 0x005347F8 与 fadeActs 0x00538160 各 0x1338、投影器、指针、阵形 0x1EC、0x0050ED68 0x50、roleListScrolls、0x005377FC 0xF0、0x0050D88C 0x28、actionQueue、命令名与种类、消息 0x00537388 起 0x28（指针与其后 9 个 dword）、生命值滚动与原值、nameFlashes、0x00536B44 0x20、0x0050EFF8 0x3C0、0x00575FC0..C8、0x0050F3E8 0x28、0x005394E8 ActState、灵力滚动与原值、enemyHitActs 0x50、0x00575FCC、roleListRows 全 1、controlRows 2、listRole 2、panelAct 0x233B、对话 choice、panelFlags 等计数与旗标、menuCommand／aimTarget／orderStep／pageListRow／0x004BF0EC／controlCursor 1、darkenedAt −1、开场位置 0x004BF114..28 = 0x60,0x60,0xF0,0xFA,0xF2,0x64、fadeActLevels 0x1D 字节、lootIds 10 字、0x00536AF8／0x00536AFC／0x00536B00 字、0x005771FA..FE、0x00577204、0x00577088、0x00577206／08、flashLevel 0x10、0x004BF1C4 dword −1、0x004BF1C8／CA −1、0x0050CE6A、0x005377D0、0x004BF1CC = 6、暗色、darkCopyTint 三字 −1、darkenUnit −1、autoTarget −1、0x004BF1D0／0x004BF1EC −1、0x005770D8 字节、场景淡化 10 项、四个面板旗标、0x00577210／12、0x00577A50、chargingRole −1、战利品名与数、unitY／drawOrder／drawTaken、0x005376B8、hitFading、0x00577258..0x005772F8 一串旗标（跳过 0x00577280、0x005772C4／C8、0x005772E8／EC）、0x004BF1E4 = 1、learnedPending、mirrored、spellStage 字、0x00577308、autoBattle、0x0057732C、darkenByTint、wonByTalk、levelUpPending、orderSorted、0x0057734C；最后 archive Flush、acts CloseAll、StopAllSounds、SetEffectEnabled(0)、消息 Clear、CloseFiles、learnedPending 0、lastLearned 清零。
  修正：unk524200 由正传 8 项改为外传 20 项（0x005375C8，0x50 字节），unk4FF318 由 0x90 项改为 0xF0 项（0x0050EFF8，0x3C0 字节），BattleMessages::unk5214FC 由 8 项改为 9 项（0x0053738C）；删除外传汇编中没有对应写入、也无读者的 enemyHitAct（按正传 0x0053BD40 保留）；fadeActs 注释改为起址 0x00538160。
- 0x00466310 PushUnits：pushFrames（0x005770CC 字）+1；hitPush（0x004BF1DE，初值 0x1A4）> 0 时 pushFrames 0、half = hitPush/2（向零取整）、fieldX（0x004BF1DC，初值 0x140）+= half、hitPush = half、shake = 镜像 ? half : −half；shake 非 0 时全体角色再全体敌人位置 x += shake（字）并返回 0；picture（无符号）> slideBackPicture（有符号）时 fieldX > 0 则走 min(fieldX, 0x1E)、shake = 镜像 ? −步 : 步 并移动全体，darkenByTint 1；restart 为 1 时 hitPush 0x1A4；返回 1。初值与 exe 相符。

## HitFrame（第 918 行）— exact

- 0x004665A0 HitFrame（源码分为 HitFrame、RoleHitFrame、EnemyHitFrame、SweepFromRole、SweepFromEnemy、StepSweep、DrainToSpirit、EndEnemyDrains、ClearHitDamage、ClearHitTotals）：spellStage 无 0x8000 或有 1 时返回 0；先取行动单位的 +0x29A2；0x400 时 YinYangAttack 为 1 才返回 1；角色：单体（0x00575F2C 0）同时命中（0x00575F30 1）打敌人时 HitAllFrame(敌,…,0,0)，0x0050CE6A = +0x29A2、0x004BF1C6、命中、slotTargets，伤害（hit, spirit, stamina）倒下时 0x004BF1CA、闪白旗标、0x00513664、闪白；打角色时 HitAllFrame 后直接 Damage（不查 0x00576084）；逐个时 HitTargetFrame，打敌人另 ApplyDrain；打角色用 slotDamage[0]、倒下记 0x004BF1C8、闪白不置旗标，清 slotDamage[0]；清伤害三项与 hitActs。全体同时命中：HitAllFrame(敌,…,0,1)，打敌人时对站着的敌人记目标、命中、sweepHit，按 slotDamage 与 lastSpirit／lastStamina（i16）伤害，倒下时 0x00577064+1、闪白、0x004BF1CA = 0、0x00513664，每个敌人后清伤害、0x00577050+1 与三表项；最后清 0x00577050、hitActs 与三表。全体逐个（SweepFromRole）：sweptUnits 为 0 返回 0；对敌人：倒下者在 sweptUnits < 敌人数时 sweptUnits+1 并清 pushFrames，其余先推进扫描（pushFrames ≥ sweepGap 时 sweptUnits+1）再 HitTargetFrame，首次命中时记目标、伤害、倒下时 0x00577064+1、0x004BF1CA = 目标、0x00513664、闪白，清伤害，0x00577050+1 ≥ sweepTotal（0x00575F1C）时 0x00577050 0、sweptUnits 1、清 hitActs 与三表返回 1；对角色（不跳过倒下者，跳过自动、客、符鬼）同理但倒下不记 0x004BF1CA。敌人：单体同时命中打角色时 HitAllFrame 失败则 DrainToSpirit(hit)；成功时 StepOrbCounters(熟练度表, 攻击者 +0x1004)、0x0050CE6A、0x004BF1C4、命中、ApplyDrain、伤害倒下时闪白旗标、hitDamage −1、0x004BF1C8、闪白，被定住且为当前单位时 result 0，清伤害与 hitActs；打敌人（0x00575F28）时 HitTargetFrame、0x004BF1C6、以 slotDamage[0] 伤害（0,0）倒下时 0x004BF1CA 与闪白；打角色逐个时同上用 slotDamage[0] 并 DrainToSpirit。全体（SweepFromEnemy）：同时命中时 HitAllFrame(角色,…,0,1)，对站着的非客非符鬼角色 StepOrbCounters、slotDamage > 0 时记入 hitDamage、0x00577374 1、ApplyDrain、命中、目标、伤害倒下时闪白旗标与 0x00577064+1、DrainToSpirit、清伤害与该项，0x00577050+1 等于 sweepTargets（0x0057704C）时 EndEnemyDrains（0x00577374 为 1 时 ApplyDrainSums，清 0x00577054..5C 与 0x00577374）并清 0x00577050 返回 1（不重置 sweptUnits）；逐个打敌人时推进扫描、站着且首次命中者记目标、以 slotDamage（0,0）伤害、倒下时闪白旗标与 0x00577060+1、清伤害，等于 sweepTargets 时结束；逐个打角色时推进扫描（无符号比较）、首次命中者 StepOrbCounters、目标、命中、0x00577374 1、ApplyDrain、伤害、倒下时 0x00577060+1 与闪白、DrainToSpirit、清该项，0x00577050+1 ≥ sweepTargets 时 EndEnemyDrains 并清伤害；结束后（非同时命中）sweptUnits 1，清 hitActs 与 slotDamage 返回 1。sweepTotal、sweepDefeated 注释改为外传地址。

## AddOneItem、StartFieldFade、AutoBattleTurn、ApplyDrain、ApplyDrainSums（第 919–926 行中的 5 个）— exact

- 0x004679E0 AddOneItem：id 0 时返回（表头静态节点 0x004C3018 的记录）；从表头（静态节点本身起）找同 id：i16 count + i16 pending ≥ 0x63 时不加，否则 pending 1 时 pending+1 否则 count+1，返回其记录；找不到时新建 0xB4 字节节点插在表头、id、载入物件，pending 1 时 pending 1 且 kind |= 0x8000，否则 count 1，返回其记录。
- 0x00467C40 StartFieldFade：六个颜色参数原为 float（按位存入 0x0050CEE0／0x00536ED8／0x005394D0 与 0x0050CEC8／0x00536ECC／0x005394B0），帧数 int 存 0x00536AF4；三个步长 = (目标 − (float)_ftol(起点)) / 帧数，存为 float（0x005373B0／0x00539498／0x00535B30）。调用方都由整数转来，源码以 int 传递取值相同。
- 0x00467CF0 AutoBattleTurn：自动战斗、有当前单位且 result 不为 3 时：turnCommand 1、turnTaken[当前] 1；autoTarget（0x004BF0F4）−1 时 0x005772B4 1、result 3 返回；否则 turnUnit = 当前、currentUnit 0、0x0057725C 1、SetTarget(1)、aimTarget = autoTarget、result 0、0x005772B8／0x005772B4 0；所瞄敌人倒下时改瞄第一个站着的敌人，全倒时 SetTarget(0)、turnCommand 0、aimTarget 1、autoTarget −1、turnTaken[turnUnit] 0、自动战斗 0。
- 0x00467E50 ApplyDrain：值 0 返回；DrainKind 1：攻击者 0x237D、drainedLife（0x00577054）−= 值，未延后（0x00577374 为 0）时以 −值 显示并 Damage(−值,0,0)，不清原值；2／4：取目标灵力／体力（i16 剩余），剩余 − 值 ≤ 0 时值 = 剩余并在目标显示；攻击者 0x237E／0x237F，未延后时显示并加到攻击者；drainedSpirit／drainedStamina（0x00577058／0x0057705C）−= 值，spiritDamage／staminaDamage = −值，原值清零。0x00468000 ApplyDrainSums：按 DrainKind 以累计值显示并 Damage。

## WalkLine、SlideUnits、StartSlideTo（第 927–929 行）— exact（含修正）

- 0x004680A0 WalkLine：LineWalker 局部（起点、终点、步进）清零后填入；模式（0x00575FB0[index]）低 15 位为 0 时：有 delay 时延迟计数（0x005372FC[index]）+1，按有符号字未到 delay 时不增速；speed 非 0 非 −1 时倍数（0x0050CED0[index]）+1，否则步数 1、倍数 0 时先置 1，倍数 > 0x40 时 +0x40 否则翻倍；模式 1：朝 (fromX, 0x32) 每次最多 14×倍数 步（Projector::LineStep），到达（LineStep 1 或 x+fromX == toX）时倍数 0、模式 0x8000，之后倍数 +1；其他模式时结果取 speed；模式低位为 0（含 0x8000）时走 步数×倍数 步（有符号 32 位乘积，≤0 不走），到达时结果 −1；输出 x = 步进 x + 当前 fromX、y = 步进 y + 当前 fromY；结果 −1 时三个状态清零返回 1，否则 0。修正：步数上限原程序为有符号乘积与有符号比较（0x00468212、0x0046824D），源码原为 u32（speed −1 且延迟未到时会走很多步）；延迟计数按有符号字比较（0x004680F9）。
- 0x004682B0 SlideUnits：0x00577364 从无写入（恒 0），实际总返回 0；按原样：slideMode 0 返回 0；模式 1 时取单位位置（第三参数 0x00577804 非 0 时角色否则敌人；源码原固定取敌人，已改），step = (x − 单位 x)/8，为 0 且不等时 ±1，fieldX += step；模式 2 时 step = (0x140 − fieldX)/4，为 0 时 fieldX 与 x 按字相等则模式 0、shake 0 返回 1，否则 ±1，fieldX += step；step 非 0 时全体角色与敌人 Move(shake, 0)，返回 1。
- 0x00468440 StartSlideTo：slideMode 1、slideTarget = x，返回 x。

## 滑动、释放、撤退、槽记录等小函数（第 930–953 行中的 12 个）— exact

- 0x00468460 StartSlideBack：slideMode 2、slideTarget = x，返回 x。0x00469810 ReleaseBattle：投影器 Release，10 个角色与 20 个敌人各自 Release。
- 0x00469860 TryRetreat：该角色 ActFlag11 为 1 时：角色 0 的 0x00483490 为 0 或 panelFlags 有 0x200 时「無法撤退!!」(0x118,0xA,0x32,0x40000002) 与 0x8C；否则 0x005772BC／0x005772C0 1、0x00577258 0、undarkening 0、turnUnit／currentUnit 0、行动单位 −1、result 0、enemiesGone = (u8)敌人数、endFade 1。
- 0x00469F30 CureHeldBy：前三个角色中 0x005761C8 记为 敌人+1 者清零并 CureEffects(0x400)。0x00469F80 MostHitEnemy：enemyHits 20 项中首个最大者（无符号）的序号+1（u8）。0x00469FB0 EnrageEnemies：站着的敌人 EnrageCheck(cause, 0)。
- 0x0046A930 AllRolesHaveEffect、0x0046A9A0 AnyRoleBuffed：按角色序号检查“同序号敌人”是否倒下（原样）、在队、效果位或增益。
- 0x0046AA00 AddSlotRecord：种类 1／2 时取第一个空记录，记单位与种类，种类 2 另取 enemyPendingOrders[单位] 的五项并清零。0x0046AAE0 InsertSlotRecord：已有同单位时返回；第一个空记录序号（无则 0）；种类 1：at −1 时放在该处，at ≤ 空位时自空位向下移一格（越界一项不写，偏差已更新为外传地址）放在 at，否则放在 0；取 pendingOrders[单位−0x14]（小于 0x14 时不取，偏差）并清零，清 0x0057727C／0x005772D0；其他种类：at ≤ 空位时下移，放在 at。
- 0x0046AC40 RemoveSlotRecord：找到该单位的记录后其后各项前移一格，最后一项全 0xFF。0x0046ACB0 TakeSlotRecord：跳过单位 > 0x14 且被定住的角色（单位 0x14 不检查，原样），取出该项；非空时其后前移（第 29 项不移），从第一个空记录（无则该处）起清为空记录（unit −1、其余 0）。偏差 0x0046ACB0 改为外传情形（只剩读到表外）。

## BattleDebugKeys（第 931 行）— exact

- 0x00468480 BattleDebugKeys：调试模式（0x004AF7A0）且按住 Ctrl（DIK 0x1D／0x9D）时：F3 音乐 Switch(4,1,0)；F4 Sleep(100) 后 0x00577808 = (x+1)%2；F1／X 各 Sleep(200) 后切换 0x00537A18／0x00576084；K 置 criticalAttack（0x00578474）并公告「強攻擊」(0x208,0xA,0x1E,2)；Z Sleep(200) 后全体角色 SetVitals(−1,−1,−1)、AddFury(−1)，再 SetAttributes(0x1964,0x9B,0x64)；T Sleep(200) 后敌人 RandomizeGauge(1)；D Sleep(200) 后非自动非客角色 Damage(0x32,0,0)；R Sleep(100) 后 Damage(0x1F4,−5,−5)；V Sleep(200) 后全体敌人 Damage(0xA,0,0)；F9 Sleep(200) 后 talk.skipAll（0x004E6CFC）= (x+1)%2；E 音乐 Switch(4,1,0) 与 Switch(开战前音乐,1,0)、EndBattle、返回 0；C spellStage|1、清 0x00577270、ClearActing、两旗标 0、行动单位 −1，0x00577260 与 hitsAllRoles 为 1 时清零（其后对 [−0x15] 角色与 [−1] 敌人的 ResetTurn 不做，偏差已更新）；F5 切换帧间隔 100／0x23 与 0x005777F8；F2 Sleep(200) 后切换 0x00577330 与 panelFlags 位 2 并公告「正常靈契」「絕對靈契」；W 站着的敌人记倒下、slotDamage 0、Damage(0x7530,0,0)，0x00577064 = 敌人数，清 roleOrder、turnTaken[10..19]、槽记录（unit −1），0x004BF1CA −1，各旗标与 turnUnit／currentUnit／行动单位／result。调试模式下另：H／J 全体 Move(±10,0) 并 shake = ±10，T／Y slideMode 1／2。任何模式下 P 切换 snapNext（0x005772E8）。返回 1。

## DrawBattleDebug、CheckBattleEnd、DrawBattlePrompt（第 932–934 行）— exact（DrawBattleDebug 修正后）

- 0x00468B20 DrawBattleDebug：0x00537A18 为 1 时字体背景 0xFFFE、阴影 0；敌人行（0xA, 0xA+0x14n）`生命:%-3d 鎖:%-1d 令:%-1d lv:%-2d`（生命、GetWord2A74、GetCommandA、物件 +0x58 等级，物件指针写 0x005135A0）；角色行（0x208, …）`鎖定:%d 命令:%-3d`；攻击顺序／我方等待顺序／排列顺序三行（y = 敌人数×0x14 + 0xA／0x1E／0x32，数字 x 0x64／0x78／0x64 起步 0x14，取槽记录单位、0x005368AC、0x0053730C）；`目前角色` (0xF0,0x46)；敌人位置写 0x00577240／0x00577244，行动槽长度取低 16 位无符号比较（0x00468ECC，源码原为 int，已改 u16），两行像素 0xEEEE；第二槽长度非 0 时 FillColor(x,y,len,3,RGB(0,0x18,0))；其余各行字符串与 exe 字节一致（CP950），坐标 (0xF0,0x1E/0x32/0x5A/0x6E)、(0x190,0xA/0x1E)、(0x1A4,0x32/0x46/0x5A)、(0xF0,0xA)，取值依次 0x00576084／darkenUnit／battleRuns、result／0x005772B8／menuCommand、0x00577270、aimTarget／0x005772BC／0x005772C0、sweepTotal／sweepTargets、0x00577050、0x0057737A、enemiesGone（u8）、0x005774E9／0x005772B4、1000 / frameMilliseconds（有符号除）。showRoleBlocks > 0（有符号）时 4 个战斗块（0x004C6700 步长 0x60）画 values[1]、[2]、[3]&0x7FFFFFFF，x 0xA 起步 0xA0。最后阴影 0、背景 0xFFFE。battle 注释改为 0x0057737A；两条偏差改为外传地址。
- 0x004692E0 CheckBattleEnd：(u8)defeatedRoles ≥ roleCount − 0x005770D4 − escortCount − guestCount（无符号）时 0x00577258 = 1，endFade 为 1 且 FadeFieldOut 为 1 时 ReturnBattleItems、frameResult 2，result 0x68 或 panelFlags 位 8 各自 Switch(开战前音乐,1,0) 并 frameResult 0。(u8)enemiesGone − (u8)0x005771EA ≥ 敌人数（有符号）或 wonByTalk 为 1 时同样置旗，淡出完成时 Switch(4,1,0)、Switch(开战前音乐,1,0)、ReturnBattleItems、frameResult 0。
- 0x00469410 DrawBattlePrompt：promptFrames 加一，≥ 300 且 0x00577088 非负时返回；result 1,2,4,5,8,0x1B,0x1E,0x29,0x50 画 0x238E；3、9 在 0x005772B8 为 1 时按 turnPointer 画 0x238F／0x2393；其余 0x00577088 非负时按 currentUnit 画 0x238D／0x238C，否则 0x23A0 于 (0x00577246,0x00577248)、帧 0x0057724C、镜像 mirrored≠0，返回 1 时帧清零。跳表字节表 0x00469560 与 case 标注一致。

## ApplyHits、CheckStoneSides、SpreadPoison、YinYangAttack、PickRoleTarget（第 939–947 行中的 5 个）— exact

- 0x00469930 ApplyHits：按 drawOrder（0x0053730C）遍历 敌人数+角色数（有符号 > 0）项。单位 < 0x14 为敌人：DrawGuardText、DamageNumbersFrame(单位,0)、ApplyPendingDamage 为 1 时 0x00513664[单位] = 单位并 RemoveSlotRecord(单位)。否则为角色 i = 单位−0x14：DrawGuardText；DamageNumbersFrame(i,1) 为 1 时 damageShown 1、0x005378A8[单位]（= slotTargets[i]）−1；ApplyPendingDamage 为 1 时 damageShown 1、0x00513664[i] = i、RemoveSlotRecord(i)（按角色序号移除，原样）。damageShown 为 1 时清零，result 0、ghostSlot（0x005770D6 有符号）的 GhostObeys 为 1、角色 0 LowHitPoints(0) 为 1 且未倒下、ghostRescued 0 时取角色 0 生命，SpendGhostSpirit(生命,上限,0x50) 为 1 则 0x0050D88C[ghost] = 0x17B7、aimTarget 1、pendingOrders[ghost].unk00 = 1、Retarget(0)、InsertSlotRecord(1, ghost+0x14, 0)、0x005772BC／0x005772C0／ghostRescued 1。
- 0x00469BC0 CheckStoneSides：行动单位 −1 且 result ≠ 0x68 时，计数不自动、非客、非鬼、未倒下且有效果位 4 的角色（u8）；有且 defeatedRoles + 数 ≥ (u8)(roleCount − 0x005770D4 − escortCount − guestCount)（8 位相减，有符号比较）且 endFade 0 时 defeatedRoles 加数、result 0x67、返回 1。否则 0x0057734C 为 0 时计数未倒下且有效果位 4 的敌人，有且 enemiesGone + 数 ≥ 敌人数且 endFade 0 时 enemiesGone 加数、0x0057734C 1、autoBattle（0x0057731C）0、result 0x63、返回 1。其余返回 0。
- 0x00469D80 SpreadPoison：参数 0 时每个角色 CountPoisonTurn，PoisonLasted 为 1 时随机 (u16)Below(roleCount) 取角色，未倒下、+0x3038 不为 1、非鬼、非 StatusBit13、非自动且不是自己时 AddPoison(本角色 PoisonBase, +0x2FD2 等级)。非 0 时敌人同样计数，随机 Below(敌人数) 后按同下标检查角色数组（原样；≥10 越界读，偏差 378 行已有）的倒下与 +0x3038，非自己时给该下标的敌人 AddPoison。BattleUnit 占位名 unk2AF8 按外传偏移改为 unk3038。
- 0x0046A000 YinYangAttack：伤害 = (u16)AccessoryProficiency(0x16) × 0x960 / 100（有符号）+ 100 写 hitDamage（0x005378F4）与 lastDamage（0x00575F34）；CastPoseFrame(0x1775) 不为 1 时返回 0。grayBandState 2；对未倒下的敌人：lastDamage = hitDamage、PushTarget(0x2389)；硬币字 0x0050CDCC 为 0 时 ObjectUnkA2 为 0、非 0 时为 1 则 lastDamage 取负并 PushTarget(0x237D)，否则 RaiseFlag26B8；≥ 0x270F 取 0x270F；SetDamageDigits、StepTargetCount(1)；Damage(…,0,0) 为 1 时 0x004BF1CA = 序号、0x00513664[序号] = 序号、flashing 1、SetDefeatPose(6)、画面填 0xFF（客户区大小，平台替换行已合并 0x0046A000）。最后两伤害清零，返回 1。
- 0x0046A320 PickRoleTarget：准则取低字节 1..6（跳表 6 项）。上限 = roleCount − 0x005770D4 − escortCount（无符号，< 1 返回 0）。参考值取角色 0；从 1 起，所在角色倒下或非队员时改取下一角色的值并加一，超过上限返回 0。找到后对角色 1..roleCount−1 中未倒下的队员，与（不更新的）参考值比较，胜出者记 序号+1：1 生命 32 位有符号小于、2 灵力／3 体力 16 位有符号大于（取第二个输出）、4 RoleAgility／5 Defense 16 位无符号小于、6 RoleAttackBonus 16 位无符号大于。

## 第二层法术图片与影像列表（第 954–961 行）— exact

- 0x0046AD80 ClearHitBits：0x005778CC 字清零。0x0046AFE0 HitPicturesDone：链表头 0x00577818 为空返回 1。0x0046AFF0 HitBits：返回 0x005778CC。
- 0x0046AD90 AddHitPicture：id ≤ 0 时错误框「第二層」「第二層魔法act號碼錯誤!!」（exe 0x004BF54C／0x004BF554）；接到链尾，0xB4 字节清零，act.action(+0xC)、x(+0xAC)、act.id(+0x4)、y(+0xAE)、mirrored(+0xA8)。
- 0x0046AE10 DrawHitPictures：链表空返回；ClearHitBits；首个 act Run 为 0 返回。逐个：载入图片 (+0x4E 帧, +0x50 图)；flags = act.flags、偏移 = act.x；mirrored 为 1 时翻转位 1、偏移非 0 时 偏移 = (u16)宽 − 偏移；target 位 0x20 时 effectA 非 0 则 AddHitPicture(effectA, effectQ, x, y, mirrored)，target 清零；target 非 0 时或入 hitBits 并清零；PlaySound(view, soundVolume)、SetSoundPan(view, x ≥ 0x140 ? 0x30 : −0x30)、view 清零；绘制于 (x − 偏移, y − act.y)，宽高、flags、0；释放；act.waiting 为 1 时 done 1。下一个为空时清掉 done 为 1 的节点，否则其 Run 为 0 时直接返回（不清理）。载入失败时的空指针绘制已登记偏差（新增行）。
- 0x0046B000 ImageList::Add：Once 位时（帧、图都同于上次）或 unkB8 同于上次则不加；接到链尾、复制 0xC8 字节（调用方 0x0048FB20、0x0049A220 先清零结构，next 为空），count 加一，记上次的帧、图与 unkB8。
- 0x0046B0C0 ImageList::Draw：逐个取图（0x00436720），level > 0 时减 step，< 0 置 0；alpha = level；以 mode|0x14 绘于 (x,y)；level > 0 时设三色调后以 mode|0x10 再绘，level 仍 > 0 则保留，否则 count 减一并移除。返回 1。0x0046B200 Empty：链表头为空返回 1。

## BattleFile 构造与 ReadFormation（第 962、964 行）— exact

- 0x0046B210 BattleFile::BattleFile：+0x2718 起 0x7B dword 清零，+0 记投影器；30 项 (i, 5i/4)（有符号除，结果非负）写 +0x3C24 起每 8 字节。
- 0x0046B2E0 ReadFormation：只读打开（失败返回 0），读 0x2714 字节文件头到 +4；计数 = (i8)头[0x1F44+战斗]，≤ 0 或 (i8)选择 > 计数时关闭返回 0；从 1 到 战斗−1 累加 (i8)计数；记录号 = 头[4 + 4×(累加+选择)]，定位到 记录号×0x1EC + 0x2714 读 0x1EC 字节到阵式，关闭返回 1。用现有 /mnt/e/Game/swd3e/battle.ffd（1034004 字节）逐战斗逐选择核对：全部记录号与记录在文件内，最大表索引 256。计数表外与文件不足的情况新增偏差行。

## BattleKeys（第 965 行）— exact（修正后）

- 0x0046B3D0 BattleKeys：先清 0x00577078。result < 2（有符号）、currentUnit 非 0、无对话窗（0x004C66F8）、0x00577999／9A／9B 三字节为 0、0x004BF58C ≠ currentUnit 时：依次检查 DIK 2..0xA 与小键盘 0x47,0x48,0x49,0x4B,0x4C,0x4D,0x4F,0x50,0x51；命令 5（防御）在 menuCommand ≠ 5 时 result 0→1、menuGrow 6、0x005772B4 1、0x005772B8 0、menuCommand 5、ConfirmKey(0)，之后 result 0、0x005772B4 0、menuCommand 1 并返回；其余命令 result 0→1、BuildCommandMenu（0x00472D50），commandEnabled（0x005378EC 起字节）为 1 时同样设置并 ConfirmKey(0)，返回。G 键（按下且 KeyRepeats：1 或 ≥15 且 %3==1）：0x00577204 字非 0、hitsAllRoles 1、0x00577260 1、result 0x63／0x64／3、有对话窗时返回；被抓（0x00495C10）或 敌人数−(u8)enemiesGone == 1 且 0x004BF1CA ≠ 0xFFFF 时返回；MarkBit11Clear 为 0 或 panelFlags 位 0x200 时 Sleep(20)、「無法撤退!!」(0x118,0xA,5,0x40000002)、音 0x8C 并返回；否则 Sleep(50) 后撤退的各项赋值（0x0057725C 1、turnUnit、result 0、0x004BF1A0 0、aimTarget = currentUnit−0x13、0x00577204 |= 0x4000、currentUnit 0、0x005772B4 0、0x005772BC／C0 1、0x0057706C 0、menuGrow 0、listRows 4），之后的 turnTaken 下标取新的 currentUnit（0）。
- 之后：0x00577328 为 1 且空格按住 1 帧时 0x00577080 |= 1；右 Ctrl 转为 Esc；右 Shift（result > 1 返回）选上一角色并在 result 1 时 ConfirmKey，anyStateRole 0xFFFF 后返回；A 在 result 1、无对话窗、0x0057722A 位 0x80 时开启自动战斗（0x005772B8 0、autoBattle 1、turnCommand 1、turnTaken[单位] 1、0x005772B4 1、menuCommand 1、result 3）并返回；右键转为 Esc；左键（KeyRepeats）有面板角色时 SelectRoleUnit 后同右 Shift 的收尾返回，否则按 hoverScrollPart 1..4 调 MenuUp／MenuDown／ScrollListBack／ScrollListForward 并记 0x00577078，指针 y 在滑块范围内（无符号）且按下第 1 帧时记 0x0057703C／0x00577040，0x005772FC 为 1 时 0x00577078 = 5，再转为空格。Enter 在 帧%3==1 时转为空格。确认（空格 1 帧或 ≥15 且 %6==1）：0x00577088 位 31、位 30 各自置位 0；hitsAllRoles 1 时取消被打角色的伤害数字；0x005772B8 为 1 时 hoverRegion 非空、否则点击区非空则释放点击区；左键未按时按 menuCommand 6／0x50 与 0x005772B8 分别处理 0x005772B4、promptFrames 0x12C 与 hoverScrollPart；清 panelFlags 位 0x20，ConfirmKey(0)。上下左右、Page Up／Down、Esc 的处理与源码一致。
- 修正：右键的选项回绕原程序以无符号比较（0x0046BEEC `jbe`），源码改为 u32 比较。继承自正传的偏差“无当前单位时按 G 读角色 [−8]”在外传不成立（G 键在 currentUnit 非 0 的条件内，0x0046B400），已删除该行。

## MouseHover（第 966 行）— exact（修正后）

- 0x0046BFB0 MouseHover：指针与 0x00513428／0x0051342C 相同且 0x005772B8 为 0 时返回 0；不同时 0x005772B8 1、promptFrames 0。记下指针；点击区非空时 Find(&hoverRegion)，hoverRegion 非空则 talk.choice = 结果。按 result 跳表（0..0x50，字节表 0x0046D128）：
  - 0：(u8)enemiesGone ≥ 敌人数时返回 0；y ∈ (0x182, 0x1E0) 时第一个 x ∈ (PortraitX[partyRoles[i]]−0x18, +0x74) 的角色 i 记 anyStateRole = i+0x14 返回 1，否则 anyStateRole 0xFFFF 返回 0。
  - 1：currentUnit 0 返回 0；鬼令（0x00577999）x (0x158,0x1B0)、行 0x8A 起 0x18 到 0x14A，记 0x004BF0F8；炼妖选项（0x0057799A）x (0x180,0x1AC)、行 0xE4..0x114，记 0x00577990；五行（0x0057799B）y (0x112,0x12C)、格 0x11C 起 0x18 到 0x194，记 0x0057798C；蓄劲角色 y (menuY+0x2C, +0x46)、两格 menuX+0x6A 起 0x30，记 menuCommand：命中时变化则音 0x2E，0x005772B4 1，返回 0（未命中 0x005772B4 0）。菜单格 x (menuX+0xA, +0xAC)、y (menuY+0x28, +0x70)：序号 = (y−…)/0x18×3 + (x−…)/0x36（无符号除），< listedCommands+5 时：未启用 0x005772B4 0 返回 0；否则变化时音 0x2E，menuCommand = 序号+1（上限 listedCommands+5），0x005772B8／B4 1，返回 1。其余 0x005772B4 0、anyStateRole 0xFFFF，面板检测取最后一个命中者，返回 0。
  - 2：hoverPageTab −1，y (0x82,0xA0) 的三个页签（0x10A 起 0x2A 到 0x188）记页签并 0x005772B8 1；列表 x (0xE0,0x180) 行 0xBE 起 0x14 到 0x14A，listScroll+行+1 ≤ (i8)listEntries 时记 pageListRow（变化音 0x2E）、hoverScrollPart 0、0x005772B4／B8 1 返回 1；否则滚动条 x (0x192,0x1A4)：上箭头 y (0x9E,0xAE) 1、下箭头 (0x12A,0x13A) 2、轨道上段 (0x0050C91C, 0x0050C918) 3、下段 (0x0050C3D4, 0x0050C3D8) 4（0x005772B4 1，返回 1），否则 hoverScrollPart、0x00577078、0x005772B4 清零返回 0。
  - 4：hoverRoleTab −1，四个页签（到 0x1B2），与 listRole 不同才记；列表 x (0xE0,0x19C)，roleListScroll+行+1 ≤ (u16)roleListEntries 时记 0x004BF0EC；否则 0x005772B4 0，左键按住返回 0，清 0x00577078／0x0057703C／0x00577040／0x005772FC，条目 ≤ 7 返回 0，滚动条 x (0x19E,0x1B0)、下箭头 (0x12E,0x13E)。
  - 3：HoverTarget（0x0046CC94 起内联）；9：HoverGhostTarget（0x0046CB22 起内联，命中另记 aiTarget 0x004BF0E4）；0x1E：HoverRefineCell（0x0046C0E2 起内联：列左界恒为 0xEA、右界 0x15A 起 0x70 到 0x23A，行 0xBA 起 0x14 到 0x11E，格 = 5×列+行+1 上限 10，记 0x004BF0EC，未命中另置 anyStateRole 0xFFFF）。三者与源码逐项一致（采样 8×8、敌人步长 2、角色步长 1、镜像换算、0x00537480 头像高亮、PortraitX 命中 [左−0xA, 左+0x96]）。
  - 5、0xA：x (0xC4,0x178)、两行 0xEC 起 0x16，记 guardCursor；8：x (0xE0,0x199)、行 0xBA 起 0x18 到 0x162 且 行+1 ≤ (i8)specialCount，记 specialRow；0x1B、0x29、0x50：列表 x (0xE0,0x194) 记 0x004BF0EC，离开处理同 4，滚动条 x (0x192,0x1A4)、下箭头 (0x12E,0x140)。其余返回 0。
- 修正：面板检测按战斗角色序号读 partyRoles 与 PortraitX，带符鬼或护驾（roleCount > 4）时原程序读到其后的 RoleActs、frameMilliseconds 与 NameX（符鬼取 PortraitX[1] = 0x5D 的面板），源码原为 4 项 std::array 越界取值。新增 BattleState::PartyRoleRead／PortraitXRead 按原布局读取，NameX、RoleActs 移入 BattleState，单元测试与 exe 字节比对；更远的读取登记偏差。roleListRow 即 0x004BF0EC（第 4、0x1B、0x1E、0x29、0x50 状态共用）。
- 待核（partyRoles 按战斗角色序号读取）：以下源码位置以战斗角色序号直接取 4 项的 partyRoles，序号 ≥ 4（符鬼、护驾）时原程序读到 RoleActs／frameMilliseconds：smooth_turn.cpp DrawChargePanel 段（currentUnit−0x14，蓄劲角色）、经验结算段（已跳过自动／客／鬼角色）、DrawCommandPanelAdvance（nameIndex）、battle_action.cpp AddSpecialPoints／AddSpellPoints（role）、battle_turn.cpp BuildCommandMenu（index）与学会绝招段（index）。验证到各函数时确认调用者能否给出 ≥ 4 的序号，能则改用 BattleState::PartyRoleRead。

## MenuUp、MenuDown（第 967–968 行）— exact（修正后）

- 0x0046D180 MenuUp／0x0046DB70 MenuDown：先清 0x005772B8；按 result 跳表（字节表 0x0046DB1C／0x0046E5D4）：
  - 1：音 0x2E；鬼令 0x004BF0F8 循环 1..8、炼妖选项 0x00577990 循环 1..2、五行打开时不动、menuCommand < 0（上）置 9／> 9（下）置 1、蓄劲角色不动；上：命令 ≤ 3 时 +6，超过 listedCommands+5 再 −3，仍超则 −3（0x005772B4 1）；下：反复 +3（> 9 减 9），> 5 且 0x0050D864+2×命令（MenuKind）为 0 时继续，0x005772B4 1。
  - 2（pageListRow／listScroll，(i8)listEntries）、4（0x004BF0EC／roleListScroll，(u16)roleListEntries，并按 listRole 写 0x0050F3D8／0x005394B4）、0x1B、0x29／0x50（另设 menuSlide 0x50）：光标在 1..7 内移动，越界时滚动并限位，只在实际移动时出音（第 4 状态上移总出音），0x005772B4 1。
  - 3：0x00577284 为 1 时不动；音 0x2E；pendingOrders[currentUnit−0x14].unk00 为 0 时按 enemyOrder（0x005368D4，orderStep 0x004BF0E0 循环）跳过倒下的敌人，记 actingEnemy、取影子框、清其他敌人标记并标记该敌人、0x005772B4／0x005777F0 1、aimTarget = 敌人+1；否则 menuCommand 0x29 时（上）从 roleCount−2 向前／（下）从 aimTarget 向后找未倒下的自动控制角色；可选角色数 = roleCount − 0x005770D4 − guestCount − escortCount（无符号）< 3 时 aimTarget 循环 1..roleCount，否则 orderStep 循环 1..(roleCount − guestCount − escortCount) 取 RoleCycle（0x004BF578），都跳过自动、客、防御（0x00491D40）与鬼角色，取影子框（RoleCycle 时 actingEnemy 清零）；之后 4 个角色清标记与头像高亮（0x00537480），标记 aimTarget 角色，0x005772B4／0x005777F0 1。
  - 5、0xA：guardCursor 循环 1..2；7：0x00577014／0x00577018 清零、controlCursor 循环 1..controlRows（0x004BF0FC）；8：specialCount 为 0 时不动，specialRow 循环 1..(i8)specialCount；0x1E：0x004BF0EC 循环 1..10；9：同第 3 状态的敌人步进，另记 aiTarget（0x004BF0E4）并把鼠标移到影子框中心（x + 宽>>1, y + 高>>1）。
- 修正：可选角色数在原程序以无符号比较（0x0046D4D7、0x0046DED4 `jb`），共用的 AimStep 改为 u32 比较。roleListRows／roleListScrolls 注释改为外传地址 0x0050F3D8／0x005394B4。

## ScrollListBack、ScrollListForward（第 969–970 行）— exact

- 0x0046E630 ScrollListBack：清 0x005772B8；result 2（pageListRow／listScroll）、4、0x1B、0x29／0x50（0x004BF0EC／roleListScroll；0x29／0x50 另设 menuSlide 0x50）各自出音 0x2E；0x00577078 为 0 且光标不在 1 时光标置 1 返回；否则滚动 −7（负则 0），0x005772B4 1；第 4 状态另按 listRole 写 0x0050F3D8／0x005394B4。
- 0x0046E820 ScrollListForward：清 0x005772B8；result 2：出音，(i8)listEntries < 7 返回；0x00577078 为 0 且 pageListRow ≠ 7 时置 7 返回；否则 listScroll +7，超过 条目−7 取 条目−7，为负时置 0 且 pageListRow = 条目；0x005772B4 1。4、0x1B、0x29／0x50（另设 menuSlide 0x50）：出音；0x00577078 为 0 且 0x004BF0EC ≠ 7 时置 min(7, (u16)条目)（无符号比较）、0x005772B4 1 并按 listRole 写行与滚动返回；否则 roleListScroll 同样 +7 限位、为负置 0 且行 = 条目，0x005772B4 1，仅第 4 状态写回行与滚动。

## MenuCancel（第 971 行）— exact

- 0x0046EAC0 MenuCancel：0x005772B8 0、0x005772B4 1。自动战斗（0x0057731C 为 1）时清 commandNames（0x0050EBF0 四个 dword）、0x005772B4 0、0x00577258 1、0x005772C0／BC 0、autoBattle 0、result 0、autoTarget（0x004BF0F4）−1、listedCommands 0，敌人 [aimTarget−1] 取消标记；currentUnit 非 0 时 result 1、0x005772C0／BC 1；返回（不收面板）。否则按 result（字节表 0x0046F0A8）：1 关闭鬼令（menuCommand 7、ghostOrderRow 取符鬼 0x004864F0）、炼妖选项（6）、五行（elementRow 1、menuCommand = listReturnCommand 0x00576FB0）；2 回菜单、pageListRow 1、清列表名（0x005774D4 五个 dword）与 0x005772C4／0x005772AC、0x005777F0 1、listRows 5、CancelCommand；3：talkRole（0x004BF1D2）≠ 0xFFFF 时清 0x00577014／18 并 result 7，否则 result 1、listRows 5、0x005772AC 0；turnPointer 1、0x00577330 0、CancelCommand、全部敌人与 4 个角色取消标记、头像高亮清零；menuCommand 2／3／0x1B／0x1E／4 分别回到状态 2（另清 0x00577290）／4／0x1B／0x1E／8，并清 0x00577284、0x005772F0、0x005772D0、0x0050D88C[角色]、面板帧、pendingOrders[角色].unk08／unk00 后返回；其他命令 menuCommand = listReturnCommand，炼妖选项字节有 0x80 时收面板、炼妖选项 1、result 0x50、menuCommand 6 返回。4 回菜单、roleListScroll 0、listRows 5、0x005772C4／AC 0、CancelCommand；5 回状态 4 并 CancelCommand；7 controlCursor 1、menuCommand 1、result 0、收面板、controlRows 2 返回；8 回菜单、0x004BF0EC 与 specialRow 1，同 2 的清理；9 打开鬼令、result 1、敌人 [aimTarget−1] 取消标记；0x1B、0x1E、0x29 回菜单、menuCommand = listReturnCommand、清列表名等、0x004BF0EC 1（0x1B 另清 roleListScroll）、CancelCommand，0x29 另置 0x005772D0 1 并清 pendingOrders[角色].unk00；0xA 回状态 0x29 并 CancelCommand；0x50 回菜单、炼妖选项 1、menuCommand 6 返回。最后 result ≠ 1 时收面板（0x0057706C／0x00577070 清零）。listReturnCommand 注释改为外传地址。

## ConfirmKey、SelectRoleUnit、CanSelectRole、MenuRight、MenuLeft（第 972、975–978 行）— exact

- 0x0046F100 ConfirmKey：0x00577204 字非 0、0x00577328 为 1、endFade 为 1 或 0x00577088 为负时返回；0x00577A50 为 0、敌人数−(u8)enemiesGone ≤ 1（有符号）、0x004BF1CA ≠ 0xFFFF 且 result ≠ 0x63 时 result 0 返回；result 0x6E 时 levelUpFrames（0x005770CE，无符号）< 0x1E 置 0x1D、== 0x1E 置 0x64 后返回；有对话窗时点击区为空则 talk.advance 1，否则 Find(&hoverRegion) 后按是否命中置 1／0，返回；0x00577258 ≠ 1、currentUnit 0、或 result ≠ 0 且 anyStateRole 0xFFFF 时调 MenuConfirm；否则 anyStateRole 0xFFFF，角色被抓时返回，音 0x2D、0x005772B4／0x005777F0 1、listRows 5、取位置到 0x00577240／44；controlledEnemy（0x004BF1D0）0xFFFF 或 (i16)talkRole ≠ 角色时 result 1、menuCommand 1、BuildCommandMenu；否则 result 7、controlRows 2，该敌人 3 个物品名（0x00489180，以 0x00577014 传入并写回）与 2 个技能名（0x00489260）成功一项各加一行，最后 0x00577014 清零。名字写入共享缓冲 0x00577494，其余读者（0x00475940、0x0045BEA0、0x0045EE20）都先清零再写，源码用局部字符串不影响结果。controlledEnemy 注释改为外传地址。
- 0x0046F480 SelectRoleUnit：result ≠ 0 且 anyStateRole 0xFFFF 时返回；对 roleOrder（0x005368AC）前 roleCount−1 项找到等于该单位且未被抓的项，与 currentUnit 交换后停止；清 commandNames 与 listedCommands。0x0046F530 CanSelectRole：roleOrder 前 roleCount 项中第一个等于该单位者返回 未被抓 ? 1 : 0，找不到返回 0。
- 0x0046F590 MenuRight／0x0046F760 MenuLeft：清 0x005772B8；result 1：鬼令或炼妖选项打开时不动；五行 elementRow 循环 1..5（无音）；menuCommand 7 且 listedCommands 2 时不动；蓄劲角色 menuCommand 循环 1..2；否则 ±1，右移余 3 为 1 时 −3、左移余 3 为 0 时 +3（有符号余数），> 5 且种类字（0x0050D864+2×命令，MenuKind）为 0 时继续同向步进（此后不再要求 > 5，可读到命令 4、5 的字）；出音 0x2E。result 2：listPage（0x00576FEC）循环 0..2、pageListRow 1，右移另清 listScroll，出音；4：listRole 循环 0..3，取回该角色的行与滚动，出音；0x1E：0x004BF0EC ±5 限于 1..10，出音。

## MenuConfirm（第 979 行）— exact（修正后）

- 0x0046F920 MenuConfirm：0x00577258 ≠ 1 时返回；清 0x005772AC；按 result 跳表（字节表 0x00471D14，200 项）分派，源码拆为 14 个 Confirm* 辅助函数，逐段与内联汇编对照：
  - 1（ConfirmCommand，0x0046F965..0x004701C9）：0x005772B4 为 0 或 menuGrow < 6 返回；鬼令（0x00576FBC 清零，第 8 行 result 9，否则 SetGhostOrder，关鬼令、menuCommand 7）；炼妖选项第 1 行 result 3 并置 0x80 后继续、第 2 行 result 0x50 置 0x80 返回；五行（SetElement、turnCommand／turnTaken 0x26、SetTarget(1)、0x0057725C 1、turnUnit、aimTarget、清 currentUnit 与 commandNames、pendingOrders.unk00 1、result 0、关五行、0x005772B4 0、listedCommands 0）；启用字节 0x005378EB[命令] 为 0 时（蓄劲角色仅命令 1）出拒绝音 0x8C；之后清 0x005772B4／0x00577284／0x005772F0、0x005777F0 1、listReturnCommand = 命令，命令 ≥ 6 且种类字非 0 时换成种类；蓄劲角色命令 1 在 0x0049C2D0 为 0 时公告「法寶經驗不足,無法繼續蓄勁!!」(0x118,0xA,0x14) 与拒绝音，否则改 0x27；命令 2 改 1。按命令 1..0x29 跳表（41 项）：1 result 3、actingEnemy；2／3／4 打开列表（SetCursorPos(0x108,0xB0)、各自清行与滚动、result 2／4／8、SetMousePosition(0xF0,0xC6)）；5 防御结束选择；6 炼妖选项（滑入 0x50）；7 鬼令（取 GhostOrder，滑入 0x64）；0x11 离开（result 6）；0x15／0x17／0x1D／0x1F／0x21／0x25 result 3；0x16／0x18 另置 0x005772B4／0x00577284；0x19 另置 0x00576F9C 位 0x10；0x1A 另清 0x00577080；0x1B／0x1E／0x29 打开对应列表；0x1C／0x20／0x22／0x23／0x24 立即行动；0x26 打开五行（滑入 0x32）；0x27 蓄劲（chargingRole）；0x28 逆攻（meterShown 时 result 3，否则立即行动并记 meterRole）。收尾：0x005772B8 为 0 且 result 3 时 NextEnemyTurn；PressConfirm；menuCommand 非 7／6 且五行未开时 listRows 4 并收面板。
  - 2（ConfirmSpell）、8（ConfirmSpecial）、0x1E（ConfirmRefine）、0x1B／0x50／0x29（ConfirmThrow／ConfirmBait／ConfirmFuse，SelectObject 类别 4／6／3）、4（ConfirmItem，含 CallEscort 与护驾放置）、0xA（ConfirmFusePlace）、5（ConfirmEscort）、7（ConfirmTalk）、9（ConfirmGhostTarget）、3（ConfirmTarget）：逐项一致（字符串「體力點數不夠!!」「沒有護駕,不能融合!!」「已融合過,不能再選!!」「法寶經驗不足,無法融合!!」与 exe 字节一致；护驾位置 (0x177,0xC8)／(0x190,0x15E)，镜像时 0x280−x；0x004818E0 的第二个参数未被读取）。
  - 结束阶段（ConfirmEnd）：0x62、0x64..0x68、0x6E..0x71、0xC8 与源码逐项对照。
- 修正：(1) 状态 2、4 的“队伍角色数 ≥ 4”为无符号比较（0x00470461、0x00470D0E），AimAtParty 改为 u32 比较，ConfirmItem 的内联副本改为调用 AimAtParty；(2) 状态 0x67 计满 0x14 帧与状态 0x68 都先 Switch(4,1,0) 停音乐（0x00471B05、0x00471B33），源码补上；(3) 学会技能的姿势为 4（0x00471B93，另见 BattleEndFrame 0x00475FAC、0x0047606C），kLearnedPose 由正传的 8 改为 4。victoryWithoutPanel 注释改为 0x00577308，4 处 “inline in 0x00462740”（正传地址）改为 MenuConfirm 0x0046F920；三条偏差的依据改为外传地址。

## DrawBattleMenu（第 980 行）— exact（修正后）

- 0x00471E80 DrawBattleMenu：result 0x67 或（currentUnit 0 且非自动战斗）时返回；currentUnit 非 0 时 StatusBit5（0x0049A950）为 1 则 RandomRoleAct 并清 currentUnit，否则 result 0→1；0x00577258 1；按 result 跳表（字节表 0x00472954）：
  - 1：无当前单位返回；蓄劲角色效果位 0x200 时清 chargingRole，否则 DrawChargePanel(menuX+0x32, menuY, menuCommand)、menuGrow 6 返回；命令面板（DrawCommandPanel，内联 0x00471F7A..0x00472120）：menuGrow ≠ 6 时 0x005772AC 0→1、菜单回到 (0xF0,0x8C)；menuGrow 限于 6、listRows 不小于 0；listedCommands ≥ 2（无符号）6 层否则 4 层的面板（0x00458BB0）；未长满时 menuGrow 加一、listRows 减一返回；长满后阴影 0、背景 0xFFFE，角色名（0x0050ECB4 + 32×partyRoles[角色]）居中 x = menuX + 8×(10 − (长度>>1 − 2))、y = menuY+0xE、色 0xFFC0、样式 0x10；menuCommand > 9 置 1；DrawCommandMenu(menuX+0xE, menuY+0x28, menuCommand)；头像 0x00458240(0x004BF11C[currentUnit]（= partyRoles[角色]）, menuX+0x1E, menuY+0x21)。
  - 2：0x005772C4 1、DrawListTabs(0xE0,0x7E)；menuGrow 7 且面板帧 0xA 时 DrawSpellList(0xE8,0x86,行,滚动)，scrollCount = (i8)条目、scrollFirst = 滚动，条目 > 7 时滚动条 0x232A 于 (0x190,0x9E) 4 行；4、8、0x1B、0x1E、0x29、0x50 各画列表（0x1B／0x50 另以 (u16)条目画 0x192 处滚动条；0x29／0x50 标题「融合」0x004BF200／「誘餌」0x004BF604，类别 3／6；0x00474AF0 的第 4 个参数未读取）；5／0xA 护驾列表 (0xC4,0xCE)；6 离开（0x00577260、hitsAllRoles 为 0 时 0x00577204 |= 0x4000、result 0、currentUnit 0）；7 控制菜单 (menuX+0xC, menuY+8)；以上各态置 0x005772BC／0x005772C0 1。
  - 3（DrawAimPointers，内联）与 9（DrawGhostAimPointer，内联）：置 0x005772C0／0x005772BC 1、字宽 0x10；全体目标时按 pendingOrders.unk00 给可选角色或站着的敌人逐个画指针 0x238F（帧 = 序号，敌人有偏移时画在偏移处）；单体时倒下的敌人沿 enemyOrder 换目标（试满敌人数则 result 1），aimTarget −1 返回，0x005772B8 为 0 时取影子框与偏移（角色另加 0xA）、标记，炼妖／诱饵对非小机关敌人 turnPointer 0，画 0x2393／0x238F；0x005772B4 为 1 时 DrawTargetTag(0xC,0xE)。第 9 状态从 0 起找第一个未倒下的敌人，另记 aiTarget。
- 修正：(1) 第 3 状态进入时置 0x005772C0／0x005772BC（0x0047233C、0x00472342），源码 DrawAimPointers 补上；(2) 命令面板角色名居中常数为 10（0x00472081），命令格 y 偏移 0x28（0x004720C6），smooth_turn.cpp 的 kMenuNameCenter（原 5）、kCommandMenuY（原 0x2A）为正传值，已改。

## RandomRoleAct（第 981 行）— exact（修正后）

- 0x004729B0 RandomRoleAct：取 Random(3)，panelFlags 位 0x200 时另取 Random(2) 代替；0 防御（menuCommand 5、menuGrow 6、result 1、0x005772B4 1、MenuConfirm）；2 离开、1 攻击：turnUnit、turnCommand（0x11／1）、result 0、turnTaken、0x0057725C 1、SetTarget(1)、0x005772C0／0x005772BC 1；其他值不动。攻击且 (u8)enemiesGone < 敌人数时取 Random(2)；roleCount − (u8)defeatedRoles − 0x005770D4 − escortCount（无符号）> 1 且该数非 0 时 pendingOrders[turnUnit].unk00 1、aimTarget = Random(roleCount)+1、RaiseUnk26B8Bit28，反复重取直到目标未倒下、非鬼、非自己；否则 aimTarget = Random(敌人数)+1，倒下时循环后移（最多敌人数次）。
- 修正：可选角色数在原程序以无符号比较（0x00472AEA `jbe`），源码原为 i32。

## BuildCommandMenu、NextEnemyTurn、NextRoleTurn（第 985–987 行）— exact（NextRoleTurn 修正后）

- 0x00472D50 BuildCommandMenu：启用表 0x005378EC 前 5 项 1、后 4 项 0（第 9 项即 hitDamage 低字节，偏差已登记），清 commandNames、commandKinds（0x0050D870 两个 dword）、listedCommands；角色 0 且 inParty[0]（0x00536EDC）为 1 时第一项「煉妖」（0x004BF614）种类 6，游戏旗标 0x5A 时第二项「符鬼」（0x004B12B4）种类 7，再取队伍角色 0 的两个饰品槽（0x004E4CCC = equipment 0x004E4CB0 + 7×4）；否则取 partyRoles[角色] 的两个饰品槽。种类在 0x15..0x31（无符号）时列出（名字 0x004BF01C[种类]），0x28 在 meterShown 为 1 时名「釋放」（0x004BF60C），0x29 无护驾时停用，0x27 在 AccessoryProficiency(0x27)（u16）< 10 时停用。meterShown 为 1、meterRole 为当前单位且 AccessoryProficiency(0x28) ≠ 100 时停用奇術；MarkBit8Clear 为 0 停用奇術；HasBlock30 为 0 停用絕技；CanUseAccessory 为 0 的列出项停用；MarkBit4 为 1 时只留攻擊与防禦。
- 0x00473150 NextEnemyTurn：actingEnemy ≥ 敌人数（有符号）时置 0；倒下时沿 enemyOrder 后移（试满敌人数则 result 1）；标记该敌人、0x005772B4 1、aimTarget = actingEnemy+1，取影子框画 0x2393 于中心。
- 0x00473270 NextRoleTurn：roles = roleCount − escortCount − guestCount；从 orderStep 起循环 1..roles 读 0x004BF578[at]，等于当前角色时记 orderStep、aimTarget = 角色+1、actingEnemy 0、0x005772B4 1。
- 修正：带符鬼时 roles 为 5，原程序读到 RoleCycle 之后的 chargingRole（0x004BF58C）并继续绕回，源码原在 at ≥ 5 时 Hang（会在原程序正常的情况下卡死）。新增 BattleState::RoleCycleRead 按原布局读取（之后依次 chargingRole、meterRole、0x004BF594 的常量 1，与 exe 字节比对），NextRoleTurn 改为读满一圈仍找不到才 Hang；新增偏差行与单元测试。

## MenuUp、MenuDown 复核（第 967–968 行，第一遍内）— exact

- 因 AimStep 改用 BattleState::RoleCycleRead，两函数退回待验证后在本遍复核：RoleCycle 路径读 0x004BF578[orderStep]（0x0046D4FA、0x0046D5D8、0x0046DEF7），以读到的值直接作角色下标检查自动／客／防御／鬼；上移时 orderStep 可大于 cycle（只检查下界），读到 chargingRole、meterRole 与 0x004BF594（常量 1，即 1 号角色，可被选中），源码现按原布局取值，字符串字节视为不可选。其余部分与前次核对一致。继承自正传的三条偏差（RoleCycle 值减一、菜单指令减到 0、越过表时 Hang）在外传不成立，已删除并并入 0x0046D180／0x0046DB70 的 RoleCycle 行。

## DrawCommandMenu（第 988 行）— exact

- 0x004732D0 DrawCommandMenu：currentUnit 0 时返回；大字体阴影 0、背景 0xFFFE；角色 +8 指向的块首 dword 为 0x30 且 MarkBit1（0x00498CC0）为 0 时启用表第 4 项置 1；BuildCommandMenu。5 个固定命令（0x004BF0C4 指针表）按 (序号%3)×0x30、(序号/3)×0x18 排在 (x,y) 起，启用色 0x0050EC74、停用色 0x00537424，样式 4；序号+1 等于光标时背景 0xF000 以样式 0x10 在 (−1,−1) 处再画并恢复背景。4 个列出命令名非空时：CanUseAccessory(种类) 为 1 且启用字节为 1 时亮色，否则启用字节清零并暗色，同样排位与高亮。鬼令打开时 menuCommand 7、盖板（0x00440980 于 (x−4, y−6) 宽 0x94、高 listedCommands ≥ 2 ? 0x54 : 0x38）、高面板 (0x154,0x6E)，8 个鬼令名（0x004BF630）于 (左+4, 0x71+0x18k+1)、选中行高亮于 (左+3, 0x71+0x18k)；炼妖选项同样（面板 (0x17C,0xC8)，2 项 0x004BF650，起始 0xCB）；五行打开时盖板、宽面板 (x,0x10E) 成功则 5 个名字（0x004BF658）于 (0x103+0x18k+1, 0x112)、高亮 (0x103+0x18k, 0x111)。最后背景 0xFFFE。名字表与 exe 字节一致。

## DrawListTabs、DrawSpellList（第 989–990 行）— exact

- 0x00473880 DrawListTabs：页签 0x2394 帧 2、1、0 依次画在 (x+0x2A×帧, y+0x20)，当前页 listPage 以帧 listPage+4 再画；中字体背景 0xF000 后又 0xFFFE；panelAct（0x0050AF70，0x29 dword）清零、listPanelFrames 0xA、id 0x2396、action 0 后 Run；Box(x, y+0x24, 0xBE, 0x98, 0,3,3, 模式 2)；WindowFrame(panelAct.frame, x+6, y+0x28, x+0xBA, y+0xB8, 0, 0x80000008)；menuGrow +2 限于 7。
- 0x004739B0 DrawSpellList：currentUnit 0 返回；中字体阴影 0、背景 0xFFFE、字宽 0x10；listEntries 0 后 CountSkills(listPage, 0, &listEntries)、ShowAllSkills；从 top+1 起逐项 ListSkill(listPage, 0, 序号, 0x005774D4, &消耗)，消耗 0xFFFF 时 ShowAllSkills 结束；消耗位 0x8000 时取体力并去掉该位，否则取灵力；图标 (i16) 1..5 画 0x23AC 帧 图标−1 于 (x−6, y+20×行+0x21)，> 5 于 (x−4, y+20×行+0x25)；现有值（i16）< 消耗时暗色否则亮色，名字于 (x+0xE, y+20×行+0x25)，"%3d" 消耗于 x+0x90，样式 0x10；行+1 等于光标时在 (x+0xD, …−1) 再画名字、Box(x−8, …+0x21, 0xC0, 0x18, 0x1F,0x14,0, 模式 5)、底部 Box(0,0x15E,0x280,0x28, 0,5,0x1F, 模式 5)、说明（0x00441200 取角色 +0x3450 的技能到 0x0050AC3C）居中 x = 0x140 − ((长度>>1)×16>>1)、y 0x168、色 0xFFFF，背景 0xFFFE、0x005772B4 1、specialCursor = 序号；满 7 行（无符号字比较）时结束。

## DrawItemList、DrawThrowList、DrawTitledItemList（第 991–993 行）— exact

- 0x00473E10 DrawItemList：currentUnit 0 返回；4 个页签（0x2394，x+0x7E 起每 0x2A 递减，y+0x20），当前 listRole 以帧 +4 再画；背景 0xF000／0xFFFE；panelAct 清零、id 0x2396、action 0、Run（不设 listPanelFrames）；Box(x, y+0x24, 0xCC, 0x98, 0,4,4, 模式 2)；WindowFrame(…, x+6, y+0x28, x+0xC8, y+0xB8, 0, 0x80000008)；阴影 0、背景 0xFFFE、字宽 0x10；roleListEntries 0 后 CountObjects(listRole)、ShowAllItems；scrollCount = (u16)条目、scrollFirst = roleListScroll，条目 > 7 时滚动条 0x232A 于 (0x19E,0x9E) 4 行；从 top+1 起 ListObject(listRole, 序号, 名, &旗标, &数量)，为 0 时 ShowAllItems 结束；旗标 0x8000 时显示："%2d" 数量，旗标 0x4000 暗色否则亮色，0x2000 时先画「+」于 x+6，再画数量 x+0x90、名字 x+0x10（y+20×行+0x2C，样式 0x10）；行+1 等于光标时名字 (x+0xF, …+0x2B)、背景、Box(x, y+20×行+0x28, 0xCE, 0x18, 0x1F,0x14,0, 模式 5)、listCursor = 序号、0x005772B4 1，listRole 3 时 DrawObjectStats(0xA4, 0x18×0x004BF0EC+0x7E)，底部说明 Box 与角色 +0x3458 的物品说明居中；显示的才计行，满 7 行（无符号字）结束。
- 0x004743C0 DrawThrowList：panelAct 同上；Box(x, y, 0xBE, 0xBC, 0,−8,−8, 模式 1)；标题框 WindowFrame(x+6, y+8, x+0xBA, y+0x18) 与列表框 (x+6, y+0x28, x+0xBA, y+0xB8)；标题「投擲」（0x004BF270）于 (x+0x50, y+8) 色 0xFFC0；阴影、背景、字宽；CountObjects(4)、ShowAllItems；从 top+1 起 0x00481AB0(0, 序号, 名, &数量)，为 0 时 ShowAllItems 结束，满 7 行结束；名字 x+0x10、"%2d" x+0x90 亮色；光标行高亮、Box 宽 0xC2、listCursor、0x005772B4 1。
- 0x00474710 DrawTitledItemList：同 DrawThrowList，Box 颜色 (0,4,4) 模式 2，标题为参数，类别为参数，滚动条于 x 0x192；ListObject 的旗标不检查；类别 3 时 DrawObjectStats(0x194, …)。

## DrawRefineList、DrawSpecialList、DrawEscortList、DrawEscortFusionList（第 994–997 行）— exact

- 0x00474AF0 DrawRefineList：panelAct 清零、id 0x2396、Run；Box(x, y, 0xF2, 0x9C, 0,−8,−8, 模式 1)；框 (x+6, y+8, x+0xEE, y+0x18) 与 (x+6, y+0x28, x+0xEE, y+0x98)；标题「煉符」（0x004BF258）于 (x+0x6A, y+8)；ThrowableObject(0, 1, 名, &数量) 的数量作 roleListEntries，ShowAllItems，0x00481BC0（只读第二参数）计可用件数，ShowAllItems；listCursor 1；两列各 5 格（x+0x10 起列距 0x70，y+0x2C 起行距 0x14），格号 > 条目时在 条目+件数 内取 ThrowableObject(1, 物, …)（件计满 (i16)数量 时换下一物），之外为「無」（0x004AF630），之内沿用首次取到的名字；光标格先画 Box(列x−4, 行y−3, 0x6A, 0x18, …) 与名字、0x005772B4 1、listCursor = 物（件为 1 时 +1，条目 0 时 −1）；各格画名字；最后 roleListEntries 加件数。
- 0x00474E80 DrawSpecialList：panelAct 不清零，id 0x2396、Run；Box(x, y, 0xBE, 0xAC, 0,−8,−8, 模式 1)；框 (x+6,y+8,x+0xBA,y+0x18)、(x+6,y+0x28,x+0xBA,y+0xA8)；标题「絕技」（0x004BF1F0）；specialCount 0 后 CountSkills(0,1)、ShowAllSkills；从 1 起 ListSkill(0,1,序号, 0x005774D4, &消耗)，0xFFFF 结束（ShowAllSkills），0 跳过；名字于 (x+0x10, y+0x16×行+0x28)；光标行高亮、Box(x, …+0x25, 0xC0, 0x18)、底部说明、specialCursor、0x005772B4 1；满 7 行结束。
- 0x004751D0 DrawEscortList：panelAct id 0x2396、Run；Box(0xC4,0xB0,0xBC,0x58, 0,4,4, 模式 0)；框 (0xC8,0xB4,0x178,0xC4)、标题「佈置護駕」（0x004BF748）于 (0xFC,0xB4)、框 (0xC8,0xD4,0x178,0x104)；光标条 Box(x−2, y+0x16×guardCursor−0x12, 0xBC, 0x18, …)；护驾（最后 escortCount 个角色）倒下时「無」暗色，否则字宽 0x12、白色名字，于 (x+0xC, y+0x16×序+8)；只有一个护驾时第二行「無」于 (x+0xC, y+0x1E)；字宽 0x10。
- 0x004753C0 DrawEscortFusionList：同上，标题「護駕融合」（0x004BF754），已融合（0x004875F0 为 1）暗色，否则白色且序号等于 guardCursor 时 DrawRefineStats(0x172, 0x18×序+0xAA)；光标条在名字之后画。

## DrawTargetTag、DrawControlMenu（第 998–999 行）— exact

- 0x00475680 DrawTargetTag：pendingOrders[currentUnit].unk00 为 1、0x00577284 为 1、aimTarget ≤ 0 或 > 敌人数时返回；名字（0x0049D3B0）长度/2（向零取整）、熟知度 = 0x0048A580(名声表 0x004D3458, ActId)、元素 = FirstResistance；镜像时 left = 0x276 − 20×半长 − x，否则 x；panelAct id 0x2396、Run；Box(left−4, y−4, 20×半长+8, 0x1C, 0,4,4, 模式 2)；熟知度（u16）≥ 10 时按元素色表（0x004BF674，12 字节一项）Box(left−4, y−3, …, 0x1F, 模式 5)；WindowFrame(left, y, 20×半长+left+4, y+0x18)；字宽 0x14 写名字于 (left+2, y+4) 白色，字宽 0x10；≥ 10 时「生命:%d/%d」（0x004BF760）于 (20×半长+left+0xC, y−2)；≥ 0xF 时行动槽长度（u16）非 0 则 FillColor(同 x, y+0x11, 长度, 3, RGB(0x18,0,0))。
- 0x00475940 DrawControlMenu：瓦片框 0x2387 于 (x−8, y−0x28) 4×2、填充 0x48，标题「控制」（0x004BF280）于 (x, y−0x20)；瓦片框 (x−8, y−8) 4×controlRows；阴影 0、背景 0xFFFE，「攻擊」（0x004BE664）于 (x,y)，光标 1 时背景 0xF000 再画；该敌人 3 个物品（共享缓冲 0x00577494 每项先清 24 字节）成功一项各下移 0x14 画名字，光标所在时 0x00577014 = id、0x00577018 = 0 并高亮（ItemName 成功时总写 id，原程序 id 变量不逐项重置也不影响）；2 个技能各以「特殊%d」（0x004BF76C）列出，光标所在时 0x00577014 0、0x00577018 = 序+1；最后「釋放」（0x004BF60C），光标为其序号时高亮；背景 0xFFFE。

## BattleEndFrame（第 1000 行）— exact

- 0x00475CB0 BattleEndFrame：有对话窗或战斗消息（0x00537388）时返回；按 result−0x60 跳表（18 项）：0x60 清 0x005772C0、0x005772B4、result；0x61 第 roleCount 个角色 GlowAct(rolePlaces[roleCount] 的 (i16)x, y) 为 1 时 levelUpRole 1（角色满 10 个时读数组之后，偏差已登记）；0x62 0x005772BC 1、DrawCraftResultPanel；0x63（EndCheckFrame，内联）清 0x00577204、autoBattle 0、0x00577A50 1，wonByTalk 与 0x0057734C 都为 0 时敌人逐个取消标记并数倒下者，不等于敌人数则 result 0 返回；否则角色逐个取消标记、ExpireEffects(1)，result 0x64；0x64 victoryWithoutPanel 时计满 0x14 帧置 endFade，否则 DrawVictoryPanel、清 0x00577260、0x005772BC／C0／0x00577270 置 1、currentUnit 0，计满 0x96 帧 ConfirmKey；0x65 levelUpRole 为 0xFF 时 CheckLevelUps，仍为 0xFF 则 endFrames 0、result 0x70；levelUpPending 为 0 时角色未倒下则 levelUpPending = RestartMainAct(2,0)，仍为 0 返回；之后画升级面板并计帧；0x6E DrawLevelUpStats；0x6F DrawNewSpell 并计帧；0x70 LearnSpecials 后有角色则音 0x160、未倒下时 RestartMainAct(4,1)（学会姿势 4，前次已改），无角色则 result 0x71，否则 DrawNewSpecial 计帧；0x71 AnnounceMastery 同样，无角色时 result 0x66、endFrames 0，lootCount 非 0 时音 0x160，否则 DrawMastery 计帧；0x66 lootCount 0 时 endFade 1，否则计满 0x96 帧 ConfirmKey 并 DrawLootPanel；0x67 置 0x005772BC／C0，panelFlags 位 8 时计满 0x1E 帧置 endFade，否则 DrawDefeatPanel、计满 0x96 帧 ConfirmKey；0x68 置两旗标，计数 > 0x14 时 ConfirmKey。
- 继承自正传的“待煉符的敌人序号为 0 时读敌人 [−1]”在外传不成立（0x62 只调 DrawCraftResultPanel，0x00478330 不读敌人数组），已删除；“0x61 角色满时读数组之后”改为外传依据。

## DrawVictoryPanel（第 1001 行）— exact

- 0x00476200 DrawVictoryPanel：panelAct id 0x2396、Run（不清零）；Box(0xC4,0xB0,0xBC, panelHeight+0x28, 0,4,4, 模式 0)；框 (0xC8,0xB4,0x178,0xC4)、「戰鬥勝利」（0x004BF7A8）于 (0xF8,0xB4)、框 (0xC8,0xD4,0x178,panelHeight+0xD4)。银两字 0x005771FA 位 15 未置时：0x004591E0（ResetMeter）、音乐 Switch(4,1,0)、0x00427080（停所有音效）、音 0x12C；0x005772F8 为 0 时逐个敌人 RollDrop（0x00488D40），非 0 时在 lootIds（0x0050EFD0，10 项）中找到则 AddOneItem(物,1) 并 lootCounts[i]++，否则以 lootCount 为槽 AddOneItem、计数、lootCount++、记名字指针（0x00537970）与物品号（第 11 种起写到表外，偏差已登记）；experienceBonus（0x00577220）非 0 时经验 0x005771FC += 经验×加成/100；moneyTimes（0x0057721E）非 0 时银两字 ×= 倍数（16 位）；逐个非自动、非客、非鬼、未倒下的角色：队伍角色等级（0x004C4F44 字节）< 0x004BF1CE 且无效果位 4 时总经验（0x004C4F18）加本场经验；0x005772F8 为 0 时 0x0047FA00(法宝熟练表 0x004D1F58, 本场法宝经验 0x005771FE) 为 1 则 0x00577A6C 1，战斗块 values[3] ≠ −1 时 values[1]++，敌人数 ≥ 3 再加一；RestartMainAct(1,1)、ExpireEffects(1)。之后 endFrames 0、银两字置位 15、加成 0、队伍银两（0x004C6370）加 (银两 & 0x7FFF)、倍数 0。GrowPanel(0xD4, 0x11C, 2) 为 1 时三行（0x004BF794／0x004BF788／0x004BF774）于 (0xD0, 0xDC／0xF0／0x104) 色 0xFFC0。
- 命名：unk53BF12／unk53BF14／unk53BF16 按用途改为 battleGold（0x005771FA）、battleExperience（0x005771FC）、battleProficiency（0x005771FE）；timingWindow 实为等级上限（0x004BF1CE，开战置 0x3C，0x00476A90、0x004767D0 也读），改为 levelCap；accessoryGained 注释改为 0x00577A6C。偏差依据改为外传地址。

## DrawLevelUpPanel、CheckLevelUps、RaiseLevel（第 1002–1004 行）— exact

- 0x00476650 DrawLevelUpPanel：levelUpRole ≠ 0xFF 或 learnedPending（0x00577300）为 1 时画面板（Box 高 panelHeight+0x28、框、「升級」0x004BF7C0 于 (0x108,0xB4)、正文框）；GrowPanel(0xD4,0xF4,3) 为 1 且 levelUpRole ≠ 0xFF 时以「%s升第%d級」（0x004BF7B4，IDA 注释只显示 "%s"）写 partyRoles[(i8)levelUpRole] 的战斗名（0x0050ECB4+32×）与等级（0x004C4F44+60×）于 (0xD0,0xDC) 色 0xFFC0。
- 0x004767D0 CheckLevelUps：逐个非自动、非客、非鬼角色：等级（u8→u16）≥ levelCap（0x004BF1CE，无符号）时 levelUpRole 0xFF 跳过；LoadLevelEntry(角色+1, 等级+1, &所需) 后总经验（有符号）< 所需同样跳过；否则清升级前／后记录（0x00513460／0x00536AB8，0x3C 字节，只此二函数使用且先清后填，源码用局部），levelUpBefore（0x0050EC78）= 记录，LevelUp(前, 角色+1, 等级, &learnedPending)、LevelUp(后, 角色+1, 等级+1, &learnedPending)；等级与经验标记取后者，生命／灵力／体力上限各加差并回满，8 项属性各加差（16 位），levelUpRole = 序号，StopSound(0x12C)、音 0x12B，结束。最后 0x00577A70 置 1。
- 0x00476A90 RaiseLevel：从当前等级起，等级 < 目标且 < levelCap 时每级同样清前后记录、存 levelUpBefore、两次 LevelUp（学会旗标传原程序未初始化、只写不读的局部），等级与经验标记取后者，总经验加两级标记之差，上限加差回满、属性加差。

## DrawLevelUpStats（第 1005 行）— exact

- 0x00476CB0 DrawLevelUpStats：levelUpRole 0xFF 返回；panelAct id、Run；Box(0xC4,0x4C,0xD4, panelHeight+0x28)；框 (0xC8,0x50,0x190,0x60)、战斗名（partyRoles[(i8)levelUpRole]）于 (0x118,0x50)、正文框 (0xC8,0x70,0x190,panelHeight+0x70)；GrowPanel(0x70,0x10C,2) 为 1 时：7 行旧值（levelUpBefore 的上限三项有符号、属性四项无符号）以 0x004BF81C..0x004BF7D4 的格式画于 (0xD8, 0x78+0x14×行) 白色。levelUpFrames（0x005770CE）为 0x64 时 9 个计数（0x005372F4 三个字、0x005394C4 六个字）清零；为 0x1D 时取 9 项差值；< 0x1E 时加一，仍 < 0x1E 返回。上限三项现值（有符号）大于旧值时画「 ==> 」于 (0x136,y)、"%4d" 现值−(i16)计数 于 (0x158,y) 色 0xF000，前一项计数为 0（首项不限）且本项计数 > 0（有符号）时减一并出音 0x125；属性四项无符号比较、现值−(u16)计数，前一项为 0 且本项非 0 时减一出音。statRolls 注释改为外传两段地址。

## DrawNewSpell、DrawNewSpecial（第 1006–1007 行）— exact

- 0x004776B0 DrawNewSpell／0x00477850 DrawNewSpecial（源码共用 DrawLearnedBox）：learnedPending 为 1 时（绝技另在 panelHeight 为 0 时出音 0x160）以「%s學到新奇術」（0x004BF830）／「%s學到新絕技」（0x004BF840）写 partyRoles[(i8)levelUpRole] 的战斗名，半长 = 长度/2（向零取整）；panelAct id、Run；Box(0xF6, 0xB0, 20×半长+8, panelHeight+8, 0,4,4, 模式 0)；WindowFrame(0xFA, 0xB4, 20×半长+0xFA, panelHeight+0xB4)；GrowPanel(0xB4, 0xEC, 3) 为 1 时文字于 (0x104,0xBC) 白色，「[%s]」（0x004BF828）技能名（0x0057747C）于 x = ((半长×16)>>1) − (长度>>2)×16 + 0xFA、y 0xD2、色 0xF000。注释中的正传地址改为外传。

## LearnSpecials（第 1008 行）— exact

- 0x00477A00 LearnSpecials：逐个非自动、非客、非鬼、未倒下的角色，战斗块（0x004C6700+96×队伍角色）values[3] 为 −1 时跳过；遍历技能链（0x004C2B68[队伍角色] 的头之后），类别（node+0x62）为 0x14 且 values[3] 位 31 未置时载入该物件到 0x00576D40、FreeDescription，values[2] = (u16)counterMax、values[3] = (u16)unk42 | 0x80000000（另有只写不读的局部最大 id）；然后 values[1] < values[2]（有符号）或 values[3] & 0x7FFFFFFF 为 0 时跳过；否则载入 (u16)values[3]，接到链尾一个清零的 0xB4 字节新节点（id、记录），learnedPending 1、技能名复制到 0x0057747C、levelUpRole = 序号，values[1] 0、values[2] = 新的 counterMax、values[3] = 新的 unk42 | 0x80000000（unk42 为 0 时 −1），返回。

## BattleMessages::DrawMastery（第 1009 行）— exact（修正后）

- 0x00477C40 DrawMastery：learnedPending 为 1 时，进化名（0x00577A54）为空则文字 =「法寶%s已完全成長!!」（0x004BF870），否则「法寶[%s]轉化為[%s]!!」（0x004BF858），半长 = 长度/2，宽 = 17×半长+0x20；Box(0xC4, 0xD0, 宽+0xC, panelHeight+8, 0,4,4, 模式 0)、WindowFrame(0xC8, 0xD4, 宽+0xC8, panelHeight+0xD4)；进化时字宽 0x11，依次在 x 0xD8 起画「法寶」（0x004BEB60，0xFFC0）、技能名（0xF000）、「轉化為」（0x004BF850，0xFFC0）、进化名（0xF000），每段后 x 加 18×(段长/2)，y 0xDA，这四次都写进同一个缓冲；GrowPanel(0xD4,0xF4,3) 为 1 时字宽 0x11 画该缓冲于 (0xD8,0xDA) 色 0xFFC0，再恢复字宽 0x10。
- 修正：进化时缓冲最后只剩进化名，原程序长满后画的是进化名（盖在句首），源码原画完整进化句，改为画进化名（原有 BUG 保留）。

## AnnounceMastery（第 1010 行）— exact（修正后）

- 0x00477EE0 AnnounceMastery：清进化名（0x00577A54，24 字节）；逐个非自动、非客、非鬼、未倒下的角色取 0x0047FF60(法宝熟练表)，为 0 跳过；否则载入该物件到 0x00576D40、FreeDescription、learnedPending 1、名字复制到 0x0057747C、levelUpRole = 序号；unk42 为 0 返回；否则在队伍角色的两个饰品槽（0x004E4CCC+64×队伍角色）里找 id（字比较）等于该物件的节点，找到则 id 0xFFDC、count 1、pending 0、workCount 0、记录名首 4 字节清零后写「無」（0x004AF630）、AddOneItem(unk42, 1)、载入 unk42、FreeDescription、进化名 = 其名字，返回；都不等则返回。
- 修正：槽内 id 与物件 id 原程序以 16 位字比较（0x00477FC0），源码原把物件 id 转 i16 后与 u16 id 按 int 比较，id ≥ 0x8000 时结果不同，改为直接比较。

## DrawLootPanel、DrawDefeatPanel、DrawCraftResultPanel、DrawMessageBox、GrowPanel（第 1011–1015 行）— exact

- 0x00478070 DrawLootPanel：底 = lootCount×0x14+0xD4；字宽 0x12；panelAct id、Run；Box(0xC4,0xB0,0xB8,panelHeight+0x28, 0,4,4, 模式 0)；框 (0xC8,0xB4,0x178,0xC4)、「戰利品」（0x004BF890）于 (0x108,0xB4)、正文框 (0xC8,0xD4,0x178,panelHeight+0xD4)；GrowPanel(0xD4, 底, 3) 为 1 且 lootCount 非 0 时逐项 "%-12s X %2d"（名字指针 0x00537970、(u16)数量 0x0053949C）于 (0xD2, 0xD4+0x14×i) 色 0xFFC0；字宽 0x10。
- 0x00478210 DrawDefeatPanel：同样的面板，「戰鬥失敗」（0x004BF8A4）于 (0x104,0xB4)；GrowPanel(0xD4,0xF4,3) 为 1 时字宽 0x11 写「隊伍全滅!!」（0x004BF898）于 (0xFE,0xD8)，字宽 0x10。
- 0x00478330 DrawCraftResultPanel：同样的面板（无标题）；GrowPanel(0xD4,0xFC,3) 为 1 时按 0x00577998：1 写「煉符成功」（0x004BF8D8）于 (0x100,0xB4) 与「得到符咒:%s」（0x004BF8CC，lootNames[0]）于 (0xD0,0xDE)，否则「煉符失敗」（0x004BF8C0）与「沒有得到東西」（0x004BF8B0）。
- 0x004784D0 DrawMessageBox：panelAct id、Run；Box(x,y,宽,高, 0,4,4, 模式 0)；WindowFrame(x+4, y+4, x+宽, y+高)；文字坐标参数都为 0 时画于 (x+2, y+4)，否则于给定坐标，色 0xFFC0、样式 0x10。
- 0x00478590 GrowPanel：步 = (底 − panelHeight − 顶) / 除数（有符号），panelHeight 加步，步为 0 时返回 1。

## DrawResistancePanel、DrawTallPanel、DrawSmallPanel、DrawWidePanel（第 1016–1019 行）— exact

- 0x004785C0 DrawResistancePanel：panelFlags 位 0x20 时：panelAct id、Run；Box(0xEC,0x56,0xB8,panelHeight+0x28, 0,4,4, 模式 0)；框 (0xF0,0x5A,0x1A0,0x6A)、单位名（0x00577A44 指向）于 (0x130,0x5A)、正文框 (0xF0,0x7A,0x1A0,panelHeight+0x7A)；GrowPanel(0x7A,0xDE,3) 为 1 时 5 个元素（名表 0x004BF61C，y 0x82 起每 0x14）：抗性（i8，单位 +0x9C）为 0 时颜色 RGB(0x10,0x10,0x10)、长度 2.0；> 0 时先画底条 FillColor(0x11C,y,0x78,3,RGB(0x1C,0x1C,0x1C))，颜色 RGB(0x1C,2,2)、长度 = (float)(抗性×0.01f×120.0f)（0x004AAE0C／0x004AAE08）；负值不改颜色与长度（沿用上一元素）；元素名于 (0xFC, y−8) 白 RGB(0x1F,0x1F,0x1F)；FillColor(0x11C, y, Ftol(长度), 3, 颜色)。"--"／"%2d%%" 格式化结果从不画出。
- 0x004787F0 DrawTallPanel／0x00478880 DrawSmallPanel：panelAct id、Run；menuSlide（0x00577A4C）> 0 时折半（有符号）；left = x − menuSlide；Box(left, y, 0x64／0x34, 0xC8／0x34, 0,4,4, 模式 2)；WindowFrame(left+4, y+4, left+0x60／0x30, y+0xC4／0x32)；返回 left。
- 0x00478900 DrawWidePanel：同样折半 menuSlide，top = y − menuSlide；Box(x, top, 0x82, 0x1E)、WindowFrame(x+4, top+4, x+0x7A, top+0x18)；返回 menuSlide ≤ 0。

## Game::DrawObjectStats（第 1020 行）— exact

- 0x00478990：panelAct id、Run；menuSlide（0x00577A4C）清 0；当前角色（currentUnit−0x14，0x005136E0 起每 0x34D4）SelectedItemId（0x00487600）；LoadObject 到 0x00576D40、FreeDescription；在抗性 5 字节（0x00576DDC）中找第一个非 0，找到元素号 = 序号+1，找不到取参数 x（越界，已登记 deviations 0x00478990，源码不画元素色框）；名望 = FindCounterValue(0x004D3458, id)（u16）；Box(x,y,0x40,0x42, 0,4,4, 模式 2)；名望（无符号）≥ 10 时 Box(x,y,0x40,0x42, 元素色表 0x004BF674[元素号], 模式 5)；WindowFrame(x+4,y+4,x+0x3C,y+0x3A)；三行「攻:」「防:」「敏:」（0x004BF8F0／EC／E8）于 (x+8, y+8+0x10×i) 色 lightColor（0x0050EC74）样式 0x10，"%3d"（u16 攻 +0x5A、防 +0x5C、敏 +0x5E）于 (x+0x22, …) 色 RGB(0,0x18,0)；小字体 0x004C3658；返回 x。色表 6 项与 exe 字节一致。
- 改注释：lightColor 由正传地址改为 0x0050EC74。

## Game::DrawRefineStats（第 1021 行）— exact（修正后）

- 0x00478C50：panelAct id、Run；menuSlide 清 0；Box(x,y,0x44,0x40, 0,4,4, 模式 2)、WindowFrame(x+4,y+4,x+0x3C,y+0x3A)；当前角色 SelectedItemId 载入 0x00576D40、FreeDescription，记下攻、防（与敏合成双字）、敏；记录清 0xA8 字节（整条记录）后载入 givenBack[guardCursor]（0x0050EFCA、0x00576FB8）、FreeDescription；三行各：标签「攻」「防」「敏」（0x004BEFF0／DC／D8，wsprintf 无参数）于 (x+8, y+8+0x10×i) 色 lightColor 样式 0x10；"%3d" 于 (x+0x22, …) 色 RGB(0,0x18,0)；箭头图 0x23B6 帧 0x13 于 (x+0x16, …)；返回 x。
- 修正：原程序双字相加后只取和的低 16 位做 movsx 再格式化（0x00478DCC、0x00478E79、0x00478F24），源码原按整个双字（含进位到高字的 x、y 高字与下一字段）格式化，高字非 0 时打印值不同；改为两字相加取 i16。

## Game::DrawChargePanel（第 1022 行）— exact（修正后）

- 0x00478FA0：panelAct id、Run；Box(x,y,0x6E,0x44, 0,4,4, 模式 2)；WindowFrame(x+6,y+8,x+0x6A,y+0x18) 与 (x+6,y+0x28,x+0x6A,y+0x40)；DrawRoleIcon(partyRoles[currentUnit−0x14]，即 0x004BF11C[currentUnit]，x+0x14, y+0x14)；"X%2d"（(i16)Unk2EB2Shown 0x0049C2C0）于 (x+0x32, y+8) 大字体 0x004C5130 色 lightColor 样式 4；两项（0x004BF66C：「繼續」0x004BF6C4、「放勁」0x004BF6BC）left = x+0x30×i：名字于 (left+8, y+0x2C) lightColor；第 0 项且 Unk2EB2BelowProficiency（0x0049C2D0）为 0 时以 darkColor（0x00537424）重画于 (x+8, y+0x2C)；i+1 等于 cursor 时背景 0xF000，于 (left+7, y+0x2B) 重画同样两笔，背景 0xFFFE；返回 2。
- 修正：partyRoles 下标（待核项）——蓄劲角色序号是否限于 0..3 未能由调用者确认（0x00471E80 只判 chargingRole），改用 PartyRoleRead 按原布局读相邻全局，并登记 deviations 0x00478FA0。
- 改注释：darkColor 由正传地址改为 0x00537424。

## BattleMessages::Add（第 1023 行）— exact

- 0x00479220：exclusive（0x00577A80）> 0（有符号）返回；分配 0x28 字节清零；x、y、frames（字）、文字指针（+0x24）、length = lstrlen；flags |= 参数；参数含 0x40 时 y = 1、slideY = −0x20；合并后 flags 含 0x1000000 时 y += stacked（0x00577A7C）×32；合并后含 2 时 stacked 加一；参数含 0x2000000 时 exclusive 加一；文字前 0xFF 字节内（遇 NUL 止）每个 '$' 置 colored（+0x20）= 1、length −= 2；四色 0x00577A74..7A = RGB(0x1F,0x1F,0x1F)、(0x1F,0x1D,0x17)、(0x1F,0x11,0)、(0x1F,0,0)；接到链表（0x00537388）末尾。源码 Add 与共用的 Append 逐项一致。

## BattleMessages::Draw、Clear（第 1024–1025 行）

- 0x00479360 Draw — exact（修正后）：链表空返回；颜色变量置 0xFFFF（每次调用一次）；字宽 0x16（不复原）。逐条：y = y+6、x = x+0x10；flags 最高位：panelAct、FillBox(0xA,0xA,0x26C,0x20, −8,−8,0xA, 模式 1)、WindowFrame(0xE,0xE,0x272,0x26, 0, 0x80000008)；0x40000000：FillColor(0xA, y, 0x26C, 0x20, 0xFF)；1：x = 0x12；2：x = 0x13B − ((20×(len>>1))>>1)；frames（i16）≤ 0 跳到下一条；0x10：y 0x10，半宽 = (22×(8−(len>>1)))>>1，slideX 为 0 时 0x280，slideX = (slideX+半宽+0xE3)>>1，盒 (slideX−半宽, 0xA, 0xB0, 0x20)；0x20：同样无初值；0x40：x = 半宽+0xE3、y = slideY = (slideY+0x20)/2（向零），盒 (0xE3, y−6)；colored：'$' 后 atoi 取颜色（无界，已登记），跳一字节，复制到 '$' 或 NUL 为止，画于 (x,y) 样式 0x10，x += 11×段长，至 NUL；否则以 0xFFFF、样式 0x10 画整段；frames 减一。第二轮：frames > 0 保留；0x20 slideX += 0x50、盒与原文字于 (slideX, 0x10)，< 0x280 保留；0x10 slideX −= 0x50，> 0 保留；0x40 slideY −= y、y ×= 2、盒 (slideX−半宽, slideY−6)、文字 (slideX, slideY)，> −0x20 保留；否则摘除，Exclusive 时 exclusive 减一，stacked > 0 时减一。
- 修正：颜色变量（var_84）在 0x0047938B 每次 Draw 只初始化一次，后一条不以 '$' 开头的彩色消息沿用前一条最后的颜色；源码原每条消息重置为 0xFFFF，改为跨消息保留。deviations 0x00479360 补记以 '$' 结尾时越过结尾 0 的读取。
- 0x00479820 Clear — exact：链表非空时逐个释放，清 exclusive、stacked；空链表不清计数。

## Game::FightTalkFrame（第 1026 行）— exact（修正后）

- 0x00479860：取命令字 movsx，加一后与 0x5A 无符号比较走 91 项跳转表（0x00479878）；−1 及表外、0、7、0x20、0x26 走默认（返回 1，不移游标）。按跳转表把汇编分段，逐命令与源码（FightTalkFrame 及其 FightTalk*／FightTalkUnits／FightTalkMisc 分支函数）对照：
  - −1：全部敌人、角色 Reset，0x00578294 清零，EndBattle、Close，返回 0。1：等待计数（0x00578294，0x8000 计数中，低 15 位由 RoleFrame／EnemyFrame 递减）；计数完毕清零、游标 +4、enabled 1、savedUnit 归还 currentUnit；否则 battleRuns 1 跑 BattleFrame，返回非 1 时同 −1 收尾后游标 +4、返回该值。2、0x2C、0x3B：三种对话框（FixedPlace 0x800、SizedLayout 1／OneLine 0x10000、Timed 8 与 NoAdvance 0x400、速度 2、windowFlags 低字并入后清零；1000 起固定 (0x1EA,0xB4)，角色动作 0x24，0x3B 角色位置 −(0x3C,0x14) 且 y 取 GetOrigin；0x2C 敌人先 SetAC0(1) 再全部 0），窗口关闭后十个角色与前十个敌人 SetAC0(0)，跳过 %Q（0xFF 内）、清说话人与 0x00578430、高字。3、4、5、6、8、9、0xA–0x1A、0x1B–0x25、0x27–0x59 逐项一致（字段地址见下）。
  - 位置读写：0x0048B500 只写两个双字的低字，0x0048B4C0 只取低字；0x16、0x32 留下的 0x005782A0／0x005782A4 高字保留原值（命令 9 以整个 0x005782A4 作敌人序号，deviations 已登记）。
- 修正：
  1. 命令 1 不写 0x0057829C（原程序该全局只写不读，删去 FightTalk::unk53CCE4，RoleFrame 中同一写入一并删去，行为不变）；unk53CCDC 更名 waitCount（0x00578294）。
  2. 命令 0xC：敌人倒下未完时原程序在 BattleFrame 前后各写 actingUnit = −1（0x0047B2A3、0x0047B2BE），源码原缺，补上。
  3. 命令 0x16、0x32：位置只写低字（新增 ReadPosition），保留高字残留。
  4. 命令 0x37：第二参数按 movsx 读取（0x0047CCF3）。
  5. 命令 0x58：金钱与价格（movsx）按无符号比较（0x0047E025 jb），源码原按有符号。
- 字段地址注释：FightTalk 各 unk 字段注外传地址（正传 +0x3B5B8，repeatLimit 之后再 +4）；BattleState unk53BFD0（0x005772D0）、unk53BFC0（0x005772BC）、unk53BF02（0x005771EA）、unk4A7626（0x004BF1CA）、unk53BFDC（0x005772DC）、enemyOrders（0x00513434）、enemyAimsAtEnemies（0x005772D4）、unk4FE5D4（0x0050D88C）、enemyHitActs（0x005135A4）、unk53BF88（0x00577284）、unk53BFF0（0x005772F0）、unk53BF74（0x00577270）、unk53C02C（0x00577330）。
- deviations：0x00479860 六行「待核」改引汇编地址，0x32 等命令的敌人数由八改为二十；新增指令 0x24 释放后读取一行。字符串核对：對話系統(GAON)、對話系統(G_AREA_OFF)、代碼值太大(%d)、代碼值重複(%d)、位置超出螢幕(x%d ,y%d ,w%d ,h%d)、錯誤、按鈕個數不對!!。

## FightTalk::Load、Seek、Close（第 1027–1029 行）— exact

- 0x0047E270 Load：drawMovedAtOnce 清零；enabled 为 0 返回；未打开时以「安装目录 + figtalk.dat」CreateFile(GENERIC_READ, 共享读, OPEN_ALWAYS)，句柄只与 0 比较（失败的 −1 照样记为已打开，其后读取失败、剧本全 0）；SetFilePointer(0x204) 再相对 4×战斗号−4（即 0x200+4×战斗号）读 4 字节偏移，SetFilePointer(偏移+0x200)；分配 0x8000 清零（旧缓冲区不释放）作剧本与游标，读 0x8000。返回值调用者不用（0x0045A03B）。
- 0x0047E3A0 Seek：释放旧剧本，分配 0x1000 清零作剧本与游标，SetFilePointer(参数+0x200) 读 0x1000，0x00578460 = 参数。
- 0x0047E420 Close：句柄非 −1 时 CloseHandle、置 −1；0x00578464、drawMovedAtOnce 清零；转入 0x0047E550：battleRuns、enabled 置 1，0x00578414..0x00578430、0x00578444..0x0057845C、0x00578294、0x0057828C、0x00578460..0x00578468 清零（0x0057829C 只写不读不建模），windowTimeout 0xFFFF；剧本已分配时释放并清剧本与游标（未分配时两者同为 0）。0x005782A0、文字位置与说话人不清。

## TalkCurvePoint（第 1030 行）— exact

- 0x0047E450：四个点（i16，movsx）转 float；Evaluate(0.0f, 点, 临时输出)（0x0043CA70，结果不用）；t = (float)(0.05f（0x004BF900）× step)；BasisWeights(t, 0x004BE010, 权重)、Weighted(权重, 点, 结果)；x、y = Ftol(结果) 写满两个双字。kTalkStep 注释补地址。

## BattleRole::BattleRole（第 1031 行）— exact

- 0x0047E660：BattleUnit 构造（0x0048B040）；+0x34BE（absorbFrames）、+0x34A8（unk2F18）字清零；分配 0x3C 字节清零作 +0x00 的角色记录（RoleRecord，static_assert 0x3C）。源码成员默认值与 ownedRecord 一致。注释：ownedRecord 0x38 改为 0x3C，skillsHead、bonusA/B/C 的正传偏移改为外传 +0x3454、+0x34AE、+0x34B0、+0x34B2（顺序布局推得）。

## BattleRole::~BattleRole、BattleRole::ClearExtras（第 1032、1034 行）— exact

- 0x0047E6D0 ~BattleRole：Release（0x004860D0），再 BattleUnit 析构（0x0048B100，源码基类析构）。
- 0x0047E930 ClearExtras：字 +0x34AE／+0x34B0／+0x34B2（bonusA/B/C）、+0x2EB2、+0x34A0、+0x34A2、+0x34AC、+0x34AA、+0x34B4、+0x34A4、+0x34A8、+0x34A6 清零，双字 +0x349C（unk2F0C）、+0x3040（partyMember）清零；+0x311C 起 0xCD 双字（extraActs 5×0xA4）、+0xBAC 起 0x52 双字（slotActs[8]、[9]，+0x68C 起每 0xA4）、+0x3074 起 0x2A 双字（unk2B24，ObjectRecord 0xA8）清零；ActState、ObjectRecord 默认值全 0。unk2B24 注释改为 +0x3074。

## BattleUnit::StepGauge（第 1033 行）— exact

- 0x0047E720：marks 0x40／0x4000、defeated、+0x3038、+0x2FF0、+0x2BDA 位 2、+0x2BC0 最高位任一成立返回 0；gauge（+0x2F2E，u16）< gaugeFull（0x004BEFF8，有符号）时：速度 = (敏捷（记录 +0x16，u16）− 减速 + 加速)/4（有符号；加速／减速按 status 0x20000000／0x08000000 取 movsx +0x2EDA），boost 为 1 时 += 速度/4 + 1；status 最高位（隐身）时扣 (熟练度（u16）×速度/100)/2 + 4；+0x2FF0 清零，gauge = gauge + 速度 − 扣（字），返回 0。满时：鬼（0x0049B9C0）为 1 则 (i16)(Random(100)+1) 与 鬼记录 +0x104（双字，有符号）比较，大于时 +0x2BDA |= 1、gauge += gaugeFull / −3（截断）、返回 0；否则 TakeGhostOrder、+0x2BDA &= ~1。+0x2FF0 = 1、+0x302C = 0；+0x3068 为 1 时清零且 guestControl 为 0 时 facing 清零；+0x2D8、+0x2DC 清零、+0x494 = 1；+0x2BDA 位 0x80 时清该位、+0x2FF0 与 gauge 清零、返回 0，否则返回 1。

## BattleRole::Setup、SetInventory、SetSkills、SetupEscort（第 1035–1038 行）— exact

- 0x0047E9C0 Setup：ClearExtras；partyMember 1；placement（+0xFC4，0x20 字节）= 参数，savedPlacement（+0xFE4）= placement；kind（+0x2FE0）= 参数 +0x1C；battleRecord（0x3C 字节）复制自记录；roleRecord（+4）= 记录，roleBlock（+8）= 块；evadeSkill（+0x2FC5）= 战斗记录 +0x1E 字节；actId（+0x2F28）= 参数 +0x14，为 0 时「戰鬥角色actNumber為0!!」（0x004BF9D8）manrole.cpp 0x10B；战斗记录 +0x26／+0x28 = +0x10／+0x12；队伍记录 +0xA、+4、+0xC、+6、+0xE、+8 依次大于 0x270F（有符号）时截为 0x270F（战斗记录在截前复制）；defeated = 战斗记录 condition 0x8000 或队伍记录生命为 0。
- 0x0047EAF0 SetInventory：+0x3458、+0x345C = 参数（源码 inventoryNode 为空即表示链表头，与之同义）。0x0047EB10 SetSkills：+0x3450、+0x3454 = 参数。
- 0x0047EB30 SetupEscort：partyMember 0；分配 0xA8 清零作 +0xC，LoadObject(参数 actId)、FreeDescription；placement、savedPlacement、kind、actId（0 时「護駕actNumber為0!!」0x004BFA0C、0x15B）；战斗记录 +0x26／+0x28／+0x16／+0x14 = 物件 +0x5A／+0x5C／+0x5E／+0x60，+0x1E = 物件 +0x9A 字节，等级 +0x2C = +0x58 字节，+0x2D 起 5 字节抗性 = +0x9C；evadeSkill = +0x1E；生命 +4 = 物件 +0x68，+0xA = +4；+0xFA（weapon +0x3E 技能脚本）= 物件 +0x64；roleRecord = battleRecord；+0x3070 清零；attackAct（+0x544）id = actId、动作 0x26，Run 为 0 时 +0x3070 = 1；attackAct 清零（0x29 双字）。weapon 注释改为 +0xBC。

## BattleRole::TakeObjectForm、SetupGhost（第 1039–1040 行）

- 0x0047ECC0 TakeObjectForm — exact：LoadObject 到 +0x3074（unk2B24）、FreeDescription；战斗记录 +0x26／+0x28／+0x16／+0x14 依次加物件 +0x5A／+0x5C／+0x5E／+0x60（字加），+0x1E 加 +0x9A 字节（movzx），等级 +0x2C 加 +0x58 字节，+0x2D 起 5 字节抗性复制自 +0x9C；roleRecord = battleRecord；evadeSkill = +0x1E。
- 0x0047ED60 SetupGhost — exact（修正后）：partyMember 0；placement、savedPlacement；ghostRecord（+0x10）；kind 1；actId = 鬼 +0x108 字，0 时「符鬼actNumber為0!!」（0x004BFA20）manrole.cpp 0x1E2；Random(5)（结果不用）；攻 +0x26 = 鬼 +0xF4、智 +0x14 = +0xFC、敏 +0x16 = +0x100、灵力上限 +0xC = +0xF0；天数 +0xC8（有符号）0..6：敏 >>= 1（无符号）、攻 −= 30×攻/100；7..10：敏 −= 30×(u16)敏/100、攻 >>= 1、智 += 30×(u16)智/100；11..24：敏 += 30×(u16)敏/100、攻 += 30×攻/100；灵力 = 上限；鬼 +0x104 = +0x10；ghost（+0x304C）1；+0x2FC0、+0x2FC2 清零；+0xFA = 鬼 +0x10C；ghostOrder（+0x34B6）为 0 时置 1；为 4 时 +0x2BDA |= 2；为 8 时置 1；roleRecord = battleRecord。
- 修正：7..10 与 11..24 天的敏捷增减原程序以 (u16)敏捷×30 的 32 位积除以 100（0x0047EE7E、0x0047EEF4 and 0FFFFh），源码原先把 30×敏捷截成 i16 再除，敏捷 ≥ 1093 时结果不同。

## BattleRole::ApplyEquipment（第 1041 行）— exact（修正后）

- 0x0047EFE0：清 accessories（+0x164 起 0x54 双字，2×ObjectRecord）；十六格（数组指针非空时）：第 0 格复制记录到 weapon（+0xBC，0xA8 字节）、weaponId（+0x34AA）= 节点 id，weapon +0x48 为 0 时「武器%d號的強攻擊act沒設!!」（0x004BFA34，id）manrole.cpp 0x23C；每格：攻 +0x26 += +0x24、防 +0x28 += +0x26、敏 +0x16 += +0x2C、智 +0x14 += +0x2E、默契 +0x18 += +0x30、闪躲 +0x1E += +0x32；力量增量非 0 时力量 +0x10 与攻各加，耐力增量非 0 时耐力 +0x12 与防各加；第 7、8 格复制记录到 accessories[0/1]，id ≠ 0xFFDC 时其 act（+0x54）= id，ApplyAccessory；第 9、10 格到此为止；其余格：元素 = (i8)记录 +0x37，记录抗性[元素] 加物件抗性字节[元素]（字节加），> 100 截 100、< 0 截 0（有符号），+0x38 = 该值；格号 < 7 且 useKind（+0x3C）为 0 时 bonusA/B/C += +0x34／+0x36／+0x38。
- 修正：断言行号原为 0x182，改为 0x23C（0x0047F036）；元素 5..7 读物件抗性之后的 altar／unkA2／unkA3（0x0047F11D），源码原取 0。deviations 补登空格的空指针行，并改写 0x0047EFE0 抗性读取的偏差说明。

## BattleRole::Regenerate（第 1042 行）— exact

- 0x0047F210：roleRecord condition 0x8000 时返回；bonusA／B／C 依次非 0 时：量 = (u16)加成 × (movsx)上限（+0xA／+0xC／+0xE）/ 100（有符号），低字为 0 时取 1，存 +0x2EB6／+0x2EB8／+0x2EBA（生命取负）；Damage(生命／灵力／体力位置)、PushTarget(0x237D／0x237E／0x237F)、SetDamageDigits(movsx 量)、PushTargetAlt(延迟)、StepTargetCount(1)，延迟依次 0、6、12（只在前项执行后加 6）；该字清零。

## BattleRole::ApplyAccessory（第 1043 行）— exact（修正后）

- 0x0047F3E0：按饰品 +0x48（movsx）− 0x32 走 10 项跳转表：0x32／0x35／0x36／0x37／0x38／0x39／0x3A 依次 equipmentFlags（+0x2BD0）|= 1／4／8／0x10／0x20／0x40／0x80；0x3B goldGuard（+0x3060）= 1；0x33 熟练度百分比 = FindCounterPercent(0x004D1F58, act +0x54)，抗性五字节中第一个非 0 的元素：记录抗性[元素] += 低字节((u16)百分比×80/100)，(i8) > 100 截 100，五个全 0 不改；0x34 半 = (u16)百分比 >> 1，非 0 时额外 = 攻×半/100，攻 = 攻 + 攻×10/100 + 额外（字）。熟练度表为全局（源码以参数传入，原函数只取一个参数）。
- 修正：0x33 原程序在 32 位算出 80×(u16)百分比/100 再取低字节加上（0x0047F528–0x0047F54A），源码原先把 80×百分比截成 i8 后再除以 100，百分比 ≥ 2 时多数结果不同。注释「lowered」改为「raised by 80%」。

## BattleRole::UseAccessorySkill（第 1044 行）— exact（修正后）

- 0x0047F5C0：staminaCost（+0x34A2）为 0 时 = AccessoryValue(种类)；种类 − 0x15 走 21 项跳转表：0x15 unk26CC（+0x2BD4）|= 1、commandA（+0x2F88）1、AccessoryProficiency、StepAccessoryCounters(0x004D1F58, 种类, 5)、extraAttacks（+0x2FCD）1，返回；0x16／0x19 commandA 0、unk26CC |= 2／0x10、commandFlags |= 0x400、commandB（+0x2F8C）= 种类；0x17／0x1A／0x1D／0x1E commandA = 种类、|= 4／0x20／0x100／0x200；0x18／0x1B |= 8／0x40、commandA、AccessoryProficiency；0x1C、0x20、0x21 commandA、AccessoryProficiency；0x1F commandFlags |= 0x400、commandA 0、commandB、AccessoryProficiency、unk29A4（+0x2EB0）= (u16)熟练度×11/100 + 1（32 位）；0x22／0x24 AccessoryProficiency、熟练度 = (熟练度>>1) − (熟练度>>1)/5（等于 熟练度/10 截断），回复生命／体力 = 熟练度×上限生命（+0xA）／现体力（+8）/100 + 该值/10；0x23 熟练度 /= 5、回复灵力（+0xC 上限）同式；0x25 commandA = 种类，返回；0x26 及表外返回；0x27 unk2EB2 加一；0x28 equipmentFlags 位 0x100 已置时清除、commandA = 种类、unk26CC |= 0x400，否则 AccessoryProficiency 并置位；0x29 无动作；返回前 StepAccessoryCounters(…, 5)。
- 修正：0x1F 原程序以 32 位算 11×熟练度（0x0047F77B），源码原截成 i16 再除。

## BattleRole::StepWornCounters（第 1045 行）— exact

- 0x0047FA00：两个饰品（+0x164 起每 0xA8）：act（+0x54）为 0 跳过；unk48（+0x48）为 0x25 时 AddTreasureExperience(表, 0x25, 量) 为 1 则 Add(0x118, 0xA, 0x28, 「龍颿法寶升級」0x004BF388, 0x40000002)；否则返回值置 1，从表头（含表头本身）按字找 id：找到且 done（+0xA）为 0 时 value（+6）+= 量，≥ counterMax（+0x44，无符号）截之，percent（+8）= Ftol(value / counterMax × 100.0f（0x004AAE10）)；找不到时链到末尾新节点（0x14 字节清零）：id、value = 量、percent 同式，counterMax 为 0 时 value 0、percent 100、done 1，表头 id（计数）加一。

## BattleUnit::StepOrbCounters、AddTreasureExperience（第 1046–1047 行）

- 0x0047FB90 StepOrbCounters — exact：两个饰品：act 非 0、unk48 为 0x33、命令 +0x10 元素在 1..5（无符号）、饰品抗性[元素−1] 非 0 时：返回值置 1，从表头找 id；找到且未满时 value += 12、截到 counterMax、percent = Ftol(value/counterMax×100.0f)；找不到时新节点 value 12、percent = Ftol(12.0f（0x004AAE14）/counterMax×100.0f)，counterMax 为 0 时 value 0、percent 100、done 1，表头计数加一。
- 0x0047FD20 AddTreasureExperience — exact（修正后）：表头计数非 0 时从 next 起找（找的位置跨饰品保留）；饰品 act 非 0 且 (movsx)unk48 等于 (u16)种类：返回值清零；找不到且之前未找到过：新节点 value = min(量, counterMax)、toNext（+0xC）= 0x32 − 量、percent，表头计数加一；找到：标记已找到，done 为 0 时 value += 量并截到 counterMax；toNext（有符号）≥ 量时 toNext −= 量且量清零，否则 toNext 0；toNext ≤ 0 时等级（+0xE）加一，按等级 <4／<8／<16／≥16 取 0x32／0x78／0x12C／0x201，再减剩余量，返回值 1；percent 重算。
- 修正：种类比较原程序为 movsx unk48 对零扩展的种类（0x0047FD58），源码原两边都按 i16。

## BattleUnit::TreasureLevel、FinishProficiency（第 1048–1049 行）— exact

- 0x0047FF00 TreasureLevel：两件饰品中第一个 unk48 为 0x25 的 act 作 id（没有时序号 2 读到 this+0x308，即 mainAct 动作脚本指针低字；源码取 0，新增 deviations 行）；从表头起按字找 id，返回其等级（+0xE），找不到返回 0。accessories 注释改为 +0x164。
- 0x0047FF60 FinishProficiency：两件饰品：act 为 0 跳过；已有完成者时返回；否则从表头找 id 相同、done 为 0、value ≥ counterMax（无符号）的节点：value = counterMax、done 1、记下 act，跳出节点循环继续下一件；返回记下的 act（u16，初值 0）。

## BattleRole::AttackFrame（第 1050 行）— exact

- 0x0047FFD0（源码 RoleFrame::AttackAdvance）：+0x2F90 非 0 或 +0x3018 为 1 返回 0；autoControl 为 1 或 StatusBit5 为 1，且命令 +0x1010 有 0x38 位时：+0x2BC0 位 0x10000000 则 SetFacing(1)，fadedIn（+0x3064）为 0 且 DrawFadeIn 为 0 时 fadedIn 1、返回 0；guestControl 为 1 时位置 (0x140,0x136)；+0x2FEC = 1；standAct（+0x358）hold 按 held 与 target 0x200；暴击（+0x34CA 或 0x00578474）且非 MarkBit4：队员则 0x00578474 = 1、动作 0x29，非强制角色 castAct（+0x1018）= weapon +0x48，强制角色动作退回 0x28；否则 id = actId、动作 0x28，MarkBit4 时 id 0xFA，强制角色 +0x2FB6 非 0 时动作 0x30（为 2 时 0x31）；Action 失败时「Attack Act」「[%s]QQ:%d號錯誤!!」（0x004BFA50／0x004BFA5C），返回 1。target 位：2 回弹（effectA 非 0 时 recoilAct id／动作、stage |= 0x4000）、0x4000 回弹结束清 hold、0x8000 地震（0x0057730C 1、0x00577030 0）、0x800 StartSlideTo(0)、0x40 StartSlideBack(0)、8 后续（0x400 时 0x00577334 1 与 StartFieldFade(六个 float + 帧数)）并清 slotActs[slot]、4 目标 RaiseFlag26B8、1 打击（+0x2E60、0x005373B4 清零、清 slotActs[slot]、Strike(1)）；stage 无 0x8000 返回 0。后续未等待：id = castAct（effectA 非 0 时取之），为 0 时置等待返回；命令位 1 时 Fly(slotActs[1], …, 起点 x = anchorX−offsetX+位置 x、y = 位置 y+anchorY−standAct.y)，成功后 standAct 与后续等待、后续 target |= 1；否则 AimAction 后画 +0x2A44 图（无空判，源码判空）；0x20 时 AddHitPicture；DrawHitPictures；target |= HitBits；有 1 时清零并 Strike(1)。后续已等待：id 非 0、非命令位 1、+0x2E60 > −0x20 时三色调 = +0x2E60（混合模式 0x2C 时 0 且 +0x2E60 = −0x20），画图，+0x2E60 −= 4（held 时加回）；否则 standAct 等待、ImageList 空、HitPicturesDone 时清 standAct、recoilAct、后续、slotActs[1]、[2]，targets 全 −1，+0x2E60 0、+0x2E62 1、+0x2FF0、fadedIn、stage 清零；无暴击时 extraAttacks 为 0 则 unk26CC &= ~1，非 0 则减一返回 0；0x00578470 字节加一、+0x34CA 与 0x00578474 清零，返回 1。

## BattleRole::RollCritical、ShowAllSkills（第 1051–1052 行）— exact

- 0x004807B0 RollCritical：autoControl 或 guestControl 为 1、unk26CC 非 0、unk2EB2 > 0（有符号）、MarkBit4 为 1、0x00578478 等于 placement.actId 任一成立返回 0；equipmentFlags 位 1 或 status 0x40000 时 critical（+0x34CA）1 返回 1；0x00578470 字节 ≥ roleCount + 8（无符号）时 critical 1、计数清零、0x00578478 = actId、返回 1；计数 < 8 返回 0；否则 (u16)Random(10) ≤ 5 返回 0，> 5 同上置暴击返回 1。attacksSinceCritical 注释补地址。
- 0x004808C0 ShowAllSkills：+0x3450 = +0x3454。

## BattleRole::PrepareCommand（第 1053 行）— exact

- 0x004808D0：commandB（+0x2F8C）≠ 2 时清 skillName（+0x2B38）前 16 字节；commandA（+0x2F88）为 0 返回 1；commandFlags 0x200 时 +0x3054 = 1、unk2F18（+0x34A8）非 0 则作 commandA，命令 +0x1010 有 0x28 时 commandB = commandA、commandA = 0xC8 返回 1，否则返回 0；commandA ≠ 0x1B 时 unk29A4（+0x2EB0）= 攻；UseAccessorySkill(commandA)、RollCritical；命令（+0x1004，0x2C 字节）清零后 LoadCommand(0)，equipmentFlags 0x20 时 actionFlags（+0x1008）|= 0x100；weapon 技能脚本（+0xFA）为 0：有 0x28 且 commandA ∈ {1, 0x17, 0x1A} 且 partyMember 1 时走路（commandB = commandA、commandA = 0xC8），返回 0；否则清命令、LoadCommand(MarkBit4 ? 0xA6 : 脚本)、同样补 0x100；StatusBit5 为 1：commandA ≠ 1 返回 1；unk26B8 位 0x10000000 时命令位 4 则 SetFacing(1)、位置 (0x154,0x140)，返回 1；否则有 0x28 时 commandB 1、commandA 0xC8，返回 1；其余：有 0x28 或 commandA 为 0x17，且上述三种之一且队员时走路，返回 1。

## BattleRole::ListSkill、CountSkills（第 1054–1055 行）— exact

- 0x00480B10 ListSkill：类别 0／1／2 取种类掩码 0x10／0xC／0x1001，其余按原值；类型 0／1 取 0x11／0x14，其余原值；ShowAllSkills 后逐个前进（+0x3450 = next），到空时 cost = 0xFFFF、返回 0xFFFF；节点种类（数据 +0x20）与掩码相交且可用者字节（+0x3A）有 5 位时，类型 0x11 计类别（+0x56）为 0x11 者、否则计类别 0x14 者；每个节点后计数等于序号即停（序号 0 时停在第一个）。类型非 0x11／0x14 时 cost = 0、返回栈上未写的字（deviations 已登记，源码取 0）；否则 LoadSkill 到 16 字节缓冲区（写满 0x2C，其后的局部即返回的元素字），名字 lstrcpy，cost 位（+0x40）：0x8000 时 = 低 15 位，0x4000 时 = 低 14 位 | 0x8000，0x800 时 = 1000，类型 0x14 时 = 1；都不满足时 cost 不改。
- 0x00480C60 CountSkills：同样的掩码与类型；逐个前进，可用者字节无 5 位或种类不交跳过，按类型计数（字节加一）。

## BattleRole::CanPaySkill（第 1056 行）— exact

- 0x00480CF0：掩码同 ListSkill；序号 0 返回 0；逐个前进计可用（字节 5 位）、种类相交且类别 0x11 者，到空返回 0；cost 位 0x8000：selectedItem（+0x3460）清空、unk2F10（+0x34A0）= 低 15 位；0x4000：selectedItem 清空、staminaCost = 低 14 位；0x800：unk2F10 = 低 11 位，ShowAllItems 后逐个找 id 相等者：找到且 count 或 pending（有符号）> 0 时 selectedItem = 它、返回 1，否则或找不到时消息链为空才 Add(0x118,0xA,0x14,「物品不夠!!」0x004BFA90,0x40000002) 并出音 0x8C，返回 0；其余：ShowAllSkills，unk2F10 非 0 时灵力（+6，有符号）≥ 它返回 1，否则（消息链空时）「靈力值不夠!!」（0x004BFA80）＋音；staminaCost 非 0 时体力（+8）≥ 它返回 1，否则「體力值不夠!!」（0x004BFA70）＋音；返回 0。

## BattleRole::SelectSkill（第 1057 行）— exact

- 0x00480F30：掩码同上；序号 0 返回 0xFFFF；类型 0 时 commandFlags |= 0x8000、取 0x11，类型 1 时 |= 0x200、取 0x14；清命令（+0x1004，0x2C）、ShowAllSkills；逐个前进（到空返回 0xFFFF），按可用字节 5 位、种类相交、类别计数，计数等于序号时：act = 数据 +0x54；LoadCommand(技能脚本 +0x3E)；roleRecord 智慧（+0x14，无符号）≥ 0x32 时 unk18（+0x101C）+= unk18×(智慧−0x32)/100；skillName 前 16 字节清零后 lstrcpy 名字；commandFlags 有 0x200 时 magicAction（+0x2FB4）= +0x48、unk2F16 = act、unk2F18 = cost 位；useKind 为 0 时三项回复 = +0x34／+0x36／+0x38；healPercent（+0x2FD0）清零，useKind 有 0x200 时置 1 并取三项回复；返回：掩码与节点种类都有位 8 时 1，否则掩码位 4（0x10）。

## BattleRole::LoadItemAction、FinishCommand（第 1058–1059 行）— exact

- 0x00481170 LoadItemAction：栈上 0xA8 字节记录清零，LoadObject(id)、FreeDescription；act = +0x54（零扩展）；LoadCommand(+0x3E)；命令元素（+0x10）为 0 时命令值（+0xE）= +0x34；commandFlags |= 0x8000。
- 0x00481200 FinishCommand：commandFlags 有 0x8000 时：selectedItem 为空则 roleRecord 灵力 −= unk2F10、< 0 时截 0；否则 UseSelectedItem(0, 0)（第二个参数被调用者弹出不读）、selectedItem 清空；unk2F10 清零；PayStamina（0x00481270）。

## BattleRole::PayStamina、TakeCommand2F18、ShowAllItems（第 1060–1062 行）— exact

- 0x00481270 PayStamina：staminaCost（+0x34A2）非 0 时 roleRecord 体力（+8）减之，< 0 截 0，staminaCost 清零。
- 0x004812A0 TakeCommand2F18：commandFlags |= 0x200、+0x3054 = 1；unk2F18 非 0 时作 commandA；+0xBAC 起 0x52 双字（slotActs[8]、[9]）清零。
- 0x004812E0 ShowAllItems：+0x3458 = +0x345C（源码 inventoryNode 置空同义）。

## BattleRole::ListObject、CountObjects（第 1063–1064 行）— exact

- 0x004812F0 ListObject：ShowAllItems、flags 清零；类别 0..3 取 0x10／0xC／0x1001／0x800，6 取 2，其余原值；逐个前进（到空 flags 0、返回 0）：种类相交时，掩码为 2 计剩余（pending − (u16)workCount + count，有符号）> 0 者，否则还要求可用字节 5 位；每个节点后计数等于序号即停；selectedItem = 节点；种类相交时名字 lstrcpy、count = 剩余（字）、flags |= 0x8000，cost 位 0x4000 时 cost = 低 14 位，itemUses[id] + 1 ≥ 3 时 cost += cost/3 且 flags |= 0x2000，(u16)cost > 体力（有符号）时 flags |= 0x4000；返回 1。
- 0x00481460 CountObjects：类别 0..6 取 0x10／0xC／0x1001／0x800／0x2000／0x8000000／2；逐个：种类相交且掩码为 2 时剩余 > 0 计一并跳下一个；种类相交、可用字节 5 位、剩余 > 0 时计一；掩码 0x2000 且 cost 位 0x2000 时再计一。

## BattleRole::SelectObject（第 1065 行）— exact

- 0x00481540：清命令（0x2C）、skillName 前 16 字节、usedItem（+0x34A4）；类别 0..6 掩码同 CountObjects；序号 0 返回 0；逐个前进（到空返回 0）计数同 CountObjects，每个节点后计数等于序号即停；commandFlags |= 0x4000、selectedItem = 节点、kind 出参 0；掩码 0x2000 且 cost 位 0x2000：LoadCommand(+0x3E)、unk29A4 = +0x24，返回 1；unk48（+0x48）非 0：掩码为 2 时直接往下，否则 LoadCommand(unk48)、cost 0x8000 时 unk2F10 = 低 15 位且 > 灵力（有符号）返回 0、0x4000 时 staminaCost = 低 14 位且 > 体力返回 0；unk48 为 0：LoadCommand(+0x3E)、同样的灵力检查，体力 cost 在 itemUses[id]+1 ≥ 3 时加倍（字），> 体力返回 0；useKind 为 0 时三项回复，healPercent 清零，useKind 0x200 时置 1 并取回复；act 出参 = castAct（+0x1018）；节点种类 0x800 时 commandFlags |= 0x1000、usedItem = act；kind 出参在掩码与种类都有 8 或掩码有 0x10 时为 1；(种类无 0x80 或有 0x8000000) 且掩码 ≠ 0x800 时 workCount 加一；返回 1。

## BattleRole::UseSelectedItem（第 1066 行）— exact

- 0x004818E0：ShowAllItems；selectedItem 为空返回 0；从表头起逐个前进并计位次，到空返回 0（光标留空），id 相等即停；光标 = 选中项、usedItem = id；useKind（+0x3C）有 0x400 且低字节次数非 0 时次数减一（高字节保留），剩余次数非 0 则不消耗；类别 6 时 commandFlags |= 0x800、absorbCategory／absorbChance = +0x4A／+0x4C；消耗（种类无 0x80 或有 0x8000000）：count 非 0 减一，否则 pending 非 0 减一，workCount（无符号）> 0 减一，selectedItem 清空；count 与 pending 都为 0 时摘除释放，ShowAllItems 后前进 位次−1 个，把该节点（位次为 1 时为表头本身）的 next 接到后面；光标所在节点 cost 位 0x2000 时返回 0，否则返回 usedItem。位次为 1 时把表头变量当节点读旗标（0x00481A87，读到 0x004C3018+0x4D），源码视为无旗标，deviations 该行改引汇编地址。

## BattleRole::ListedObject、ThrowableObject、CountUsableObjects（第 1067–1069 行）— exact

- 0x00481AB0 ListedObject：第一个参数不读；逐个前进计 cost 位 0x2000 者，到空返回 0，每个节点后计数等于序号即停；名字 lstrcpy、数量 = pending + count（字），返回 1。
- 0x00481B20 ThrowableObject：throwable 为 0 时遇 id 0x300 即停（不写 selectedItem），否则种类有 0x8000000 且 id ≠ 0x300 者计数；每个节点后计数等于序号即停并写 selectedItem；到空返回 0；可用字节 5 位时写名字与数量；返回 1。
- 0x00481BC0 CountUsableObjects：出参清零；种类有 0x8000000、id ≠ 0x300、可用字节 5 位者累加 pending + count（字）。

## BattleRole::StartAbsorb（第 1070 行）— exact

- 0x00481C10：absorbSource（+0x2A54）= 来源 StepIdleAction；来源 GetOrigin；粒子效果（+0x108C，0x58 字节）清零；像素 = RLE 解码来源图（无空判，deviations 已登记）；宽高 = 图 +0xC／+0xE；x = 原点 x − 1、y = 原点 y；lifeBase 0x14、speed 0x1E；targetLeft = anchorX − originX + 位置 x − 0x32，targetTop = 位置 y − mainAct.y + 0x28，目标宽高 1；bursts = 高 < 100（无符号）时 高 − 10，否则 高 >> 1；divisor 0x28；三色调 5；旗标 0x56，来源镜像时 |= 1；commandFlags |= 0x800；投影器 SetScreen(GetSystemMetrics(0), (1))（平台替换为 640×480，已登记）；throwSteps（+0x2B88）清零、flight（+0x106C，0x20 字节）、attackAct（+0x544）、extraActs（+0x311C）清零。deviations 两行改引汇编地址。

## BattleRole::AbsorbFrame（第 1071 行）— exact

- 0x00481DB0（源码 RoleFrame::AbsorbFrameAdvance）：absorbFrames（+0x34BE）加一；三色调 5；absorbing（+0x303C）1；Explosion（0x00439250）为 1 时：absorbFrames、absorbing 清零，释放像素，效果（0x58）、extraSteps（+0x3488，5 双字）、flight、attackAct 清零，返回 1；bursts 为 0 返回 0；五个火花：x = 效果 x + 宽>>2，y = 高 − bursts + 效果 y + 偏移（−5／−10／−15／0／+5），FlyExtraAct(0x1C2B／0x1C2C／0x1C2D／0x1C2B／0x1C2C, 0, 槽, x, y, 0xE／0xA／0xC／8／0x10)，后四个在 absorbFrames（有符号）≥ 10／20／30／40 时；返回 0。

## BattleRole::BeatsLevel、StepBack（第 1072–1073 行）— exact

- 0x00481FF0 BeatsLevel：等级参数为 0 返回 0；记录等级（字节）< 参数（无符号）时 Random(100) ≤ 35；差 = 等级 − 参数：0..7 时 Random(100) ≤ 70，8..12 时 ≤ 90，≥ 13 返回 1。
- 0x00482080 StepBack（源码 RoleFrame::StepBackAdvance）：defeated 为 1 时 +0x2FEC 清零返回 1；glowLevel（+0x2E64）= 模式 0 ? 2 : 6；IsHeld 为 1 时 fadeLevel（+0x2B70，有符号）> glow 则减一返回 0，否则 0xF 返回 1；fadeLevel ≤ glow 时 recoilAct 清零、fadeLevel 0xF，模式 1 时 +0x2FEC 1 且 autoControl 为 1 时 defeated 1，返回 1；否则 +0x2FEC 1，recoilAct id = actId、动作 0x2F（actId 1 时 0x2E，autoControl 时 0x24），hold 按 held；Run 失败返回 1；取图（无空判，源码判空）、actOffsetX、frameFlags（actId 1 时异或 1）；fadeLevel 为 0xF 且模式 1 时 view 0x2F、出音 0x2F、声像 facing 为 1 时 −0x30 否则 0x30、view 清零；位置 x 在模式 1 时按 (facing ≠ 1 或 autoControl ≠ 0) 加 0x1E 否则减 0x1E；frameFlags 含 0x14 时 alpha = signal；画图于 (位置 x − actOffsetX, 位置 y − act.y)；fadeLevel 减一，返回 0。

## BattleUnit::Word2A86Bit13、Word2A86Bit12、BattleRole::ThrowAct（第 1074–1076 行）— exact

- 0x00482350／0x00482360：commandFlags（+0x2FB0）位 13／位 12。
- 0x00482370 ThrowAct（源码 RoleFrame::ThrowActAdvance）：attackAct id 0x186B、动作 0、hold 按 held；Run 失败返回 0；取图（无空判）、actOffsetX = act.x、frameFlags；facing 为 1 时 frameFlags ^= 1、actOffsetX = 图宽 − act.x；目标 GetOffset：两字都非 0 时终点 = 原点 + 偏移 − (actOffsetX, act.y)，否则 = 绘制位置 − (actOffsetX, act.y)（字运算，取 movsx）；flight（+0x106C）清零，起点 x = anchorX − originX + 位置 x − actOffsetX、y = 位置 y + mainAct.anchorY − mainAct.y − act.y；最多 throwSteps×8 步 LineStep，x 到达终点 x 即停；throwSteps 加一；出音 act.view、view 清零；到达时画于终点、throwSteps 与 flight、attackAct 清零返回 1，否则画于当前点返回 0。

## BattleRole::CatchAct（第 1077 行）— exact

- 0x00482650（源码 RoleFrame::CatchActAdvance）：attackAct id 0x186B、动作 1、hold 按 held；Run 失败返回 0；取图（无空判）；目标 GetOffset 两字都非 0 时起点 = 原点 + 偏移 − (act.x, act.y)，否则 = 绘制位置 − (act.x, act.y)（字，取 movsx）；flight 清零；终点 x = 位置 x − originX + mainAct.anchorX − act.x，y = 位置 y + mainAct.anchorY − mainAct.y − act.y；LineStep 循环与到达判断、throwSteps、出音同 ThrowAct；到达画于终点（act.flags）并清 throwSteps、flight、attackAct 返回 1，否则画于当前点返回 0。

## BattleRole::GlowAct（第 1078 行）— exact

- 0x004828E0（源码 RoleFrame::GlowActAdvance）：attackAct id = usedItem、动作 0x24、hold 按 held；Run 失败返回 0；取图；出音 act.view，随后清的是 recoilAct.view（+0x4F8，原有 BUG 保留）；glowFlags（+0x2B90）= act.flags、glowOffsetX（+0x2B94）= act.x；facing 为 0 时 glowFlags ^= 1、glowOffsetX = 图宽 − act.x；glowPhase（+0x2F82）0：三色调 = glowLevel/2 − 0x1F（向零），glowLevel += 2，> 0x3E 时相位 1；相位 1：同样的色调，glowLevel −= 2、absorbFrames 加一，≤ 0 时相位 2；画于 (x − glowOffsetX, y − act.y)、旗标 |= 4；相位 2 时出音 0x6A，glowLevel、glowFlags、glowOffsetX、相位、absorbFrames、hurtStep（+0x2F94）清零，attackAct 与 extraActs 清零，返回 1；否则返回 0。

## BattleRole::FlyExtraAct（第 1079 行）— exact

- 0x00482B40（源码 RoleFrame::FlyExtraActAdvance，返回值只被 AbsorbFrame 调用且不用）：extraActs[槽]（+0x311C 起每 0xA4）id、动作、hold；Run 失败返回；取图；flight 清零，起点 = (x − act.x, y − act.y)（字，movsx），终点 x = 位置 x − originX + mainAct.anchorX − act.x、y = 位置 y + mainAct.anchorY − mainAct.y − act.y；LineStep extraSteps[槽]×步数 次；extraSteps 加一；出音 view 并清零；当前 x ≥ 终点 x − unk2674（+0x2B7C）时画于 (终点 x − unk2674, 终点 y)、extraSteps 与 flight 清零（返回 1），否则画于当前点（返回 0）。

## BattleRole::PoseAtTarget（第 1080 行）— exact

- 0x00482D70（源码 RoleFrame::PoseAtTargetAdvance）：standAct 动作 0x32、hold 按 held、id = actId；Run 失败返回 0；取图；waiting 非 0 时清 standAct 返回 1；facing 为 1 且 +0x374 为 0 时 flags ^= 1、+0x374 = flags | 0x8000、act.x = 图宽 − act.x；目标绘制位置减 (act.x, act.y)（字）；影高（0x004EDA10）= 图高/3，斜切（0x004ED9A0）= 图高>>2，三色调 −6（autoControl 为 1 时 −1）；出音 view、声像 x > 0x140 时 0x30 否则 −0x30；shadowMode（+0x2BAC）= (flags & 0x8000000F) | 0xC；view 清零；影画于 (x − 5, y − 影高 + act.y)，本体画于 (x, y)；返回 0。

## BattleRole::Steal（第 1081 行）— exact

- 0x00482FC0：目标物件数据（0x0049D3B0）stealItem（+0x8C）为 0 返回 25000；AccessoryProficiency(0x17)；熟练度（+0x2BE6）< 100（无符号）时：r = Random(10)，cut = 熟练度/25，cut > (u16)r 时值 0，否则 r − cut（字）；值 ≥ stealDifficulty（+0x8E，无符号）返回 0；返回 stealItem 并清零。

## BattleRole::AttackPose（第 1082 行）— exact

- 0x00483060（源码 RoleFrame::AttackPoseAdvance）：standAct id = actId、+0x2FEC = 1、动作 0x3C、hold 按 held；Run 失败返回 0；取图；actOffsetX = act.x、frameFlags = flags、actAnchorX = anchorX；facing 为 1 时 frameFlags ^= 1、actOffsetX = 图宽 − act.x，anchorX 非 0 时 actAnchorX = 图宽 − anchorX；影高 = 图高/3、三色调 −6、斜切 = 图高>>2；出音 view、声像 −0x30、view 清零；shadowMode = (flags & 0x8000000F) | 0xC；影画于 (位置 x − act.x, 位置 y − 影高)，本体画于 (位置 x − act.x, 位置 y − act.y)；target 有 1 或 8 时清零返回 0x9B78；未等待返回 0；否则 +0x2FEC 清零、standAct 清零返回 2。

## BattleRole::CanTarget、CanAbsorb、BattleUnit::MarkBit11Clear（第 1083–1085 行）— exact

- 0x004832A0 CanTarget：调用目标 Data()（结果不用），返回 1。
- 0x004832C0 CanAbsorb：目标数据；GetHitPoints(生命, 上限)；种类有 0x20 或无 0x800 返回 0；absorbChance（+0x34BC）非 0 且 absorbCategory 等于目标类别（+0x56）时 Random(10) < absorbChance；类别 > 10（无符号）返回 0；高 = 目标等级（+0x58）− 角色等级：≥ 12 返回 0；低 = 角色等级 − 目标等级 ≥ 10 返回 1；高 7..11 时生命 ≤ 上限/4；低 5..9 时生命 ≤ 上限/3 返回 1，否则 Random(100) ≤ 80；高 1..6 时同样先比 上限/3，否则 Random(100) ≤ 20 − 5×高；低 ≥ 5 或 < 0 返回 0；低 0..4：先比 上限/3，否则 Random(100) ≤ 10×低 + 20。
- 0x00483490 MarkBit11Clear：marks（+0x2BD8）位 0x800 为 0。

## BattleRole::DirectAttack（第 1086 行）— exact

- 0x004834A0（源码 RoleFrame::DirectAttackAdvance）：standAct id = actId、+0x2FEC = 1、动作 0x2A、hold 按 held；Run 失败返回 0；取图；frameFlags、actOffsetX、actAnchorX 及 facing 镜像同 AttackPose；影 y = 目标绘制 y − act.y（字）；影高 图高/3、斜切 图高>>2、色调 −6（autoControl 时 −1）；出音、声像 x（位置 x − actOffsetX）≥ 0x140 时 0x30 否则 −0x30；shadowMode = (frameFlags & 0x8000000F) | 0xC；view 清零；影画于 (x, 影 y − 影高 + act.y)，本体 (x, 位置 y − act.y)；target 有 1 或 8 时：清零、stage = 0x8000、目标 RaiseFlag26B8，伤害 = (i16)HitDamage，0x00575F34 = 伤害（≥ 0x270F 截之），0x005378F4 += 伤害，目标 SetDamageDigits、StepTargetCount(1)；stage ≠ 0x8000 返回 0；slotActs[0] id 0x1C9A、动作 0，AimAction(0, 0)，画图于 aim（无空判）；standAct 等待时 stage 清零、slotActs[0] 与 standAct 清零返回 1，否则返回 0。

## BattleRole::DrainShare（第 1087 行）— exact

- 0x00483830：equipmentFlags 无 0x10 返回 0；AccessoryProficiency(0x37)、StepAccessoryCounters(0x004D1F58, 0x37, 0xC)；熟练度 >>= 1（字）；量 = 量×熟练度/100 + 1（有符号），返回 1。

## BattleRole::Pose36Frame（第 1088 行）— exact

- 0x004838A0：roleRecord 生命 ≤ 上限/4（有符号）返回 1；standAct id = actId、动作 0x36（不设 hold）；Run 失败返回 0；取图（无空判）；overlayX（+0x2EC0）= +0x62、actAnchorX = anchorX、原点 x = act.x；facing 为 1 时 flags ^= 1、原点 x = 图宽 − act.x、anchorX 非 0 时 actAnchorX = 图宽 − anchorX；出音、声像 左端 ≥ 0x140 时 0x30 否则 −0x30、view 清零；影高 图高/3、色调 −6、斜切 图高>>2，shadowMode = (flags & 0x8000000F) | 0xC，影画于 (左端, 位置 y − 影高)、本体 (左端, 位置 y − act.y)；rotation（+0x4E）与 actionCode（+0x50）都非 0 时取图 (rotation, actionCode−1)（+0x2A4C，无空判）画于 (位置 x + overlayX − 原点 x, 位置 y − act.y + offsetY)、flags | 4，等待时四个字段清零；等待时清 standAct 返回 1，否则返回 0。

## BattleRole::CastAt（第 1089 行）— exact

- 0x00483B70（源码 RoleFrame::CastAtAdvance）：+0x2FEC = 1、standAct id = actId、动作 0x34（不设 hold）；Run 失败返回 0；取图；actAnchorX、flags、原点 x 与 facing 镜像同 Pose36Frame；影色调 −6（autoControl 时 −1）、出音、声像 x ≥ 0x140 时 0x30、shadowMode、view 清零、影与本体两次绘制；target 无 1／8 返回 0；法术 ≤ 0（有符号）返回 1；slotActs[0] id = 法术、动作 0，Run 失败返回 0；出音其 view 并清零；取其图（无空判）；actAnchorX = 其 anchorX、旗标、原点 x 按 facing 镜像；画于 目标绘制位置 − (原点 x, 其 y)（字，movsx）；其等待时清 slotActs[0] 与 standAct 返回 1，否则返回 0。
- 修正：施法姿势动作原程序为 0x34（0x00483B9A），源码 kCastPose 原沿用正传的 0x2D，改为 0x34（只有 CastAt 使用）。结论改为 exact（修正后）。

## BattleRole::MagicFrame（第 1090 行）— exact

- 0x00483ED0（源码 RoleFrame::MagicFrameAdvance）：slotActs[8]（+0xBAC）动作 = magicAction（+0x2FB4）、+0x2FEC = 1、id = actId；Action；target 有 2：effectA 非 0 时 stage |= 0x4000、recoilAct id／动作 = effectA／effectQ，有 0x200 时 hold 1，清 2 与 effectA／Q；stage 有 0x4000 且 Recoil(anchorY) 为 1 时清该位、target 与 hold 清零、recoilAct 清零；target 有 8：有 0x400 时 0x00577334 = 1、StartFieldFade（六个 float + 帧数）并清 0x400，spellStage（0x00577378）无 0x8000 时置之；spellStage 有 1 时 stage 清零、slotActs[8] 清零返回 1，否则返回 0。

## Game::DrainSpellFrame（第 1091 行）— exact

- 0x004840A0（角色的成员函数，源码 Game::DrainSpellFrame(role, target)／DamageFrame::DrainSpellFrameAdvance）：slotActs[8] id = actId、动作 = magicAction、+0x2FEC = 1；Run 失败返回 0；取图（无空判）；actOffsetX、flags、actAnchorX 与 facing 镜像；影高 /3、斜切 >>2、色调 −6（autoControl −1）；出音、声像 x ≥ 0x140 时 0x30、view 清零；shadowMode；影画于 (x − 5, 位置 y − 影高)、本体 (x, 位置 y − act.y)；暗色 0x0057722C／2E／30 = act +0x68／+0x6A／+0x6C，DarkenByTint（0x004655A0，参数不读），三者任一非 0 时 0x00577290 = 1；target 无 1／8 返回 0；目标绘制位置（不用）；absorbFrames 加一；Fly(slotActs[0], 0, castAct, anchorX − actOffsetX + 位置 x, anchorY + 位置 y − act.y, 命令目标, unk28, 1) 不为 1 返回 0；目标 RaiseFlag26B8；伤害 = (i16)HitDamage，0x00575F34 截到 0x270F，0x005378F4 += 伤害，目标 SetDamageDigits、StepTargetCount(1)；0x00575F34 取负；角色 PushTarget(0x237D)、SetDamageDigits(该值)、PushTargetAlt(0)、StepTargetCount(1)、PushTarget(0x237F)、SetDamageDigits(0x005378F4)、PushTargetAlt(8)、StepTargetCount(1)；角色 Damage(0x00575F34, 0, (字)0x005378F4)；absorbFrames 清零，slotActs[0] 与 [8] 清零，返回 1。

## BattleRole::BeamSpellFrame（第 1092 行）— exact

- 0x004844B0（源码 RoleFrame::BeamSpellFrameAdvance）：+0x2FEC = 1、slotActs[8] id = actId、动作 = magicAction；Run 失败返回 0；取图；原点 x、flags、actAnchorX 与 facing 镜像；target 有 8 时 slotTint（+0x2E60）= −0x1F、stage |= 3、target 与 glowLevel 清零；stage 为 0：出音、声像、view 清零，影（/3、色调 −6、>>2）画于 (x − 5, 位置 y − 影高)、本体 (x, 位置 y − act.y)；stage 位 1：glowLevel（有符号）> −0x1E 时色调 = glowLevel、叠加画本体、glowLevel 减一，否则清位 1；位 2：slotActs[9] id 0x17FE、动作 0，Run 失败返回 0，取图，aimOffsetX（+0x2EC8）、aimAnchorX（+0x2EBE）、flags 取其值并按 facing 镜像，出音其 view、声像（按法术姿势的 x）、清 view；slotTint > 0 时 stage = 4、glowLevel 与 slotTint 清零，否则色调 = slotTint、叠加画于 (anchorX − aimAnchorX − 原点 x + 位置 x, 位置 y − 其 anchorY + 法术 anchorY − 法术 y)、slotTint 加一；stage 为 4：Fly(slotActs[0], 0, 0x17FE, 位置 x − 原点 x + anchorX, 位置 y + 法术 anchorY − 法术 y, −1, unk28, 0) 为 1 时目标 RaiseFlag26B8、伤害记账同 DirectAttack、stage = 0x4000、glowLevel = −0x1F；位 0x4000：slotActs[1] id 0x1F88、动作 0，DrawAimed 为 1 时 stage = 0x2000；位 0x2000：glowLevel > 0 时清零 glowLevel、stage 与 slotActs[8]、[9]、[0]、[1] 返回 1，否则色调 = glowLevel、以当前图与旗标叠加画于 (x, 位置 y − 法术 y)、glowLevel += 2；返回 0。

## BattleRole::ChargeAttackFrame（第 1093 行）— exact

- 0x00484B20（源码 RoleFrame::ChargeAttackFrameAdvance 与 ChargeOnTargetAdvance，后者在原程序内联）：+0x2F90 非 0 或 +0x3018 为 1 返回 0；+0x2FEC = 1；slotActs[8+槽] id = actId、动作 = magicAction，Action；target 有 8 且有 0x100：trails[0]（+0x1314）未开始时清零、started 1、八个图号 0xFFFF、tintBlue 0xC、fadeLevel 0；法术 hold 1；attackAct id = actId、动作 = magicAction + 0x28，图号 < 2 时反复 Run（失败返回 0）；滑行两笔（alpha 8，(actX − fadeLevel, actY)，frameFlags | 0x14 与 tintBlue 10 的 | 0x10）；fadeLevel ±10（facing 0 加）；attackAct hold 1；其 target 位 1 时目标 RaiseFlag26B8 并清零；fadeLevel % 150 ≠ 0 返回 0；否则 endDash（法术 target、glowLevel、+0x2B80 清零、+0x2FB8 = 0xFFFF、两 hold 清零）、charging（+0x3004）1。charging 为 1：attackAct 已等待则反向滑行、fadeLevel 回到 0 时 endDash 与 charging 清零；未等待则内联段：attackAct 同 id／动作 Run（失败返回 0）、取图、出音并清 view、shadowMode = flags、actOffsetX = x、记 anchorX，facing 镜像；冲刺点 = 目标绘制位置 − (actOffsetX, y)；trails[1] 未开始时同样初始化；direction 2、count 3、fadeStep 1，Trail；再取图画于冲刺点；其 target 有 8 时 stage |= 0x800、清 target 与 slotActs[1]；有 0x800 时 slotActs[1] id = castAct（effectA 非 0 取之）、动作 0，未等待时 Run（失败返回 0）、取图、shadowMode／aimOffsetX 与 facing 镜像、出音、声像 facing 1 时 0x30 否则 −0x30、清 view，再整体镜像一次（shadowMode ^= 1、aimOffsetX = 宽 − aimOffsetX、anchorX 非 0 时取 宽 − anchorX），两边 anchor 都有值时画于 (冲刺 x − 后续 anchorX + 冲刺 anchorX, 冲刺 anchorY − 后续 anchorY + 冲刺 y)，否则 (0, 0)，其 target 位 1 时 slotTint 清零、清 target、Strike(0)。之后：法术 target 有 8 时 stage |= 0x8000 并清零；有 1 时 Strike(0)、清零、stage |= 0x8000、slotTint 与 0x005373B4 清零、清 slotActs[0]；stage 无 0x8000 返回 0；slotActs[0] 动作 0、id = castAct，AimAction(法术 anchorX, anchorY)，其 target 位 1 时 slotTint 清零、清 target、Strike(0)；其等待时清两 effectA，法术未等待或仍 charging 返回 0，否则清法术、attackAct、recoilAct、slotActs[0]、[1]、全部 trails（0x4C0 字节）、targets −1、slotTint、+0x2FF0、stage、+0x2BDE 清零、0x00578470 加一、0x00578474 清零、charging 清零，返回 1；未等待且 stage 无 0x4000 时以 shadowPicture 调色板画图于 aim；返回 0。AfterimageTrail 注释的正传偏移改为外传偏移（同值）。

## Game::MagicAttackFrame、RollChance（第 1094–1095 行）— exact

- 0x004854B0（角色成员函数，源码 Game::MagicAttackFrame／DamageFrame::MagicAttackFrameAdvance）：+0x2F90 非 0 或 +0x3018 为 1 返回 0；+0x2FEC = 1；slotActs[8] hold 按 target 0x200；id、动作，Action；target 2：回弹（effectA 非 0 时 stage |= 0x4000 与 recoilAct），0x200 时 hold 1，清 2 与 effectA／Q；stage 0x4000 且 Recoil(anchorY) 为 1 时 target、hold 清零、清位；0x8000 地震；4：清 target、stage |= 0x8000、目标 RaiseFlag26B8；8：0x400 时淡入场（StartFieldFade）并清之，清 8、stage |= 0x8000、slotTint 清零、target 清零、清 slotActs[0]；1：stage |= 0x8000、slotTint 清零、target 清零、Strike(0)、清 slotActs[0]；暗色三字与 0x00577290；stage 无 0x8000 返回 0；slotActs[0] 动作 0、id = castAct（effectA 非 0 时 id／动作 = effectA／Q）；id 为 0 时置等待；命令位 1 时 Fly(…, anchorX − actOffsetX + 位置 x, 位置 y + standAct.anchorY − standAct.y, 命令目标, unk28, 0)，非 1 返回 0，否则 target |= 1、standAct 与后续等待、slotTint 清零；否则 AimAction(法术 anchorX, anchorY)；后续 target：0x8000 地震、4 目标 RaiseFlag26B8 并清零、1 时 slotTint 与 0x005373B4 清零、清零、Strike(0)；后续未等待：stage 无 0x4000 时以 shadowPicture 调色板画于 aim，返回 0；已等待：stage 有 0x4000 返回 0；清两 effectA；id 非 0 且非命令位 1：slotTint > −0x20 时三色调 = slotTint、以本图调色板画于 aim、slotTint −= 8（held 时加回）返回 0，否则 slotTint 清零；法术未等待、ImageList 非空或 slotTint 非 0 返回 0；否则清法术、attackAct、recoilAct、后续、全部 trails、targets −1、slotTint、+0x2FF0、stage、+0x2BDE、0x00578474，0x00578470 加一，返回 1。
- 0x00485AD0 RollChance：r = Random(100)；返回 (u16)(百分比×70/100 + 10) ≥ (u16)r。

## BattleRole::SwarmSpellFrame（第 1096 行）— exact

- 0x00485B10（源码 RoleFrame::SwarmSpellFrameAdvance）：slotActs[8] 动作、+0x2FEC、id，Action；回弹与 Recoil 处理同 MagicFrame（Recoil 成功时 target、hold 清零、清位）；target 有 1 或 8：stage |= 0x8000、target 清零、取目标绘制位置（命令 actionFlags 位 2 时取 (0,0)）、ClearMoveAxes，unk2F0C（+0x349C）= unk2F24（+0x34B4），unk2F24 加一；stage 无 0x8000 返回 0；glowLevel 加一，(i16)glowLevel % 3 == 1 且队列数（+0x2F9C，无符号）< 8 时重新取目标位置，AddQueuedAction(castAct, unk2F0C, 位置 x + 0x004BF9B0[队列数 & 7], 位置 y + 0x004BF9C0[…], 目标 x, 目标 y − 0x28, 1, 0x34, 0, 0)、TrailLastQueuedAction(0, 0, 0xC)、出音 0x3E；Queued(unk2F0C) 不为 1 返回 0；stage 清零，法术未等待返回 0，否则 glowLevel、unk2F0C、unk2F24 清零，清 slotActs[0] 与 [8]，返回 1。两张偏移表与 exe 字节一致。

## BattleRole::LeapSpellFrame（第 1097 行）— exact

- 0x00485DD0（源码 RoleFrame::LeapSpellFrameAdvance）：slotActs[8] id、+0x2FEC、动作；stage 0 时 Action；stage 1 时动作加一，目标绘制位置（字读取）DrawActionAt；stage 2 时命令旗标 |= 8、动作 += 2，Home(id, 动作) 为 1 时 stage 3；法术等待时清零、stage 加一；target 有 8 时 spellStage 补 0x8000、target 清零；spellStage 有 1 且 stage ≥ 3（有符号）时 stage 清零、清法术，返回 1，否则返回 0。

## BattleRole::Strike、CancelCommand、Release（第 1098–1100 行）— exact

- 0x00485F30 Strike：slotTint 与 0x005373B4 清零；非鬼时 0x005373B4 = StepCounterProgress(0x004C6348, weaponId, weapon counterMax（+0x100）, weapon +0x42（+0xFE）)；status 0x80 时闪避值 8；evadable 为 1、evadeRolled（+0x305C）为 0 且目标 RollEvade(闪避值) 非 0 时 evadeRolled 1 返回；0x00575F34 清零后 = (i16)HitDamage，非 0 时加 (i16)0x005373B4，≥ 0x270F 截之；0x005378F4 += 伤害；伤害 ≠ −1 时目标 SetDamageDigits、StepTargetCount(1)；目标 RaiseFlag26B8；evadeRolled 1。
- 0x00486030 CancelCommand：commandFlags 清零、命令（0x2C）清零、skillName 前 16 字节、unk2F10、staminaCost、magicAction、unk2F16、unk2F18、三项回复、unk29A4、usedItem 清零；selectedItem 非空时光标节点 workCount（无符号）> 0 减一，selectedItem 清空。光标在表头时把表头当节点（deviations 该行改引汇编地址）。
- 0x004860D0 Release：+0x3118（unk2B24 的描述指针）非空时释放并清零；+0x00（battleRecord）非空时释放并清零。

## BattleRole::BoomerangFrame（第 1101 行）— exact

- 0x00486110（源码 RoleFrame::BoomerangFrameAdvance）：standAct 动作 0x2A 的姿势绘制同 DirectAttack（不设 hold）；target 无 1／8 返回 0；slotActs[0] 动作 0、id = 参数；目标绘制位置；stage 0：+0x349C 为 0 时 AddQueuedAction(id, 0, 位置, 目标, 0x1C, 0, −1, 4)、TrailLastQueuedAction(0,0,0xC)、+0x349C = 1；Queued(0) 为 1 时 +0x349C 清零、stage 1、目标 RaiseFlag26B8；stage 1：同样以 (目标→位置, 0x20, 7, −1, 4) 排队，Queued 为 1 时清 +0x349C、stage 2、出音 0x114；stage 2 时清零 stage、slotActs[0] 与 standAct，返回 1，否则返回 0。

## 符鬼命令小函数（第 1102–1110 行）— exact

- 0x004864C0 SetGhostOrder：+0x2BDA &= ~2；ghostOrder（+0x34B6）= 参数（字）；参数（双字）为 4 时 +0x2BDA |= 2（调用者 0x0046F9A3／0x0047186F 传菜单值 0x004BF0F8，取值为小整数）。
- 0x004864F0 GhostOrder：零扩展返回 ghostOrder。0x00486500 RollHalf：(i16)Random(100) ≥ 50。
- 0x00486520／0x00486530 GhostOrderIs5／Is6：ghostOrder 等于 5／6。
- 0x00486540 TakeSkillUsed：+0x2BDA 位 4 时清之并返回 鬼 +0x104（有符号）≥ 0x33，否则 0；0x00486570 TakeAttackUsed：位 8 时清之，ghostOrder 为 1 或 3 且 +0x104 ≥ 0x51 返回 1；0x004865B0 TakeHealUsed：位 0x10 时清之并返回 +0x104 ≥ 0x51；0x004865E0 TakeOrder7：位 0x20 时清之返回 1，否则 0。

## BattleRole::GhostObeys、RestoreGhostSpirit、HasCommandEffect、SetGhostActs（第 1111–1114 行）— exact

- 0x00486600 GhostObeys：+0x2BDA 无位 2 返回 0；(i16)(Random(100)+1) > 鬼 +0x104（有符号）时 +0x2BDA |= 1 返回 0；否则返回 1。
- 0x00486640 RestoreGhostSpirit：+0x2BDA 无位 0x80 返回 0；量 = (i16)(Random(10)+5) × 记录灵力上限（+0xC，movsx）/ 100；PushTarget(0x237E)、SetDamageDigits((i16)量)、StepTargetCount(1)、Damage(0, 量, 0)，返回 1。
- 0x004866C0 HasCommandEffect：目标 HasEffectBits(命令 ailment（+0x1A，零扩展)) 为 1 返回 1，否则 HasEffectBits(命令 +0x08) 为 1 返回 1，否则 0。
- 0x00486700 SetGhostActs：ghostActs（+0x34CC）= 参数，返回参数。

## BattleRole::ChooseGhostSkill（第 1115 行）— exact

- 0x00486710：ghostActs 非 1 返回 1；ghostSkill（+0x34B8）= Random(3)；鬼技能（+0x118 起双字）为 0 时 ghostActs 清零返回 1；载入该物件到 +0x3074、FreeDescription；unk2F10 = cost 位 & 0x7FFF；记录灵力（有符号）< unk2F10 返回 1；清命令、LoadCommand(技能脚本)；种类 8／4／0x1000 时 +0x2BDA |= 8／0x10／4；返回 1。

## BattleRole::ChooseGhostSkillOf（第 1116 行）— exact

- 0x004867F0：模式 1：鬼技能[2]（+0x120）为 0 则无；载入（不先清空）、FreeDescription、unk2F10 = cost & 0x7FFF，灵力 < 它则无；清命令、LoadCommand、+0x2BDA |= 4、ghostSkill = 2，返回 1。模式 2／3：前两个技能非 0 者清空 +0x3074 后载入，种类有 4（模式 2）／0x18（模式 3）者记合格；两个都合格时 ghostSkill = Random(2) | 0x8000，否则取第一个合格者 | 0x8000，都不合格时 ghostSkill 保持原值；ghostSkill 无 0x8000 时清零并无；否则按其低 15 位载入技能（不先清空），灵力不足则无；清命令、LoadCommand、+0x2BDA |= 0x10／8、ghostSkill = 序号，返回 1。其余模式与「无」：unk2F10 清零、命令清零，返回 0。

## BattleRole::TakeGhostOrder（第 1117 行）— exact

- 0x00486B60：按 ghostOrder − 1 走 8 项跳转表：1 ghostActs = RollHalf，为 1 时 ChooseGhostSkill；2 +0x2BDA |= 0x80、ghostActs 0、RestoreGhostSpirit；3 ghostActs 1、ChooseGhostSkillOf(3)；4 +0x2BDA |= 2；5／6／8 ghostActs = RollHalf，为 1 时 ChooseGhostSkillOf(3)；7 +0x2BDA |= 0x20；其余不动。

## BattleRole::GhostSkillCommand、GhostAttackCommand（第 1118–1119 行）— exact

- 0x00486C40 GhostSkillCommand：skillName 前 16 字节清零；+0x2BDA 有 0x40 时清之返回 0；ghostActs 非 1、技能[ghostSkill] 为 0 或载入后灵力 < cost & 0x7FFF 时 GhostAttackCommand 并返回 0；否则清命令、LoadCommand(+0x3E)，act = +0x54，skillName 前 16 字节清零后 lstrcpy 名字，unk29A4 += 命令值（+0xE），SetCommand(2)，commandFlags |= 0x8000，kind 出参 = 种类有 0x1000 或 4 时 1 否则 0；useKind 为 0 时三项回复，healPercent 清零，useKind 0x200 时置 1 并取回复；返回 1。
- 0x00486E10 GhostAttackCommand：skillName 前 16 字节、命令清零；LoadCommand(weapon 技能脚本 +0xFA)；SetCommand(1)；+0x2BDA &= ~1；unk29A4 += 攻；命令旗标有 0x18 且 commandA 为 1 时 commandB = 1、commandA = 0xC8。常量 kHealCommandFlags／kHealCommand 按其意义（走路位、走路命令 0xC8，与 PrepareCommand 同）改名 kWalkingFlags／kWalkCommand。

## BattleRole::SpendGhostSpirit、LevelGapRoll（第 1120–1121 行）— exact

- 0x00486E90 SpendGhostSpirit：费用 = 基数×百分比/100 − 减量，存 +0x2EB6（字）；记录灵力（有符号）< 费用时：< 费用×40/100 则 +0x2EB6 清零返回 0，否则 +0x2EB6 = 灵力；灵力 −= +0x2EB6；SetCommand(2)、commandFlags |= 0x8000、+0x2BDA = (+0x2BDA & ~1) | 0x40，返回 1。
- 0x00486F40 LevelGapRoll：skillName 前 16 字节清零；差 = (u16)等级参数 − 记录等级（字节）：≥ 0x14 时 (u16)Random(100) < 0x55 返回 1；≥ 0xF 且目标数据五个计数（+0x6E／+0x72／+0x76／+0x7A／+0x7E）都为 0 时 Random < 0x32 返回 1；生命 < 上限/3（有符号）且差 ≥ 0xA 时 Random < 0x28 返回 1；字差 1..8 返回 0，9..15 时 Random < 0xA 返回 1，16..19 时 Random < 0x14 返回 1，否则 0。

## BattleRole::LowHitPoints、SetElement、SetUnk34C0、Unk34C0、LowSpirit、LowStamina、StartSpecialAct（第 1122–1128 行）— exact

- 0x00487090 LowHitPoints：参数 0 时 roleRecord 生命 < 上限/2（有符号），参数 1 时 < 上限/3，否则 0。
- 0x004870F0 SetElement：AccessoryProficiency(0x26)；at = (i8)元素 − 1；at 等于战斗记录 +0x36（抗性[9]）时战斗记录抗性[at] = +0x38 字节，否则 = (i8)+0x38 × (u16)熟练度/100 的低字节；战斗记录抗性[(i8)队伍记录 +0x37] = 0；队伍记录 +0x37 = 元素 − 1。下标无界（deviations 0x004870F0 已登记）。
- 0x00487160 SetUnk34C0：+0x34C0 = 参数（返回值调用者 0x00470586 不用）；0x00487170 Unk34C0：movsx 返回。
- 0x00487180 LowSpirit／0x004871A0 LowStamina：roleRecord 灵力／体力 < 上限/4（有符号）。
- 0x004871C0 StartSpecialAct：equipmentFlags 无 0x80 返回 0；slotActs[10]（+0xCF4）id 0x17BF、动作 0；0x005772BC = 1、+0x34D0 = 1，返回 1。

## BattleRole::SpecialActFrame（第 1129 行）— exact（修正后）

- 0x00487200：equipmentFlags 无 0x80 或 +0x34D0 非 1 返回；0x005772BC = 1；slotActs[10] Run 失败返回；出音其 view、声像 0x30，清的是 standAct.view（+0x3B0，原有 BUG 保留）；取图（无空判）画于 (位置 x − act.x, 位置 y − act.y)；未等待返回；+0x34D0、0x005772BC 清零，ResetTurn（0x0048B7A0）、ExpireEffects(1)（0x00494840），equipmentFlags &= ~0x80，defeated 清零，AccessoryProficiency(0x3A)；roleRecord 生命 = 上限×40/100，再加 (u16)熟练度 × (上限×90/100) / 100（字加）；condition &= 0x7FFF；PushTarget(0x237D)、SetDamageDigits(生命)、StepTargetCount(1)；slotActs[10] 清零。
- 修正：原程序以 32 位算 40×上限再除以 100（0x00487326–0x00487341），源码原先把 40×上限截成 i16 再除，上限 > 819 时生命不同。
- 待核（截位乘积）：本批已四次发现源码把乘积先截成 i16／i8 再除而原程序以 32 位计算（SetupGhost、ApplyAccessory、UseAccessorySkill、SpecialActFrame）。同类写法尚有 battle_effects.cpp（cut×攻／防）、battle_unit.cpp（kEnragedSharePercent×attack）、battle_role.cpp Grown 与 SetupGuest 的等级、敏捷；验证到这些函数时逐一按汇编确认乘积宽度。

## BattleUnit::ZoomEntranceFrame（第 1130 行）— exact

- 0x004873C0：暗色三字 −12、0x00577290 = 1、DarkenByTint（0x004655A0 即置 0x005777F4 = 1，源码直接置位）；standAct 动作 0x24、id = 参数；stage 0：Run 失败返回 0，取图，锚单位绘制位置，SetZoom(act.x, act.y, 0xC18 − glowLevel, 同)，画于 (x − 宽 + act.x, y − act.y)、旗标 | 0x81（无空判）；glowLevel += 0x28，≥ 0x800（有符号）时清零、stage 1、清 standAct；返回 0。stage 1：id 0x1C98、动作 0，Run 失败返回 0，取图，画于 (x − act.x, y − act.y)；等待时 glowLevel、stage 清零、清 standAct 返回 1，否则返回 0。

## RaiseFlag26B8Bit17、Flag26B8Bit17、SelectedItemId、LevelInReach、HitPointsAtMost（第 1131–1134、1136 行）— exact

- 0x004875E0／0x004875F0：+0x2BC0 |= 0x20000；返回其位 17。0x00487600 SelectedItemId：selectedItem（+0x3460）的 id 字（无空判）。
- 0x00487610 LevelInReach：+0x3074 清零后载入物件 id、FreeDescription；高 = 物件等级（+0x58，零扩展）− 记录等级（字节），低 = 其反；范围（u16）≤ 0x14：高 1..5 或低恰为 5 返回 1；0x15..0x28：高或低 1..10；0x29..0x3C：1..20；0x3D..0x50：1..35；0x51..0x64：1..50，命中返回 1；否则 0。
- 0x00487A70 HitPointsAtMost：roleRecord 生命 ≤ 上限 / 参数（有符号整除）。

## BattleUnit::CastPoseFrame（第 1135 行）— exact

- 0x004877A0：standAct id = actId、+0x2FEC = 1、动作 0x34；Run 失败返回 0；取图（无空判）；anchorX、原点 x 与 facing 镜像；影高 /3、斜切 >>2、色调 −6（autoControl 时 −1）；出音、声像 位置 x（字，有符号）≥ 0x140 时 0x30 否则 −0x30；shadowMode、view 清零；影与本体绘制；target 无 1／8 返回 0；参数 ≤ 0 返回 1；slotActs[0] 动作 0、id = 参数，Run 失败返回 0；出音其 view、取图、view 清零，画于 (0, 0)；其等待时清 slotActs[0] 与 standAct 返回 1，否则返回 0。

## BattleRole::TakeRandomItem（第 1137 行）— exact（修正后）

- 0x00487A90：ShowAllItems 后逐项计数，种类（+0x62）0xB..0xE 时置标志；无则返回 0；roll = Random((u16)计数) + 1（0x00487AD5 `and ebx, 0FFFFh`）；重新遍历，步数（字）≥ roll（0x00487B1B 字比较）且种类合格时选中，走到尽头时步数清零、重掷、重新遍历；数量非 0 减数量，否则待入非 0 减待入；两者皆 0 时 UnlinkItem(inventoryHead, id)；返回节点 id（释放后读取，deviations 0x00487A90 已登记）。
- 修正：源码原先以 u32 计数调用随机数、以 int 保存 roll；改为与原程序相同的低字。

## BattleEnemy 构造／析构、AdvanceGauge、Setup、Reload、Unk62And8A（第 1138–1143 行）— exact

- 0x00487B90：BattleUnit 构造（0x0048B040）后分配 0xA8 字节清零的物件记录存 +0xC；0x00487BF0：Release（0x00489410）后 BattleUnit 析构（0x0048B100）。
- 0x00487C40 AdvanceGauge：marks 有 0x40 或 0x4000、或 +0x2BC0 位 31 时返回 0；gauge（+0x2F2E，零扩展）≥ 0x004BEFF8（有符号）时 +0x2FF0 = 1、+0x302C = 0，+0x3068 为 1 则清之并 facing = 0，返回 1；否则 step = (加速量 + 敏捷（+0x5E 零扩展）− 减速量)/4（有符号），参数 1 时 step += step/4 + 1；隐身时 loss = (u16)熟练度×step/100/2 + 4；+0x2FF0 = 0、gauge = gauge − loss + step（字），返回 0。
- 0x00487D60 Setup：placement 8 dword 复制到 +0xFC4 与 +0xFE4，+0x2BBC = 0，LoadObject，SetDisplayName（0x0049B170），kind = 放置 +0x1C，+0x50 = +0x68（dword），evadeSkill = +0x9A，actId = +0x54，为 0 时用放置 +0x14；+0x2B8C = 0；LoadCommand(+0x64)、FreeDescription；探测：+0x3070 = 0，attackAct（+0x544）id = actId、动作 0x26，Run 失败 +0x3070 = 1，清 attackAct；+0x2FDD = 0，三个物品计数皆 0 时 1，两个技能计数皆 0 时 2。
- 0x00487E90 Reload：LoadObject、+0x50 = +0x68、evadeSkill、LoadCommand、FreeDescription，actId = +0x54，为 0 时用参数 id；同上探测。
- 0x00487F80 Unk62And8A：出参一 = +0x66，出参二 = +0x90；+0x2BC0 有 0x200000 时出参一 += (u16)+0x66×30/100（字加）。
- object_record.h 的 maxHitPoints 注释改为 swd3e 地址。

## Game::EnemyAttackFrame（第 1144 行）— exact（smooth/smooth_damage.cpp EnemyAttackFrameAdvance）

- 0x00487FE0 全部分支逐条对照：+0x2F90 非 0 返回 0；+0x2FEC = 1，hold 由 held／target 0x200 置；命令 0x8 且未淡入时 DrawFadeOut（0x004903B0）、+0x2BC0 位 28 时 SetFacing(1)、DrawFadeIn（0x004906F0）、fadedIn = 1；动作 0x28，+0x2FB6 非 0 时 0x30、为 2 时 0x31；Action（0x0049A220）失败报 "Attack Act" "[%s]QQ:%d號錯誤!!"（0x004BFA5C，CP950 核对）。
- +0x2BC0 位 16：清 standAct，shadowMode = drawFlags & 0x80000003，红 = glowLevel，≥ 15 时绿蓝 = glow − 15（其余沿用全局残值，painter 选项共享同样保留），画 (actX, actY)、| 0x10；++glow ≤ 0x1F 返回 0；否则目标绘制位置、SetPosition，glow 清零、dying = 1、fixedPose = 2、hp（+0x68 dword）= 0、0x00577360 = 1，清 standAct／castAct／recoilAct／attackAct／slotActs[1]／trails（0x130 dword）／targets（−1×4 dword）、stage／+0x2FF0／fadedIn／nudge，gauge += Random(0x78) + 10，返回 1。
- Recoils、Recoil 完成、FollowsUp（FadesField 以六个 float 加一个 int 调 0x00467C40；TargetsUnit 时 AddQueuedAction 十个参数顺序核对）、Quakes（0x0057730C = 1、0x00577030 = 0）、SlidesTo／SlidesBack（0x00468440／0x00468460）、UnkTarget4 与两处打击（0x11 位；status 0x80 时 chance 8；未掷时 RollEvade；位 1 时 RaiseFlag26B8；闪避 EnrageCheck(0x1B, 0)；伤害 movsx，≥ 9999 截 9999，累加 hitDamage，−1 跳过，否则 SetDamageDigits，DrainKind ≤ 1 时 StepTargetCount(1)；第二处不回写 chance，此后亦不再读，等价）。
- 暗色三字取 act +0x3C0..+0x3C4，DarkenByTint，非零时 0x00577294 = 1；stage 无 0x8000 返回 0；castAct 等待且 HitPicturesDone：id 0、TargetsUnit 或 slotTint ≤ −32 时 attackAct 等待且 images 空则清理返回 1，否则 0；否则三色 = slotTint，shadowMode & 0x2C 时三色 0、slotTint = −32，以影图调色板画于 aim，slotTint −= 4，held 时加回。其余：castAct id = 命令 +0x1018 或 effectA，0 时置等待；TargetsUnit 时 Queued（位 24 时 facing 暂置 1，完成后清位 24），否则 AimAction；以本图调色板画；AddsPicture 时 AddHitPicture(effectA, effectQ, aim + anchor, 0)；DrawHitPictures、HitBits 并入，打击。
- 三色全局对应：0x004ED9A4 红、0x004EC180 绿、0x004EC178 蓝（FieldView::DrawRole 0x0041419D 与 cellTint 三全局的对应）。

## CanHeal、IsWeakEnemy、ChooseHeal、LoadSkillAction、PrepareCommand、LoadEnemyCommand（第 1145–1150 行）— exact

- 0x00488AB0 CanHeal：+0x2BC0 有 0x200000 返回 0；+0x82 或 +0x86 非 0。
- 0x00488AF0 IsWeakEnemy：开头 `and eax, 200000h; cmp eax, 1` 永不成立（原有死检查，源码省略等价）；生命 = +0x2BBC 非 0 时用之否则 +0x68，< +0x50/3（有符号）。
- 0x00488B30 ChooseHeal：marks 0x100 或 +0x98 为 0 返回 0；roll = (u16)Random(10)：≤ 4 时 +0x82 非 0 则 healChoice = 1 返回 1；5..9 时 +0x86 非 0 则 2 返回 1；其余返回 0（> 9 的分支不可达）。
- 0x00488BB0 LoadSkillAction：LoadObject(+0x14)，出参 = (u16)其 act，lstrcpyA(skillName, 其名)，LoadCommand(+0x52 即技能脚本)，unk29A4 += 命令值，SetCommand(2)，commandFlags |= 0x8000。偏差行 0x00488BB0 改为 swd3e 偏移：技能名 16 字节（+0x2B38），其后 +0x2B48 是 FlyAct 步数（0x00495E39 起），+0x2B68／+0x2B6C 仅由 0x004917DE 清零；battle_unit.h 注释与 unk2660／unk2664 名称同步为 swd3e 偏移（unk2B68／unk2B6C）。
- 0x00488C20 PrepareCommand：+0x2FB6 非 0 时命令旗标无 2 返回 0，有则 commandFlags |= 0x8000、SetCommand(2)、返回 castAct；否则 unk29A4 = 0、命令 0xB dword 清零、LoadCommand(+0x64)、unk29A4 += +0x5A、SetCommand(1)，返回 0。
- 0x00488CA0 LoadEnemyCommand：命令清零、unk29A4 = 0，LoadCommand(选择 1 时 +0x78 否则 +0x7C)，unk29A4 = 命令值；旗标有 2 时出参 = castAct、commandFlags |= 0x8000、SetCommand(2) 返回 0；否则 +0x2FB6 = 选择、SetCommand(1) 返回 1。

## RollDrop、CatchRoll（第 1151–1152 行）— exact

- 0x00488D40 RollDrop：+0x88 为 0 或 (u16)Random(20) ≥ +0x8A 返回 0，否则 +0x88。
- 0x00488D80 CatchRoll：marks 0x800 返回 −1；Random(12) 结果不用；+0x9B 为 0 返回 −1；StatusBit5（0x0049A950）为 1：Random(2) 为 0 返回 −1，Random(10) < 3 返回 1，否则显示名字（wsprintfA "%s" ObjectName 到 +0xF84，Messages.Add(0x118, 0xA, 0x28, 0x40000002)）返回 0；+0x2BC0 有 0x4000：Random(10) < 8 清位返回 1，否则 −1；差 = (u8)等级 − 等级（+0x58 零扩展）：> 10 时 Random(100) < 0x50、> 5 时 < 0x32 返回 1，失败 EnrageCheck(0x13, 0) 后 StatusBit5 为 1 显示名字返回 0，否则 −1；< 5 返回 −1；= 5 时 roll = Random(10)，StatusBit5 为 1 显示名字返回 0，roll < +0x9B 返回 1，否则 EnrageCheck(0x13, 0) 返回 −1。

## RunAway、ItemName、SkillName、IsSmallMachine、CommandValues、SetResistances、SetWords66（第 1153–1159 行）— exact

- 0x00488F50 RunAway（smooth_unit.cpp RunAwayAdvance）：fadeLevel（+0x2B70）≤ 6 时 defeated = 1、清 recoilAct 返回 1；recoilAct 图号为 1 时出音 0x10F；id = actId、+0x2FEC = 1、动作 0x24、hold 依 held；Run 失败返回 1；取图，frameFlags = act 旗标，actOffsetX = act.x；fadeLevel 为 0xF 时出音 0x2F、声像依 facing ±0x30、view 清零；frameFlags ^= 1，facing 为 1 时再翻转且 act.x 非 0 时 actOffsetX = 宽 − act.x；位置 x ±0x19（facing 1 加）；以本图调色板画于 (位置 x − actOffsetX, 位置 y − act.y)；fadeLevel 减 1 返回 0。
- 0x00489180 ItemName：选择 0／1／2 取 +0x6C／+0x70／+0x74，为 0 或选择越界返回 0；LoadObject(+0x14)，出参 id 一律取 +0x6C（原有 BUG 保留），lstrcpyA 名字，返回 1。0x00489260 SkillName：选择 0／1 取 +0x78／+0x7C，同上（无 id 出参）。
- 0x004892D0 IsSmallMachine：种类（+0x20）有 0x20 返回 0；无 0x800 返回 0；+0x56 ≤ 10 返回 1。
- 0x00489300 CommandValues：出参一 = 命令 +0x22（+0x1026），出参二 = 命令 +0x2A 字节零扩展。UnitCommand 的 unk1E（+0x20）、unk20（+0x22）按 swd3e 偏移改名 unk20、unk22。
- 0x00489320 SetResistances：五个字参数的低字节写 +0x9C..+0xA0。0x00489350 SetWords66：六个字参数非 0 时分别写 +0x6C..+0x76。

## SetWords72、BattleEnemy::Release、HitPointsAndAttack、OutOfChoices、Game::EnemySkillFrame（第 1160–1164 行）— exact

- 0x004893C0 SetWords72：四个字参数非 0 时写 +0x78／+0x7A／+0x7C，第四个也写 +0x7A（原有 BUG 保留）。
- 0x00489410 Release：+0xC 非空时释放并清空。0x00489430 HitPointsAndAttack：+0x68（dword）+ (u16)+0x5A。
- 0x00489440 OutOfChoices：+0x2FD9 为 1 且 +0x6E／+0x72／+0x76 皆 0 返回 1；+0x2FDA 为 1 且 +0x7A／+0x7E 皆 0 返回 1；否则 0。
- 0x00489490 EnemySkillFrame（smooth_damage.cpp EnemySkillFrameAdvance）：+0x2FEC = 1；命令 0x8 且未淡入时淡出、位 28 SetFacing(1)、淡入、fadedIn = 1；standAct 动作 = +0x2B98、id = actId，Action（结果不用）；Recoils（effectA 非 0 时 recoilAct id／动作、stage |= 0x4000；Holds 时 hold = 1；清位与 effectA／Q）；Recoil 完成时 target、hold 清零、stage &= ~0x4000、清 recoilAct；FollowsUp 时 FadesField 调 0x00467C40（六 float 一 int）并清位，spellStage 无 0x8000 时置位；spellStage 有 1 时 stage 清零、清 standAct 返回 1，否则 0。

## ObjectUnkA2、FirstResistance、Named、ClearTurnCommand（第 1165–1168 行）— exact

- 0x004896C0：+0xA2 字节。0x004896D0：+0x9C 起五字节中第一个非 0 的序号 + 1，皆 0 返回 0。0x004896F0：种类（+0x20）低字节位 5。
- 0x00489710 ClearTurnCommand：命令 +0x18（+0x101C）、+0x2BE8 字清零，+0x2B98、+0x2FF4、+0x3024 dword 清零，skillName 前 16 字节清零。

## BattleUnit 构造／析构与计量、位置函数（第 1189–1203 行）— exact

- 0x0048B040 构造：targets（+0x2F72）四 dword −1，+0x2FBC 字 0，unk29A2（+0x2EAE）0xFFFF，fadeLevel 0xF，brightLevel 1，glowLevel 0，fadeStep（+0x2F26）4，flashPhase（+0x2F84）2，flashFall（+0x2F86）0x18，+0x2E60／+0x2E62／+0x2F82／+0x2F2A／+0x2F88 字与 +0x2FC6 字节清零，colorPair（+0x2BC4）0x062B062B，effects 头（+0x2A7C）0，skillName 前 16 字节与自身物件（+0x14，0xA8 字节）清零；与成员默认值逐项对照（其余成员在原程序中位于零初始化的全局单位表）。0x0048B100 析构：自身物件说明文字（+0xB8）非空时释放。
- 0x0048B130 SetTarget：+0x3024 = 参数并返回。0x0048B140 GaugeLength：ftol((u16)gauge / 0x004BEFF8 × 62.0f)。0x0048B170 FillSecondGauge：命令 +0x18 非 0 时 +0x2BE8 = 0x960，否则 0。0x0048B190 SecondGaugeLength：ftol((i16)+0x2BE8 × 0.00041666668f × 62.0f)（两常量 od 核对；53 位精度下与 double 相同）。0x0048B1C0 ResetGauge：gauge = 0x004BEFF8 低字。0x0048B1D0 RandomizeGauge：gauge = 360/(Random(10) + 1) + 50；参数 1 时 kind 为 0 则 gauge = 满 − 1 并返回 kind（0），否则 gauge = 满 − 300 并返回之；参数非 1 返回参数。
- 0x0048B230 GetDrawPosition：+0x2BE2 非 0 时取 savedPlacement 的 x／y（+0xFFA／+0xFFC），否则 placement（+0xFDA／+0xFDC）。0x0048B280 GetOffset：主动作锚点（+0x32E／+0x330），commandFlags 0x200 时 slotActs[8]（+0xC26／+0xC28）；facing 1 且主动作锚点 x 非 0 时 x = 阴影图宽 − 主动作锚点 x；kind 0 且 commandA（+0x2F88）为 4 时取 standAct 锚点。0x0048B310 GetOrigin：位置 − origin（+0x2EC2／+0x2EC4）；0x200 时 HasFixedOrigin（0x00491CE0）为 1 则 (0xF0, 0x12C) − slotActs[8] 的 x／y，否则 位置 − 其 x／y。0x0048B3A0 GetShadowRect：defeated 1 且 kind 0、或 actId 为 0 时返回；动作 0x24，主动作动作非 0 时用之，defeated 1 时 0x27；Run、取图，facing 1 时 x = 宽 − x；矩形 = (位置 x − x, 位置 y − y, 宽, 高)。0x0048B4A0 Move、0x0048B4C0 SetPosition（placement 8 dword 复制到 savedPlacement）、0x0048B500 GetPosition。
- 空指针偏差：补登记 0x0048B280、0x0048B3A0，以及本批前面的 0x00487200、0x004873C0、0x004877A0、0x00487FE0；0x00488F50 行引用汇编。全工程扫描源码中的空指针注释，补登记已验证函数 0x00459200、0x004594B0、0x00477A00、0x00477EE0、0x004838A0、0x00458670、0x00458BB0；未验证的 0x0048CA10、0x00497350、0x004975E0、0x0049C0D0 在验证时登记。

## StepIdleAction、SelectEntry、取值函数、ClearIdleAction、SetCommand、ClearFlag26B8（第 1204–1214 行）— exact

- 0x0048B520 StepIdleAction：stepAct（+0xEE0）= mainAct（+0x2B4），Run 失败返回 0；否则 picture = GetPicture(+0xF2A, +0xF2C) 并返回。0x0048B570 SelectEntry：参数非 0 返回 +0x0（battleRecord），否则 +0x4（roleRecord）。
- 0x0048B590／0x0048B5A0：+0x2FEC／+0x2FF4。0x0048B5B0／0x0048B5C0／0x0048B5D0／0x0048B5E0：commandA（+0x2F88）、commandB（+0x2F8C）、+0x2F90、+0x2EAE 字。0x0048B5F0 ClearIdleAction：mainAct 清零。0x0048B680 ClearFlag26B8：+0x2BC0 &= 0x7FFFFFFF。
- 0x0048B610 SetCommand：2／0xD／3／0x29 时 commandB = 命令、commandA = 0；0xE／0xF 时 commandFlags |= 0x4000，6／0x50 时 |= 0x800，再同前；其余 commandA = 命令；都返回 1。
