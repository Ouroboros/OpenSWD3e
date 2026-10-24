# 第一遍汇编验证：RoleControl::ControlPlayer（0x00402AA0）

结论：exact（修正后）。依据 reverse/analysis/asm/00402AA0.asm 全文对照；0x00403DAC 为函数尾声（各处跳转到它即返回）。

## 调试键

- debugMode（0x004AF7A0）为 1 且右 Ctrl、右 Shift、Home、End 同按：置旗标 0x52。旗标 0x52 时：
  - F12（0x58）且 leader.id 为 0xFFFF、talkWindows（0x004C66F8）为空：Sleep 500，storyState=3，menuMode=1，存档页=0，slotsLoad=0，ResetSaveSlots，返回。
  - Ctrl（0x1D 或 0x9D）：F（0x21）且旗标 0xC：battleRequest=RollEncounter(encounterSteps,1)，为 0 返回；否则 StopAllSounds（0x00427080）、ActArchive::CloseAll（0x00437D80）、TswArchive::Flush（0x004367D0）、StartBattle、inBattle=1、Sleep 300、解除地图单元映射（失败记 0x004AF2FC）、关闭映射、角色 1..roleCount-1 清 0x1000000 位，返回。F9（0x43）：skipAll=(skipAll+1)%2，Sleep 350，日志 0x004AF2EC/0x004AF2DC，返回。Q（0x10）：inactive=1、ClearFrameInterval、systemFlags|=4、清旗标 1，返回。S/L（0x1F/0x26，空闲时）：存档页=1、slotsLoad=0/1、Sleep 500、menuMode=1/2、storyState=3、ResetSaveSlots，返回。F1/F2（0x3B/0x3C）：debugText/debugCells 取反，debugKeyFrames=0，Sleep 300，返回。F3（0x3D）：旗标 0x14 取反、debugKeyFrames=0、日志 0x004AF2D0/0x004AF2C0、Sleep 300，返回。Tab（0x0F）与小键盘 +/-（0x4E/0x4A）：layerFrames（0x004AF714）±1，夹在 1..0x40，debugKeyFrames=0，继续。
  - F4（0x3E）：running=0、ShowCursor(1)、对话框 0x6A；返回 0 时 running=1；都返回。
  - F7（0x41）：systemFlags|=0x80、Sleep 250、AscentGame 开始（0x64,0x2332,0,0x2332,1,0x2332,2,0x64）与 8 个障碍（动作 3,4,5,6,3,4,5,6），继续。
  - F6（0x40）：物品 0x65..0x4AF 逐个读取对象，类别 0..9 或 {0x12,0x13,0x11,0x10,0xF,0x14,0x15,0x18} 时以模式 0 给 10 个，链表参数为 `&0x004C3018`（背包链表头本身）；Sleep 500。
- R 键（config.keys[KeyR]，0x004D0BD0）：running=RunToggle[running]（0x004AA38C）、Sleep 200——平台替换，见 deviations.md。

## 地图操作

- advance=0；counterWait 为 0 时 counterAlpha-=2，负则 0。systemFlags&0x40、mouseSuppressed、任一步进非 0、menuState 非 0 时返回。
- 滚屏与走步旗标用固定阈值 0x130、0xF0、0x258、0x1C2（无符号比较），与地图宽高 ×16 比较相机右、下边。实现按画布宽高（viewWidth/2−16、viewHeight/2、viewWidth−0x28、viewHeight−0x1E）计算，默认 640×480 时与原值相同；宽地图下按画布计算是用户要求的增强（deviations.md 第 92 行宽地图条目，已补列 0x00402AA0）。
- Space/Enter：按下且 heldFrames==1。无选项时 Space 置 advance=1；有对话框时 Enter 使 advance+1；有选项时两键之一：选项旗标 0x1000 则释放选项，都返回。Space 且 leader 空闲：FindRoleAhead(AheadX/AheadY[方向])，角色存在、无旗标 4、talkState 为 0 时转向（旗标 0x800 时角色保存并转向、运行动作，失败报 Talk1，格式 0x004AF288）、主角转向 (f&0xFFFE)+((f-1)&1)、运行主角动作（失败报 Talk2，格式 0x004AF244）、复制事件到 leader、日志、advance=0，返回。Enter 且 leader 空闲：menuState=0x80000001，返回。playerHold 且无选项：返回。
- 方向键：左/右（互斥）置方向 2/3 与 stepX；上/下（互斥）在已有左右时把 2→4/3→7（上）或 2→6/3→5（下），否则方向 0/1；有选项时改为移动选择（heldFrames==1 或 &7==7，绕回，choiceMoved=1）。pressed 记左右的 press 再或上上下的 press。
- 旗标 0x14 未置且有步进时 Slide；StepEvent 先用滑后的步进、事件为 0 再用原步进；找到角色时事件取角色 event；旗标 0x18 时事件取脚下格子 &0xFF、找到的角色清为 -1。
- 有事件：无角色时查区域（找不到记 0x004AF1E4 日志；原程序随后仍读空指针 +0x10，见 deviations.md），区域旗标置位时步进清零。leader 空闲时：无角色且有区域则写 leader（id 0xFFFD，位置为主角减 StepX/StepY[方向]×16）并记日志；有角色则 Collision 转向（第二次运行的是对方的动作，保留原 BUG）。
- 有步进：counterWait=0；按旗标设置 scrollStep（0x004D0E80/0x004D1288）或 walkStep（0x004C2B60/0x004C2B64）；动作非 0x10 时置 8，pressed&2 或 running 时置 0x10；方向或动作变化时 delay=0；动作变化时设速度（mapWalkSpeed，跑步 ×2，skipAll 时 0x10）。地图无遇敌旗标（currentMap+0x1770）未置时 encounterSteps++，满足条件则 RollEncounter(steps,0)，开战时保存音乐模式（0x0049D3B0）、StopAllSounds、ActArchive::CloseAll、TswArchive::Flush、StartBattle、步进清零、解除映射（失败记 0x004AF138），之后 battleRequest=0。
- 无步进：动作变化时 delay=0；动作=0（idleFrames>0x80 时也写 0，swd3e 不显示待机姿势）；counterWait++，超过 0x64 或 alpha&0xF 非 0 时 alpha++，上限 0x10。
- 末尾 idleFrames++，动作非 0 且非 0x34 时清零；debugKeyFrames++。

## 修正的差异

1. 两处进战斗前原程序调用 0x004367D0（TswArchive::Flush：清缓存、档案保持打开），实现调用的是 TswArchive::CloseAll（0x004366B0）。已改为 Flush。
2. （更正）滚屏/走步阈值按画布推算属宽地图增强，保留实现原写法，见上。
3. F6 原程序把背包链表头本身传给 AddItem（0x00454970 从 *list 开始找），实现在背包非空时传的是首节点的 next（沿用 swd32 的写法），会跳过首个物品并在错误位置插入。已改为 party.inventory；deviations.md 中相应的“空背包空指针”说法删除。

## 依赖数据

- AheadX 0x004AA398、AheadY 0x004AA3B8、RunToggle 0x004AA38C、StepX/StepY 0x004AA34C：tests/game_state_test.cpp 比对 exe 字节。
- 字符串 0x004AF288、0x004AF244、0x004AF1E4、0x004AF2EC、0x004AF2DC、0x004AF2D0、0x004AF2C0、0x004AF2FC：已比对（CP950 转 UTF-8 与实现一致）。
- 实现对方向下标取 &7、对脚下格子判空：方向恒为 0..7、格子指针在地图载入后恒非空，无可见差异。
