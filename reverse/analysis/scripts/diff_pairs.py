"""Per matched pair, aligns the swd3e and swd32 instructions (fingerprint token files) and lists what changed:
operands of instructions with the same shape (structure offsets, constants, addresses) and inserted or deleted
instruction runs. Writes reverse/analysis/diff_swd32/<swd3e address>.txt and summary.tsv.

Token lines (FINGERPRINT_TOKENS of fingerprints.py): masked bytes, shape, original text, tab separated.

Usage: python3 -I diff_pairs.py <match functions.tsv> <swd3e tokens> <swd32 tokens> <out dir>
"""

import csv
import difflib
import os
import re
import sys

match_path, tok_a, tok_b, out_dir = sys.argv[1:5]
os.makedirs(out_dir, exist_ok=True)


def load(directory, address):
    path = os.path.join(directory, "%08X.txt" % address)
    if not os.path.exists(path):
        return None
    rows = []
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            parts = line.rstrip("\n").split("\t")
            if len(parts) >= 3:
                rows.append((parts[0], parts[1], parts[2]))
    return rows


NUM = re.compile(r"\b[0-9A-F]+h\b|\b\d+\b")


def operand_kind(text_a, text_b):
    if "[" in text_a and "[" in text_b:
        return "offset"
    if re.search(r"\b(dword|word|byte)_[0-9A-F]+|\bunk_|\boff_|\bstru_", text_a + text_b):
        return "global"
    if re.search(r"\bsub_|\bloc_|\bj_", text_a + text_b):
        return "code"
    return "constant"


with open(match_path, encoding="utf-8", newline="") as fh:
    pairs = [r for r in csv.DictReader(fh, delimiter="\t") if r["b_address"]]

summary = []
for pair in pairs:
    a = int(pair["a_address"], 16)
    b = int(pair["b_address"], 16)
    rows_a = load(tok_a, a)
    rows_b = load(tok_b, b)
    if rows_a is None or rows_b is None:
        continue
    matcher = difflib.SequenceMatcher(None, [r[1] for r in rows_b], [r[1] for r in rows_a], autojunk=False)
    lines = []
    counts = {"same": 0, "offset": 0, "constant": 0, "global": 0, "code": 0, "inserted": 0, "deleted": 0}
    for op, b0, b1, a0, a1 in matcher.get_opcodes():
        if op == "equal":
            for k in range(b1 - b0):
                rb, ra = rows_b[b0 + k], rows_a[a0 + k]
                if rb[0] == ra[0]:
                    counts["same"] += 1
                    continue
                kind = operand_kind(ra[2], rb[2])
                counts[kind] += 1
                lines.append("~ %-8s swd32: %-48s swd3e: %s" % (kind, rb[2], ra[2]))
        else:
            if b1 > b0:
                counts["deleted"] += b1 - b0
                for r in rows_b[b0:b1]:
                    lines.append("- swd32 only: %s" % r[2])
            if a1 > a0:
                counts["inserted"] += a1 - a0
                for r in rows_a[a0:a1]:
                    lines.append("+ swd3e only: %s" % r[2])
    changed = sum(v for k, v in counts.items() if k != "same")
    with open(os.path.join(out_dir, "%08X.txt" % a), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("swd3e 0x%08X %s <- swd32 0x%08X %s; method=%s reusable=%s\n" % (
            a, pair["a_name"], b, pair["b_name"], pair["method"], pair["reusable"]))
        fh.write(" ".join("%s=%d" % kv for kv in counts.items()) + "\n")
        fh.write("\n".join(lines) + ("\n" if lines else ""))
    summary.append(["0x%08X" % a, "0x%08X" % b, pair["method"], pair["reusable"], str(len(rows_a)), str(len(rows_b)),
                    str(changed)] + [str(counts[k]) for k in ("offset", "constant", "global", "code", "inserted", "deleted")])

with open(os.path.join(out_dir, "summary.tsv"), "w", encoding="utf-8", newline="\n") as fh:
    fh.write("a_address\tb_address\tmethod\treusable\ta_insns\tb_insns\tchanged\toffset\tconstant\tglobal\tcode\t"
             "inserted\tdeleted\n")
    for row in summary:
        fh.write("\t".join(row) + "\n")
print("pairs", len(summary), "identical", sum(1 for r in summary if r[6] == "0"))
