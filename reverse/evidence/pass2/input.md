# 第二遍：input（按键轮询）

方法见 reverse/analysis/pass2-method.md。

## 0x00404E00 Controls::Poll — exact

汇编：now(0x004C15F4)=帧时间(0x004C4654)；鼠标 = 0x0043BDD0(&指针 x 0x004C2FFC, &y 0x004C3000)；依配置键 0x004D0B90+4·槽 逐个 0x0043BD90 后 0x004050E0，顺序为槽 0、1、12、2、3、4、5、6、9、10、8、7、16、17、18、19（按键状态基址 0x004D1638，每项 0x10）；左键槽 15 用 鼠标&1、右键槽 14 用 鼠标&2；0x004C160C=now；0x004E6CDC 非 0 时左键四项清零并减一；清旗标 9；指针未动且鼠标为 0 时 0x004E4DDC 加一，超过 0x1C2（无符号）置旗标 9，否则计数清零；记下指针。
对照：一致（kPollOrder 顺序、IdleFlag 9、IdleFrames 0x1C2、ButtonState 四项）。

## 0x004050E0 Controls::Update — exact

汇编：press&=~0x4000；按下：releasedAt≠0 时 next=0x004AA3D8[clicks]，heldFrames=0、press=clicks=next；releasedAt=0；heldFrames 加一。松开：heldFrames=0；releasedAt==0 时记 now 返回；否则 press=0，(now−releasedAt) 无符号 >0x78 时 clicks=0。返回 heldFrames（调用者不读）。
对照：一致；0x004AA3D8 = {1,2,3,3}，与 ClickSequence 一致，ClickGap 0x78。
