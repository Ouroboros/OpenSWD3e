"""Lists the pending functions of a module with their swd32 pairing and the operand differences from
reverse/analysis/diff_swd32/ (at most `limit` difference lines each).

Usage: python3 -I module_todo.py <workspace> <module> [limit]
"""

import csv
import os
import sys

WS, MODULE = sys.argv[1], sys.argv[2]
LIMIT = int(sys.argv[3]) if len(sys.argv) > 3 else 12
with open(os.path.join(WS, "reverse/functions.tsv"), encoding="utf-8", newline="") as fh:
    rows = [r for r in csv.DictReader(fh, delimiter="\t", quoting=csv.QUOTE_NONE) if r["module"] == MODULE]
for r in rows:
    if r["status"] != "pending_implementation":
        continue
    print("== %s %s %s" % (r["address"], r["source"] or "-", r["notes"].split(";")[0]))
    path = os.path.join(WS, "reverse/analysis/diff_swd32", r["address"][2:] + ".txt")
    if os.path.exists(path):
        with open(path, encoding="utf-8") as fh:
            lines = fh.read().splitlines()
        print("   " + lines[1])
        for line in lines[2:2 + LIMIT]:
            print("   " + line)
        if len(lines) > 2 + LIMIT:
            print("   ... %d more" % (len(lines) - 2 - LIMIT))
