# 与 swd32（OpenSWD3）的函数映射

用途：只用于加速第一遍还原和定位结构布局。逻辑依据仍是 swd3e 自身汇编；复用代码只记 `implemented_pending_review`，第二遍照常逐函数验证并计次。

## 来源

- 对照程序：`/mnt/e/Game/swd3/swd3-rebuild`，指纹程序提交 `e570f5f`，配对程序提交 `6f7214a`；其主程序 `swd32.exe` 为免DVD版（原版为 `swd3.exe`），光盘检查一带配不上属预期。
- 清单：`scripts/export_funcs.py`（原样复制自对照仓库）→ `asm/`、`decomp/`（1700份）。
- 指纹：对照仓库 `reverse/analysis/scripts/fingerprints.py`（只读运行）→ `export/swd3e_fingerprints.jsonl`、`export/swd3e_funclets.tsv`（0行，尾块口径）。
- 配对：对照仓库 `reverse/analysis/scripts/match_functions.js`（只读运行，a=swd3e，b=swd32）→ `export/match_swd32/`。

复现命令（S=对照仓库，E=reverse/analysis，记号目录放 tmp/）：

```text
FINGERPRINT_TOKENS=tmp/tok3e python3 -I S/reverse/analysis/scripts/fingerprints.py E/asm E/work/swd3e.exe E/export/swd3e_fingerprints.jsonl E/export/swd3e_funclets.tsv
FINGERPRINT_TOKENS=tmp/tok32 python3 -I S/reverse/analysis/scripts/fingerprints.py S/reverse/analysis/asm S/reverse/analysis/work/swd32.exe tmp/swd32_fp.jsonl tmp/swd32_fc.tsv
node S/reverse/analysis/scripts/match_functions.js E/export/swd3e_fingerprints.jsonl tmp/tok3e S/reverse/analysis/export/swd32_fingerprints.jsonl tmp/tok32 E/export/match_swd32 --a-crt=4A1096-4AA000 --b-crt=487AF0-497FCC
```

选项必须放在位置参数之后。swd3e CRT 区取 `0x4A1096..0x4AA000`（CRT函数入口均在此范围；`0x4A93xx` 起的 EH 尾块不是函数入口）。

## 结果

- 配上1183/1700（覆盖swd3e指令74.5%），方法：hash 444、shape 166、anchor 108、calls 102、callers 207、similarity 31、neighbours 125；地址顺序一致1094对；`reusable=yes` 369。
- `functions.tsv` 末尾两列 `order_consistent`、`block_overlap` 用于按风险排序抽查。
- 全局地址映射731（全部一致）；结构偏移218组（`offsets.tsv`，同一组偏移可能属于不同结构，需结合 functions 列）。
- 未配上517，其中至少147个推断为外传新增函数。

## 已知风险

- 非 hash 方法只表示对应关系，不保证代码相同。
- 已处理：CRT/游戏区硬分区；4条指令以内、非 hash 且不在地址递增序列上的小函数撤回为 unmatched。原疑似错配 `0x4A1FD2`、`0x4A4F08`、`0x4A78B9`、`0x409DB0`、`0x42B1B0` 现均为 unmatched。
- 待查：`0x43DAB0`→swd32 `0x485910`（similarity 0.600，`order_consistent=no`，`block_overlap` 0.333）；其余 `order_consistent=no` 的89对使用前逐个核对。
