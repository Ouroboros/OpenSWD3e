# IDAT batch script: idat -A -S"ida_decompile.py <out dir> <address>..." <db>
# Query-only: decompiles the given functions into <out dir>/<address>.c, writing the Hex-Rays failure (address and
# reason) in place of the text when one fails.
import os

import ida_auto
import ida_funcs
import ida_hexrays
import ida_pro
import idc

out_dir = idc.ARGV[1]
os.makedirs(out_dir, exist_ok=True)
ida_auto.auto_wait()
ida_hexrays.init_hexrays_plugin()
for text in idc.ARGV[2:]:
    ea = int(text, 16)
    failure = ida_hexrays.hexrays_failure_t()
    cfunc = ida_hexrays.decompile_func(ida_funcs.get_func(ea), failure, 0)
    with open(os.path.join(out_dir, "%08X.c" % ea), "w", encoding="utf-8", newline="\n") as fh:
        if cfunc:
            fh.write(str(cfunc))
        else:
            fh.write("// decompile failed at 0x%08X: %s (code %d)\n" % (failure.errea, failure.desc(), failure.code))
ida_pro.qexit(0)
