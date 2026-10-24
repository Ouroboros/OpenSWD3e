"""Summarises the output of ida_scope_check.py against reverse/functions.tsv.

Usage: python3 -I scope_summary.py <workspace> <scope_check.json>

Prints the function starts of the database missing from the table (and the reverse), the address-taken targets by
class (a function start, an instruction inside a function, an inner byte, code or data outside every function), the
inner targets with their sources, the runtime table 0x0050A8B0 and the indirect branch sites by owner function.
"""

import collections
import csv
import json
import os
import sys

WS, PATH = sys.argv[1], sys.argv[2]
data = json.load(open(PATH, encoding="utf-8"))
rows = list(csv.DictReader(open(os.path.join(WS, "reverse/functions.tsv"), encoding="utf-8"), delimiter="\t"))
table = {int(row["address"], 16) for row in rows}
ida = {int(f["start"], 16) for f in data["functions"]}
print("functions: table %d, database %d" % (len(table), len(ida)))
print("  in the database, not in the table:", ["0x%08X" % a for a in sorted(ida - table)][:40])
print("  in the table, not in the database:", ["0x%08X" % a for a in sorted(table - ida)][:40])

by_class = collections.Counter(t["class"] for t in data["targets"])
print("address-taken .text targets:", dict(by_class))

for t in data["targets"]:
    if t["class"] == "func_start" and int(t["target"], 16) not in table:
        print("  function start not in the table:", t["target"], t["kinds"], t["sources"][:3])

for kind in ("in_func_head", "in_func_inner", "no_func_code", "no_func_data"):
    items = [t for t in data["targets"] if t["class"] == kind]
    print("%s: %d" % (kind, len(items)))

    for t in items[:200]:
        print("  %s owner %s kinds %s sources %s" % (t["target"], t["owner"], ",".join(t["kinds"]),
                                                      " ".join(t["sources"][:4])))

print("table 0x0050A8B0:", " ".join(data["table_50A8B0"]))
print("  xrefs:", data["table_50A8B0_xrefs"])
owners = collections.Counter(site["func"] for site in data["indirect"])
print("indirect branch sites: %d in %d functions" % (len(data["indirect"]), len(owners)))

# An in-function label taken as an address must belong to the function of its source: an operand of an instruction
# of that function, or a dword of the data after its body (a switch table) before the next function.
starts = sorted(int(f["start"], 16) for f in data["functions"])


def region_of(address):
    import bisect

    index = bisect.bisect_right(starts, address) - 1
    return "0x%08X" % starts[index] if index >= 0 else None


print("in-function labels whose sources lie outside their function:")

for t in data["targets"]:
    if t["class"] not in ("in_func_head", "in_func_inner"):
        continue

    foreign = [s for s in t["sources"] if region_of(int(s, 16)) != t["owner"] or
               not any(k.startswith("operand") or k == "data:.text" for k in t["kinds"])]

    if foreign:
        print("  %s (%s) owner %s kinds %s from %s" % (t["target"], t["class"], t["owner"], ",".join(t["kinds"]),
                                                     " ".join(foreign)))
