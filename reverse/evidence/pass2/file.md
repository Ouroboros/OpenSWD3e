# 第二遍：文件（cFile）

Win32 文件调用为平台替换（deviations 104、105；整读缓存见 81），以下核对各函数的出口与返回值。

## 0x0043CAE0 File::File — exact

汇编：句柄 = −1，+4/+8/+0xC 清 0，错误文字 +0x10 起 16 个 dword 清 0。对照：一致。

## 0x0043CB10 File::~File — exact

汇编：有映射时：有视图则 UnmapViewOfFile，失败记错误后直接返回（文件不关）；关映射失败同样返回；句柄 ≠ −1 时 CloseHandle，释放文件名。对照：一致。

## 0x0043CB90 File::Open — exact

汇编：旧句柄 ≠ −1 时关闭；SetFileAttributesA(名, 普通)；CreateFileA(名, 访问, 不共享, 0, 方式, 0x10000080, 0)，失败 → 句柄 −1、记错误、返回 0；成功 → 复制文件名、定位到开头、返回 1。
对照：一致（先清只读属性；共享方式与路径编码见 deviations 104、105）。

## 0x0043CC30 File::Close — exact

汇编：有映射时同析构的释放，失败记错误返回 0；句柄 ≠ −1 时 CloseHandle，失败记错误返回 0，成功 → 释放文件名、错误文字清 0、句柄 −1；返回 1。对照：一致。

## 0x0043CCC0 File::CreateMapping — exact

汇编：句柄 −1 → 0；CreateFileMappingA(句柄, 0, PAGE_READONLY, 0, 文件大小, 0) 写 +4，失败记错误返回 0，否则 1。对照：一致（空文件不能映射）。

## 0x0043CD10 File::CloseMapping — exact

汇编：无映射 → 1；有视图时 UnmapViewOfFile，失败记错误返回 0，成功清 +8；CloseHandle 映射，失败记错误返回 0，成功清 +4 返回 1。对照：一致。

## 0x0043CD70 File::MapView — exact

汇编：已有视图时先释放，失败记错误返回 0；MapViewOfFile(映射, FILE_MAP_READ, 0, 偏移, 长度) 写 +8 并返回。对照：一致（长度 0 到文件尾）。

## 0x0043CDC0 File::UnmapView — exact

汇编：参数 ≠ 当前视图 → 0x00411F10("cFile ,CloseView Src/Dest difference ") 返回 0；参数非空时 UnmapViewOfFile，失败记错误返回 0；+8 清 0，返回 1。对照：一致；日志字串与 exe 字节相同（注释地址改为 0x004BE090）。

## 0x0043CE20 File::Size — exact

汇编：句柄 −1 → −1；否则 GetFileSize。对照：一致。

## 0x0043CE40 File::Truncate — exact

汇编：句柄 −1 → 0；SetEndOfFile。对照：一致。

## 0x0043CE60 File::Read — exact

汇编：句柄 −1、缓冲为空或请求为 0 → 记错误返回 0（长度不改）；ReadFile 失败 → 长度 = 0、记错误、返回 0；成功 → 长度 = 实读、返回 1。对照：一致。

## 0x0043CEC0 File::Write — exact

汇编：同 Read，换 WriteFile。对照：一致。

## 0x0043CF20 File::Seek — exact

汇编：句柄 −1 → 记错误返回 0；SetFilePointer(句柄, 距离, &符号扩展的高位, 方式)，结果 −1 时记错误；返回结果。对照：一致。

## 0x0043CF80 File::SeekBegin — exact

汇编：返回 0x0043CF20(0, 距离) + 1。对照：一致。

## 0x0043CF90 File::SeekEnd — exact

汇编：返回 0x0043CF20(2, 距离) + 1。对照：一致。

## 0x0043CFA0 File::Tell — exact

汇编：句柄 −1 或 SetFilePointer(0, 当前) 失败 → 记错误返回 0；否则写出位置返回 1。对照：一致。

## 0x0043CFF0 File::WriteExact — exact

汇编：0x0043CEC0 失败 → 0；实写 ≠ 请求 → 记错误返回 0；否则 1。对照：一致（错误文字为平台文字）。

## 0x0043D040 File::LastWriteTime — exact

汇编：句柄 −1 → 0；GetFileTime(句柄, 创建, 访问, 写入)。对照：一致（游戏只用写入时间）。

## 0x0043D070 File::WriteByte — exact

汇编：0x0043CFF0(&参数, 1)。对照：一致。

## 0x0043D080 File::WriteDword — exact

汇编：0x0043CFF0(&参数, 4)。对照：一致。

## 0x0043D090 File::ReadExact — exact

汇编：0x0043CE60 失败 → 记错误返回 0；成功（含短读）→ 1。对照：一致。

## 0x0043D0D0 File::ReadByte — exact

汇编：0x0043D090(缓冲, 1)。对照：一致。

## 0x0043D0E0 File::ReadDword — exact

汇编：0x0043D090(缓冲, 4)。对照：一致。

## 0x0043D0F0 File::SetError — exact

汇编：FormatMessageA(FROM_SYSTEM, 0, 错误码, 0, +0x10, 0x40, 0)。对照：平台替换（文字取平台错误说明，限 0x40 字节）。

## 0x0043D110 File::ErrorText — exact

汇编：返回 this+0x10。对照：一致。
