# 第二遍：文字输入框（TextInput）

字符步进按代码页 950 规则替代 CharNextA／CharPrevA（deviations 44）。

## 0x0043D4A0 TextInput::TextInput — exact

汇编：+4 = x、+8 = y、+0xC = 容量，+0x18 = 0、+0x1C = 1、+0x14 = 0；缓冲 = 分配 容量+1 并清零；光标先写 0x0043D550(文字, 容量)，复制这么多字节，再写 0。对照：一致。

## 0x0043D530 TextInput::GetResult — exact

汇编：返回 +0x14。对照：一致。

## 0x0043D540 TextInput::~TextInput — exact

汇编：释放缓冲。对照：一致。

## 0x0043D550 TextFitLength — exact

汇编：累计 = 0；逐个 CharNextA，下一字符不前进即停，累计+字符字节 > 上限即停，否则累加；返回累计。对照：一致。

## 0x0043D590 TextInput::GetX — exact

汇编：返回 +4。对照：一致。

## 0x0043D5A0 TextInput::GetY — exact

汇编：返回 +8。对照：一致。

## 0x0043D5B0 TextInput::GetCursor — exact

汇编：返回 +0x10。对照：一致。

## 0x0043D5C0 TextInput::CopyText — exact

汇编：目标清零 尺寸 字节；上限 = min(尺寸, 容量)；复制 0x0043D550(缓冲, 上限) 字节，返回 1。对照：一致（以字串返回）。

## 0x0043D620 TextInput::HandleMessage — exact

汇编：WM_KEYDOWN 按 wParam−8 查字节表 0x0043D864（解码：8 退格、13 回车、27 Esc、35 End、36 Home、37 左、39 右、45 Insert、46 Delete，其余默认）：退格 = 左移后删除；回车 +0x14 = 1；Esc = 2；End/Home 第一次移动返回 1 时一直移到不能再移；Insert = 设置插入模式(取反)；Delete = 删除；各分支返回 1。WM_CHAR：+0x18 ≠ 0 → 1；8 字节清零缓冲；待尾字节旗 0x0050A0F8 = 1 时高位字节丢弃，否则插入并清旗；否则高位字节插入并置旗，< 0x1F（有符号）丢弃，'%' 播放音效 0x8C（音量 0x004C4F0C）不插入，其余插入并清旗；返回 1。WM_INPUTLANGCHANGE：+0x18 = ImmIsIME(hkl)，返回 1。WM_IME_CHAR：缓冲 = 高字节、低字节，插入，返回 0。其他消息返回 1。
对照：一致（DeleteAtCursor 恒返回 1，CursorLeft/Right 只返回 0/1，返回值与循环条件相同；输入法判断经平台层）。

## 0x0043D890 TextInput::SetInsertMode — exact

汇编：+0x1C = 1（参数不用），返回 1。对照：一致。

## 0x0043D8A0 TextInput::GetInsertMode — exact

汇编：返回 +0x1C。对照：一致。

## 0x0043D8B0 TextInput::CursorLeft — exact

汇编：光标 = CharPrevA(缓冲, 缓冲+光标) − 缓冲，返回光标 ≠ 0。对照：一致。

## 0x0043D8E0 TextInput::CursorRight — exact

汇编：下一 = CharNextA(缓冲+光标)，未前进 → 0；否则光标 = 下一 − 缓冲，返回 1。对照：一致。

## 0x0043D920 TextInput::InsertText — exact

汇编：+0x1C ≠ 1 → 返回；临时缓冲 = 分配 0x0043D550(文字, 0x400)+容量+1 并清零；复制光标前的字节；插入 = 0x0043D550(文字, 容量−光标) 字节接在光标处；保留 = 0x0043D550(缓冲+光标, 容量−末尾) 字节接在其后；光标 = 末尾；原缓冲清零（容量+1）后写回 末尾+保留 字节；释放临时缓冲，返回 1。对照：一致。

## 0x0043DA30 TextInput::DeleteAtCursor — exact

汇编：下一 = CharNextA(缓冲+光标)；0x0043D550(下一, 容量)（结果不用）；memmove(缓冲+光标, 下一, 容量 − (下一−缓冲) + 1)；返回 1。对照：一致。
