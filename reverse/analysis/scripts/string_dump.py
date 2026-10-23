"""Lists the NUL-terminated strings of swd3e.exe between two addresses (CP950 decoded, control characters escaped).

Usage: python3 -I string_dump.py <exe> <start address> <end address>
"""

import sys

EXE, START, END = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16)
IMAGE_BASE = 0x400000

with open(EXE, "rb") as fh:
    data = fh.read()

at = START - IMAGE_BASE
end = END - IMAGE_BASE
while at < end:
    if data[at] == 0:
        at += 1
        continue
    stop = data.index(b"\0", at)
    raw = data[at:stop]
    try:
        text = raw.decode("cp950")
    except UnicodeDecodeError:
        text = None
    if text is not None and len(raw) >= 2 and all(ch.isprintable() or ch in "\r\n\t" for ch in text):
        print("0x%08X\t%s" % (at + IMAGE_BASE, text.encode("unicode_escape").decode("ascii") if not text.isprintable() else text))
    at = stop + 1
