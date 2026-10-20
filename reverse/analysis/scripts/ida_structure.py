# IDAT batch script: idat -A -S"ida_structure.py <outdir>" <db>
# Query-only: per-function callees, import use, string refs, data refs, vtable/table membership
import os
import json
import idc
import idautils
import ida_auto
import ida_bytes
import ida_funcs
import ida_nalt
import ida_segment
import ida_pro
import ida_xref

OUT = idc.ARGV[1]
ida_auto.auto_wait()
text = ida_segment.get_segm_by_name(".text")
TS, TE = text.start_ea, text.end_ea

imp = {}
for i in range(ida_nalt.get_import_module_qty()):
    mod = ida_nalt.get_import_module_name(i)

    def cb(ea, name, ordv, mod=mod):
        imp[ea] = "%s!%s" % (mod, name)
        return True

    ida_nalt.enum_import_names(i, cb)

strings = {}
for s in idautils.Strings():
    try:
        strings[s.ea] = str(s)
    except Exception:
        pass

table_refs = {}
for seg_ea in idautils.Segments():
    seg = ida_segment.getseg(seg_ea)
    if seg.start_ea == TS:
        continue
    buf = ida_bytes.get_bytes(seg.start_ea, seg.end_ea - seg.start_ea) or b""
    for off in range(0, len(buf) - 3, 4):
        v = int.from_bytes(buf[off:off + 4], "little")
        f = ida_funcs.get_func(v)
        if f and f.start_ea == v:
            table_refs.setdefault(v, []).append(seg.start_ea + off)

funcs = []
for fea in idautils.Functions():
    callees = set()
    imports = set()
    strs = []
    globs = set()
    for ea in idautils.FuncItems(fea):
        for x in idautils.XrefsFrom(ea, 0):
            t = x.to
            if x.type in (ida_xref.fl_CN, ida_xref.fl_CF, ida_xref.fl_JN, ida_xref.fl_JF):
                g = ida_funcs.get_func(t)
                if g and g.start_ea == t and t != fea:
                    callees.add(t)
            if t in imp:
                imports.add(imp[t])
            elif t in strings:
                strs.append(strings[t][:80])
            elif x.type in (ida_xref.dr_R, ida_xref.dr_W, ida_xref.dr_O) and not (TS <= t < TE):
                globs.add(t)
            elif x.type == ida_xref.dr_O and (TS <= t < TE):
                g = ida_funcs.get_func(t)
                if g and g.start_ea == t:
                    callees.add(t)
    funcs.append({
        "ea": fea,
        "name": ida_funcs.get_func_name(fea),
        "size": sum(e - s for s, e in idautils.Chunks(fea)),
        "callees": sorted(callees),
        "callers": sorted(set(ida_funcs.get_func(x.frm).start_ea for x in idautils.XrefsTo(fea, 0)
                              if ida_funcs.get_func(x.frm) and idc.get_segm_name(x.frm) == ".text")),
        "imports": sorted(imports),
        "strings": strs[:20],
        "globals": sorted(globs)[:200],
        "tables": table_refs.get(fea, [])[:10],
    })

with open(os.path.join(OUT, "structure.json"), "w", encoding="utf-8") as fp:
    json.dump(funcs, fp, ensure_ascii=False)
ida_pro.qexit(0)
