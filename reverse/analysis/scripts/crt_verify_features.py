"""Features of the unverified CRT, platform-import and Miles-thunk rows for the pass-1 identity check (a variant of
crt_features.py: every row of module crt, platform or audio_mss not yet assembly_exact, ten leading instructions,
strings with their line breaks escaped).

Usage: python3 -I crt_verify_features.py <workspace>
"""
import csv
import json
import os
import sys

WS = sys.argv[1]
CRT_START = 0x4A1096
with open(os.path.join(WS, "reverse/functions.tsv"), encoding="utf-8", newline="") as fh:
    rows = list(csv.DictReader(fh, delimiter="\t", quoting=csv.QUOTE_NONE))
names = {}
for r in rows:
    src = r["source"]
    if src:
        names[int(r["address"], 16)] = src.split(" -> ")[0].replace("CRT:", "")
struct = {f["ea"]: f for f in json.load(open(os.path.join(WS, "reverse/analysis/export/structure.json"), encoding="utf-8"))}
for r in rows:
    if r["module"] not in ("crt", "platform", "audio_mss") or r["status"] == "assembly_exact":
        continue
    a = int(r["address"], 16)
    s = struct.get(a, {})
    callees = [names.get(c, "%X" % c) for c in s.get("callees", [])]
    game_callers = sum(1 for c in s.get("callers", []) if c < CRT_START)
    crt_callers = sum(1 for c in s.get("callers", []) if c >= CRT_START)
    first = []
    path = os.path.join(WS, "reverse/analysis/asm", "%08X.asm" % a)
    if os.path.exists(path):
        with open(path, encoding="utf-8") as fh:
            for line in fh:
                if line[:1].isdigit() or line[:1] in "ABCDEF":
                    first.append(line[48:].strip().split("    ;")[0])
                if len(first) >= 10:
                    break
    print("%08X size=%d game=%d crt=%d imp=%s str=%s calls=%s | %s" % (
        a, s.get("size", 0), game_callers, crt_callers,
        ",".join(i.split("!")[1] for i in s.get("imports", [])), "|".join(s.get("strings", [])).replace("\n", "\\n").replace("\r", "\\r")[:60],
        ",".join(callees)[:80], " ; ".join(first)[:150]))
