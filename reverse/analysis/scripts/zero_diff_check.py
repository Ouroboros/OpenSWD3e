"""For the pending functions whose normalised instructions equal their swd32 pair (diff_swd32 changed = 0), compares
the callees position by position and the globals: a callee pair agrees when it is a matched pair or both are CRT
routines of the same name. Prints each function as ok (all agree) or with the disagreeing positions.

Usage: python3 -I zero_diff_check.py <workspace> <swd3-rebuild>
"""

import csv
import json
import os
import re
import sys

WS, REF = sys.argv[1], sys.argv[2]


def tsv(path):
    with open(path, encoding="utf-8", newline="") as fh:
        return list(csv.DictReader((l for l in fh if not l.startswith("#")), delimiter="\t", quoting=csv.QUOTE_NONE))


def crt_name(source):
    m = re.match(r"CRT:([^ ]+(?: [a-z]+)?)", source or "")
    if not m:
        return None
    return m.group(1).replace("_dbg", "").replace("_base", "")


ours = {r["address"]: r for r in tsv(os.path.join(WS, "reverse/functions.tsv"))}
theirs = {r["address"]: r for r in tsv(os.path.join(REF, "reverse/functions.tsv"))}
pairs = {r["a_address"]: r["b_address"] for r in tsv(os.path.join(WS, "reverse/analysis/export/match_swd32/functions.tsv"))
         if r["b_address"]}
globals_ok = {(r["a_address"], r["b_address"]) for r in tsv(os.path.join(WS, "reverse/analysis/export/match_swd32/globals.tsv"))
              if r["consistent"] == "yes"}
diff = {r["a_address"]: int(r["changed"]) for r in tsv(os.path.join(WS, "reverse/analysis/diff_swd32/summary.tsv"))}


def load(path):
    out = {}
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            rec = json.loads(line)
            out[rec["address"]] = rec
    return out


fp_a = load(os.path.join(WS, "reverse/analysis/export/swd3e_fingerprints.jsonl"))
fp_b = load(os.path.join(REF, "reverse/analysis/export/swd32_fingerprints.jsonl"))
ok = []
for a, row in sorted(ours.items()):
    if row["status"] != "pending_implementation" or diff.get(a) != 0 or a not in pairs:
        continue
    b = pairs[a]
    ca, cb = fp_a[a]["callees"], fp_b[b]["callees"]
    problems = []
    if len(ca) != len(cb):
        problems.append("callee count %d/%d" % (len(ca), len(cb)))
    for x, y in zip(ca, cb):
        if "!" in x or "!" in y:
            if x.split("!")[-1] != y.split("!")[-1]:
                problems.append("import %s/%s" % (x, y))
            continue
        if pairs.get(x) == y:
            continue
        nx = crt_name(ours.get(x, {}).get("source"))
        ny = crt_name(theirs.get(y, {}).get("source"))
        if nx is not None and nx == ny:
            continue
        problems.append("callee %s(%s)/%s(%s)" % (x, nx or ours.get(x, {}).get("source", "?")[:40], y,
                                                 ny or theirs.get(y, {}).get("source", "?")[:40]))
    ga, gb = fp_a[a]["globals"], fp_b[b]["globals"]
    if len(ga) != len(gb):
        problems.append("globals %d/%d" % (len(ga), len(gb)))
    for x, y in zip(ga, gb):
        if (x, y) not in globals_ok:
            problems.append("global %s/%s" % (x, y))
    if fp_a[a]["strings"] != fp_b[b]["strings"]:
        problems.append("strings differ")
    if fp_a[a].get("constants") != fp_b[b].get("constants"):
        problems.append("constants differ")
    if problems:
        print("CHECK %s %s <- %s: %s" % (a, row["source"], b, "; ".join(problems[:4])))
    else:
        ok.append(a)
        print("OK %s %s <- %s" % (a, row["source"], b))
print("ok", len(ok))
