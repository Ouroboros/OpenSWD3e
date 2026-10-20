# IDAT batch script: idat -A -S"ida_gap_probe.py <outdir>" <db>
# Query-only: details for unowned code regions, code pointers in data, non-code ranges in .text
import os
import json
import idc
import idautils
import ida_auto
import ida_bytes
import ida_funcs
import ida_segment
import ida_pro
import ida_xref

OUT = idc.ARGV[1]
ida_auto.auto_wait()
text = ida_segment.get_segm_by_name(".text")
TS, TE = text.start_ea, text.end_ea


def fname(ea):
    f = ida_funcs.get_func(ea)
    return ("0x%08X" % f.start_ea) if f else None


def unowned_regions():
    regs = []
    for ea in idautils.Heads(TS, TE):
        if ida_bytes.is_code(ida_bytes.get_full_flags(ea)) and ida_funcs.get_func(ea) is None:
            nxt = ea + ida_bytes.get_item_size(ea)
            if regs and regs[-1][1] == ea:
                regs[-1][1] = nxt
            else:
                regs.append([ea, nxt])
    return regs


# pointers into .text stored in data segments
ptrs = {}
for s in idautils.Segments():
    seg = ida_segment.getseg(s)
    if seg.start_ea == TS:
        continue
    ea = seg.start_ea
    while ea + 4 <= seg.end_ea:
        v = ida_bytes.get_dword(ea)
        if TS <= v < TE:
            ptrs.setdefault(v, []).append(ea)
        ea += 4

out = {"regions": [], "noncode": [], "data_ptrs_not_func_start": []}
for s, e in unowned_regions():
    prev = idc.prev_head(s, TS)
    heads = list(idautils.Heads(s, e))
    refs = []
    for h in heads:
        for x in idautils.XrefsTo(h, 0):
            if s <= x.frm < e:
                continue
            refs.append({"from": "0x%08X" % x.frm, "to": "0x%08X" % h, "type": x.type,
                         "from_func": fname(x.frm), "from_seg": idc.get_segm_name(x.frm)})
    last = idc.prev_head(e, s)
    out["regions"].append({
        "start": "0x%08X" % s, "end": "0x%08X" % e, "heads": len(heads),
        "prev": "0x%08X %s" % (prev, idc.GetDisasm(prev)), "prev_func": fname(prev),
        "next_func": fname(e), "last": idc.GetDisasm(last),
        "data_ptrs": ["0x%08X" % p for p in ptrs.get(s, [])],
        "refs": refs[:30],
        "disasm": ["0x%08X %s" % (h, idc.GetDisasm(h)) for h in heads[:12]],
    })

# non-code ranges in .text that are not padding
ea = TS
cur = None
rngs = []
while ea < TE:
    fl = ida_bytes.get_full_flags(ea)
    sz = 1 if ida_bytes.is_unknown(fl) else max(1, ida_bytes.get_item_size(ea))
    if not ida_bytes.is_code(fl) and ida_funcs.get_func(ea) is None:
        if cur and cur[1] == ea:
            cur[1] = ea + sz
        else:
            cur = [ea, ea + sz]
            rngs.append(cur)
    ea += sz
for s, e in rngs:
    data = ida_bytes.get_bytes(s, e - s) or b""
    if data and all(b in (0, 0x90, 0xCC) for b in data):
        continue
    refs = []
    for h in range(s, min(e, s + 0x400)):
        for x in idautils.XrefsTo(h, 0):
            refs.append("0x%08X->0x%08X t%d %s" % (x.frm, h, x.type, fname(x.frm)))
    out["noncode"].append({"start": "0x%08X" % s, "end": "0x%08X" % e, "size": e - s,
                           "prev_func": fname(idc.prev_head(s, TS)), "name": idc.get_name(s),
                           "bytes": data[:32].hex(), "refs": refs[:10],
                           "data_ptrs": ["0x%08X" % p for p in ptrs.get(s, [])][:5]})

# data pointers to .text addresses that are code heads without function start
for v, srcs in sorted(ptrs.items()):
    f = ida_funcs.get_func(v)
    if f and f.start_ea == v:
        continue
    fl = ida_bytes.get_full_flags(v)
    out["data_ptrs_not_func_start"].append({
        "target": "0x%08X" % v, "is_code": ida_bytes.is_code(fl), "is_head": ida_bytes.is_head(fl),
        "in_func": fname(v), "srcs": ["0x%08X" % p for p in srcs[:5]], "nsrc": len(srcs)})

with open(os.path.join(OUT, "gap_probe.json"), "w", encoding="utf-8") as fp:
    json.dump(out, fp, ensure_ascii=False, indent=1)
ida_pro.qexit(0)
