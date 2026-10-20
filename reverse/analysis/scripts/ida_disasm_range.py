# IDAT batch script: idat -A -S"ida_disasm_range.py <outfile> <start> <end>" <db>
# Query-only: list functions and their full disassembly in [start, end)
import idc
import idautils
import ida_auto
import ida_funcs
import ida_bytes
import ida_pro

out_path = idc.ARGV[1]
start = int(idc.ARGV[2], 16)
end = int(idc.ARGV[3], 16)
ida_auto.auto_wait()
with open(out_path, "w", encoding="utf-8") as fp:
    ea = start
    while ea < end and ea != idc.BADADDR:
        f = ida_funcs.get_func(ea)
        if f and f.start_ea == ea:
            fp.write("\n== func 0x%08X..0x%08X %s flags=0x%X\n" % (f.start_ea, f.end_ea, ida_funcs.get_func_name(ea), f.flags))
        fl = ida_bytes.get_full_flags(ea)
        tag = "C" if ida_bytes.is_code(fl) else "D"
        fp.write("%s 0x%08X  %s\n" % (tag, ea, idc.generate_disasm_line(ea, 0)))
        ea = idc.next_head(ea, end)
ida_pro.qexit(0)
