# IDAT batch script: idat -A [-c -dANALYSIS=...] -S"ida_export.py <mode> <outdir>" <input|db>
# mode=initial : clear AF_FLIRT before auto_wait, export, save database
# mode=reopen  : verify reopened database, export, exit without changes
import os
import re
import json
import idc
import idaapi
import idautils
import ida_ida
import ida_auto
import ida_bytes
import ida_funcs
import ida_nalt
import ida_segment
import ida_name
import ida_entry
import ida_pro
import ida_ua
import ida_xref

MODE = idc.ARGV[1] if len(idc.ARGV) > 1 else "reopen"
OUT = idc.ARGV[2] if len(idc.ARGV) > 2 else os.path.dirname(idc.get_idb_path())
os.makedirs(OUT, exist_ok=True)
LOG = open(os.path.join(OUT, "ida_%s_log.txt" % MODE), "w", encoding="utf-8")


def log(msg):
    LOG.write(msg + "\n")
    LOG.flush()
    print(msg)


def fail(msg):
    log("FATAL: " + msg)
    LOG.close()
    ida_pro.qexit(2)


af_before = ida_ida.inf_get_af()
log("af_at_start=0x%08X flirt=%d" % (af_before, 1 if af_before & ida_ida.AF_FLIRT else 0))
if MODE == "initial":
    ida_ida.inf_set_af(af_before & ~ida_ida.AF_FLIRT)
    af_now = ida_ida.inf_get_af()
    log("af_after_clear=0x%08X" % af_now)
    if af_now & ida_ida.AF_FLIRT:
        fail("AF_FLIRT still set before auto_wait")
ida_auto.auto_wait()
af_final = ida_ida.inf_get_af()
log("af_final=0x%08X flirt=%d" % (af_final, 1 if af_final & ida_ida.AF_FLIRT else 0))
if af_final & ida_ida.AF_FLIRT:
    fail("AF_FLIRT set after auto analysis")

# ---- input identity
sha = ida_nalt.retrieve_input_file_sha256()
md5 = ida_nalt.retrieve_input_file_md5()
info = {
    "mode": MODE,
    "idb": idc.get_idb_path(),
    "input_path": ida_nalt.get_input_file_path(),
    "input_sha256": sha.hex() if sha else None,
    "input_md5": md5.hex() if md5 else None,
    "input_size": ida_nalt.retrieve_input_file_size(),
    "imagebase": "0x%08X" % ida_nalt.get_imagebase(),
    "procname": ida_ida.inf_get_procname(),
    "is_64bit": ida_ida.inf_is_64bit(),
    "min_ea": "0x%08X" % ida_ida.inf_get_min_ea(),
    "max_ea": "0x%08X" % ida_ida.inf_get_max_ea(),
    "start_ea": "0x%08X" % ida_ida.inf_get_start_ea(),
    "af": "0x%08X" % af_final,
    "af2": "0x%08X" % ida_ida.inf_get_af2(),
    "func_count": ida_funcs.get_func_qty(),
}

segs = []
for s in idautils.Segments():
    seg = ida_segment.getseg(s)
    segs.append({
        "name": ida_segment.get_segm_name(seg),
        "class": ida_segment.get_segm_class(seg),
        "start": "0x%08X" % seg.start_ea,
        "end": "0x%08X" % seg.end_ea,
        "perm": seg.perm,
        "bitness": seg.bitness,
    })
info["segments"] = segs

entries = []
for i in range(ida_entry.get_entry_qty()):
    ordv = ida_entry.get_entry_ordinal(i)
    ea = ida_entry.get_entry(ordv)
    entries.append({"ordinal": ordv, "ea": "0x%08X" % ea, "name": ida_entry.get_entry_name(ordv)})
info["entries"] = entries

imports = []
for i in range(ida_nalt.get_import_module_qty()):
    mod = ida_nalt.get_import_module_name(i)
    items = []

    def cb(ea, name, ordv, items=items):
        items.append({"ea": "0x%08X" % ea, "name": name, "ordinal": ordv})
        return True

    ida_nalt.enum_import_names(i, cb)
    imports.append({"module": mod, "items": items})
info["imports"] = imports

# ---- functions
bad_name = re.compile(r"^unknown_libname_[0-9]+$")
funcs = []
bad = []
libs = []
for fea in idautils.Functions():
    f = ida_funcs.get_func(fea)
    name = ida_funcs.get_func_name(fea)
    chunks = [(c[0], c[1]) for c in idautils.Chunks(fea)]
    size = sum(e - s for s, e in chunks)
    flags = f.flags
    seg = ida_segment.getseg(fea)
    rec = {
        "ea": "0x%08X" % fea,
        "end": "0x%08X" % f.end_ea,
        "name": name,
        "size": size,
        "chunks": len(chunks),
        "flags": "0x%X" % flags,
        "lib": bool(flags & ida_funcs.FUNC_LIB),
        "thunk": bool(flags & ida_funcs.FUNC_THUNK),
        "noret": bool(flags & ida_funcs.FUNC_NORET),
        "seg": ida_segment.get_segm_name(seg) if seg else "",
        "user_name": ida_name.is_uname(name) if name else False,
        "callers": len(set(idautils.CodeRefsTo(fea, 0))),
        "drefs": len(list(idautils.DataRefsTo(fea))),
    }
    funcs.append(rec)
    if bad_name.match(name or ""):
        bad.append(rec["ea"] + " " + name)
    if rec["lib"]:
        libs.append(rec["ea"] + " " + name)
info["unknown_libname"] = bad
info["func_lib"] = libs

