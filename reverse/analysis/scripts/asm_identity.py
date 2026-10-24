"""Checks that swd3e functions paired by hash are the same code as their swd32 functions.

Usage: python3 -I asm_identity.py <swd3e workspace> <swd32 workspace> <swd3e address>...

Each address is looked up in reverse/functions.tsv (its notes carry swd32=<address>). Both instruction listings
(reverse/analysis/asm/<address>.asm) are reduced to their mnemonics and operands, code labels and branch targets
becoming offsets from the function start. The instructions must then be equal except for the addresses of data and
callees, which must correspond one to one: an swd3e address always stands for the same swd32 address and the other
way round, across all the functions checked (the mapping is learnt as the listings are compared and printed at the
end). Prints one line per function: same, or the first instruction pair that differs or breaks the mapping.
"""

import re
import sys

root3e, root32 = sys.argv[1], sys.argv[2]
wanted = ['%08X' % int(a, 16) for a in sys.argv[3:]]
TOKEN = re.compile(r'\b(dword_|word_|byte_|unk_|off_|flt_|dbl_|sub_|loc_|locret_|jpt_|def_|stru_|asc_|a)([0-9A-F]{6,8})\b')

pairs = {}
for line in open(root3e + '/reverse/functions.tsv', encoding='utf-8'):
    cells = line.rstrip('\n').split('\t')
    if len(cells) < 10:
        continue
    found = re.search(r'swd32=0x([0-9A-Fa-f]+)', cells[9])
    if found:
        pairs[cells[2][2:].upper()] = found.group(1).upper().zfill(8)


def instructions(path):
    rows = []
    for line in open(path, encoding='utf-8', errors='replace'):
        if line.startswith(';') or not re.match(r'^[0-9A-F]{8} ', line):
            continue
        text = line[41:].split(';', 1)[0].strip()
        text = re.sub(r'^sp=-?\d+\s+', '', text)
        rows.append((int(line[:8], 16), re.sub(r'\s+', ' ', text)))
    return rows


def split(rows):
    start = rows[0][0]
    end = rows[-1][0] + 16
    out = []
    for _, text in rows:
        values = []

        def name(match):
            value = int(match.group(2), 16)
            if start <= value < end:
                return 'L%X' % (value - start)
            values.append(value)
            return 'G'

        out.append((TOKEN.sub(name, text), values))
    return out


forward = {}
backward = {}

for address in wanted:
    old = pairs.get(address)
    if old is None:
        print('%s: no swd32 pair' % address)
        continue
    a = split(instructions('%s/reverse/analysis/asm/%s.asm' % (root3e, address)))
    b = split(instructions('%s/reverse/analysis/asm/%s.asm' % (root32, old)))
    problem = None
    if len(a) != len(b):
        problem = 'lengths %d / %d' % (len(a), len(b))
    for index, ((text3e, values3e), (text32, values32)) in enumerate(zip(a, b)):
        if problem:
            break
        if text3e != text32 or len(values3e) != len(values32):
            problem = 'differs at %d: [%s] / [%s]' % (index, text3e, text32)
            break
        for new, previous in zip(values3e, values32):
            if forward.setdefault(new, previous) != previous or backward.setdefault(previous, new) != new:
                problem = 'mapping broken at %d: %08X -> %08X (had %08X / %08X)' % (
                    index, new, previous, forward[new], backward[previous])
                break
    print('%s vs swd32 %s: %s' % (address, old, problem or 'same (%d instructions)' % len(a)))

print('mapping (%d addresses):' % len(forward))
for new in sorted(forward):
    print('  %08X -> %08X' % (new, forward[new]))
