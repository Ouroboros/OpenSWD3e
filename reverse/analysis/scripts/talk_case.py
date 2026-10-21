"""Shows a RunEvent command of swd3e next to the rebuild's code for it.

Usage: python3 -I talk_case.py <workspace> <code>...

For each command code: the lines of the swd3e decompilation (reverse/analysis/decomp/0042BAF0.c) under its `case`
labels (local declarations left out), then the rebuild's `case EventCommand::<name>:` blocks of src/talk/*.cpp up to
the next case label at the same indentation.
"""

import os
import re
import sys

WS = sys.argv[1]
CODES = [int(code, 0) for code in sys.argv[2:]]

header = open(os.path.join(WS, "src/talk/event_command.h"), encoding="utf-8").read()
enum = header.split("enum class EventCommand")[1].split("};")[0]
names = {}
for name, value in re.findall(r"\n\s+(\w+) = (0x[0-9A-Fa-f]+|\d+),", enum):
    names.setdefault(int(value, 0), name)

decomp = open(os.path.join(WS, "reverse/analysis/decomp/0042BAF0.c"), encoding="utf-8", errors="replace").read()
decomp_lines = [line for line in decomp.split("\n") if not re.match(r"^\s+[A-Za-z_][\w ]*[ *]+\w+(\[\w+\])?; // ", line)]
label = re.compile(r"^\s*case (0x[0-9A-F]+|\d+)u?:")

sources = []
talk_dir = os.path.join(WS, "src/talk")
for name in sorted(os.listdir(talk_dir)):
    if name.endswith(".cpp"):
        sources.append((name, open(os.path.join(talk_dir, name), encoding="utf-8").read().split("\n")))


def decomp_block(code):
    out = []
    taking = False
    for number, line in enumerate(decomp_lines):
        match = label.match(line)
        if match:
            taking = int(match.group(1), 0) == code
            if taking:
                out.append("--- line %d" % number)
            continue
        if taking:
            out.append(line.rstrip())
    return out


def source_blocks(name):
    out = []
    pattern = re.compile(r"^(\s*)case EventCommand::%s:" % re.escape(name))
    for file_name, lines in sources:
        for index, line in enumerate(lines):
            match = pattern.match(line)
            if not match:
                continue
            indent = match.group(1)
            out.append("--- %s:%d" % (file_name, index + 1))
            end = index + 1
            while end < len(lines):
                text = lines[end]
                if re.match(r"^%s(case |default:)" % re.escape(indent), text) or re.match(r"^%s?}" % indent[:-4], text) and text.strip() == "}" and len(text) - len(text.lstrip()) < len(indent):
                    break
                end += 1
            out.extend(lines[index:end])
    return out


for code in CODES:
    name = names.get(code, "?")
    print("=" * 20, "0x%02X %s" % (code, name))
    print("\n".join(decomp_block(code)))
    print("-" * 20, "rebuild")
    print("\n".join(source_blocks(name)) if name != "?" else "(no name)")
