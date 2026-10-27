# 第二遍：设置与启动

## 0x004277E0（已移除：安装命令行写出默认 Env.dat）— exact

汇编：new cFile，以 OPEN_ALWAYS 打开 <安装目录>Env.dat，成功则关闭后以参数调用 0x004278C0；关闭、析构；EH unwind 0x004A9490、handler 0x004A949B。
对照：随安装命令行一并移除，为已登记的平台替换（deviations 103）；源码注释中的正传 EH 地址改为上列外传地址。

## 0x004278C0 Config::Update — exact

汇编：以读写打开 Env.dat，读入 0x1000 字节缓冲；首 dword ≠ 0xFFFFFFFF（旧格式）时整体后移 4 字节并标记；首 dword 写 0xFFFFFFFF；+4..+0x13 依次写 16 个按键槽（0x004D0B90 起的 4、6、3、5、1、12、9、0、10、2、16、17、18、19、7、8 号的低字节）；+0x14 = unknown14（dword），+0x18..+0x1D = 音效、音乐、快取、关闭特效、文字速度、战斗速度；旧格式时 +0x1E 起 16 字节与三个掩码字清 0；+0x2E 起写目录与光驱两个字符串，其后一字节 = 0x004E55A4，再一字节 = 0（0x00427B5C，会话标记）；从头写回 0x32+两串长度。调用者（0x00452970、0x00451D30）传入 目录、光驱、0x004C4F0C、0x004E5814、0x004D1434、0x64、旗标 0x48、0x004C3654、0x004C6610。
对照：平台替换（deviations 103）：同一组值写入 [game]（按键、unknown14、六个设置字节，会话标记清除，掩码与调试位保留），字段对应一致；EH 注释改为 0x004A94B0/0x004A94BB，会话标记注释改为 0x00427B5C。

## 0x00427BD0 Config::SaveKeys — exact

汇编：以写打开 Env.dat，16 字节缓冲依次写上述 16 个按键槽的低字节，从偏移 0 写出 16 字节（盖掉格式标记）。
对照：平台替换（deviations 103）：只改 [game.keys]，键槽与低字节一致（StoreKeys）；EH 注释改为 0x004A94D0/0x004A94DB。

## 0x00427D80 Config::ImportEnv — exact

汇编：读入 Env.dat；旧格式（首 dword ≠ −1）：取 +0x2A 起两串后的字节作输出 0，0x00428160 设默认键，以默认值（6、6、0x3C、0x64、1、2、0x0A）调 0x004278C0 后重读；新格式：+4..+0x13 → 16 个按键槽（同上顺序），+0x14 dword → 输出，+0x18..+0x1D → 六个设置字节输出，+0x1E/+0x20/+0x22 → 掩码字 0x004AF70C/0E/10，+0x24&1 → 0x004AF7A0，+0x2E 起两串复制到目录与光驱，第二串之后的字节 → 输出 0；返回是否读到文件。
对照：平台替换（deviations 103）：一次性导入，新格式按上列偏移取值（KeysAt 4、Unknown14At 0x14、SettingsAt 0x18、MasksAt 0x1E、DebugAt 0x24、StringsAt 0x2E、EnvKeyOrder 与 0x004D0B90 槽号一致），旧格式与缺文件取默认；读入缓冲越界见 deviations 46；kKeySlots 注释的正传表地址改为 0x004BED40（按键设定页行序的 u16 槽表，exe 字节核对一致）。

## 0x00428080（已移除：枚举光驱）— exact

汇编：0x004D0E60 起 32 字节填 0xFF；从 'j' 到 'z' 每个盘符调 GetDriveTypeA（比较 5 后的分支被 nop 掉，结果不用），把盘符逐个写入 0x004D0E60，返回计数。
对照：随光盘检查一并移除，为已登记的平台替换（deviations 101）。

## 0x00428160 Config::SetDefaultKeys — exact

汇编：0x004D0B90 起各槽 = 1、0x39、0x36、0xCB、0xC8、0xCD、0xD0、0xC9、0xD1、0x9D、0xCF、（11 不写）、0x1C、（13–15 不写）、0x13、0x1E、0x22、0x3B。
对照：一致（槽 0..19 与 Input 扫描码常量逐项相同）。
