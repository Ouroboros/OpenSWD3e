"""Aligns the swd3e work with the code base copied from swd3-rebuild (stage 2 of GOAL version 3).

1. Source comments: "// swd32.exe 0x<b>" becomes "// swd3e.exe 0x<a>" for a matched pair; a swd32 EH funclet becomes
   a note of the swd3e parent; an unmatched swd32 function keeps its address marked "(unmatched in swd3e)".
2. Header member comments "// 0x00<b>" (scripts/list_globals.sh) become the swd3e address from the global map;
   unmapped ones become "// swd32 0x00<b>" so they are no longer listed as swd3e globals.
3. reverse/functions.tsv: a matched function takes the module and source of its swd32 counterpart and records the
   pairing in notes; reusable pairs (and matched CRT functions, mapped to the standard library) become
   implemented_pending_review; unmatched functions take the module of their matched neighbours when both agree.

Usage: python3 -I align_swd32.py <workspace> <swd3-rebuild> [--dry-run]
"""

import csv
import os
import re
import sys

WS = sys.argv[1]
REF = sys.argv[2]
DRY = "--dry-run" in sys.argv
MATCH = os.path.join(WS, "reverse/analysis/export/match_swd32")


def read_tsv(path):
    with open(path, encoding="utf-8", newline="") as fh:
        lines = [line for line in fh if not line.startswith("#")]
    return list(csv.DictReader(lines, delimiter="\t", quoting=csv.QUOTE_NONE))


pairs = read_tsv(os.path.join(MATCH, "functions.tsv"))
b_to_a = {}
a_pair = {}
for row in pairs:
    if row["b_address"]:
        b = int(row["b_address"], 16)
        a = int(row["a_address"], 16)
        b_to_a[b] = a
        a_pair[a] = row

globals_map = {}
for row in read_tsv(os.path.join(MATCH, "globals.tsv")):
    if row["consistent"] == "yes":
        globals_map[int(row["b_address"], 16)] = int(row["a_address"], 16)

funclet_parent = {}
for row in read_tsv(os.path.join(REF, "reverse/analysis/export/swd32_funclets.tsv")):
    funclet_parent[int(row["funclet"], 16)] = int(row["parent"], 16)

ref_rows = {int(r["address"], 16): r for r in read_tsv(os.path.join(REF, "reverse/functions.tsv"))}

stats = {"func_comment_matched": 0, "func_comment_funclet": 0, "func_comment_unmatched": 0,
         "global_mapped": 0, "global_unmapped": 0, "files_changed": 0}
FUNC_RE = re.compile(r"// swd32\.exe 0x([0-9A-Fa-f]{8})")
GLOBAL_RE = re.compile(r"^(    [^/\n]*;\s*)// 0x(00[45][0-9A-F]{5})", re.M)


def remap_function(match):
    b = int(match.group(1), 16)
    if b in b_to_a:
        stats["func_comment_matched"] += 1
        return "// swd3e.exe 0x%08X" % b_to_a[b]
    if b in funclet_parent and funclet_parent[b] in b_to_a:
        stats["func_comment_funclet"] += 1
        return "// swd3e.exe EH funclet (tail) of 0x%08X, swd32 0x%08X" % (b_to_a[funclet_parent[b]], b)
    stats["func_comment_unmatched"] += 1
    return "// swd32.exe 0x%08X (unmatched in swd3e)" % b


def remap_global(match):
    b = int(match.group(2), 16)
    if b in globals_map:
        stats["global_mapped"] += 1
        return "%s// 0x%08X" % (match.group(1), globals_map[b])
    stats["global_unmapped"] += 1
    return "%s// swd32 0x%s" % (match.group(1), match.group(2))


for root in ("src", "tests"):
    for directory, _, files in os.walk(os.path.join(WS, root)):
        for name in files:
            if not name.endswith((".cpp", ".h", ".c")):
                continue
            path = os.path.join(directory, name)
            with open(path, encoding="utf-8", newline="") as fh:
                text = fh.read()
            new = FUNC_RE.sub(remap_function, text)
            if name.endswith(".h"):
                new = GLOBAL_RE.sub(remap_global, new)
            if new != text:
                stats["files_changed"] += 1
                if not DRY:
                    with open(path, "w", encoding="utf-8", newline="") as fh:
                        fh.write(new)

# function table
table_path = os.path.join(WS, "reverse/functions.tsv")
with open(table_path, encoding="utf-8", newline="") as fh:
    header = fh.readline().rstrip("\n").split("\t")
    rows = [dict(zip(header, line.rstrip("\n").split("\t"))) for line in fh if line.strip()]

modules = {}
for row in rows:
    a = int(row["address"], 16)
    pair = a_pair.get(a)
    if pair is None:
        continue
    b = int(pair["b_address"], 16)
    ref = ref_rows.get(b, {})
    modules[a] = ref.get("module", "unknown") or "unknown"

ordered = sorted(int(r["address"], 16) for r in rows)
status_counts = {}
for row in rows:
    a = int(row["address"], 16)
    pair = a_pair.get(a)
    old_notes = row["notes"]
    if pair is not None:
        b = int(pair["b_address"], 16)
        ref = ref_rows.get(b, {})
        row["module"] = modules[a]
        row["source"] = ref.get("source", "")
        reusable = pair["reusable"] == "yes"
        is_crt = row["module"] == "crt"
        tag = "swd32=0x%08X method=%s conf=%s reusable=%s order=%s overlap=%s" % (
            b, pair["method"], pair["confidence"], "yes" if reusable else "no",
            pair.get("order_consistent", ""), pair.get("block_overlap", ""))
        row["notes"] = tag + "; " + old_notes
        if row["status"] == "pending_implementation" and (reusable or is_crt):
            row["status"] = "implemented_pending_review"
    else:
        index = ordered.index(a)
        before = next((modules[x] for x in reversed(ordered[:index]) if x in modules), None)
        after = next((modules[x] for x in ordered[index + 1:] if x in modules), None)
        if before is not None and before == after:
            row["module"] = before
            row["notes"] = "swd32=unmatched module by neighbours; " + old_notes
        else:
            row["module"] = "unknown"
            row["notes"] = "swd32=unmatched neighbours %s|%s; %s" % (before, after, old_notes)
    status_counts[row["status"]] = status_counts.get(row["status"], 0) + 1

if not DRY:
    with open(table_path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\t".join(header) + "\n")
        for row in rows:
            fh.write("\t".join(row[h].replace("\t", " ") for h in header) + "\n")

module_counts = {}
for row in rows:
    module_counts[row["module"]] = module_counts.get(row["module"], 0) + 1
print(stats)
print(status_counts)
print(sorted(module_counts.items(), key=lambda kv: -kv[1]))
