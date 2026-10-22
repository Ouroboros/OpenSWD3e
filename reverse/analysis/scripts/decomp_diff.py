"""Diffs the decompilation of a swd3e function against that of its swd32 pair with the names that only carry
addresses (sub_, dword_, unk_, loc_, ...) and the declarations normalised away, leaving the lines whose logic differs.
The globals and callees still differ by name in the output only when they map to different swd32 names.

Usage: python3 -I decomp_diff.py <workspace> <swd3-rebuild> <swd3e address> [context lines] [field map name]
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

# The globals paired through the aligned instructions of the paired functions (global_map.py) fill the gaps.
asm_globals_path = os.path.join(WS, "reverse/analysis/export/match_swd32/globals_asm.tsv")
if os.path.exists(asm_globals_path):
    best = {}
    with open(asm_globals_path, encoding="utf-8") as fh:
        fh.readline()
        for line in fh:
            theirs_address, mine_address, votes, share = line.rstrip("\n").split("\t")
            key = int(mine_address, 16)
            if int(votes) >= 2 and float(share) >= 0.6 and (key not in best or int(votes) > best[key][1]):
                best[key] = (int(theirs_address, 16), int(votes))
    for key, (value, _) in best.items():
        global_map.setdefault(key, value)

# Optional structure offsets (field_map.py, fields_<name>.tsv given as the fifth argument): the swd32 offsets are
# rewritten to the swd3e ones before diffing.
field_map = {}
if len(sys.argv) > 5:
    with open(os.path.join(WS, "reverse/analysis/export/match_swd32/fields_%s.tsv" % sys.argv[5]), encoding="utf-8") as fh:
        fh.readline()
        best = {}
        for line in fh:
            theirs_offset, mine_offset, votes, share = line.rstrip("\n").split("\t")
            key = int(theirs_offset, 16)
            if int(votes) >= 2 and float(share) >= 0.6 and (key not in best or int(votes) > best[key][1]):
                best[key] = (int(mine_offset, 16), int(votes))
        field_map = {key: value for key, (value, _) in best.items()}
field_keys = sorted(field_map)


def translate_offset(offset):
    """The swd3e offset of a swd32 one: mapped, or inside a run of mapped offsets moved by the same amount."""
    if offset in field_map:
        return field_map[offset]
    lower = [key for key in field_keys if key < offset]
    upper = [key for key in field_keys if key > offset]
    if lower and upper:
        below, above = lower[-1], upper[0]
        if field_map[below] - below == field_map[above] - above:
            return offset + field_map[below] - below
    return None


def translate_index(offset):
    """An offset as a byte offset, or as a dword or word index of a typed pointer."""
    for scale in (1, 4, 2):
        if offset * scale < 0x100:
            continue
        mapped = translate_offset(offset * scale)
        if mapped is not None and mapped % scale == 0:
            return mapped // scale
    return offset

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
        if not translate and field_map:
            line = re.sub(
                r"(\+ )0x([0-9A-F]+)\b",
                lambda m: m.group(1) + "0x%X" % translate_index(int(m.group(2), 16)),
                line,
            )
            line = line.replace("0x98u", "0xA4u")  # memset of an ActState (0x98 bytes in swd32, 0xA4 in swd3e)
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
