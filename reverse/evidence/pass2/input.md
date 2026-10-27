# 第二遍：input（按键轮询）

方法见 reverse/analysis/pass2-method.md。

## 0x00404E00 Controls::Poll — exact

汇编：now(0x004C15F4)=帧时间(0x004C4654)；鼠标 = 0x0043BDD0(&指针 x 0x004C2FFC, &y 0x004C3000)；依配置键 0x004D0B90+4·槽 逐个 0x0043BD90 后 0x004050E0，顺序为槽 0、1、12、2、3、4、5、6、9、10、8、7、16、17、18、19（按键状态基址 0x004D1638，每项 0x10）；左键槽 15 用 鼠标&1、右键槽 14 用 鼠标&2；0x004C160C=now；0x004E6CDC 非 0 时左键四项清零并减一；清旗标 9；指针未动且鼠标为 0 时 0x004E4DDC 加一，超过 0x1C2（无符号）置旗标 9，否则计数清零；记下指针。
对照：一致（kPollOrder 顺序、IdleFlag 9、IdleFrames 0x1C2、ButtonState 四项）。

## 0x004050E0 Controls::Update — exact

汇编：press&=~0x4000；按下：releasedAt≠0 时 next=0x004AA3D8[clicks]，heldFrames=0、press=clicks=next；releasedAt=0；heldFrames 加一。松开：heldFrames=0；releasedAt==0 时记 now 返回；否则 press=0，(now−releasedAt) 无符号 >0x78 时 clicks=0。返回 heldFrames（调用者不读）。
对照：一致；0x004AA3D8 = {1,2,3,3}，与 ClickSequence 一致，ClickGap 0x78。

## 0x0043BA60 Input::~Input — exact

汇编：跳到 0x0043BFA0。对照：一致。

## 0x0043BA70 Input::Init — exact

汇编：DirectInputCreate、键盘与鼠标设备的格式、协作级别、事件通知、缓冲大小与 Acquire，各步失败时 MessageBoxA（"Direct Input Failure!"、"CreateEvent Error"、"SetEventNotification"、"Set buffer size(SysMouse)"）并返回 1 或 0；最后以错误大小取鼠标状态，原点 +0x10C/+0x110 取按键状态前 8 字节；返回 0。
对照：平台替换，输入由窗口事件提供，总是成功返回 0（deviations 37）。

## 0x0043BD70 Input::PollKeyboard — exact

汇编：键盘 GetDeviceState(256, 本对象 +0) 写按键状态，返回 1。
对照：一致（读取经平台层；保留上一帧与已用键属增强，见 deviations 98、99）。

## 0x0043BD90 Input::IsKeyDown — exact

汇编：返回 状态[码] & 0x80。对照：一致。

## 0x0043BDA0 Input::FirstKeyDown — exact

汇编：从 0 到 255 找第一个 0x80 置位的码并返回，没有 → 0。对照：一致。

## 0x0043BDC0 Input::PressKey — exact

汇编：状态[码] |= 0x80（返回码地址的寄存器值，调用方不读）。对照：一致。

## 0x0043BDD0 Input::ReadMouse — exact

汇编：鼠标 GetDeviceState(0x1C)；x = (轴x − 原点x)×灵敏度/10、y 同理（有符号）；键 0、键 1 的 0x80 分别为 bit0、bit1；x<0 → 0 且原点 = 轴，>639 → 639 且原点 = 轴 − 6390/灵敏度；y 同理（479、4790）；写出 x、y，返回按键位。
对照：平台替换，位置取窗口中的指针并夹在范围内（deviations 36、99），按键位一致。

## 0x0043BEF0 Input::SetMousePosition — exact

汇编：x ≥640 → 639，≤0 → 0；y ≥480 → 479，≤0 → 0；取鼠标轴，原点 = 轴 − (x/灵敏度)×10、y 同理。
对照：夹取一致；移动原点改为移动指针（deviations 36）。

## 0x0043BF80 Input::SetSensitivity — exact

汇编：+0x114 = ftol(参数（double）× 10.0)（0x004AA690 字节核对为 10.0）。对照：一致。

## 0x0043BFA0 Input::Release — exact

汇编：DirectInput 对象非空时：键盘、鼠标设备各 Unacquire、Release 并清 0，再 Release DirectInput 并清 0。
对照：平台替换（deviations 37）。
