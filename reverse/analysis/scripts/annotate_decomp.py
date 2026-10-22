"""Prints the decompilation of a swd3e function with the globals the C++ sources already name replaced by
`Class::member` (taken from the `// 0x<address>` comments after member declarations in src/**/*.h) and the callees the
function table maps replaced by their source symbol. Declarations are dropped. Globals inside a named member's span are
shown as `member+offset` when the member's size is not known only for the exact address.

Usage: python3 -I annotate_decomp.py <workspace> <swd3e address> [...]
"""

import glob
import os
import re
import sys

WS = sys.argv[1]
names = {}
DECL = re.compile(r"^\s*(?:static\s+)?(?:inline\s+)?(?:const\s+)?[\w:<>,\s\*&]+?\b(\w+)(?:\s*=[^;]*|\s*\{[^;]*\})?;\s*//\s*(0x[0-9A-Fa-f]{8})\b")
for path in glob.glob(os.path.join(WS, "src/**/*.h"), recursive=True):
    cls = None
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            m = re.match(r"^(?:class|struct)\s+(\w+)", line)
            if m:
                cls = m.group(1)
            m = DECL.match(line)
            if m:
                address = int(m.group(2), 16)
                if address >= 0x004A0000:
                    names.setdefault(address, "%s::%s" % (cls or "?", m.group(1)))

callees = {}
with open(os.path.join(WS, "reverse/functions.tsv"), encoding="utf-8") as fh:
    fh.readline()
    for line in fh:
        p = line.rstrip("\n").split("\t")
        if len(p) > 5 and p[5]:
            callees[int(p[2], 16)] = p[5].split(":")[-1]

DECLARATION = re.compile(r"^  [a-zA-Z_][a-zA-Z_0-9 *]*(\[[0-9A-Fx]*\])?; //")


def name_global(match):
    address = int(match.group(2), 16)
    if address in names:
        return "{%s}" % names[address]
    return match.group(0)


def name_call(match):
    address = int(match.group(1), 16)
    if address in callees:
        return "%s<%X>" % (callees[address], address)
    return match.group(0)


for argument in sys.argv[2:]:
    address = int(argument, 16)
    path = os.path.join(WS, "reverse/analysis/decomp/%08X.c" % address)
    print("== 0x%08X" % address)
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            if DECLARATION.match(line):
                continue
            line = re.sub(r"\b(dword|word|byte|unk|qword|off|stru)_\??([0-9A-F]{6,8})\b", name_global, line)
            line = re.sub(r"\bsub_([0-9A-F]{6,8})\b", name_call, line)
            sys.stdout.write(line)
