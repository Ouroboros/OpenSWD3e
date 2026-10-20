"""From the CHECK lines of zero_diff_check.py: the functions whose only disagreements are swd3e's zeroed block
allocation and its free (0x0043DA70/0x0043DAB0) against swd32's malloc/free, with the C++ body of their source
searched for code depending on the content of a fresh block (debug fill, uninitialised memory).

Usage: python3 -I heap_only_check.py <workspace> <zero_check.txt>
"""

import os
import re
import sys

WS, CHECK = sys.argv[1], sys.argv[2]
ALLOC = re.compile(r"callee 0x0043DA70\([^)]*\)/0x[0-9A-F]+\(malloc\)")
FREE = re.compile(r"callee 0x0043DAB0\([^)]*\)/0x[0-9A-F]+\(free\)")
RISK = re.compile(r"uninitiali|debug heap|debug fill|0xCD|garbage|stale|left over|keeps? (its|the) (old|previous)", re.I)


def body(source):
    path, _, symbol = source.partition(":")
    full = os.path.join(WS, path)
    if not os.path.exists(full) or not symbol:
        return None
    text = open(full, encoding="utf-8").read()
    name = re.escape(symbol)
    m = re.search(r"\n[^\n]*\b" + name + r"\(", text)
    if not m:
        return None
    start = text.rfind("\n\n", 0, m.start()) + 1
    depth = 0
    i = text.find("{", m.end())
    if i < 0:
        return None
    j = i
    while j < len(text):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                break
        j += 1
    return text[start:j + 1]


for line in open(CHECK, encoding="utf-8"):
    if not line.startswith("CHECK "):
        continue
    head, _, problems = line.rstrip("\n").partition(": ")
    rest = FREE.sub("", ALLOC.sub("", problems)).replace(";", "").strip()
    parts = head.split()
    address, source = parts[1], parts[2]
    if rest:
        print("OTHER %s %s: %s" % (address, source, problems[:160]))
        continue
    text = body(source)
    if text is None:
        print("NOBODY %s %s" % (address, source))
        continue
    risky = sorted(set(m.group(0) for m in RISK.finditer(text)))
    print("%s %s %s %s" % ("RISK" if risky else "HEAP", address, source, ",".join(risky)))