# ---- unowned code heads in executable segments
unowned = []
for s in idautils.Segments():
    seg = ida_segment.getseg(s)
    if not (seg.perm & ida_segment.SEGPERM_EXEC) and ida_segment.get_segm_class(seg) != "CODE":
        continue
    for ea in idautils.Heads(seg.start_ea, seg.end_ea):
        if ida_bytes.is_code(ida_bytes.get_full_flags(ea)) and ida_funcs.get_func(ea) is None:
            unowned.append(ea)
regions = []
for ea in unowned:
    nxt = ea + ida_bytes.get_item_size(ea)
    if regions and regions[-1]["next"] == ea:
        regions[-1]["next"] = nxt
        regions[-1]["heads"] += 1
    else:
        regions.append({"start": ea, "next": nxt, "heads": 1})
reg_out = []
for r in regions:
    refs = set()
    ea = r["start"]
    while ea < r["next"]:
        for x in idautils.XrefsTo(ea, 0):
            if not (r["start"] <= x.frm < r["next"]):
                refs.add((x.frm, ea, x.type))
        ea = idc.next_head(ea, r["next"]) if idc.next_head(ea, r["next"]) != idc.BADADDR else r["next"]
    reg_out.append({
        "start": "0x%08X" % r["start"],
        "end": "0x%08X" % r["next"],
        "heads": r["heads"],
        "first_insn": idc.GetDisasm(r["start"]),
        "ext_refs": ["0x%08X->0x%08X t%d" % t for t in sorted(refs)][:20],
    })
info["unowned_code_heads"] = len(unowned)
info["unowned_code_regions"] = len(reg_out)

# ---- non-code bytes inside code segments (unexplored / data)
gaps = []
for s in idautils.Segments():
    seg = ida_segment.getseg(s)
    if not (seg.perm & ida_segment.SEGPERM_EXEC):
        continue
    ea = seg.start_ea
    cur = None
    while ea < seg.end_ea:
        fl = ida_bytes.get_full_flags(ea)
        sz = max(1, ida_bytes.get_item_size(ea)) if not ida_bytes.is_unknown(fl) else 1
        is_gap = not ida_bytes.is_code(fl) and ida_funcs.get_func(ea) is None
        if is_gap:
            if cur and cur[1] == ea:
                cur[1] = ea + sz
            else:
                cur = [ea, ea + sz]
                gaps.append(cur)
        ea += sz
gap_out = []
for g in gaps:
    data = ida_bytes.get_bytes(g[0], g[1] - g[0]) or b""
    kind = "pad" if data and all(b in (0x00, 0x90, 0xCC) for b in data) else "data_or_unexplored"
    gap_out.append({"start": "0x%08X" % g[0], "end": "0x%08X" % g[1], "size": g[1] - g[0], "kind": kind})
info["code_seg_noncode_ranges"] = len(gap_out)
info["code_seg_noncode_nonpad"] = sum(1 for g in gap_out if g["kind"] != "pad")

# ---- indirect calls/jumps
indirect = []
for fea in idautils.Functions():
    for ea in idautils.FuncItems(fea):
        insn = ida_ua.insn_t()
        if ida_ua.decode_insn(insn, ea) == 0:
            continue
        m = insn.get_canon_mnem()
        if m in ("call", "jmp") and insn.Op1.type in (ida_ua.o_reg, ida_ua.o_phrase, ida_ua.o_displ):
            sw = idaapi.get_switch_info(ea)
            indirect.append({"func": "0x%08X" % fea, "ea": "0x%08X" % ea, "insn": idc.GetDisasm(ea), "switch": sw is not None})
        elif m in ("call", "jmp") and insn.Op1.type == ida_ua.o_mem:
            tgt = insn.Op1.addr
            seg = ida_segment.getseg(tgt)
            if seg and ida_segment.get_segm_name(seg) != ".idata" and not idc.get_name(tgt).startswith("__imp_"):
                sw = idaapi.get_switch_info(ea)
                indirect.append({"func": "0x%08X" % fea, "ea": "0x%08X" % ea, "insn": idc.GetDisasm(ea), "switch": sw is not None})
info["indirect_branches"] = len(indirect)
info["indirect_non_switch"] = sum(1 for i in indirect if not i["switch"])


def dump(name, obj):
    with open(os.path.join(OUT, name), "w", encoding="utf-8") as fp:
        json.dump(obj, fp, ensure_ascii=False, indent=1)


suffix = "" if MODE == "initial" else "_" + MODE
dump("info%s.json" % suffix, info)
dump("functions%s.json" % suffix, funcs)
dump("unowned_regions%s.json" % suffix, reg_out)
dump("code_noncode%s.json" % suffix, gap_out)
dump("indirect%s.json" % suffix, indirect)
log("func_count=%d unknown_libname=%d func_lib=%d unowned_heads=%d regions=%d noncode_nonpad=%d indirect=%d" % (
    info["func_count"], len(bad), len(libs), len(unowned), len(reg_out), info["code_seg_noncode_nonpad"], len(indirect)))
if bad:
    fail("unknown_libname present")

if MODE == "reopen":
    # readability probe: disassemble entry function head
    st = ida_ida.inf_get_start_ea()
    lines = []
    ea = st
    for _ in range(8):
        lines.append("0x%08X  %s" % (ea, idc.GetDisasm(ea)))
        ea = idc.next_head(ea, idc.BADADDR)
    log("entry_probe:\n" + "\n".join(lines))
    LOG.close()
    ida_pro.qexit(0)

LOG.close()
ida_pro.qexit(0)
