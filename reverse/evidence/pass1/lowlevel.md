# 第一遍：字体、输入、图形设备、文件、内存管理、文字输入与压缩（第 471–570 行）

## hash 配对一组（84 个）— exact

- asm_identity.py 结果见 lowlevel_identity.txt：83 个与 swd32 对应函数逐条相同（123 个地址一一对应）；0x0043D920 TextInput::InsertText 只在一处栈变量名上不同（IDA 在外传里命名为 lpMem、正传为 var_4），去掉变量名与地址后两份指令逐条相同。
- 源码函数体与 swd3-rebuild 中已为 assembly_exact 的版本逐行相同，例外：0x0043B590 TextFont::DrawText 只多出平滑层的绘制录制（PaintRecord，不改像素结果）；0x0043D4A0 TextInput 构造把复制文字的三行抽成 Fill（同样截到 NUL、按 TextFitLength 截断），成员初值与汇编（+0x10 先 0xC0C0 后 0、+0x14 0、+0x18 0、+0x1C 1）一致；0x0043C8E0 外传登记为 MapScale::GetKind（同一段 `mov eax,[ecx]` 也读其他对象的首字段）；0x0043CAE0、0x0043D540 为默认构造／析构。

## 其余 16 个 — exact

- 0x0043B960 TextFont::FillRect：左右按目标宽、上按 0 夹住，下界用未夹的 y+高 与目标高比较；首行逐点填色，其后各行从首行复制 (右−左)×2 字节（左 ≥ 右时 0 字节）。平滑层录制钩子不改结果。
- 0x0043BA60 Input::~Input：转到 0x0043BFA0 Release。
- 0x0043C290 Graphics::CreateSurfaces（平台替换，第 297 行）：先查显存；协作模式 8（窗口）：建主面、取 IDirectDrawSurface4、建裁剪器，按桌面尺寸建后台面，分别记两面的 pitch 与高；全屏：建主面、裁剪器，pitch 与高记两次。失败时 "DirectDraw Create Primary Surface Error!!" 经 0x004360C0 报并释放设备。重建以逻辑画面尺寸代替桌面尺寸。
- 0x0043C950 Graphics::PrimarySize：两个指针各自非空时写主面 pitch（+0x54）与高（+0x58）。
- 0x0043CE60 File::Read、0x0043CEC0 File::Write（平台替换）：句柄为 −1、缓冲为空或长度为 0 时记系统错误文字返回 0；API 失败时长度清零、记错误、返回 0；成功时写回实际长度返回 1。
- 0x0043D120 MemoryManager 构造：私有堆（偏差新行），计数、峰值、已分配清零，32 个桶头清零，建 32 个备用节点，失败时以 "MemMgr" 报 "MemMgrObject node Create Failed"。
- 0x0043D1B0 析构：32 个桶依次从头取节点，报 "Resource not release ,Size[%d]"（标题 "MemMgr report"），FreeBlock 失败时以其文字再报，节点挂回备用链；备用链逐个 FreeNode，失败时报；节点计数不为 0 时报 "Resource node not match"；HeapDestroy 部分为平台替换。外传不再有经内存管理器分配的函数，桶在重建中始终为空。
- 0x0043D2D0 NewNode：0x10 字节节点清零，计数 +1，峰值取大者。0x0043D310 FreeNode：空节点返回 0；FreeBlock 失败返回 0（不释放、不减计数）；否则释放、计数 −1，返回 1。
- 0x0043D350 FreeBlock：空节点写 "Internal error : NodeContainFree" 返回 0；有块时检查前两个与尾部两个 0xABCDDCBA，不符时写 "block[0x%x][%d] overwrited! [%2X …]"（地址、大小、用户区前 16 字节）并记失败；释放块、已分配 −= 大小，块、大小、+0xC 清零（+8 保留）；无块时返回 1。地址印 0 属平台替换。
- 0x0043D470 FormatLastError：FormatMessageA(GetLastError) 写入 +0x22C（平台替换为 strerror）。
- 0x0043DA70 HeapBlock::Allocate：大小 ≤ 0（有符号）返回空；否则分配 (大小+0xF)&~3 字节并清零。重建另以容器分配失败时返回空。0x0043DAB0 Free：非空时释放。
- 0x0043DC30 Random::LcgSeed：0x004BE594 = 种子（返回值即种子）。
- 0x0043DC40 LzoEncoder::Compress：长度 ≤ 13 时全为尾部字面量，否则核心（0x0043DD30）返回尾部数；尾部非 0：输出仍在开头且 ≤ 238 时一字节 (t+0x11)，≤ 3 并入前一指令的低位（out[−2] |= t），≤ 18 为 t−3，否则 0 后接 (t−18) 的 0xFF 进位长度；复制字面量；写结束标记 0x11 0 0，输出长度 = 写入字节数，返回 0。偏差第 59 行改为汇编出处。
