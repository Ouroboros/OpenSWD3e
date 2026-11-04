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
- 重开核对：`ida_export.py reopen` 读取到相同 SHA-256、`AF=0xDF7FFFF7`、函数1710个（菜单处理入口拆分后，见 scope-and-gaps.md），入口汇编可读（`reverse/analysis/export/ida_reopen_log.txt`、`info_reopen.json`）。

## FLIRT 禁用

- `cfg/ida.cfg` 的 `__PC__` 段：`ANALYSIS = 0xDFFFFFF7`；清除 `AF_FLIRT`（`0x00800000`）后启动参数 `0xDF7FFFF7`。
- 脚本在 `auto_wait()` 前再次清除并核对：`af_after_clear=0xDF7FFFF7`；自动分析后与重开后均为 `0xDF7FFFF7`。
- `^unknown_libname_[0-9]+$` 名称：0。
- `FUNC_LIB` 函数：仅 `0x004A20A7 start`（IDA PE 加载器对入口点设置，非签名识别）。

## 关联模块与补丁情况

- 静态导入：KERNEL32、USER32、GDI32、ADVAPI32、ole32、DDRAW、DINPUT、IMM32、WINMM、`binkw32.dll`（14项）、`mss32.dll`（45项）。目标目录内对应 `binkw32.dll`（SHA-256 `8f920bcb…eaebb`）、`Mss32.dll`（`fe62f1eb…b781cc`）；`Mp3dec.asi`（`51765b5a…d562`）为 Miles 运行时插件，主程序未直接导入。
- 动态加载：`LoadLibraryA` 仅用于 `KERNEL32`（`GetDiskFreeSpaceExA`）和 CRT 的 `user32.dll`；未发现对 `swd3e.dll`、`swd3.dll`、`Kaede.dll` 的引用。
- 版本：主程序是 2000 年 12 月的 CD 版 V2.03（大宇官网更新档 `swd3e203.exe` 中的 `swd3e.exe`，大小与 PE 时间戳 2000-12-22 一致），另有 6 字节免光盘修改，位于检查光盘的 `0x00411FD0`、`0x00428080`（偏差表已作平台替换）。DVD 版更新档 `swd3eDvd203.exe` 的主程序为 `swd3eDvd.exe`（1187840 字节，2000-12-26），与本程序不同；两个更新档中的 talk1–4、MAPS、NAMEX、FIGTALK、level 与目标目录 CRC 一致，两个更新档的 mon.dat 彼此相同，与目标目录的不同（目标目录为 2009 年修改的版本）。
- Steam 版（`E:\Game\Steam\steamapps\common\SWD3E`）的 talk1–4.dat 与本目录不同（talk1～3 分别多 14、44、661 字节，talk4 同大小）。文件开头 0x200 字节是 MZ 头（记录文件大小），之后是相对 0x200 的事件偏移表，表长到第一个剧本开始为止；剧本里的跳转地址为低字加段字（相对 0x200）。比对方法：在 `%Q` 处切段对齐两份文件，同长段逐字节比较，差异字节若组成的地址在两边指向相同内容即视为地址平移；事件表每项按对齐结果映射，全部指向同一处（插入代码处除外）。实际差异：8 处错别字；古维然「各送99份」选项（事件 4326）；海中建木隐藏物品玄武之沂改沙塵之珠（6270）；高玉玫拒绝条件改为五种晶矿任一 ≥3（4591，跳转目标两边均为拒绝台词）及台词（4590）；洛阳面馆十碗分支补上老板台词（CD/DVD 版该处文字指令残缺）；巴蜀古王墓祭坛通道（343）与两处入口（3393、3396，旗标 7092 之后）移除角色 15（斛律安）；螫魂塔（5385）放置并显示角色 19（单小小）；两句旁白（162、5305）的对话框尺寸参数。先前按指令解析比对得到的「过场音效与等待指令」等差异是解析在 Steam 文件中错位所致，字节比对证实两边相同。
- 同目录 `swd3e_cheat.exe`（同大小，SHA-256 `1abafdc7…07cb`）与主程序仅3字节不同，均位于函数 `0x00402AA0`：VA `0x402B49..0x402B4A` 主程序为 `12 06`、cheat 为 `00 00`；VA `0x402F6B` 主程序为 `74`（jz）、cheat 为 `75`（jnz）。主程序修改时间为2021-07-03，是否为原始发行字节未决；还原以给定 `swd3e.exe` 字节为准。
