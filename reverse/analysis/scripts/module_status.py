"""Per module counts of reverse/functions.tsv joined with reverse/analysis/diff_swd32/summary.tsv.

Usage: python3 -I module_status.py <workspace>
"""

import csv
import os
import sys

WS = sys.argv[1]
with open(os.path.join(WS, "reverse/functions.tsv"), encoding="utf-8", newline="") as fh:
    rows = list(csv.DictReader(fh, delimiter="\t", quoting=csv.QUOTE_NONE))
diff = {}
with open(os.path.join(WS, "reverse/analysis/diff_swd32/summary.tsv"), encoding="utf-8", newline="") as fh:
    for r in csv.DictReader(fh, delimiter="\t"):
        diff[r["a_address"]] = int(r["changed"])

table = {}
for r in rows:
    m = table.setdefault(r["module"], {"total": 0, "impl": 0, "exact": 0, "zero": 0, "small": 0, "large": 0,
                                       "unmatched": 0})
    m["total"] += 1
    if r["status"] == "assembly_exact":
        m["exact"] += 1
    if r["status"] in ("implemented_pending_review", "assembly_exact"):
        m["impl"] += 1
        continue
    changed = diff.get(r["address"])
    if changed is None:
        m["unmatched"] += 1
    elif changed == 0:
        m["zero"] += 1
    elif changed <= 20:
        m["small"] += 1
    else:
        m["large"] += 1
print("%-12s %5s %5s %5s | pending: %5s %5s %5s %5s" % ("module", "total", "impl", "exact", "zero", "<=20", ">20",
                                                         "unm"))
for name, m in sorted(table.items(), key=lambda kv: -kv[1]["total"]):
    print("%-12s %5d %5d %5d | %14d %5d %5d %5d" % (name, m["total"], m["impl"], m["exact"], m["zero"], m["small"],
                                                    m["large"], m["unmatched"]))
tot = {k: sum(m[k] for m in table.values()) for k in ("total", "impl", "exact", "zero", "small", "large", "unmatched")}
print("TOTAL", tot)
