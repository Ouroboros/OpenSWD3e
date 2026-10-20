"""Marks functions of reverse/functions.tsv implemented_pending_review (when pending) with a note on why the code
in their source is the swd3e implementation; the note goes before the previous notes.

Usage: python3 -I mark_implemented.py <workspace> <note> <address>...
"""

import os
import sys

WS, NOTE = sys.argv[1], sys.argv[2]
wanted = {"0x%08X" % int(a, 16) for a in sys.argv[3:]}
path = os.path.join(WS, "reverse/functions.tsv")
with open(path, encoding="utf-8", newline="") as fh:
    lines = fh.read().split("\n")
found = set()
for i, line in enumerate(lines[1:], 1):
    cells = line.split("\t")
    if len(cells) < 10 or cells[2] not in wanted:
        continue
    if not cells[5]:
        raise SystemExit("%s has no source" % cells[2])
    if cells[6] == "pending_implementation":
        cells[6] = "implemented_pending_review"
    cells[9] = NOTE + "; " + cells[9]
    lines[i] = "\t".join(cells)
    found.add(cells[2])
missing = sorted(wanted - found)
if missing:
    raise SystemExit("not in the table: " + " ".join(missing))
with open(path, "w", encoding="utf-8", newline="") as fh:
    fh.write("\n".join(lines))
counts = {}
for line in lines[1:]:
    cells = line.split("\t")
    if len(cells) > 6:
        counts[cells[6]] = counts.get(cells[6], 0) + 1
print(len(found), "marked", counts)
