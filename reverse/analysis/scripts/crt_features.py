"""Features of the pending CRT functions for their identification: size, imports, strings, named callees, callers
from the game code (below 0x004A1096) and CRT, and the first instructions of the listing.

Usage: python3 -I crt_features.py <workspace>
"""

import csv
import json
import os
import sys

WS = sys.argv[1]
CRT_START = 0x4A1096
with open(os.path.join(WS, "reverse/functions.tsv"), encoding="utf-8", newline="") as fh:
    rows = list(csv.DictReader(fh, delimiter="\t", quoting=csv.QUOTE_NONE))
names = {}
for r in rows:
    src = r["source"]
    if src:
        names[int(r["address"], 16)] = src.split(" -> ")[0].replace("CRT:", "")
struct = {f["ea"]: f for f in json.load(open(os.path.join(WS, "reverse/analysis/export/structure.json"), encoding="utf-8"))}
for r in rows:
    if r["module"] != "crt" or r["status"] != "pending_implementation":
        continue
    a = int(r["address"], 16)
    s = struct.get(a, {})
    callees = [names.get(c, "%X" % c) for c in s.get("callees", [])]
    game_callers = sum(1 for c in s.get("callers", []) if c < CRT_START)
    crt_callers = sum(1 for c in s.get("callers", []) if c >= CRT_START)
    first = []
    path = os.path.join(WS, "reverse/analysis/asm", "%08X.asm" % a)
    if os.path.exists(path):
        with open(path, encoding="utf-8") as fh:
            for line in fh:
                if line[:1].isdigit() or line[:1] in "ABCDEF":
                    first.append(line[48:].strip().split("    ;")[0])
                if len(first) >= 6:
                    break
    print("%08X size=%d game=%d crt=%d imp=%s str=%s calls=%s | %s" % (
        a, s.get("size", 0), game_callers, crt_callers,
        ",".join(i.split("!")[1] for i in s.get("imports", [])), "|".join(s.get("strings", []))[:60],
        ",".join(callees)[:80], " ; ".join(first)[:150]))
