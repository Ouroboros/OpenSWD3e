"""Lists the calls of the error box (0x0040C530) in a function's assembly with their caption and text strings.

Usage: python3 -I error_calls.py <asm listing> <swd3e.exe>

For each `call sub_40C530` the two pushes nearest before it (text, then caption) are read: an `offset` operand gives
the string at that address (Big5 decoded); a register push is shown as is.
"""

import re
import struct
import sys

asm_path, exe_path = sys.argv[1], sys.argv[2]
image = open(exe_path, "rb").read()
pe = struct.unpack_from("<I", image, 0x3C)[0]
sections = struct.unpack_from("<H", image, pe + 6)[0]
optional = struct.unpack_from("<H", image, pe + 20)[0]
base = struct.unpack_from("<I", image, pe + 24 + 28)[0]
table = pe + 24 + optional
spans = []
for index in range(sections):
    at = table + 40 * index
    virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from("<IIII", image, at + 8)
    spans.append((base + virtual_address, max(virtual_size, raw_size), raw_pointer))


def text_at(address):
    for start, size, raw in spans:
        if start <= address < start + size:
            offset = raw + address - start
            end = image.index(b"\0", offset)
            return image[offset:end].decode("cp950", errors="replace")
    return "?"


lines = open(asm_path, encoding="utf-8", errors="replace").read().split("\n")
push = re.compile(r"\bpush\s+(.*?)(?:\s*;.*)?$")
for number, line in enumerate(lines):
    if "call    sub_40C530" not in line:
        continue
    operands = []
    back = number - 1
    while back >= 0 and len(operands) < 3:
        match = push.search(lines[back])
        if match:
            operands.append((back, lines[back]))
        back -= 1
    shown = []
    for at, text in operands[:3]:
        address = re.search(r"-> ([0-9A-F]{8})", text)
        operand = push.search(text).group(1).strip()
        if address and "offset" in operand:
            shown.append("%s=%r" % (address.group(1), text_at(int(address.group(1), 16))))
        else:
            shown.append(operand)
    location = line.split()[0]
    print("%s %s" % (location, " | ".join(reversed(shown))))
