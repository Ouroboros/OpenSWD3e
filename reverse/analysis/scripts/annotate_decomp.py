"""Prints the decompilation of a swd3e function with the globals the C++ sources already name replaced by
`Class::member` (taken from the `// 0x<address>` comments after member declarations in src/**/*.h) and the callees the
function table maps replaced by their source symbol. Declarations are dropped. Globals inside a named member's span are
shown as `member+offset` when the member's size is not known only for the exact address.

With --unit the battle unit offsets of `this` (`*(_TYPE *)(this + 0xN)`, `*(this + N)` scaled by the pointee of
`this`) are shown as the BattleUnit, BattleRole and BattleEnemy members the headers place there (`{member}`,
`{member[i].field}` inside an ActState member, `{member+0xD}` elsewhere inside one).

Usage: python3 -I annotate_decomp.py <workspace> [--unit] <swd3e address> [...]
"""

import glob
import os
import re
import sys

WS = sys.argv[1]
UNIT = "--unit" in sys.argv
ADDRESSES = [a for a in sys.argv[2:] if a != "--unit"]
names = {}
DECL = re.compile(r"^\s*(?:static\s+)?(?:inline\s+)?(?:const\s+)?[\w:<>,\s\*&]+?\b(\w+)(?:\s*=[^;]*|\s*\{[^;]*\})?;\s*//\s*(0x[0-9A-Fa-f]{8})\b")
for path in glob.glob(os.path.join(WS, "src/**/*.h"), recursive=True):
    cls = None
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            m = re.match(r"^(?:class|struct)\s+(\w+)", line)
            if m:
                cls = m.group(1)
            m = DECL.match(line)
            if m:
                address = int(m.group(2), 16)
                if address >= 0x004A0000:
                    names.setdefault(address, "%s::%s" % (cls or "?", m.group(1)))

callees = {}
with open(os.path.join(WS, "reverse/functions.tsv"), encoding="utf-8") as fh:
    fh.readline()
    for line in fh:
        p = line.rstrip("\n").split("\t")
        if len(p) > 5 and p[5]:
            callees[int(p[2], 16)] = p[5].split(":")[-1]

DECLARATION = re.compile(r"^  [a-zA-Z_][a-zA-Z_0-9 *]*(\[[0-9A-Fx]*\])?; //")

# The unit members by offset (with their ActState arrays) and the ActState fields.
ACT_STATE = 0xA4
act_fields = {}
with open(os.path.join(WS, "src/res/act_state.h"), encoding="utf-8") as fh:
    for line in fh:
        m = re.search(r"\b(\w+)(?:\[\w+\])?(?:\s*=[^;]*)?;\s*//\s*\+0x([0-9A-F]+)\b", line)
        if m:
            act_fields.setdefault(int(m.group(2), 16), m.group(1))
unit_members = []
for header in ("src/battle/battle_unit.h", "src/battle/battle_role.h", "src/battle/battle_enemy.h"):
    with open(os.path.join(WS, header), encoding="utf-8") as fh:
        for line in fh:
            m = re.search(r"^\s+(.+?)\s+(\w+)(?:\s*=[^;]*|\s*\{[^;]*\})?;\s*//\s*\+0x([0-9A-F]+)\b", line)
            if m:
                type_text = m.group(1)
                count = 1
                c = re.search(r"std::array<ActState,\s*(\w+)>", type_text)
                if c:
                    count = int(c.group(1), 0) if c.group(1)[0].isdigit() else {"SlotActs": 11, "ExtraActs": 5}.get(c.group(1), 1)
                acts = "ActState" in type_text
                unit_members.append((int(m.group(3), 16), m.group(2), acts, count))
unit_members.sort()


def unit_name(offset):
    best = None
    for start, name, acts, count in unit_members:
        if start <= offset:
            best = (start, name, acts, count)
        else:
            break
    if best is None:
        return None
    start, name, acts, count = best
    delta = offset - start
    if acts and delta < ACT_STATE * count:
        index, inner = divmod(delta, ACT_STATE)
        field = act_fields.get(inner, "+0x%X" % inner)
        return "%s%s.%s" % (name, "[%d]" % index if count > 1 else "", field)
    if delta == 0:
        return name
    if delta < 0x40:
        return "%s+0x%X" % (name, delta)
    return None


