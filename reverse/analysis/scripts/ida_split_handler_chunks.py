# IDAT batch script: idat -A -S"ida_split_handler_chunks.py <log.json> <out dir> <address>..." <db>
# Each address is a handler entry stored by code as an offset (the menu's handler tables 0x00440CF0 and 0x0044DC60)
# that auto-analysis attached as a tail chunk to the functions jumping into it. The chunk is removed from every
# parent with idc.remove_fchunk (ida_funcs.remove_func_tail refuses these tails) and a function is made at the
# address; the jumps of the parents become tail calls. The new functions and the parents are listed into
# <out dir>/asm/<address>.asm and decompiled into <out dir>/decomp/<address>.c in the format of export_funcs.py.
import json
import os

import ida_auto
import ida_bytes
import ida_funcs
import ida_hexrays
import ida_ida
import ida_lines
import ida_name
import ida_pro
import ida_xref
import idautils
import idc

AF_FLIRT = 0x00800000


def hx(ea):
    return "%08X" % ea


def chunks(ea):
    return [[hx(s), hx(e)] for (s, e) in idautils.Chunks(ea)]


def asm_listing(fea):
    lines = []
    fn = ida_funcs.get_func(fea)
    lines.append("; function %s %s flags=0x%X frsize=%d frregs=%d argsize=%d" % (
        hx(fea), ida_funcs.get_func_name(fea), fn.flags, fn.frsize, fn.frregs, fn.argsize))
    for (cs, ce) in idautils.Chunks(fea):
        lines.append("; chunk %s-%s" % (hx(cs), hx(ce)))
        ea = cs
        while ea < ce and ea != idc.BADADDR:
            flags = ida_bytes.get_full_flags(ea)
            size = ida_bytes.get_item_size(ea)
            name = ida_name.get_name(ea)
            if name and ea != fea and (ida_name.is_public_name(ea) or flags & ida_bytes.FF_NAME or flags & ida_bytes.FF_LABL):
                lines.append("%s:" % name)
            raw = ida_bytes.get_bytes(ea, min(size, 16)) or b""
            text = idc.generate_disasm_line(ea, ida_lines.GENDSM_REMOVE_TAGS)
            spd = idc.get_spd(ea)
            refs = []
            for xr in idautils.XrefsFrom(ea, 0):
                if xr.type == ida_xref.fl_F:
                    continue
                refs.append("%s" % hx(xr.to))
            lines.append("%s %-32s sp=%-5s %s%s" % (
                hx(ea), raw.hex(), "" if spd is None else str(spd), text,
                ("    ; -> " + ",".join(refs)) if refs else ""))
            ea += max(1, size)
    return "\n".join(lines) + "\n"


def main():
    log_path = idc.ARGV[1]
    out_dir = idc.ARGV[2]
    entries = [int(a, 16) for a in idc.ARGV[3:]]
    ida_ida.inf_set_af(ida_ida.inf_get_af() & ~AF_FLIRT)
    ida_auto.auto_wait()
    log = {"af": "0x%08X" % ida_ida.inf_get_af(), "entries": []}
    touched = set()

    for ea in entries:
        rec = {"entry": hx(ea)}
        chunk = ida_funcs.get_fchunk(ea)
        rec["refs"] = [hx(x.frm) for x in idautils.XrefsTo(ea, 0)]

        if chunk is None:
            rec["error"] = "not in a function"
            log["entries"].append(rec)
            continue

        rec["chunk"] = [hx(chunk.start_ea), hx(chunk.end_ea)]
        rec["tail"] = bool(chunk.flags & ida_funcs.FUNC_TAIL)

        if not rec["tail"] or chunk.start_ea != ea:
            rec["error"] = "not the start of a tail chunk"
            log["entries"].append(rec)
            continue

        parents = []
        it = ida_funcs.func_parent_iterator_t(chunk)
        ok = it.first()

        while ok:
            parents.append(it.parent())
            ok = it.next()

        # A live parent iterator keeps its chunk locked, and IDA then refuses to remove that chunk.
        del it
        del chunk
        rec["parents"] = [hx(p) for p in parents]
        rec["parent_chunks_before"] = {hx(p): chunks(p) for p in parents}
        touched.update(parents)
        log["entries"].append(rec)

    # A removal can fail after another entry of the same region became a function; failed entries are tried again
    # after the analysis settles.
    for attempt in range(3):
        for rec in log["entries"]:
            if "error" in rec:
                continue

            ea = int(rec["entry"], 16)
            fn = ida_funcs.get_func(ea)

            if fn is not None and fn.start_ea == ea:
                continue

            # The tail's owner goes last: removing it first fails while other functions still share the tail.
            owner = hx(ida_funcs.get_fchunk(ea).owner)
            order = [p for p in rec["parents"] if p != owner] + [p for p in rec["parents"] if p == owner]
            removed = {}

            for p in order:
                chunk = ida_funcs.get_fchunk(ea)
                still = chunk is not None and any(
                    s == ea for (s, _) in idautils.Chunks(int(p, 16))
                )
                removed[p] = bool(idc.remove_fchunk(int(p, 16), ea)) if still else True

            step = {"removed": removed, "added": bool(ida_funcs.add_func(ea))}

            # On a retry a parent that still refuses is deleted and made again after the entry (its jump to the
            # entry then becomes a tail call, its other tails come back).
            if not step["added"] and attempt > 0:
                refusing = [p for p, ok in removed.items() if not ok]
                step["deleted"] = {p: bool(ida_funcs.del_func(int(p, 16))) for p in refusing}
                step["added_after_delete"] = bool(ida_funcs.add_func(ea))
                step["readded"] = {p: bool(ida_funcs.add_func(int(p, 16))) for p in refusing}

            rec.setdefault("attempts", []).append(step)
            touched.add(ea)

        ida_auto.auto_wait()

    for rec in log["entries"]:
        ea = int(rec["entry"], 16)
        fn = ida_funcs.get_func(ea)
        rec["start_after"] = hx(fn.start_ea) if fn else None
        rec["chunks_after"] = chunks(ea) if fn and fn.start_ea == ea else None
        rec["parent_chunks_after"] = {p: chunks(int(p, 16)) for p in rec.get("parents", [])}

    asm_dir = os.path.join(out_dir, "asm")
    dec_dir = os.path.join(out_dir, "decomp")
    os.makedirs(asm_dir, exist_ok=True)
    os.makedirs(dec_dir, exist_ok=True)
    have_hr = ida_hexrays.init_hexrays_plugin()

    for fea in sorted(touched):
        fn = ida_funcs.get_func(fea)

        if fn is None or fn.start_ea != fea:
            continue

        with open(os.path.join(asm_dir, hx(fea) + ".asm"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(asm_listing(fea))

        if have_hr:
            try:
                cf = ida_hexrays.decompile(fea)
                text = str(cf) if cf else "// decompile returned None\n"
            except Exception as exc:
                text = "// decompile failed: %s\n" % exc
            with open(os.path.join(dec_dir, hx(fea) + ".c"), "w", encoding="utf-8", newline="\n") as fh:
                fh.write(text)

    log["functions"] = sum(1 for _ in idautils.Functions())

    with open(log_path, "w", encoding="utf-8") as fp:
        json.dump(log, fp, indent=1)


try:
    main()
    ida_pro.qexit(0)
except Exception:
    import traceback
    with open(idc.ARGV[1], "w", encoding="utf-8") as fh:
        fh.write(traceback.format_exc())
    ida_pro.qexit(3)
