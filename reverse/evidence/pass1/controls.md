# 第一遍汇编验证：Controls（0x00404E00、0x004050E0）

结论均为 exact。

## 0x00404E00 Poll

- now（0x004C15F4）= 帧时间（0x004C4654）；读鼠标到指针坐标（0x004C2FFC/0x004C3000）。
- 按键槽（0x004D1638 起每槽 0x10，键码 config.keys 0x004D0B90 起每项 4）更新顺序：0,1,12,2,3,4,5,6,9,10,8,7,16,17,18,19，之后左键（鼠标位 1）→槽 15、右键（位 2）→槽 14；与 kPollOrder 一致。
- lastPoll（0x004C160C）= now；mouseSuppressed（0x004E6CDC）非 0 时槽 15 四个字段清零并减 1。
- 清旗标 9；指针与上次相同且无按键时 idle（0x004E4DDC）加 1，超过 0x1C2（无符号）置旗标 9；否则 idle=0；记录本次指针。

## 0x004050E0 Update

- press 清位 0x4000。按下：之前已松开（releasedAt≠0）时 press=clicks=ClickSequence[clicks]（0x004AA3D8 {1,2,3,3}，tests/input_test.cpp 比对）、heldFrames=0；releasedAt=0，heldFrames+1。
- 未按下：heldFrames=0；releasedAt 为 0 时记为 now 并返回（press 保留一帧）；否则 press=0，now−releasedAt（无符号）>0x78 时 clicks=0。
- 返回值（heldFrames）无调用者使用，实现为 void。