SCALE = {"int": 1, "_DWORD": 4, "unsigned int": 4, "_WORD": 2, "__int16": 2, "unsigned __int16": 2,
         "_BYTE": 1, "char": 1, "unsigned __int8": 1, "_DWORD *": 4}


def name_global(match):
    address = int(match.group(2), 16)
    if address in names:
        return "{%s}" % names[address]
    return match.group(0)


def name_call(match):
    address = int(match.group(1), 16)
    if address in callees:
        return "%s<%X>" % (callees[address], address)
    return match.group(0)


for argument in ADDRESSES:
    address = int(argument, 16)
    path = os.path.join(WS, "reverse/analysis/decomp/%08X.c" % address)
    print("== 0x%08X" % address)
    with open(path, encoding="utf-8", errors="replace") as fh:
        lines = fh.readlines()
    pointee = 1
    signature = re.search(r"__thiscall \w+\(([\w ]+?) ?(\*?)this", lines[0]) if lines else None
    if signature:
        pointee = SCALE.get(signature.group(1).strip(), 1) if signature.group(2) else 1
    for line in lines:
            if DECLARATION.match(line):
                continue
            if UNIT:
                def typed(match):
                    name = unit_name(int(match.group(2), 0))
                    return "{%s}" % name if name else match.group(0)

                def scaled(match):
                    name = unit_name(int(match.group(1), 0) * pointee)
                    return "{%s}" % name if name else match.group(0)

                line = re.sub(r"\*\(\(?(?:const )?(?:_BYTE|_WORD|_DWORD|char|__int16|int|unsigned __int8|"
                              r"unsigned __int16|unsigned int|float|LPVOID|void \*|_DWORD \*|_WORD \*) \*\)\("
                              r"(?:this|a1) \+ ((0x[0-9A-F]+|\d+))\)", lambda m: typed(m), line)
                def address_of(match):
                    name = unit_name(int(match.group(1), 0))
                    return "&{%s}" % name if name else match.group(0)

                line = re.sub(r"\(\w[\w ]*\*\)\((?:this|a1) \+ (0x[0-9A-F]+|\d+)\)", address_of, line)

                def cast_scaled(match):
                    size = SCALE.get(match.group(2).strip(), 1) if match.group(2) else 1
                    name = unit_name(int(match.group(3), 0) * size)
                    if not name:
                        return match.group(0)
                    return "{%s}" % name if match.group(1) else "&{%s}" % name

                line = re.sub(
                    r"(\*)?\(\((?:const )?(_BYTE|_WORD|_DWORD|char|__int16|int|unsigned __int8|unsigned __int16|"
                    r"unsigned int|LPSTR|CHAR) \*?\)(?:this|a1) \+ (0x[0-9A-F]+|\d+)\)",
                    cast_scaled,
                    line,
                )
                line = re.sub(
                    r"\((?:LPSTR|LPCSTR|char \*|CHAR \*)\)(?:this|a1) \+ (0x[0-9A-F]+|\d+)\b",
                    lambda m: "&{%s}" % (unit_name(int(m.group(1), 0)) or "+0x%X" % int(m.group(1), 0)),
                    line,
                )
                line = re.sub(r"\*\((?:this|a1) \+ (0x[0-9A-F]+|\d+)\)", scaled, line)

                def scaled_address(match):
                    name = unit_name(int(match.group(1), 0) * pointee)
                    return "&{%s}" % name if name else match.group(0)

                line = re.sub(r"\b(?:this|a1) \+ (0x[0-9A-F]+|\d+)\b", scaled_address, line)
            line = re.sub(r"\b(dword|word|byte|unk|qword|off|stru)_\??([0-9A-F]{6,8})\b", name_global, line)
            line = re.sub(r"\bsub_([0-9A-F]{6,8})\b", name_call, line)
            sys.stdout.write(line)
