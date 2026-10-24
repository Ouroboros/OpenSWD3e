"""Lists the globals one function's assembly uses and the source members annotated with their addresses.

Usage: python3 reverse/analysis/scripts/asm_globals.py <workspace> <address> [<address>...]

For every global address (dword_/word_/byte_/unk_/off_/stru_/flt_ names and immediate offsets) of the function's
listing in reverse/analysis/asm/, prints the first lines of src/ headers and sources that mention the address
(0x00XXXXXX), so the assembly can be read against the members of the rebuild.
"""

import os
import re
import subprocess
import sys


def mentions(root, address):
    pattern = '0x%08X' % address
    out = subprocess.run(['rg', '-n', '--no-heading', pattern, os.path.join(root, 'src')],
                         capture_output=True, text=True).stdout
    hits = []
    for line in out.split('\n'):
        if not line:
            continue
        path, number, text = line.split(':', 2)
        hits.append('%s:%s %s' % (os.path.relpath(path, root), number, text.strip()[:100]))
    return hits


def main():
    root = sys.argv[1]
    for arg in sys.argv[2:]:
        address = int(arg, 16)
        path = os.path.join(root, 'reverse/analysis/asm', '%08X.asm' % address)
        text = open(path, encoding='utf-8', errors='replace').read()
        names = set(int(m, 16) for m in re.findall(r'\b(?:dword|word|byte|unk|off|stru|flt|qword)_([0-9A-F]{6,8})\b', text))
        names |= set(int(m, 16) for m in re.findall(r'\b([0-9A-F]{6})h\[', text))
        print('== %08X: %d globals' % (address, len(names)))
        for name in sorted(names):
            if not 0x4AF000 <= name < 0x580000:
                continue
            hits = mentions(root, name)
            print('  0x%08X %s' % (name, hits[0] if hits else '-'))
            for hit in hits[1:3]:
                print('             %s' % hit)


main()
