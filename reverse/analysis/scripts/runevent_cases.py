"""List the cases of a decompiled RunEvent switch with the bytes each advances the script by.

Usage: python3 -I runevent_cases.py <decomp.c> <command_header.h>

For every `case 0xNN:` label of the decompilation, the line it starts at and the distinct `*v7 += N` advances found
up to the next label are printed, next to the length the rebuild's command table (kCommandBytes) gives the code.
"""

import re
import sys


def main():
    decomp_path, header_path = sys.argv[1], sys.argv[2]
    lines = open(decomp_path, encoding="utf-8", errors="replace").read().split("\n")
    header = open(header_path, encoding="utf-8").read()
    names = {}
    for name, value in re.findall(r"\n\s+(\w+) = (0x[0-9A-Fa-f]+|\d+),", header.split("enum class EventCommand")[1]):
        names[name] = int(value, 0)
    lengths = {}
    for name, length in re.findall(r"CommandLength\{EventCommand::(\w+), (\d+)\}", header):
        if name in names:
            lengths[names[name]] = int(length)
    label = re.compile(r"^\s*case (0x[0-9A-F]+|\d+)u?:")
    advance = re.compile(r"\*v7 \+= (0x[0-9A-F]+|\d+);")
    cases = []
    for number, line in enumerate(lines, 1):
        match = label.match(line)
        if match:
            cases.append((int(match.group(1), 0), number))
    cases.sort(key=lambda item: item[1])
    for index, (code, start) in enumerate(cases):
        end = cases[index + 1][1] if index + 1 < len(cases) else len(lines)
        found = sorted({int(value, 0) for value in advance.findall("\n".join(lines[start - 1:end - 1]))})
        known = lengths.get(code)
        print("0x%02X line %5d advances %-20s table %s" % (code, start, found, known))


if __name__ == "__main__":
    main()
