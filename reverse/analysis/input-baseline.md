# 输入基线

## 输入参数

| 项 | 值 |
| --- | --- |
| target | `/mnt/e/Game/swd3e`（Windows：`E:\Game\swd3e`） |
| main | `swd3e.exe` |
| idat | `D:\Dev\Crack\IDA\idat.exe`（WSL：`/mnt/d/Dev/Crack/IDA/idat.exe`，IDA Professional 9.3） |
| lang | C++20 |

## 主程序身份

| 项 | 值 | 来源 |
| --- | --- | --- |
| 大小 | 1191936 字节 | 文件系统；IDA `retrieve_input_file_size` |
| SHA-256 | `5532e5b1270935dfe359873930531ed61eb3cf8c35e20b2f8353f4b7ba3f1f94` | `sha256sum`；IDA `retrieve_input_file_sha256`（新建与重开一致） |
| MD5 | `03dd1e496cddd738f328b06404746a37` | IDA `retrieve_input_file_md5` |
| 格式 | PE32 GUI，Intel 80386 | `file`；IDA `procname=metapc`、32位 |
| 映像基址 | `0x00400000` | PE头；IDA |
| 入口 | `0x004A20A7`（MSVC CRT `start`，调用 `_WinMain@16` `0x0040A140`） | PE头 AddressOfEntryPoint=`0xA20A7`；IDA |
| PE时间戳 | `0x3A42BF83` | PE文件头 |

### 节表（文件原貌）

| 节 | VA | 原始大小 | 原始偏移 | IDA加载 |
| --- | --- | --- | --- | --- |
| .text | `0x00401000` | `0xA9000` | `0x1000` | 是（`0x401000..0x4AA000`） |
| .rdata | `0x004AA000` | `0x5000` | `0xAA000` | 是（`.idata` `0x4AA000..0x4AA320` 与 `.rdata` `0x4AA320..0x4AF000`） |
| .data | `0x004AF000` | `0x13000` | `0xAF000` | 是（含未初始化部分，至 `0x57C000`） |
| .rsrc | `0x0057C000` | `0x61000` | `0xC2000` | 否（IDA `PE_LOAD_RESOURCES=NO`），见范围缺口 |

## 工作副本与数据库

- 工作副本：`reverse/analysis/work/swd3e.exe`，SHA-256 与原文件一致。原程序未改动。
- 正式工作数据库：`reverse/analysis/swd3e.i64`（唯一正式库）。
- 新建命令：`idat.exe -A -c -dANALYSIS=0xDF7FFFF7 -o<swd3e.i64> -S"ida_export.py initial <export>" <work\swd3e.exe>`，临时目录 `TMP/TEMP` 指向工作区 `tmp/`。
- 重开核对：`ida_export.py reopen` 读取到相同 SHA-256、`AF=0xDF7FFFF7`、函数1700个，入口汇编可读（`reverse/analysis/export/ida_reopen_log.txt`、`info_reopen.json`）。

## FLIRT 禁用

- `cfg/ida.cfg` 的 `__PC__` 段：`ANALYSIS = 0xDFFFFFF7`；清除 `AF_FLIRT`（`0x00800000`）后启动参数 `0xDF7FFFF7`。
- 脚本在 `auto_wait()` 前再次清除并核对：`af_after_clear=0xDF7FFFF7`；自动分析后与重开后均为 `0xDF7FFFF7`。
- `^unknown_libname_[0-9]+$` 名称：0。
- `FUNC_LIB` 函数：仅 `0x004A20A7 start`（IDA PE 加载器对入口点设置，非签名识别）。

## 关联模块与补丁情况

- 静态导入：KERNEL32、USER32、GDI32、ADVAPI32、ole32、DDRAW、DINPUT、IMM32、WINMM、`binkw32.dll`（14项）、`mss32.dll`（45项）。目标目录内对应 `binkw32.dll`（SHA-256 `8f920bcb…eaebb`）、`Mss32.dll`（`fe62f1eb…b781cc`）；`Mp3dec.asi`（`51765b5a…d562`）为 Miles 运行时插件，主程序未直接导入。
- 动态加载：`LoadLibraryA` 仅用于 `KERNEL32`（`GetDiskFreeSpaceExA`）和 CRT 的 `user32.dll`；未发现对 `swd3e.dll`、`swd3.dll`、`Kaede.dll` 的引用。
- 同目录 `swd3e_cheat.exe`（同大小，SHA-256 `1abafdc7…07cb`）与主程序仅3字节不同，均位于函数 `0x00402AA0`：VA `0x402B49..0x402B4A` 主程序为 `12 06`、cheat 为 `00 00`；VA `0x402F6B` 主程序为 `74`（jz）、cheat 为 `75`（jnz）。主程序修改时间为2021-07-03，是否为原始发行字节未决；还原以给定 `swd3e.exe` 字节为准。
