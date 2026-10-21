"""Diffs a switch-heavy swd3e function against its swd32 pair case by case.

Usage: python3 -I case_diff.py <workspace> <swd3-rebuild> <swd3e address> [summary|<case code>...]

Both decompilations are cut at their `case N:` labels (the lines from a label to the next one, a code met twice
joined), normalised as decomp_diff.py does (names that only carry addresses, swd3e globals and callees mapped to their
swd32 pairs, the role strides), and compared code by code. `summary` lists each code as same, differs (with the count
of changed lines), swd3e only or swd32 only; given codes print their diffs. With --loose the indentation, the names of the globals and the way a case
ends (break or goto) are not compared, leaving the differences of logic.
"""

import difflib
import os
import re
import sys

WS, REF, ADDRESS = sys.argv[1], sys.argv[2], "0x%08X" % int(sys.argv[3], 16)
LOOSE = "--loose" in sys.argv
WANTED = [arg for arg in sys.argv[4:] if arg != "--loose"] or ["summary"]

pairs = {}
with open(os.path.join(WS, "reverse/analysis/export/match_swd32/functions.tsv"), encoding="utf-8") as fh:
    header = fh.readline().rstrip("\n").split("\t")
    for line in fh:
        row = dict(zip(header, line.rstrip("\n").split("\t")))
        pairs[row.get("a_address", "")] = row.get("b_address", "")
other = pairs.get(ADDRESS)
if not other:
    raise SystemExit("no swd32 pair for " + ADDRESS)

global_map = {}
with open(os.path.join(WS, "reverse/analysis/export/match_swd32/globals.tsv"), encoding="utf-8") as fh:
    header = fh.readline().rstrip("\n").split("\t")
    for line in fh:
        row = dict(zip(header, line.rstrip("\n").split("\t")))
        if row.get("a_address") and row.get("b_address"):
            global_map[int(row["a_address"], 16)] = int(row["b_address"], 16)
function_map = {int(k, 16): int(v, 16) for k, v in pairs.items() if k and v and v.startswith("0x")}

NAME = re.compile(r"\b(sub|dword|word|byte|unk|loc|off|stru|qword|flt|dbl|asc|jpt|def|LABEL)_([0-9A-F]+)\b")
LABEL = re.compile(r"^\s*case (0x[0-9A-F]+|\d+)u?:")


def normalise(line, translate):
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

    line = re.sub(r"\s*//.*$", "", line)
    line = NAME.sub(name, line)
    if translate:
        for mine_stride, their_stride in (("0xE4", "0xD8"), ("0x39", "0x36"), ("0x72", "0x6C")):
            line = re.sub(r"\b%s \*" % mine_stride, their_stride + " *", line)
    line = re.sub(r"\bv\d+\b", "v", line)
    line = re.sub(r"\ba[A-Z]\w*", "aSTR", line)
    if LOOSE:
        line = re.sub(r"\b(dword|word|byte|unk|qword|flt|dbl|stru|off|asc)_\??[0-9A-F]+", r"\1_G", line)
        line = re.sub(r"\b(break|goto LABEL);", "END;", line)
        types = r"(?:unsigned |signed )?(?:__int8|__int16|__int64|int|char|_BYTE|_WORD|_DWORD|LPVOID|LPCSTR|LPSTR|float|double|void)"
        line = re.sub(r"\*?\(" + types + r"(?: \*+)?\)\s*", "", line)
        line = re.sub(r"\b[ijkmn]\b", "v", line)
        line = line.replace("[0]", "").replace("&", "")
        line = re.sub(r"\s+", " ", line).strip()
    return line.rstrip()


def cases(path, translate):
    with open(path, encoding="utf-8", errors="replace") as fh:
        lines = fh.read().split("\n")
    blocks = {}
    current = None
    for line in lines:
        if re.match(r"^\s+[A-Za-z_][\w ]*[ *]+\w+(\[\w+\])?; // ", line):
            continue
        match = LABEL.match(line)
        if match:
            current = int(match.group(1), 0)
            blocks.setdefault(current, [])
            continue
        if current is not None:
            text = normalise(line, translate)
            if text.strip():
                blocks[current].append(text)
    return blocks


mine = cases(os.path.join(WS, "reverse/analysis/decomp/%s.c" % ADDRESS[2:]), True)
theirs = cases(os.path.join(REF, "reverse/analysis/decomp/%s.c" % other[2:]), False)

if WANTED == ["summary"]:
    for code in sorted(set(mine) | set(theirs)):
        if code not in theirs:
            print("0x%02X swd3e only (%d lines)" % (code, len(mine[code])))
        elif code not in mine:
            print("0x%02X swd32 only" % code)
        else:
            changed = sum(1 for line in difflib.unified_diff(theirs[code], mine[code], n=0, lineterm="")
                          if line[:1] in "+-" and not line.startswith(("+++", "---")))
            print("0x%02X %s" % (code, "same" if changed == 0 else "differs (%d)" % changed))
else:
    for wanted in WANTED:
        code = int(wanted, 0)
        print("=== case 0x%02X" % code)
        for line in difflib.unified_diff(theirs.get(code, []), mine.get(code, []), "swd32", "swd3e", n=2,
                                         lineterm=""):
            print(line)
