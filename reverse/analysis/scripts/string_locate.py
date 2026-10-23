"""Finds in swd3e.exe the strings the sources still give swd32 addresses for: every u8"..." / "..." literal on a line
whose comment names a swd32 address ("swd32.exe 0x..." or "swd32 0x...") is encoded in CP950 and searched for as a
NUL-terminated string in the file. Prints one line per literal: the source place, the swd3e addresses found (none: the
text differs in swd3e) and the literal. With "apply" a literal found exactly once gets its comment address replaced by
the swd3e one.

Usage: python3 -I string_locate.py <workspace> <exe> [apply] <source>...
"""

import re
import sys

WS, EXE = sys.argv[1], sys.argv[2]
APPLY = len(sys.argv) > 3 and sys.argv[3] == "apply"
SOURCES = sys.argv[4:] if APPLY else sys.argv[3:]
IMAGE_BASE = 0x400000

with open(EXE, "rb") as fh:
    data = fh.read()

LINE = re.compile(r'(?:u8)?"((?:[^"\\]|\\.)*)"[^/]*//\s*(swd32(?:\.exe)? (0x[0-9A-Fa-f]{8})(?: \(unmatched in swd3e\))?)')


def unescape(text):
    return text.encode("utf-8").decode("unicode_escape").encode("latin-1").decode("utf-8")


def find(raw):
    hits = []
    needle = raw + b"\0"
    start = 0
    while True:
        at = data.find(needle, start)
        if at < 0:
            break
        if at == 0 or data[at - 1] == 0:
            hits.append(at + IMAGE_BASE)  # the sections are file-aligned like their virtual addresses here
        start = at + 1
    return hits


for path in SOURCES:
    with open(path, encoding="utf-8") as fh:
        lines = fh.read().split("\n")
    changed = False
    for number, line in enumerate(lines):
        match = LINE.search(line)
        if not match:
            continue
        try:
            text = unescape(match.group(1))
            raw = text.encode("cp950")
        except (UnicodeError, ValueError):
            print("%s:%d\tUNENCODABLE\t%s" % (path, number + 1, match.group(1)))
            continue
        hits = find(raw)
        print("%s:%d\t%s\t%s" % (path, number + 1, ",".join("0x%08X" % h for h in hits) or "NONE", text))
        if APPLY and len(hits) == 1:
            lines[number] = line.replace(match.group(2), "0x%08X" % hits[0])
            changed = True
    if changed:
        with open(path, "w", encoding="utf-8") as fh:
            fh.write("\n".join(lines))
