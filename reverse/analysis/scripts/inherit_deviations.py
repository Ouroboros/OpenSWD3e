"""Writes reverse/analysis/deviations.md from the swd3-rebuild list: the addresses of the function column are converted
to swd3e through the function and global maps (reverse/analysis/export/match_swd32/); unmapped ones stay "swd32 0x...".
Every inherited row is marked to be confirmed against the swd3e assembly.

Usage: python3 -I inherit_deviations.py <workspace> <swd3-rebuild>
"""

import csv
import os
import re
import sys

WS, REF = sys.argv[1], sys.argv[2]
MATCH = os.path.join(WS, "reverse/analysis/export/match_swd32")


def read_tsv(path):
    with open(path, encoding="utf-8", newline="") as fh:
        return list(csv.DictReader(fh, delimiter="\t", quoting=csv.QUOTE_NONE))


address_map = {}
for row in read_tsv(os.path.join(MATCH, "globals.tsv")):
    if row["consistent"] == "yes":
        address_map[int(row["b_address"], 16)] = int(row["a_address"], 16)
for row in read_tsv(os.path.join(MATCH, "functions.tsv")):
    if row["b_address"]:
        address_map[int(row["b_address"], 16)] = int(row["a_address"], 16)

ADDRESS = re.compile(r"0x00([0-9A-F]{6})")
converted = unmapped = 0


def convert(match):
    global converted, unmapped
    b = int(match.group(0), 16)
    if b in address_map:
        converted += 1
        return "0x%08X" % address_map[b]
    unmapped += 1
    return "swd32 0x%08X" % b


with open(os.path.join(REF, "reverse/analysis/deviations.md"), encoding="utf-8") as fh:
    lines = fh.read().splitlines()

out = [
    "# 与原程序的已知偏差",
    "",
    "允许的类别见 architecture.md。每条记录：函数地址、类别、原行为、新行为、依据。",
    "",
    "下表继承自 swd3-rebuild：函数栏的地址已按 reverse/analysis/export/match_swd32/ 换算为 swd3e 地址，"
    "无对应的保留为 `swd32 0x…`；全部待按 swd3e 汇编逐条核对，核对后把依据栏改为 swd3e 的证据并去掉“待核”。",
    "",
]
for line in lines:
    if line.startswith("| ") and not line.startswith("| 函数") and not line.startswith("| ---"):
        cells = line.split(" | ")
        cells[0] = ADDRESS.sub(convert, cells[0])
        cells[-1] = cells[-1].rstrip(" |") + "；待核（继承自 swd3-rebuild） |"
        out.append(" | ".join(cells))
    elif line.startswith("| 函数") or line.startswith("| ---"):
        out.append(line)
with open(os.path.join(WS, "reverse/analysis/deviations.md"), "w", encoding="utf-8", newline="\n") as fh:
    fh.write("\n".join(out) + "\n")
print("rows", sum(1 for l in out if l.startswith("| ") and not l.startswith("| 函数") and not l.startswith("| ---")),
      "addresses converted", converted, "unmapped", unmapped)
