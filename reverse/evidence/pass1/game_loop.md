# 第一遍汇编验证：主程序循环（0x0040A140 起）

## 0x0040A140 Game::Run — exact（平台替换已登记）

- 原程序：已有 "SoftstarSwd3" 类窗口即返回 0；找到 Ghost.exe 窗口（"SsSwd3eg"/"Ghost"）发 0x401 并 Sleep 1000；安装命令行（0x00411C10）返回 1 时返回 0；错误日志写分隔线（0x004B0B3C）；CoInitialize、注册窗口类（图标 0x7D、光标 0x67、黑刷）、CreateWindowExA（640×480，WM_CREATE 中做开始检查），ShowWindow、UpdateWindow、SendMessageA(0x404) 进入开始流程；srand(time(0))，再 Random::Seed(time(0))（0x0043DAC0）；running=1。
- 消息循环：有消息则 GetMessage（WM_QUIT 时返回 wParam）、Translate、Dispatch；无消息时 running≠0：系统旗标 0x20 → 视频帧（0x0049C800），旗标位 0 或 inactive → Sleep(0)，否则 MainFrame；running=0：active==1 → 暂停横幅（0x00411F20），否则 Sleep(0)。
- 实现：SDL 窗口与事件循环；开始检查（CheckStart）后 Start；先 crtRandom.Seed(time) 再 random.Seed(time)；LoopFrame 的分派条件与上述一致。单实例检查、Ghost.exe 通知、安装命令行、消息循环形态见 deviations.md 0x0040A140 各条。

## 0x0040A380 Game::HandleEvent（窗口过程）与 Game::Start（消息 0x404）— exact（平台替换已登记）

- 旗标 0x50（输入框）时先交给 TextInput::HandleMessage（0x0043D620），返回 0 即视为已处理；WM_KEYUP 等未处理的消息（返回 1）照常继续，故 F8、P 键不受输入框影响，与实现先处理 F8/P 等价。
- WM_KEYUP：F8（0x77）切换 running；视频中 Esc（0x1B）停止视频；旗标 1 且系统旗标位 0 未置时 P（0x50）：剩余空间 >64 MB 则 MessageBeep、取第一个不存在的 "%sScrnShot\%05d.bmp"（0..0x1869D，全占用时为最后一个）、存 640×480 BMP。
- WM_SIZE（旗标 1）：wParam 4/3/1 → Activate(0)，2/0 → Activate(1)；WM_ACTIVATEAPP（旗标 1）：按 wParam 激活/停用；WM_CREATE：记窗口、CheckStart 失败则发 WM_DESTROY；WM_DESTROY：旗标 1 或系统旗标 4 时 ReleaseGame，CoUninitialize，PostQuitMessage；WM_SYSKEYDOWN 回车：active==1 时 Activate(0) 否则 Activate(1)；WM_SYSCOMMAND 0xF130/0xF140/0xF170/0xF090 吞掉。
- 0x404：PlaySound 资源 0x78；SetDefaultKeys；InitInstall；清旗标 0x12，AnyExists 时置 0x12；启动器对话框 0x6B：1（继续）→ StartGraphics、menuState=0、menuMode=3、storyState=3、存档页 1、读档、ResetSaveSlots、FindNewest；2（开始）→ StartGraphics；6（离开）→ WM_DESTROY。
- 实现中 Alt+Enter、WM_ACTIVATEAPP、启动音效时机、启动器可跳过等见 deviations.md（0x0040A380 各条）。资源缺失提示文字由 swd32.exe 改为 swd3e.exe。

## 0x0040A7B0 Game::MainFrame — exact（修正后；帧时钟偏差已登记）

- 系统旗标 0x10 时直接返回；active==0 时 Sleep(0) 返回；0x004C30D8=time(0)（无读取者，成员由 `unk4A99FC` 改名 `frameWallTime`）；now=timeGetTime，未满帧间隔返回（帧时钟按 deviations.md 0x0040A7B0 的固定步长增强）。
- 设备旗标位 0 时清旗标 3、4。menuState==0 时两个计时器（旗标 0x10：计数减 1，到负且领队空闲、无菜单、无存档画面时置 −1、清 0x10/0x12、置 0x11、释放场景各列表与选项、角色 1..n−1 ReleaseRole 并清 talkState、领队设为计时事件；旗标 0x4A：计数到负置 −1、清 0x4A、置 0x4B）。
- 帧开始与帧间隔记录，PollKeyboard、Controls::Poll，指针动作 0xD；小游戏进行中：Frame 返回 1 时清 0x80，帧末；存档画面：counterWait=0x400、counterAlpha=0x10、SaveScreenFrame，帧末。
- 未在战斗、旗标 0xD 未置、battleRequest 位 31 置位：去掉位 31，非 0 则进入战斗（保存音乐模式、停音效、ActArchive::CloseAll、TswArchive::Flush、解除映射（失败记 0x004B0BFC），view 无论成败清零，CloseMapping，StartBattle，inBattle=1，角色 1..n−1 清 0x1000000）。0x004E581C=0（无读取者，改名 `frameUnkCleared`），battleRequest=0。
- 战斗中：FightTalkFrame 结果 0：清 battleRequest/inBattle，三种字体按屏幕重设，talk.unit=0xB，重新映射（失败记 "After fight ,Remapping map fail!"，0x004B0BD0），音乐切回 Switch(之前模式,1,1)；结果 2：清 0x10/0x12、置 0x11、清 inBattle/battleRequest、重设字体与 talk.unit、重新映射（不检查结果、不记日志）、领队运行事件 0x63；结果 3：清 inBattle/battleRequest、清 0x11、重新映射（失败记日志）；之后 AudioPump 并返回（不做退出检查）。
- 不在战斗：MusicCue；菜单中：counterWait=0x400、counterAlpha=0x10、字距 0x16/0x12/0x10，菜单号 1、3..6 → Menu::Run，2 → 商店；帧末。地图：变量 2 非 0 时鬼魂计数到 900 归零、日数循环、饱食度减 3+Below(5)（不低于 0）；HandlePointer、ControlPlayer；running、inactive、菜单、战斗、存档画面任一时只记帧耗时并帧末；否则帧计数 >20 时归零并使时钟 +1（>1000 归 0）；运行领队事件；系统旗标 0x25 均未置时 RolePath::Update、设字距、FieldView::Render；记帧耗时；帧末：AudioPump，系统旗标 4 时发 WM_CLOSE（实现为 Destroy）。
- 差异（已修正）：败北后的重新映射原程序不检查结果、不记日志，实现原先与另两种情况共用会记日志的版本；已分开。重复的注释段落删除。

## 0x0040AEC0 Game::Activate — exact（窗口操作为平台替换）

- 先 ClearFrameInterval。停用：StopAllSounds；inBattle==1 时 ReleaseBattleSurfaces；三种字体 Shutdown；active=0、inactive=1；最小化窗口（平台）。激活：还原窗口与位置（平台）；active=1；恢复主表面与离屏表面（平台）、Painter::Init(离屏)；三种字体按屏幕重设；inactive=0；inBattle==1 时 CreateBattleSurfaces；等待主表面翻页链恢复（平台）；SetFrameInterval(0x23)。
- 实现在主表面尚未建立时直接返回：原程序只在旗标 1（图形已启动后）处理这些消息，此分支不会出现。
