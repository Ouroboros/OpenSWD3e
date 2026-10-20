# Build reverse/functions.tsv and module summary from IDAT exports.
# usage: python3 -I build_function_table.py <workspace>
import json
import os
import sys

WS = sys.argv[1]
EXP = os.path.join(WS, "reverse", "analysis", "export")
funcs = json.load(open(os.path.join(EXP, "functions_reopen.json"), encoding="utf-8"))
struct = {f["ea"]: f for f in json.load(open(os.path.join(EXP, "structure.json"), encoding="utf-8"))}
by_ea = {int(f["ea"], 16): f for f in funcs}
fix = json.load(open(os.path.join(EXP, "bounds_fix_log.json"), encoding="utf-8"))

# Anchor functions: (entry address, module, evidence). Evidence is the string/import seen in that function.
ANCHORS = [
    (0x402AA0, "event", "Act Err(KeyCole:Talk*), RoleEventBy/MapEvent strings"),
    (0x405150, "path", "Act Err(Path:*)"),
    (0x406780, "path", "CompanyWalk"),
    (0x406EC0, "save", "%ssave\\%d.sav, swdghost.dat"),
    (0x409C80, "save", "%sFame.dat"),
    (0x409D70, "main", "dynamic initializer table 0x4AF004.. entries precede WinMain"),
    (0x40A140, "main", "RegisterClassEx SoftstarSwd3, CreateWindowEx, message loop"),
    (0x40AEC0, "main", "SetWindowPos/ShowWindow window setup"),
    (0x40B370, "dialog_frame", "SetupDialog, Act Err(Dialog:*)"),
    (0x40BEA0, "dialog_frame", "lstrlenA text layout next to SetupDialog"),
    (0x40C460, "misc", "Illegal Role id"),
    (0x40E420, "misc", "Act Err(Misc:Init:*)"),
    (0x40F5C0, "misc", "Act Err(Misc:newRoleInit)"),
    (0x40FA20, "misc", "Act Err(Talk:Talkend)"),
    (0x40FBE0, "cheat", "CheatDialog"),
    (0x410A80, "cheat", "SendMessageA dialog controls next to CheatDialog"),
    (0x411D10, "sysinfo", "GetDiskFreeSpaceExA via GetProcAddress"),
    (0x4121C0, "sysinfo", "c:\\, SWD3E_1 volume label"),
    (0x4126A0, "picpaint", "Act Err(picPaint:*)"),
    (0x4152A0, "picpaint", "Act Err(PicPaint:drawRoleHead)"),
    (0x4277E0, "env_config", "%sEnv.dat"),
    (0x427D80, "env_config", "%sEnv.dat"),
    (0x428080, "startup", "GetDriveTypeA a:\\"),
    (0x428EC0, "startup", "DrawKernel/DDraw/DirectInput init failed, Born.avi, Save/Data/Music dirs"),
    (0x4291B0, "mapdata", "env.dat, %sdata\\%d.cm, mcache.dat"),
    (0x42A7D0, "mapdata", "HUGE.lmf"),
    (0x42B4C0, "talk", "Act Err(Talk:*)"),
    (0x433650, "talk", "%sTalk%d.dat, HUGE.lmf"),
    (0x433C10, "dlg_message", "Act Err(DlgMessage:*)"),
    (0x435250, "dlg_message", "lstrlenA after DlgMessage"),
    (0x436050, "ddraw_error", "DDERR_* message box"),
    (0x4360C0, "ddraw_error", "DDERR_* message box"),
    (0x4364D0, "respack", "all_*.tsw / all_*.act packs"),
    (0x438450, "respack", "ReadFile/SetFilePointer pack readers"),
    (0x439890, "particle", "particle_put(..)"),
    (0x439BA0, "gdi_text", "GetSystemMetrics/GDI font+text"),
    (0x439FC0, "gdi_text", "CreateFontIndirectA"),
    (0x43BA70, "dinput", "cDINPUT, Direct Input Failure!"),
    (0x43C0D0, "ddraw", "DirectDraw Create Error!!"),
    (0x43C740, "ddraw", "GetPixel/SetPixel"),
    (0x43CB10, "file", "CreateFile/MapViewOfFile wrapper"),
    (0x43D0F0, "file", "FormatMessageA"),
    (0x43D120, "memmgr", "MemMgr"),
    (0x43D470, "memmgr", "FormatMessageA/GetLastError after MemMgr"),
    (0x43D550, "strutil", "CharNextA/CharPrevA helpers"),
    (0x43DA30, "strutil", "CharNextA"),
    (0x43F4A0, "menu", "menu number formats"),
    (0x4405E0, "menu", "Act Err(Menu:*)"),
    (0x444EC0, "menu", "Act Err(Botl::GetOriginal*)"),
    (0x452800, "menu", "Act Err(Menu::DrawActAlpha)"),
    (0x453A40, "menu", "v2.03, swdghost.dat"),
    (0x459010, "menu", "wsprintfA list formats"),
    (0x459BD0, "battle", "battle.ffd"),
    (0x45AAD0, "battle", "music\\Battle01.mp3"),
    (0x45EE20, "battle", "battle formats"),
    (0x468B20, "battle", "fMenu:%d mMove%d si:%d"),
    (0x46F920, "battle", "SetCursorPos battle cursor"),
    (0x479860, "battle", "large battle routine, music\\"),
    (0x47E270, "figtalk", "figtalk.dat"),
    (0x47E420, "figtalk", "CloseHandle figtalk"),
    (0x47E9C0, "manrole", "C:\\cpp\\SWD102e\\manrole.cpp"),
    (0x47EFE0, "manrole", "C:\\cpp\\SWD102e\\manrole.cpp"),
    (0x47FFD0, "act", "Attack Act"),
    (0x487FE0, "act", "Attack Act"),
    (0x489260, "act", "lstrcpyA act names"),
    (0x489750, "org", "mon.dat"),
    (0x48A180, "org", "level.dat, C:\\cpp\\SWD102e\\org.cpp"),
    (0x48AD00, "org", "Load fame decompress"),
    (0x48BA60, "role", "Wait Act"),
    (0x48CA10, "role", "C:\\cpp\\SWD102e\\role.cpp"),
    (0x4906F0, "role", "Fade Act"),
    (0x491A00, "battle_msg", "$1%s$0"),
    (0x498560, "battle_msg", "$1%s$0"),
    (0x49C440, "video", "Bink*"),
    (0x49C800, "video", "Sleep in video playback"),
    (0x49C930, "sound", "AIL sequence"),
    (0x49E550, "sound", "AIL stream"),
]
CRT_START = 0x4A1096

