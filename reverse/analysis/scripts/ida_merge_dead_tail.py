# IDAT batch script: idat -A -S"ida_merge_dead_tail.py <logfile> <parent> <orphan> ..." <db>
# Merge an unreferenced function that directly follows its parent's last instruction back into the parent.
import json
import idc
import idautils
import ida_auto
import ida_funcs
import ida_pro

out_path = idc.ARGV[1]
pairs = idc.ARGV[2:]
ida_auto.auto_wait()
log = []
for i in range(0, len(pairs), 2):
    parent = int(pairs[i], 16)
    orphan = int(pairs[i + 1], 16)
    pf = ida_funcs.get_func(parent)
    of = ida_funcs.get_func(orphan)
    rec = {"parent": "0x%08X" % parent, "orphan": "0x%08X" % orphan}
    refs = [x.frm for x in idautils.XrefsTo(orphan, 0)]
    rec["refs"] = ["0x%08X" % r for r in refs]
    ok = (pf is not None and of is not None and pf.start_ea == parent and of.start_ea == orphan
          and pf.end_ea == orphan and not refs)
    rec["precheck"] = ok
    if ok:
        end = of.end_ea
        rec["del_func"] = bool(ida_funcs.del_func(orphan))
        rec["set_end"] = bool(ida_funcs.set_func_end(parent, end))
        rec["new_end"] = "0x%08X" % ida_funcs.get_func(parent).end_ea
    log.append(rec)
with open(out_path, "w", encoding="utf-8") as fp:
    json.dump(log, fp, indent=1)
ida_pro.qexit(0)
