# IDAT batch script: idat -A -S"ida_fix_bounds.py <outdir>" <db>
# Boundary completion for unowned code heads; every action is logged with its evidence.
import os
import json
import idc
import idautils
import ida_auto
import ida_bytes
import ida_funcs
import ida_segment
import ida_pro
import ida_ua
import ida_xref

OUT = idc.ARGV[1]
ida_auto.auto_wait()
text = ida_segment.get_segm_by_name(".text")
TS, TE = text.start_ea, text.end_ea
LOG = []
EH_MAGIC = 0x19930520

PTRS = {}
for _seg_ea in idautils.Segments():
    _seg = ida_segment.getseg(_seg_ea)
    if _seg.start_ea == TS:
        continue
    _buf = ida_bytes.get_bytes(_seg.start_ea, _seg.end_ea - _seg.start_ea) or b""
    for _off in range(0, len(_buf) - 3, 4):
        _v = int.from_bytes(_buf[_off:_off + 4], "little")
        if TS <= _v < TE:
            PTRS.setdefault(_v, []).append(_seg.start_ea + _off)


def log(action, ea, **kw):
    rec = {"action": action, "ea": "0x%08X" % ea}
    rec.update(kw)
    LOG.append(rec)


def owner(ea):
    f = ida_funcs.get_func(ea)
    return f.start_ea if f else None


def unowned_heads():
    return [ea for ea in idautils.Heads(TS, TE)
            if ida_bytes.is_code(ida_bytes.get_full_flags(ea)) and ida_funcs.get_func(ea) is None]


def regions():
    regs = []
    for ea in unowned_heads():
        nxt = ea + ida_bytes.get_item_size(ea)
        if regs and regs[-1][1] == ea:
            regs[-1][1] = nxt
        else:
            regs.append([ea, nxt])
    return regs


def mnem(ea):
    return idc.print_insn_mnem(ea)


def is_flow_break(ea):
    """previous item does not fall through into the next address"""
    fl = ida_bytes.get_full_flags(ea)
    if not ida_bytes.is_code(fl):
        return True
    m = mnem(ea)
    return m in ("retn", "ret", "jmp", "int3") or (m == "int" and idc.get_operand_value(ea, 0) == 3)


# ---- step 1: misdisassembled EH magic string "VC20XC00"
for s, e in regions():
    data = ida_bytes.get_bytes(s, 8)
    if data == b"VC20XC00":
        ida_bytes.del_items(s, ida_bytes.DELIT_SIMPLE, 8)
        ok = ida_bytes.create_strlit(s, 8, idc.STRTYPE_C)
        log("code_to_string", s, size=8, ok=bool(ok), evidence="bytes VC20XC00 (MSVC EH magic), no xrefs, not reachable by flow")

# ---- step 2: C++ EH: __ehhandler stubs, unwind and catch funclets -> tails of parent
cxx_handlers = set()
stubs = []
for s, e in regions():
    for h in idautils.Heads(s, e):
        if mnem(h) == "mov" and idc.get_operand_type(h, 0) == idc.o_reg and idc.print_operand(h, 0) == "eax" \
                and idc.get_operand_type(h, 1) == idc.o_imm:
            nx = idc.next_head(h, e)
            if nx != idc.BADADDR and mnem(nx) == "jmp" and idc.get_operand_type(nx, 0) == idc.o_near:
                fi = idc.get_operand_value(h, 1)
                if ida_bytes.get_dword(fi) == EH_MAGIC:
                    stubs.append((h, nx + ida_bytes.get_item_size(nx), fi, idc.get_operand_value(nx, 0)))
                    cxx_handlers.add(idc.get_operand_value(nx, 0))

for hnd in sorted(cxx_handlers):
    if ida_funcs.get_func(hnd) is None:
        ok = ida_funcs.add_func(hnd)
        f = ida_funcs.get_func(hnd)
        log("create_func", hnd, ok=bool(ok), end="0x%08X" % (f.end_ea if f else 0),
            evidence="common jmp target of __ehhandler stubs (mov eax, offset FuncInfo; jmp) = __CxxFrameHandler")

