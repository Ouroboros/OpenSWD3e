"""Checks that swd3e functions paired by hash are the same code as their swd32 functions.

Usage: python3 -I asm_identity.py <swd3e workspace> <swd32 workspace> <swd3e address>...

Each address is looked up in reverse/functions.tsv (its notes carry swd32=<address>). Both instruction listings
(reverse/analysis/asm/<address>.asm) are reduced to their mnemonics and operands, code labels and branch targets
becoming offsets from the function start. The instructions must then be equal except for the addresses of data and
callees, which must correspond one to one: an swd3e address always stands for the same swd32 address and the other
way round, across all the functions checked (the mapping is learnt as the listings are compared and printed at the
end; named symbols take part in the mapping too). An EH handler chunk (with an SEH_ label) is left out of both
listings and counted. Prints one line per function: same, or the first instruction pair that differs or breaks the
mapping.
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


KEYWORDS = {
    'byte', 'word', 'dword', 'qword', 'tbyte', 'ptr', 'offset', 'short', 'large', 'near', 'far', 'st',
    'eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp', 'esp', 'ax', 'bx', 'cx', 'dx', 'si', 'di', 'bp', 'sp',
    'al', 'ah', 'bl', 'bh', 'cl', 'ch', 'dl', 'dh', 'cs', 'ds', 'es', 'fs', 'gs', 'ss',
}
NAME = re.compile(r'(?<![\w@?$])([A-Za-z_?$][\w@?$]*)')
# A named stack variable ([esp+20h+Buffer.dwLength]); arg_ and var_ names stay as they are.
LOCAL = re.compile(r'\[(e[sb]p)((?:\+[0-9A-F]+h)?)\+(?!arg_|var_)[A-Za-z_][\w.]*\]')


def instructions(path):
    """The instructions of the function's chunks; a chunk holding an SEH_ label (the EH handler funclet the compiler
    places after the code) is left out and counted."""
    chunks = [[]]
    seh = [False]
    for line in open(path, encoding='utf-8', errors='replace'):
        if line.startswith('; chunk') and chunks[-1]:
            chunks.append([])
            seh.append(False)
            continue
        if line.startswith('SEH_'):
            seh[-1] = True
            continue
        if line.startswith(';') or not re.match(r'^[0-9A-F]{8} ', line):
            continue
        text = line[41:].split(';', 1)[0].strip()
        text = re.sub(r'^sp=-?\d+\s+', '', text)
        chunks[-1].append((int(line[:8], 16), re.sub(r'\s+', ' ', text)))
    rows = [row for chunk, handler in zip(chunks, seh) if not handler for row in chunk]
    return rows, sum(seh)


def split(rows):
    start = rows[0][0]
    end = rows[-1][0] + 16
    out = []
    for _, text in rows:
        mnemonic, _, operands = text.partition(' ')
        operands = LOCAL.sub(r'[\1\2+V]', operands)
        values = []

        def symbol(match):
            word = match.group(1)
            found = TOKEN.fullmatch(word)
            if found:
                value = int(found.group(2), 16)
                if start <= value < end:
                    return 'L%X' % (value - start)
                values.append(value)
                return 'G'
            if word.lower() in KEYWORDS or re.match(r'^(arg|var)_[0-9A-F]+$|^st\d$', word):
                return word
            values.append(word)
            return 'G'

        out.append((mnemonic + ' ' + NAME.sub(symbol, operands), values))
    return out


forward = {}
backward = {}

def shown(value):
    return '%08X' % value if isinstance(value, int) else value


for address in wanted:
    old = pairs.get(address)
    if old is None:
        print('%s: no swd32 pair' % address)
        continue
    rows3e, handlers3e = instructions('%s/reverse/analysis/asm/%s.asm' % (root3e, address))
    rows32, handlers32 = instructions('%s/reverse/analysis/asm/%s.asm' % (root32, old))
    a = split(rows3e)
    b = split(rows32)
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
                problem = 'mapping broken at %d: %s -> %s (had %s / %s)' % (
                    index, shown(new), shown(previous), shown(forward[new]), shown(backward[previous]))
                break
    note = ''
    if handlers3e or handlers32:
        note = ', EH handler chunks left out %d / %d' % (handlers3e, handlers32)
    print('%s vs swd32 %s: %s%s' % (address, old, problem or 'same (%d instructions)' % len(a), note))

print('mapping (%d symbols):' % len(forward))
for new in sorted(forward, key=lambda value: (isinstance(value, str), shown(value))):
    print('  %s -> %s' % (shown(new), shown(forward[new])))