fix_note = {}
for a in fix["actions"]:
    ea = int(a["ea"], 16)
    if a["action"] == "create_func":
        ev = a.get("evidence")
        fix_note[ea] = "boundary_fix:create(" + (ev if isinstance(ev, str) else "; ".join(ev)) + ")"
tails = {}
for a in fix["actions"]:
    if a["action"] in ("append_tail", "extend_func") and a.get("ok"):
        p = int(a["parent"], 16)
        tails.setdefault(p, []).append("%s %s" % (a.get("kind", "extend"), a["ea"]))

anchors = sorted(ANCHORS)


def range_module(ea):
    prev = None
    nxt = None
    for a in anchors:
        if a[0] <= ea:
            prev = a
        elif nxt is None:
            nxt = a
    return prev, nxt


def is_import_thunk(f):
    if not f["thunk"]:
        return False
    s = struct.get(int(f["ea"], 16), {})
    if s.get("imports"):
        return True
    return any(by_ea.get(c, {}).get("thunk") and struct.get(c, {}).get("imports") for c in s.get("callees", []))


rows = []
mods = {}
for f in sorted(funcs, key=lambda r: int(r["ea"], 16)):
    ea = int(f["ea"], 16)
    notes = []
    if is_import_thunk(f):
        mod = "import_thunk"
        notes.append("FUNC_THUNK import jump stub")
    elif ea >= CRT_START:
        mod = "crt"
        notes.append("MSVC static runtime range (after import thunks, start=0x4A20A7)")
    else:
        prev, nxt = range_module(ea)
        if prev and prev[0] == ea:
            mod = prev[1]
            notes.append("anchor: " + prev[2])
        elif prev and nxt and prev[1] == nxt[1]:
            mod = prev[1]
            notes.append("between %s anchors 0x%X..0x%X" % (prev[1], prev[0], nxt[0]))
        else:
            mod = None
        mods[ea] = mod
    if is_import_thunk(f) or ea >= CRT_START:
        mods[ea] = mod
    rows.append([ea, f, mod, notes])

