"""Prints, for each given swd3e address, its row (source, pairing), the operand differences against swd32
(reverse/analysis/diff_swd32/) and the C++ body of its source symbol.

Usage: python3 -I show_fn.py <workspace> <address>... [--no-body] [--diff N]
"""

import csv
import os
import re
import sys

WS = sys.argv[1]
args = sys.argv[2:]
show_body = "--no-body" not in args
limit = 60
if "--diff" in args:
    limit = int(args[args.index("--diff") + 1])
addresses = [a for a in args if a.startswith("0x")]
with open(os.path.join(WS, "reverse/functions.tsv"), encoding="utf-8", newline="") as fh:
    rows = {r["address"]: r for r in csv.DictReader(fh, delimiter="\t", quoting=csv.QUOTE_NONE)}


def body(source):
    path, _, symbol = source.partition(":")
    full = os.path.join(WS, path)
    if not os.path.exists(full) or not symbol:
        return None
    text = open(full, encoding="utf-8").read()
    m = re.search(r"\n[^\n;]*\b" + re.escape(symbol) + r"\([^;]*?\)\s*(const)?\s*\n\{", text)
    if not m:
        return None
    start = text.rfind("\n\n", 0, m.start()) + 1
    depth = 0
    j = text.find("{", m.end() - 1)
    while j < len(text):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                break
        j += 1
    return text[start:j + 1]


for a in addresses:
    a = "0x%08X" % int(a, 16)
    r = rows.get(a, {})
    print("######## %s %s [%s] %s" % (a, r.get("source", ""), r.get("status", ""), r.get("notes", "").split(";")[0]))
    path = os.path.join(WS, "reverse/analysis/diff_swd32", a[2:] + ".txt")
    if os.path.exists(path):
        lines = open(path, encoding="utf-8").read().splitlines()
        for line in lines[1:2 + limit]:
            print("  " + line)
        if len(lines) > 2 + limit:
            print("  ... %d more" % (len(lines) - 2 - limit))
    if show_body and r.get("source"):
        text = body(r["source"])
        print(text if text else "  (body not found)")
