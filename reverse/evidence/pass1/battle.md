# 第一遍验证：战斗模块

## 与正传相同的 43 个函数（第 830–985 行之间）— exact

asm_identity 判为与正传相同（battle_identity.txt），且源码与正传重建中已为 assembly_exact 的版本相同：ItemPriority 0x004575E0、SetGaugeFull 0x00457610、BattleHud::DrawTileBox 0x00457630、DrawPictureFrame0 0x004578C0、DrawBar 0x00457B00、DrawPictureFrame 0x00457B50、DrawGauge 0x00457BA0、DrawGaugeAlt 0x00457C20、DrawGaugeWide 0x00457CA0、DrawNumber 0x00457E10、DrawNumberDigit 0x00457F70、FillColor 0x004580C0、DrawEffectAt 0x004580F0、DrawCursorAnimation 0x004581D0、CreateBattleSurfaces 0x00459B50、ReleaseBattleSurfaces 0x00459BA0、Game::FadeFieldOut 0x00466290、SortItems 0x00467AB0、SortSkills 0x00467B00、Game::FieldFadeFrame 0x00467B50、Game::QuakeFrame 0x004695B0、Game::ReturnBattleItems 0x004697A0、BattleState::SetReturnedItem 0x00469BA0、BattleFile::ReadHeader 0x0046B260、Game::SelectPreviousRole 0x0046F380、Game::SelectNextRole 0x0046F400、Controls::PressConfirm 0x00472C60、Game::NextSelectableRole 0x00472C90、Game::PreviousSelectableRole 0x00472D00；以及 14 个全局初始化 thunk（0x00459880、0x004598B0、0x004598E0、0x00459910、0x00459940..0x004599D0，构造／析构登记，与正传逐条相同，映射到 Game::Game）。

源码差别：FillColor 多一个守卫（颜色低字等于 RLE 标记时不绘制，已登记 deviations.md 0x004580C0）。
