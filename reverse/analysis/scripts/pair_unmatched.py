"""Proposes swd3e counterparts for the swd32 functions the sources still give as unmatched ("swd32.exe 0x... (unmatched
in swd3e)"): each such swd32 function and each unpaired swd3e function of the same module is described by its callers
and callees mapped through the existing pairs (swd32 -> swd3e); candidates are ranked by the overlap (Jaccard) of the
two sets, with the size ratio as a tie-break. Writes reverse/analysis/export/match_swd32/unmatched_candidates.tsv.

Usage: python3 -I pair_unmatched.py <workspace> <swd3-rebuild> <module> <source>...
"""

import collections
import os
import re
import sys

WS, REF, MODULE = sys.argv[1], sys.argv[2], sys.argv[3]
SOURCES = sys.argv[4:]

pairs = {}
with open(os.path.join(WS, "reverse/analysis/export/match_swd32/functions.tsv"), encoding="utf-8") as fh:
    header = fh.readline().rstrip("\n").split("\t")
    for line in fh:
        row = dict(zip(header, line.rstrip("\n").split("\t")))
        if row.get("a_address") and row.get("b_address", "").startswith("0x"):
            pairs[int(row["b_address"], 16)] = int(row["a_address"], 16)

rows = {}
with open(os.path.join(WS, "reverse/functions.tsv"), encoding="utf-8") as fh:
    fh.readline()
    for line in fh:
        p = line.rstrip("\n").split("\t")
        rows[int(p[2], 16)] = p

CALL = re.compile(r"call\s+(?:sub|j_sub)_([0-9A-F]+)")


def graph(directory):
    callees = collections.defaultdict(set)
    callers = collections.defaultdict(set)
    sizes = {}
    for name in os.listdir(directory):
        if not name.endswith(".asm"):
            continue
        caller = int(name[:8], 16)
        count = 0
        with open(os.path.join(directory, name), encoding="utf-8", errors="replace") as fh:
            for line in fh:
                if re.match(r"^[0-9A-F]{8} ", line):
                    count += 1
                m = CALL.search(line)
                if m:
                    callee = int(m.group(1), 16)
                    callees[caller].add(callee)
                    callers[callee].add(caller)
        sizes[caller] = count
    return callees, callers, sizes


mine_callees, mine_callers, mine_sizes = graph(os.path.join(WS, "reverse/analysis/asm"))
their_callees, their_callers, their_sizes = graph(os.path.join(REF, "reverse/analysis/asm"))
paired_mine = set(pairs.values())

wanted = []
for path in SOURCES:
    with open(path, encoding="utf-8") as fh:
        for number, line in enumerate(fh):
            for m in re.finditer(r"swd32\.exe (0x[0-9A-F]{8}) \(unmatched in swd3e\)", line):
                wanted.append((int(m.group(1), 16), "%s:%d" % (path, number + 1)))

candidates = [a for a, p in rows.items() if p[4] == MODULE and a not in paired_mine]


def mapped(addresses):
    return {("p", pairs[a]) for a in addresses if a in pairs}


def own(addresses):
    return {("p", a) for a in addresses if a in paired_mine}


out = os.path.join(WS, "reverse/analysis/export/match_swd32/unmatched_candidates.tsv")
with open(out, "w", encoding="utf-8") as fh:
    fh.write("swd32\tsource\tswd3e\tscore\tsize_swd32\tsize_swd3e\tsecond\n")
    for theirs, where in wanted:
        their_set = {("in",) + x for x in mapped(their_callers[theirs])} | {("out",) + x for x in mapped(their_callees[theirs])}
        ranked = []
        for mine in candidates:
            my_set = {("in",) + x for x in own(mine_callers[mine])} | {("out",) + x for x in own(mine_callees[mine])}
            union = their_set | my_set
            if not union:
                continue
            score = len(their_set & my_set) / len(union)
            ratio = min(their_sizes.get(theirs, 1), mine_sizes.get(mine, 1)) / max(their_sizes.get(theirs, 1), mine_sizes.get(mine, 1), 1)
            ranked.append((score, ratio, mine))
        ranked.sort(reverse=True)
        best = ranked[0] if ranked else (0, 0, 0)
        second = ranked[1] if len(ranked) > 1 else (0, 0, 0)
        fh.write("0x%08X\t%s\t0x%08X\t%.2f\t%d\t%d\t0x%08X:%.2f\n" % (
            theirs, where, best[2], best[0], their_sizes.get(theirs, 0), mine_sizes.get(best[2], 0), second[2], second[0]))
print("wanted", len(wanted), "candidates", len(candidates), "->", out)
