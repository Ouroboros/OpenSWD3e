"""Surveys the fields of the swd3e battle units (BattleUnit, BattleRole, BattleEnemy): every `*(_TYPE *)(this + 0xN)`
style access in the decompilation of the battle module's __thiscall functions, its width and the functions using it,
against the offsets the C++ headers give their members ("// +0xN"). Writes reverse/analysis/export/unit_layout.tsv
(offset, widths, functions, C++ member or empty) and prints the offsets the headers do not name.

Usage: python3 -I unit_layout.py <workspace>
"""

import collections
import os
import re
import sys

WS = sys.argv[1]
HEADERS = ["src/battle/battle_unit.h", "src/battle/battle_role.h", "src/battle/battle_enemy.h"]
UNIT_SIZE = 0x34D4

modules = {}
with open(os.path.join(WS, "reverse/functions.tsv"), encoding="utf-8") as fh:
    fh.readline()
    for line in fh:
        p = line.rstrip("\n").split("\t")
        modules[p[2]] = p[4]

WIDTH = {"_BYTE": 1, "_WORD": 2, "_DWORD": 4, "char": 1, "__int16": 2, "int": 4, "unsigned __int8": 1,
         "unsigned __int16": 2, "unsigned int": 4, "float": 4, "LPVOID": 4, "void *": 4}
TYPED = re.compile(r"\*\(\(?(?:const )?(_BYTE|_WORD|_DWORD|char|__int16|int|unsigned __int8|unsigned __int16|"
                   r"unsigned int|float|LPVOID|void \*) \*\)\(this \+ (0x[0-9A-F]+|\d+)\)")
SCALED = re.compile(r"\*\(this \+ (0x[0-9A-F]+|\d+)\)")
SIGNATURE = re.compile(r"__thiscall sub_\w+\(([\w ]+?) ?\*?this")

uses = collections.defaultdict(lambda: [set(), set()])
for name in sorted(os.listdir(os.path.join(WS, "reverse/analysis/decomp"))):
    address = "0x" + name[:8]
    if modules.get(address) != "battle":
        continue
    with open(os.path.join(WS, "reverse/analysis/decomp", name), encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    signature = SIGNATURE.search(text)
    if not signature:
        continue
    pointee = signature.group(1).strip()
    element = {"int": 1, "_DWORD": 4, "unsigned int": 4, "_WORD": 2, "__int16": 2, "unsigned __int16": 2,
               "_BYTE": 1, "char": 1, "unsigned __int8": 1}.get(pointee, 1)
    if "(_BYTE *)this" in text:
        pass
    for match in TYPED.finditer(text):
        offset = int(match.group(2), 0)
        if offset < UNIT_SIZE:
            uses[offset][0].add(WIDTH[match.group(1)])
            uses[offset][1].add(address)
    for match in SCALED.finditer(text):
        offset = int(match.group(1), 0) * element
        if offset < UNIT_SIZE:
            uses[offset][0].add(element)
            uses[offset][1].add(address)

named = {}
for header in HEADERS:
    with open(os.path.join(WS, header), encoding="utf-8") as fh:
        for line in fh:
            match = re.search(r"\b(\w+)(?:\s*=[^;]*)?;\s*//\s*\+0x([0-9A-F]+)\b", line)
            if match:
                named.setdefault(int(match.group(2), 16), match.group(1))

out = os.path.join(WS, "reverse/analysis/export/unit_layout.tsv")
missing = 0
with open(out, "w", encoding="utf-8") as fh:
    fh.write("offset\twidths\tfunctions\tmember\n")
    for offset in sorted(uses):
        widths, functions = uses[offset]
        member = named.get(offset, "")
        fh.write("0x%X\t%s\t%s\t%s\n" % (offset, ",".join(map(str, sorted(widths))),
                                        " ".join(sorted(functions)), member))
        if not member:
            missing += 1
print("offsets", len(uses), "unnamed", missing, "->", out)
