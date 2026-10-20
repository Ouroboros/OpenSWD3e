"""Diffs the decompilation of a swd3e function against that of its swd32 pair with the names that only carry
addresses (sub_, dword_, unk_, loc_, ...) and the declarations normalised away, leaving the lines whose logic differs.
The globals and callees still differ by name in the output only when they map to different swd32 names.

Usage: python3 -I decomp_diff.py <workspace> <swd3-rebuild> <swd3e address> [context lines]
"""

import difflib
import os
import re
import sys

WS, REF, ADDRESS = sys.argv[1], sys.argv[2], "0x%08X" % int(sys.argv[3], 16)
CONTEXT = int(sys.argv[4]) if len(sys.argv) > 4 else 1

pairs = {}
with open(os.path.join(WS, "reverse/analysis/export/match_swd32/functions.tsv"), encoding="utf-8") as fh:
    header = fh.readline().rstrip("\n").split("\t")
    for line in fh:
        row = dict(zip(header, line.rstrip("\n").split("\t")))
        pairs[row.get("a_address", "")] = row.get("b_address", "")
other = pairs.get(ADDRESS)
if not other:
    raise SystemExit("no swd32 pair for " + ADDRESS)

# The globals matched between the two binaries map swd3e names to swd32 ones before the rest is normalised.
global_map = {}
globals_path = os.path.join(WS, "reverse/analysis/export/match_swd32/globals.tsv")
if os.path.exists(globals_path):
    with open(globals_path, encoding="utf-8") as fh:
        header = fh.readline().rstrip("\n").split("\t")
        for line in fh:
            row = dict(zip(header, line.rstrip("\n").split("\t")))
            if row.get("a_address") and row.get("b_address"):
                global_map[int(row["a_address"], 16)] = int(row["b_address"], 16)
function_map = {int(k, 16): int(v, 16) for k, v in pairs.items() if k and v and v.startswith("0x")}

NAME = re.compile(r"\b(sub|dword|word|byte|unk|loc|off|stru|qword|flt|dbl|asc|jpt|def|LABEL)_([0-9A-F]+)\b")


def load(path, translate):
    with open(path, encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    body = text[text.index("{"):] if "{" in text else text
    out = []
    for line in body.split("\n"):
        if re.match(r"^\s+[A-Za-z_][\w ]*[ *]+\w+(\[\w+\])?; // ", line):
            continue  # local declarations
        line = re.sub(r"\s*//.*$", "", line)

        def name(match):
            kind, value = match.group(1), int(match.group(2), 16)
            if translate:
                if kind == "sub" and value in function_map:
                    return "sub_%X" % function_map[value]
                if value in global_map:
                    return "%s_%X" % (kind, global_map[value])
            if kind in ("loc", "LABEL", "jpt", "def"):
                return kind
            return "%s_%X" % (kind, value) if not translate else "%s_?%X" % (kind, value)

        line = NAME.sub(name, line)
        if translate:
            # The map roles: 0xE4 bytes in swd3e, 0xD8 in swd32 (as bytes, dwords and words).
            for mine_stride, their_stride in (("0xE4", "0xD8"), ("0x39", "0x36"), ("0x72", "0x6C")):
                line = re.sub(r"\b%s \*" % mine_stride, their_stride + " *", line)
        line = re.sub(r"\bv\d+\b", "v", line)
        line = re.sub(r"\ba[A-Z]\w*", "aSTR", line)
        if line.strip():
            out.append(line.rstrip())
    return out


mine = load(os.path.join(WS, "reverse/analysis/decomp/%s.c" % ADDRESS[2:]), True)
theirs = load(os.path.join(REF, "reverse/analysis/decomp/%s.c" % other[2:]), False)
for line in difflib.unified_diff(theirs, mine, "swd32 " + other, "swd3e " + ADDRESS, n=CONTEXT, lineterm=""):
    print(line)