# resolve gaps by call-graph majority between neighbouring anchor modules
for _ in range(3):
    for r in rows:
        ea, f, mod, notes = r
        if mod is not None:
            continue
        prev, nxt = range_module(ea)
        cand = set(x[1] for x in (prev, nxt) if x)
        s = struct.get(ea, {})
        votes = {}
        for n in s.get("callers", []) + s.get("callees", []):
            m = mods.get(n)
            if m in cand:
                votes[m] = votes.get(m, 0) + 1
        if votes:
            best = max(votes.items(), key=lambda kv: kv[1])
            if list(votes.values()).count(best[1]) == 1:
                r[2] = best[0]
                mods[ea] = best[0]
                notes.append("gap %s|%s resolved by call graph %s" % (
                    prev[1] if prev else "-", nxt[1] if nxt else "-",
                    ",".join("%s=%d" % kv for kv in sorted(votes.items()))))
for r in rows:
    if r[2] is None:
        ea = r[0]
        prev, nxt = range_module(ea)
        r[2] = "unknown"
        r[3].append("between %s(0x%X) and %s(0x%X), no deciding evidence" % (
            prev[1] if prev else "-", prev[0] if prev else 0, nxt[1] if nxt else "-", nxt[0] if nxt else 0))

out = os.path.join(WS, "reverse", "functions.tsv")
with open(out, "w", encoding="utf-8", newline="\n") as fp:
    fp.write("order\tbinary\taddress\tname\tmodule\tsource\tstatus\tverification_count\tevidence\tnotes\n")
    for i, (ea, f, mod, notes) in enumerate(rows, 1):
        extra = ["size=%d" % f["size"], "chunks=%d" % f["chunks"]]
        if f["lib"]:
            extra.append("FUNC_LIB(IDA entry-point flag, no FLIRT)")
        if f["noret"]:
            extra.append("noreturn")
        if f["thunk"] and not is_import_thunk(f):
            extra.append("FUNC_THUNK(single jmp to internal code)")
        if f["callers"] == 0 and f["drefs"] == 0 and not struct.get(ea, {}).get("tables") and f["name"] != "start":
            extra.append("no xref (unreferenced in binary)")
        if ea in fix_note:
            extra.append(fix_note[ea])
        if ea in tails:
            extra.append("tails: " + ", ".join(tails[ea]))
        note = "; ".join(notes + extra).replace("\t", " ").replace("\n", " ")
        fp.write("%d\tswd3e.exe\t0x%08X\t%s\t%s\t\tpending_implementation\t0\t\t%s\n" % (
            i, ea, f["name"], mod, note))

summary = {}
for ea, f, mod, notes in rows:
    s = summary.setdefault(mod, {"count": 0, "bytes": 0, "min": ea, "max": ea})
    s["count"] += 1
    s["bytes"] += f["size"]
    s["min"] = min(s["min"], ea)
    s["max"] = max(s["max"], ea)
with open(os.path.join(EXP, "module_summary.json"), "w", encoding="utf-8") as fp:
    json.dump({k: dict(v, min="0x%08X" % v["min"], max="0x%08X" % v["max"]) for k, v in summary.items()},
              fp, indent=1)
print("rows", len(rows))
for k, v in sorted(summary.items(), key=lambda kv: kv[1]["min"]):
    print("%-14s %5d funcs %7d bytes 0x%08X..0x%08X" % (k, v["count"], v["bytes"], v["min"], v["max"]))
