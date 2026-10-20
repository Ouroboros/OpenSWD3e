"""Applies reverse/analysis/crt_identification.tsv to reverse/functions.tsv: source "CRT:<name> -> <mapping>",
status implemented_pending_review (the CRT is mapped, not rebuilt) and notes with the identification evidence.

Usage: python3 -I apply_crt.py <workspace>
"""

import os
import sys

WS = sys.argv[1]
ids = {}
with open(os.path.join(WS, "reverse/analysis/crt_identification.tsv"), encoding="utf-8") as fh:
    for line in fh:
        if line.startswith("#") or not line.strip():
            continue
        address, name, mapping, evidence = line.rstrip("\n").split("\t")
        ids[address] = (name, mapping, evidence)
path = os.path.join(WS, "reverse/functions.tsv")
with open(path, encoding="utf-8", newline="") as fh:
    lines = fh.read().split("\n")
header = lines[0].split("\t")
done = 0
for i, line in enumerate(lines[1:], 1):
    cells = line.split("\t")
    if len(cells) < len(header) or cells[2] not in ids:
        continue
    name, mapping, evidence = ids[cells[2]]
    cells[4] = "crt"
    cells[5] = "CRT:%s -> %s" % (name, mapping)
    if cells[6] == "pending_implementation":
        cells[6] = "implemented_pending_review"
    cells[9] = "%s (MSVC 6 static CRT, identified by %s); not rebuilt, %s [prior: %s]" % (name, evidence, mapping, cells[9])
    lines[i] = "\t".join(cells)
    done += 1
with open(path, "w", encoding="utf-8", newline="") as fh:
    fh.write("\n".join(lines))
print("applied", done, "of", len(ids))
