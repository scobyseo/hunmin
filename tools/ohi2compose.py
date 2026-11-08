#!/usr/bin/env python3
"""ohi2compose.py — transcribe 옛한글 combination rules from OHI's literal
codepoint tables into a hunmin compose JSON.

Normalization (docs/SPEC-P2.md): JONG-context rows whose first element is a
CHO codepoint (0x1100-0x1112) are converted to that jamo's JONG form, matching
how the engine stores slot contents. 2022-12-24 pat.im deletions applied.
"""
import json, pathlib, re, sys
try:  # issue #272 phase 2: one ROOT derivation, with a file-relative fallback
    from _common import ROOT as _COMMON_ROOT
except ImportError:  # run from outside tools/: derive from this file
    _COMMON_ROOT = __import__("pathlib").Path(__file__).resolve().parents[1]

SRC = _COMMON_ROOT / "data-orig/ohi/keyboard_table_combination.js"
CHO2JONG = {0x1100:0x11A8,0x1101:0x11A9,0x1102:0x11AB,0x1103:0x11AE,0x1105:0x11AF,
            0x1106:0x11B7,0x1107:0x11B8,0x1109:0x11BA,0x110A:0x11BB,0x110B:0x11BC,
            0x110C:0x11BD,0x110E:0x11BE,0x110F:0x11BF,0x1110:0x11C0,0x1111:0x11C1,
            0x1112:0x11C2}
DELETED_2022 = {(0x113E, 0x1109), (0x114E, 0x110C)}

def grab(text, name):
    m = re.search(re.escape(name) + r"\s*=\s*\[(.*?)\n\s*\];", text, re.S)
    return m.group(1) if m else ""

def rows(block) -> list[tuple[int, int, int]]:
    out: list[tuple[int, int, int]] = []
    for pk, r in re.findall(r"\[\s*(0x[0-9A-Fa-f]{8})\s*,\s*(0x[0-9A-Fa-f]{4,6})\s*\]", block):
        p = int(pk, 16)
        a, b = p >> 16, p & 0xFFFF
        out.append((a, b, int(r, 16)))
    return out

def main():
    t = SRC.read_text(encoding="utf-8")
    merged: dict[tuple[int, int], int] = {}
    for a, b, r in reversed(rows(grab(t, "hangeul_combination_table_full"))):
        merged[(a, b)] = r
    for a, b, r in reversed(rows(grab(t, "K3_3shin_p_yet"))):
        merged[(a, b)] = r           # shin-p yet aliases win
    norm: dict[tuple[int, int], int] = {}
    for (a, b), r in merged.items():
        if (a, b) in DELETED_2022:
            continue
        if 0x11A8 <= b <= 0x11FF or 0xD7CB <= b <= 0xD7FB:   # JONG-context row
            a = CHO2JONG.get(a, a)
        ok = all((0x1100 <= cp <= 0x11FF) or (0xA960 <= cp <= 0xA97F) or
                 (0xD7B0 <= cp <= 0xD7FF) for cp in (a, b, r))
        if not ok:
            print(f"skip odd row {a:X}+{b:X}->{r:X}", file=sys.stderr)
            continue
        norm.setdefault((a, b), r)          # merged-order priority wins
    items = [{"a": chr(a), "b": chr(b), "r": chr(r)}
             for (a, b), r in sorted(norm.items())]
    out = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path("libhunmin/data/compose")
    (out / "sin3-p2-yet-ohi.json").write_text(json.dumps(
        {"schemaVersion": 1, "id": "sin3-p2-yet-ohi",
         "source": "OHI literal tables (transcribed facts, pat.im/1136 section 2-4 rules); 2022-12-24 deletions applied",
         "items": items}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"ohi2compose: {len(items)} rows")

main()
