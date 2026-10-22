"""Maps the structure offsets of swd32 to those of swd3e through the paired functions of the given modules: the
instructions of each pair are aligned by their skeleton (mnemonic and operands with every number and name blanked) and
the displacements of the aligned register-based memory operands ([reg+disp], not esp/ebp) at or above a minimum are
paired and counted. Writes reverse/analysis/export/match_swd32/fields_<name>.tsv (swd32, swd3e, votes, share).

Usage: python3 -I field_map.py <workspace> <swd3-rebuild> <name> <minimum offset> <module> [...]
"""

import collections
import difflib
import os
import re
import sys

WS, REF, NAME, MINIMUM = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4], 0)
MODULES = set(sys.argv[5:])

modules = {}
with open(os.path.join(WS, "reverse/functions.tsv"), encoding="utf-8") as fh:
    fh.readline()
    for line in fh:
        p = line.rstrip("\n").split("\t")
        modules[p[2]] = p[4]

pairs = []
with open(os.path.join(WS, "reverse/analysis/export/match_swd32/functions.tsv"), encoding="utf-8") as fh:
    header = fh.readline().rstrip("\n").split("\t")
    for line in fh:
        row = dict(zip(header, line.rstrip("\n").split("\t")))
        if row.get("a_address") and row.get("b_address") and modules.get(row["a_address"]) in MODULES:
            pairs.append((row["a_address"], row["b_address"]))

LINE = re.compile(r"^[0-9A-F]{8} [0-9a-f]+\s+sp=\S+\s+(.*?)(?:\s+; -> [0-9A-F]{8})?\s*$")
NAMES = re.compile(r"\b(?:sub|loc|dword|word|byte|unk|off|stru|asc|qword|flt|dbl|locret|j)_[0-9A-F]+\b")
NUMBER = re.compile(r"\b[0-9][0-9A-F]*h?\b")
MEMORY = re.compile(r"\[(e(?:ax|bx|cx|dx|si|di))(?:\+e\w\w(?:\*\d)?)?\+([0-9A-F]+)h?\]")


def load(path):
    lines = []
    if not os.path.exists(path):
        return lines
    with open(path, encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            match = LINE.match(raw.rstrip("\n"))
            if not match:
                continue
            text = re.sub(r"\s+", " ", match.group(1))
            skeleton = NUMBER.sub("N", NAMES.sub("S", text))
            displacements = [int(m.group(2), 16) for m in MEMORY.finditer(text)]
            lines.append((skeleton, displacements))
    return lines


votes = collections.Counter()
for a_address, b_address in pairs:
    a = load(os.path.join(WS, "reverse/analysis/asm/%s.asm" % a_address[2:]))
    b = load(os.path.join(REF, "reverse/analysis/asm/%s.asm" % b_address[2:]))
    if not a or not b:
        continue
    matcher = difflib.SequenceMatcher(None, [x[0] for x in b], [x[0] for x in a], autojunk=False)
    for block in matcher.get_matching_blocks():
        for k in range(block.size):
            db = b[block.a + k][1]
            da = a[block.b + k][1]
            if len(db) == len(da):
                for x, y in zip(db, da):
                    if x >= MINIMUM and y >= MINIMUM:
                        votes[(x, y)] += 1

by_b = collections.defaultdict(list)
for (x, y), n in votes.items():
    by_b[x].append((n, y))

out = os.path.join(WS, "reverse/analysis/export/match_swd32/fields_%s.tsv" % NAME)
with open(out, "w", encoding="utf-8") as fh:
    fh.write("swd32\tswd3e\tvotes\tshare\n")
    for x in sorted(by_b):
        total = sum(n for n, _ in by_b[x])
        for n, y in sorted(by_b[x], reverse=True):
            fh.write("0x%X\t0x%X\t%d\t%.2f\n" % (x, y, n, n / total))
print("pairs", len(pairs), "offsets", len(by_b), "->", out)