starts = set()
eh_items = []
for stub, stub_end, fi, hnd in stubs:
    parents = set()
    for x in idautils.XrefsTo(stub, 0):
        o = owner(x.frm)
        if o is not None:
            parents.add(o)
    max_state = ida_bytes.get_dword(fi + 4)
    p_unwind = ida_bytes.get_dword(fi + 8)
    n_try = ida_bytes.get_dword(fi + 12)
    p_try = ida_bytes.get_dword(fi + 16)
    targets = []
    for i in range(max_state):
        act = ida_bytes.get_dword(p_unwind + 8 * i + 4)
        if act:
            targets.append(("unwind", act))
    for i in range(n_try):
        ent = p_try + 20 * i
        n_catch = ida_bytes.get_dword(ent + 12)
        p_hand = ida_bytes.get_dword(ent + 16)
        for j in range(n_catch):
            targets.append(("catch", ida_bytes.get_dword(p_hand + 16 * j + 12)))
    eh_items.append((stub, stub_end, fi, parents, targets))
    starts.add(stub)
    for _, t in targets:
        starts.add(t)

reg_ends = set(e for _, e in regions())
bounds = sorted(starts | reg_ends)


def piece_end(ea):
    for b in bounds:
        if b > ea:
            return b
    return ea + ida_bytes.get_item_size(ea)


for stub, stub_end, fi, parents, targets in eh_items:
    if len(parents) != 1:
        log("eh_skip", stub, reason="parent count %d" % len(parents), funcinfo="0x%08X" % fi)
        continue
    parent = parents.pop()
    pf = ida_funcs.get_func(parent)
    pieces = [("ehhandler", stub, stub_end)] + [(k, t, piece_end(t)) for k, t in targets]
    for kind, s, e in pieces:
        if ida_funcs.get_func(s) is not None:
            log("eh_already_owned", s, kind=kind, owner="0x%08X" % owner(s), parent="0x%08X" % parent)
            continue
        ok = ida_funcs.append_func_tail(pf, s, e)
        log("append_tail", s, end="0x%08X" % e, parent="0x%08X" % parent, kind=kind, ok=bool(ok),
            evidence="FuncInfo 0x%08X referenced by parent via __ehhandler stub 0x%08X" % (fi, stub))

# ---- step 3: extend parent with contiguous trailing pieces referenced only by it
for s, e in regions():
    prev = idc.prev_head(s, TS)
    pf = ida_funcs.get_func(prev)
    if pf is None or pf.end_ea != s:
        continue
    ext = set()
    data_srcs = []
    for h in idautils.Heads(s, e):
        for x in idautils.XrefsTo(h, 0):
            if s <= x.frm < e:
                continue
            if idc.get_segm_name(x.frm) == ".text":
                ext.add(owner(x.frm))
            else:
                data_srcs.append(x.frm)
    ptr_srcs = PTRS.get(s, [])
    pm = mnem(prev)
    in_scope_table = all(idc.get_segm_name(p) == ".rdata" for p in ptr_srcs) and ptr_srcs
    only_parent = ext == {pf.start_ea}
    no_refs = not ext and not ptr_srcs and not data_srcs
    prev_indirect = pm == "jmp" and idc.get_operand_type(prev, 0) in (idc.o_reg, idc.o_phrase, idc.o_displ)
    prev_trap = pm in ("int3",) or (pm == "int" and idc.get_operand_value(prev, 0) == 3)
    reason = None
    if only_parent and not ptr_srcs:
        reason = "external refs only from parent 0x%08X" % pf.start_ea
    elif no_refs and (prev_indirect or prev_trap):
        reason = "no refs; previous insn '%s' in parent is non-returning/indirect" % idc.GetDisasm(prev)
    elif in_scope_table and not ext and mnem(s) == "mov" and idc.print_operand(s, 0) == "esp":
        reason = "SEH __except block (mov esp,[ebp-18h]) referenced from scope table %s" % ",".join("0x%08X" % p for p in ptr_srcs)
    if reason:
        ok = ida_funcs.set_func_end(pf.start_ea, e)
        log("extend_func", s, end="0x%08X" % e, parent="0x%08X" % pf.start_ea, ok=bool(ok), evidence=reason)

