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
