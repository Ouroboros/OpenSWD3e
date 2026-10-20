# IDAT batch script: idat -A -S"ida_fix_dinput_table.py <logfile>" <db>
# Remove bogus nullsub functions created inside the DIOBJECTDATAFORMAT keyboard table
# (0x4A0040, 256 entries x 16 bytes, referenced by DIDATAFORMAT at 0x4A1040) and restore dwords.
import json
import idc
import ida_auto
import ida_bytes
import ida_funcs
import ida_offset
import ida_pro

out_path = idc.ARGV[1]
TABLE = 0x4A0040
ENTRY = 16
COUNT = 256
FORMAT = 0x4A1040
GUID_KEY = 0x4ABCB0
DIDFT_FLAGS = 0x8000000C
ida_auto.auto_wait()
log = []

fmt_ok = (ida_bytes.get_dword(FORMAT + 0) == 0x18 and ida_bytes.get_dword(FORMAT + 4) == 0x10
          and ida_bytes.get_dword(FORMAT + 16) == COUNT and ida_bytes.get_dword(FORMAT + 20) == TABLE)
log.append({"check": "DIDATAFORMAT", "ok": fmt_ok})
bad_entries = []
for i in range(COUNT):
    e = TABLE + i * ENTRY
    ok = (ida_bytes.get_dword(e) == GUID_KEY and ida_bytes.get_dword(e + 4) == i
          and ida_bytes.get_dword(e + 8) == (DIDFT_FLAGS | (i << 8)) and ida_bytes.get_dword(e + 12) == 0)
    if not ok:
        bad_entries.append("0x%08X" % e)
log.append({"check": "entries", "bad": bad_entries})
if fmt_ok and not bad_entries:
    for ea in (0x4A0C64, 0x4A0C74, 0x4A0CE4, 0x4A0CF4):
        f = ida_funcs.get_func(ea)
        rec = {"ea": "0x%08X" % ea, "had_func": f is not None}
        if f is not None and f.start_ea == ea:
            rec["del_func"] = bool(ida_funcs.del_func(ea))
        log.append(rec)
    ida_bytes.del_items(TABLE, ida_bytes.DELIT_SIMPLE, COUNT * ENTRY)
    for i in range(COUNT):
        e = TABLE + i * ENTRY
        for k in range(4):
            ida_bytes.create_dword(e + 4 * k, 4)
        ida_offset.op_plain_offset(e, 0, 0)
    idc.set_name(TABLE, "c_dfDIKeyboard_objects", idc.SN_NOWARN)
    idc.set_name(FORMAT, "c_dfDIKeyboard", idc.SN_NOWARN)
    idc.set_name(GUID_KEY, "GUID_Key", idc.SN_NOWARN)
    remaining = [("0x%08X" % ea) for ea in range(TABLE, TABLE + COUNT * ENTRY)
                 if ida_funcs.get_func(ea) is not None or ida_bytes.is_code(ida_bytes.get_full_flags(ea))]
    log.append({"remaining_code_in_table": remaining[:10]})
with open(out_path, "w", encoding="utf-8") as fp:
    json.dump(log, fp, indent=1)
ida_pro.qexit(0)