# ---- step 4: create functions at evidenced entries, retry with dependency ordering
def data_ptr_srcs(ea):
    return PTRS.get(ea, [])


def entry_evidence(ea, region_start, region_end):
    ev = []
    ptrs = data_ptr_srcs(ea)
    if ptrs:
        ev.append("data ptr " + ",".join("0x%08X" % p for p in ptrs[:3]))
    for x in idautils.XrefsTo(ea, 0):
        if region_start <= x.frm < region_end:
            continue
        src_is_code = ida_bytes.is_code(ida_bytes.get_full_flags(x.frm))
        if x.type in (ida_xref.fl_CN, ida_xref.fl_CF):
            ev.append("call from 0x%08X" % x.frm)
        elif x.type in (ida_xref.fl_JN, ida_xref.fl_JF) and src_is_code and mnem(x.frm) == "jmp" \
                and idc.get_operand_type(x.frm, 0) == idc.o_near and is_flow_break(idc.prev_head(ea, TS)):
            ev.append("jmp from 0x%08X (target after flow break)" % x.frm)
        elif x.type == ida_xref.dr_O and src_is_code and idc.get_segm_name(x.frm) == ".text":
            ev.append("offset from insn 0x%08X" % x.frm)
    if not ev and ea == region_start and is_flow_break(idc.prev_head(ea, TS)):
        prev = idc.prev_head(ea, TS)
        if idc.is_align(ida_bytes.get_full_flags(prev)) or mnem(prev) in ("retn", "ret"):
            ev.append("no xref; starts after '%s' (orphan)" % idc.GetDisasm(prev))
    return ev


for rnd in range(20):
    progress = False
    cands = []
    for s, e in regions():
        for h in idautils.Heads(s, e):
            ev = entry_evidence(h, s, e)
            if ev:
                cands.append((h, ev))
    cand_set = set(h for h, _ in cands)
    for h, ev in cands:
        if ida_funcs.get_func(h) is not None:
            continue
        if not ida_funcs.add_func(h):
            log("create_fail", h, round=rnd, evidence=ev)
            continue
        f = ida_funcs.get_func(h)
        swallowed = [c for c in cand_set if c != h and ida_funcs.get_func(c) is not None
                     and ida_funcs.get_func(c).start_ea == h]
        if f.start_ea != h or swallowed:
            ida_funcs.del_func(h)
            log("create_postponed", h, round=rnd, swallowed=["0x%08X" % c for c in swallowed])
            continue
        last = idc.prev_head(f.end_ea, h)
        log("create_func", h, end="0x%08X" % f.end_ea, round=rnd, evidence=ev,
            last_insn=idc.GetDisasm(last), chunks=len(list(idautils.Chunks(h))))
        progress = True
    if not progress:
        # allow creation even if it swallows a candidate that is only a jmp target inside the same region
        break

ida_auto.auto_wait()
remaining = []
for s, e in regions():
    remaining.append({"start": "0x%08X" % s, "end": "0x%08X" % e,
                      "first": idc.GetDisasm(s), "prev": idc.GetDisasm(idc.prev_head(s, TS))})
with open(os.path.join(OUT, "bounds_fix_log.json"), "w", encoding="utf-8") as fp:
    json.dump({"actions": LOG, "remaining_regions": remaining}, fp, ensure_ascii=False, indent=1)
ida_pro.qexit(0)
