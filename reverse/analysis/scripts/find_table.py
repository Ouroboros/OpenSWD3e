"""Finds where swd3e.exe keeps a table of swd32.exe: the bytes at a swd32 address searched in swd3e's image.

Usage: python3 -I find_table.py <swd32.exe> <swd3e.exe> <swd32 address>:<bytes> ...

Prints each swd32 address with the swd3e virtual addresses holding the same bytes (both images based at 0x00400000
with file offset = address - 0x00400000 for the sections searched).
"""

import sys

BASE = 0x400000
old = open(sys.argv[1], "rb").read()
new = open(sys.argv[2], "rb").read()

for spec in sys.argv[3:]:
    address, length = spec.split(":")
    address, length = int(address, 16), int(length, 0)
    needle = old[address - BASE:address - BASE + length]
    found = []
    at = new.find(needle)

    while at >= 0 and len(found) < 8:
        found.append("0x%08X" % (at + BASE))
        at = new.find(needle, at + 1)

    print("0x%08X (%d bytes): %s" % (address, length, " ".join(found) if found else "none"))
