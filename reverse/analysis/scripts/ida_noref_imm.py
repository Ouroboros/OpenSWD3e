# Query-only: operands whose imm/addr value lies in image range but have no XrefsFrom to that value
import json
import idc
import idautils
import ida_auto
import ida_bytes
import ida_funcs
import ida_ua
import ida_pro
import ida_segment

out = idc.ARGV[1]
ida_auto.auto_wait()
LO, HI = 0x401000, 0x57C000
stats = {"insns": 0, "addr_operands": 0, "noref": 0, "by_kind": {}, "branch_multi_ref": 0}
samples = []
for fea in idautils.Functions():
    for ea in idautils.FuncItems(fea):
        insn = ida_ua.insn_t()
        if ida_ua.decode_insn(insn, ea) == 0:
            continue
        stats["insns"] += 1
        refs = set(x.to for x in idautils.XrefsFrom(ea, 0))
        if len(refs) > 2 and insn.get_canon_mnem() == "jmp":
            stats["branch_multi_ref"] += 1
        for op in insn.ops:
            if op.type == ida_ua.o_void:
                break
            vals = []
            if op.type == ida_ua.o_imm:
                vals.append(("imm", op.value & 0xFFFFFFFF))
            if op.type in (ida_ua.o_mem, ida_ua.o_displ, ida_ua.o_phrase):
                vals.append(("addr", op.addr & 0xFFFFFFFF))
            for kind, v in vals:
                if not (LO <= v < HI):
                    continue
                stats["addr_operands"] += 1
                if v not in refs:
                    seg = idc.get_segm_name(v)
                    key = "%s:%s" % (kind, seg)
                    stats["noref"] += 1
                    stats["by_kind"][key] = stats["by_kind"].get(key, 0) + 1
                    if len(samples) < 40:
                        samples.append("0x%08X %s  [%s]" % (ea, idc.GetDisasm(ea), key))
stats["samples"] = samples
with open(out, "w", encoding="utf-8") as fp:
    json.dump(stats, fp, indent=1)
ida_pro.qexit(0)
