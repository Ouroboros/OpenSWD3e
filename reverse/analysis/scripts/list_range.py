"""Functions of reverse/functions.tsv in an address range with module, status, size, callers and callees (named
where the table has a source), for identifying unmatched code.

Usage: python3 -I list_range.py <workspace> <start hex> <end hex> [module]
"""

import csv
import json
import os
import sys

WS = sys.argv[1]
START, END = int(sys.argv[2], 16), int(sys.argv[3], 16)
ONLY = sys.argv[4] if len(sys.argv) > 4 else None
with open(os.path.join(WS, "reverse/functions.tsv"), encoding="utf-8", newline="") as fh:
    rows = list(csv.DictReader(fh, delimiter="\t", quoting=csv.QUOTE_NONE))
names = {int(r["address"], 16): (r["source"].split(":")[-1] if r["source"] else r["address"]) for r in rows}
struct = {f["ea"]: f for f in json.load(open(os.path.join(WS, "reverse/analysis/export/structure.json"), encoding="utf-8"))}
for r in rows:
    a = int(r["address"], 16)
    if not START <= a < END or (ONLY and r["module"] != ONLY):
        continue
    s = struct.get(a, {})
    callers = [names.get(c, "%X" % c) for c in s.get("callers", [])]
    callees = [names.get(c, "%X" % c) for c in s.get("callees", [])]
    tables = s.get("tables", [])
    print("%s %-10s %-26s size=%-5d callers=%s callees=%s%s" % (
        r["address"], r["module"], r["status"][:12], s.get("size", 0), ",".join(callers)[:70], ",".join(callees)[:70],
        " tables=" + ",".join("%X" % t for t in tables[:3]) if tables else ""))
