# 逆向还原整体执行计划

版本：3（独立仓库，规则与代码结构对齐swd3-rebuild）

## 目标与范围

目标目录`/mnt/e/Game/swd3e`，主程序`swd3e.exe`（SHA-256 `5532e5b1…3f1f94`，PE32 x86），实现语言C++20；IDAT路径见AGENTS.md。范围为`swd3e.exe`的函数全集（游戏代码、静态链接的MSVC CRT、全局初始化thunk、EH funclet）及其依赖的常量表、全局数据、`.rsrc`资源与游戏数据文件。尽量还原原模块与代码结构，以正经的C++类和结构体组织；已与正传（swd3-rebuild，只读参考）配对的部分，目录、类名与函数名保持平行，结构布局以swd3e汇编为准。原逻辑和逻辑BUG完整保留，越界读写类BUG不复现并逐条记录；平台层按AGENTS.md使用SDL3与FFmpeg n9.0，窗口化显示（逻辑640×480，窗口默认1280×960、可拉伸，最近邻等比缩放加黑边，Alt+Enter全屏）。Windows x64为主平台（x86保留为可选构建），须可正常游玩；Linux只要求编译、单元测试通过并用于代码验证。

## 不做的事项

不修复原程序逻辑BUG，不改造原业务逻辑，不扩展目标范围外的功能；不修改swd3-rebuild，不与其共享源码或构建；不还原未被主程序加载的同目录程序（`swd3e_cheat.exe`、`swd3.win.exe`、`Ghost.exe`、`swd3e.dll`等）；不重写MSVC CRT。

## 整体执行计划

由用户START GOAL后执行以下阶段。

1. 接续：读取IDAT初步分析、工作数据库、结构概览、初始函数表与swd32配对结果，核对输入基线及范围缺口。
2. 对齐：参照swd3-rebuild建立architecture.md、deviations.md、构建配置（cmake/、scripts/deps/）与规范脚本；按配对结果把函数表的模块与源码位置对齐swd3-rebuild，配对信息写入notes；依据结构偏移与全局地址映射整理外传的结构体与全局变量布局。
3. 还原：先移植hash相同且可复用的函数，再改写其余已配对函数，最后从汇编还原未配对与外传新增的函数；CRT映射到标准库；持续修正归属，闭合间接调用、运行时跳转表与`.rsrc`资源缺口。
4. 验证：全部还原后，从函数表第一行开始逐遍对照swd3e汇编，修正差异直至bug-for-bug一致。
5. 交付：Linux构建与单元测试通过；Windows构建通过且可正常游玩。

## 整体进度

- 当前阶段：第2阶段（对齐）基本完成，第3阶段（还原）进行中。
- 已识别函数数：1710；模块已按swd32配对对齐（`unknown` 65个）。
- 函数总数：待确定（间接调用目标、运行时跳转表未闭合；`.rsrc`资源已提取至third_party/asset/launcher）。
- 已实现／总数：1157／待确定（已识别1710）。
- 已验证／总数：0／待确定（已识别1710）。
- 待实现／待验证：553／1710（以已识别函数计）。

## 当前工作

以swd3-rebuild提交`6f7214a`为底稿复制源码、测试与构建配置，Linux与Windows构建通过，Linux测试128项通过、43项列为待外传化（tests/pending_swd3e.txt）。res模块（tsw、act档案与动作解释器）已按swd3e汇编重写；CRT全部映射；与swd32代码一致的函数已登记；role与role_path（寻路器）模块已按swd3e核对完毕；外传新增的小游戏（src/minigame/，0x0049EFF0起13个函数）已还原；外传的“Act Err”报告统一改为错误对话框。停点：三段式角色名、三框命名（0x004172A0）与标题系统页已按外传改写；剧本指令209随RunEvent（0x0042BAF0，外传新增指令0xC2～0xD6）整体改写时接入；map模块全部改写完毕（含符鬼跟随精灵）；地图进入LoadCity及其被调函数已按外传改写；符鬼数据（0x004D0C10、swdghost.dat）已还原；剧情指令RunEvent（0x0042BAF0）已逐条按外传改写（含新增指令0xC1～0xD6、命名指令209与Bink影片模块）；audio模块的三组音乐队列（0x00578A70）已按外传重写，地图、剧情、系统页、读档与战斗的音乐调用均改用队列；talk、pic、saveload、launcher与audio模块的剩余已配对函数已按外传改写（启动器为外传的五按钮布局）；game模块的主循环、新游戏、主帧（含攀升小游戏与符鬼计时）、设备初始化与释放、存档页已按外传改写，角色表扩为300项；存档帧（0x00406EC0，格式见reverse/analysis/save-format.md）也已改写；menu模块按reverse/analysis/menu-rewrite.md分页改写：框架（左侧角色栏与三种面板）、主页、符鬼喂食页、物品页、装备页与法术页已完成；下一处是祭坛、系统页签、图鉴与商店各页，之后是battle模块与unknown。

## 下一步

按依赖自底向上改写剩余已配对函数（codec、gfx、draw、pic、font、audio、map、talk、battle、menu、game），以reverse/analysis/diff_swd32/与decomp_diff.py的差异为线索，其中map的地图载入（0x0042A7D0，HUGE.lmf）与audio的音乐对象（0x00578A70）需整体按swd3e重写；各模块“Act Err”调用改为错误对话框0x0040C530；再从汇编还原未配对函数并确定`unknown`归属。

## GOAL完成条件

范围缺口已闭合，函数全集均有实现映射并通过汇编对照，范围内数据与资源均已核对且有证据，包含原有逻辑BUG，偏差均已登记，无剩余行为差异；Linux构建与单元测试通过；Windows构建通过且可正常游玩。

## 工作入口

路径相对工作区根目录。

- 规则：AGENTS.md
- 函数表：reverse/functions.tsv
- 源码：src/、include/；单元测试：tests/（第2阶段建立）
- 构建：CMakeLists.txt、CMakePresets.json、cmake/（第2阶段按AGENTS.md重建）
- 工作数据库：reverse/analysis/swd3e.i64（输入reverse/analysis/work/swd3e.exe）
- 输入基线：reverse/analysis/input-baseline.md
- 结构概览：reverse/analysis/structure-overview.md
- 范围与缺口：reverse/analysis/scope-and-gaps.md
- 与swd32的函数映射（还原线索）：reverse/analysis/swd32-mapping.md、reverse/analysis/export/match_swd32/
- IDAT导出与查询脚本：reverse/analysis/export/、reverse/analysis/scripts/
- 逐函数汇编清单与反编译：reverse/analysis/asm/、reverse/analysis/decomp/
- 代码架构与已知偏差：reverse/analysis/architecture.md、reverse/analysis/deviations.md（第2阶段建立）
- 验证证据：reverse/evidence/（产生后补充）
- 临时目录：tmp/
