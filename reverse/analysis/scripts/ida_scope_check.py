# IDAT batch script: idat -A -S"ida_scope_check.py <outdir>" <db copy>
# Query-only: the address-taken code targets (data dwords and instruction operands that hold a .text address), the
# indirect branch sites, the runtime jump table 0x0050A8B0 and the function starts, for the scope check of
# reverse/analysis/scope-and-gaps.md. Run it on a copy of the database in the workspace tmp/.
import json
import os

import ida_auto
import ida_bytes
import ida_funcs
import ida_pro
import ida_segment
import ida_ua
import idautils
import idc

OUT = idc.ARGV[1]
ida_auto.auto_wait()
text = ida_segment.get_segm_by_name(".text")
TS, TE = text.start_ea, text.end_ea


def classify(target):
    f = ida_funcs.get_func(target)
    flags = ida_bytes.get_full_flags(target)

    if f is not None and f.start_ea == target:
        return "func_start", "0x%08X" % f.start_ea

    if f is not None:
        return ("in_func_head" if ida_bytes.is_head(flags) and ida_bytes.is_code(flags) else "in_func_inner",
                "0x%08X" % f.start_ea)

    return ("no_func_code" if ida_bytes.is_code(flags) else "no_func_data"), None


targets = {}


def note(target, source, kind):
    entry = targets.setdefault(target, {"sources": [], "kinds": set()})

    if len(entry["sources"]) < 8:
        entry["sources"].append("0x%08X" % source)

    entry["kinds"].add(kind)


# Data dwords (every non-.text segment, and the data items inside .text) holding a .text address.
for seg_ea in idautils.Segments():
    seg = ida_segment.getseg(seg_ea)
    name = ida_segment.get_segm_name(seg)
    ea = seg.start_ea

    while ea + 4 <= seg.end_ea:
        flags = ida_bytes.get_full_flags(ea)

        if seg.start_ea == TS and ida_bytes.is_code(flags):
            ea = ida_bytes.next_not_tail(ea) if ida_bytes.is_head(flags) else ea + 1
            continue

        value = ida_bytes.get_dword(ea)

        if TS <= value < TE:
            note(value, ea, "data:" + name)

        ea += 1 if seg.start_ea == TS else 4

# Instruction operands holding a .text address other than the target of a direct near call or jump.
indirect = []

for fea in idautils.Functions():
    for ea in idautils.FuncItems(fea):
        insn = ida_ua.insn_t()

        if ida_ua.decode_insn(insn, ea) == 0:
            continue

        mnem = insn.get_canon_mnem()
        is_branch = mnem in ("call", "jmp") or mnem.startswith("j") or mnem.startswith("loop")

        for op in insn.ops:
            if op.type == ida_ua.o_void:
                break

            if is_branch and op.type in (ida_ua.o_near, ida_ua.o_far):
                continue

            for value in (op.value, op.addr):
                if TS <= value < TE:
                    note(value, ea, "operand:" + mnem)

        if mnem in ("call", "jmp") and insn.ops[0].type in (ida_ua.o_reg, ida_ua.o_phrase, ida_ua.o_displ,
                                                            ida_ua.o_mem):
            indirect.append({"site": "0x%08X" % ea, "func": "0x%08X" % fea, "mnem": mnem,
                             "text": idc.generate_disasm_line(ea, 0)})

result = {"functions": [], "targets": [], "indirect": indirect, "table_50A8B0": []}

for fea in idautils.Functions():
    f = ida_funcs.get_func(fea)
    result["functions"].append({"start": "0x%08X" % fea, "end": "0x%08X" % f.end_ea,
                                "name": ida_funcs.get_func_name(fea)})

for target, entry in sorted(targets.items()):
    kind, owner = classify(target)
    result["targets"].append({"target": "0x%08X" % target, "class": kind, "owner": owner,
                              "kinds": sorted(entry["kinds"]), "sources": entry["sources"]})

for index in range(64):
    value = ida_bytes.get_dword(0x50A8B0 + 4 * index)
    result["table_50A8B0"].append("0x%08X" % value)

result["table_50A8B0_xrefs"] = ["0x%08X" % x.frm for x in idautils.XrefsTo(0x50A8B0)]

with open(os.path.join(OUT, "scope_check.json"), "w", encoding="utf-8") as fp:
    json.dump(result, fp, ensure_ascii=False, indent=1)

ida_pro.qexit(0)
