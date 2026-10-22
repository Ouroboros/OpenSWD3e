"""Maps the globals of swd32 to those of swd3e through the paired functions: the instructions of each pair are aligned
by their skeleton (mnemonic and operands with every number and name blanked) and the data addresses the aligned lines
refer to ("; -> address" in the listings) are paired and counted. Writes
reverse/analysis/export/match_swd32/globals_asm.tsv (swd32, swd3e, votes, share of the swd32 address's votes).

Usage: python3 -I global_map.py <workspace> <swd3-rebuild>
"""

import collections
import difflib
import os
import re
import sys

WS, REF = sys.argv[1], sys.argv[2]
DATA_START = 0x004A0000  # .rdata and .data of both binaries lie above the code

pairs = []
with open(os.path.join(WS, "reverse/analysis/export/match_swd32/functions.tsv"), encoding="utf-8") as fh:
    header = fh.readline().rstrip("\n").split("\t")
    for line in fh:
        row = dict(zip(header, line.rstrip("\n").split("\t")))
        if row.get("a_address") and row.get("b_address"):
            pairs.append((row["a_address"], row["b_address"]))

LINE = re.compile(r"^[0-9A-F]{8} [0-9a-f]+\s+sp=\S+\s+(.*?)(?:\s+; -> ([0-9A-F]{8}))?\s*$")
NAME = re.compile(r"\b(?:sub|loc|dword|word|byte|unk|off|stru|asc|qword|flt|dbl|locret|j)_[0-9A-F]+\b")
NUMBER = re.compile(r"\b[0-9][0-9A-F]*h?\b")


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
            skeleton = NUMBER.sub("N", NAME.sub("S", text))
            target = int(match.group(2), 16) if match.group(2) else None
            lines.append((skeleton, target))
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
            tb = b[block.a + k][1]
            ta = a[block.b + k][1]
            if tb is not None and ta is not None and tb >= DATA_START and ta >= DATA_START:
                votes[(tb, ta)] += 1

by_b = collections.defaultdict(list)
for (tb, ta), n in votes.items():
    by_b[tb].append((n, ta))

out = os.path.join(WS, "reverse/analysis/export/match_swd32/globals_asm.tsv")
with open(out, "w", encoding="utf-8") as fh:
    fh.write("swd32\tswd3e\tvotes\tshare\n")
    for tb in sorted(by_b):
        total = sum(n for n, _ in by_b[tb])
        for n, ta in sorted(by_b[tb], reverse=True):
            fh.write("0x%08X\t0x%08X\t%d\t%.2f\n" % (tb, ta, n, n / total))
print("pairs", len(pairs), "swd32 globals", len(by_b), "->", out)
