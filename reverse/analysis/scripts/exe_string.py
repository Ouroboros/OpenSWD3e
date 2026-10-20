"""Prints the NUL-terminated strings of swd3e.exe at the given virtual addresses, decoded from CP950 (Big5), with
control characters escaped.

Usage: python3 -I exe_string.py <exe> <hex address>...
"""

import sys

IMAGE_BASE = 0x400000
data = open(sys.argv[1], "rb").read()
for text in sys.argv[2:]:
    address = int(text, 16)
    at = address - IMAGE_BASE
    end = data.index(b"\0", at)
    raw = data[at:end]
    try:
        decoded = raw.decode("cp950")
    except UnicodeDecodeError:
        decoded = repr(raw)
    print("0x%08X %s" % (address, decoded.encode("unicode_escape").decode("ascii") if any(ord(c) < 0x20 for c in decoded)
                          else decoded))
