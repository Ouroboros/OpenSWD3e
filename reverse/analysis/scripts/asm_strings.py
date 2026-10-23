"""Lists the strings the given swd3e functions refer to: every operand of their listings resolving to an address
("; -> ADDR") that starts a NUL-terminated CP950 string in swd3e.exe, with the IDA name and the decoded text
(control characters escaped).

Usage: python3 -I asm_strings.py <workspace> <address> [...]
"""

import os
import re
import sys

WS = sys.argv[1]
IMAGE_BASE = 0x400000
with open(os.path.join(WS, "reverse/analysis/work/swd3e.exe"), "rb") as fh:
    data = fh.read()

REF = re.compile(r"(?:offset\s+(\w+).*?)?;\s*->\s*([0-9A-F]{8})")

for argument in sys.argv[2:]:
    address = int(argument, 16)
    path = os.path.join(WS, "reverse/analysis/asm/%08X.asm" % address)
    seen = []
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            for match in REF.finditer(line):
                target = int(match.group(2), 16)
                if target in (s[0] for s in seen) or not (0x4A0000 <= target < 0x580000):
                    continue
                at = target - IMAGE_BASE
                if at >= len(data):
                    continue
                stop = data.find(b"\0", at)
                raw = data[at:stop] if stop >= 0 else b""
                try:
                    text = raw.decode("cp950")
                except UnicodeDecodeError:
                    continue
                if len(raw) < 2 or not all(ch.isprintable() or ch in "\r\n\t" for ch in text):
                    continue
                seen.append((target, match.group(1) or "", text.encode("unicode_escape").decode("ascii")
                             if not text.isprintable() else text))
    print("== 0x%08X" % address)
    for target, name, text in seen:
        print("0x%08X\t%s\t%s" % (target, name, text))
