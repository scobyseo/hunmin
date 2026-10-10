#!/usr/bin/env python3
"""Convert vendored libhangul 0.2.0 keyboard data into hunmin layout JSON.

Deterministic: same inputs -> byte-identical outputs (tests/selfcheck rely on this).
Sources: data-orig/libhangul/*.xml  (+ hangulkeyboard.h for ahn/romaja combos,
which upstream never shipped as XML).

Encoding contract: docs/SPEC-SCHEMA.md
"""
import argparse
import json
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

US_SHIFTED_TO_BASE = {
    "~": "`", "!": "1", "@": "2", "#": "3", "$": "4", "%": "5", "^": "6",
    "&": "7", "*": "8", "(": "9", ")": "0", "_": "-", "+": "=", "{": "[",
    "}": "]", ":": ";", '"': "'", "<": ",", ">": ".", "?": "/", "|": "\\",
}

# libhangul builtin id -> (hunmin layout id, compose table ids)
LAYOUTS = {
    "2":   ("dubeolsik",        ["modern"]),
    "2y":  ("dubeolsik-yet",    ["modern-full"]),
    "32":  ("sebeolsik-3-2",    ["modern"]),
    "39":  ("sebeolsik-3-90",   ["modern"]),
    "3f":  ("sebeolsik-final",  ["modern"]),
    "3s":  ("sebeolsik-noshift", ["modern"]),
    "3y":  ("sebeolsik-yet",    ["modern-full"]),
    "ro":  ("romaja",           ["romaja"]),
    "ahn": ("ahnmatmae",        ["ahn"]),
}


def slot(cp: int):
    if 0x1100 <= cp <= 0x115E:
        return "L"
    if (0x1161 <= cp <= 0x11A7 or 0xA960 <= cp <= 0xA97C
            or 0xD7B0 <= cp <= 0xD7C6):
        return "V"
    if 0x11A8 <= cp <= 0x11FF or 0xD7CB <= cp <= 0xD7FB:
        return "T"
    return None


def convert_item(key: int, value: int, warn):
    """-> (base_key_id, mod, encoded_value) or None."""
    if not (0x20 <= key <= 0x7E):
        warn(f"ignored key U+{key:04X} (outside printable ASCII)")
        return None
    if value == 0:
        return None  # explicit null entry: key intentionally unbound
    ch = chr(key)
    if "A" <= ch <= "Z":
        base, mod = ch.lower(), "shift"
    elif ch in US_SHIFTED_TO_BASE:
        base, mod = US_SHIFTED_TO_BASE[ch], "shift"
    else:
        base, mod = ch, "plain"
    if value < 0x100:
        return base, mod, "=" + chr(value)
    if slot(value) is None or value in (0x115F, 0x1160):
        return base, mod, "=" + chr(value)   # non-jamo symbol: literal (e.g. “ ✳)
    return base, mod, chr(value)


def dump(obj) -> str:
    return json.dumps(obj, ensure_ascii=False, indent=2, sort_keys=False) + "\n"


def convert_keyboard(xml_path: Path, warn):
    root = ET.parse(xml_path).getroot()
    lid = root.get("id")
    if lid not in LAYOUTS:
        warn(f"{xml_path.name}: unknown keyboard id {lid!r}, skipped")
        return None
    hid, compose = LAYOUTS[lid]
    name = {"en": lid}
    for n in root.findall("name"):
        text = n.text
        if text is None:
            continue
        if n.get("{http://www.w3.org/XML/1998/namespace}lang") == "ko":
            name["ko"] = text.strip()
        elif n.get("xml:lang") is None:
            name["en"] = text.strip()
    keys = {}
    for m in root.findall("map"):
        if m.get("id") != "0":
            warn(f"{xml_path.name}: map id={m.get('id')} not supported")
            continue
        for it in m.findall("item"):
            key_str = it.get("key")
            value_str = it.get("value")
            if key_str is None or value_str is None:
                continue
            r = convert_item(int(key_str, 16), int(value_str, 16), warn)
            if r is None:
                continue
            base, mod, val = r
            ent = keys.setdefault(base, {})
            if mod in ent:
                warn(f"{xml_path.name}: duplicate {base}+{mod}, last wins")
            ent[mod] = val
    layout = {
        "schemaVersion": 1,
        "id": hid,
        "name": name,
        "engine": "romaja" if lid == "ro" else "std",
        "aliases": [lid],
        "source": f"libhangul-0.2.0 data/keyboards/{xml_path.name}",
        "libhangulType": root.get("type"),
        "compose": compose,
        "keys": {k: keys[k] for k in sorted(keys)},
    }
    return hid, dump(layout)


def convert_combination(xml_path: Path):
    root = ET.parse(xml_path).getroot()
    cid = {"hangul-combination-default.xml": "modern",
           "hangul-combination-full.xml": "modern-full"}.get(xml_path.name)
    items = []
    for it in root.findall("item"):
        first = it.get("first")
        second = it.get("second")
        result = it.get("result")
        if first is None or second is None or result is None:
            continue
        items.append({"a": chr(int(first, 16)), "b": chr(int(second, 16)),
                      "r": chr(int(result, 16))})
    items.sort(key=lambda i: (i["a"], i["b"]))
    return cid, dump({"schemaVersion": 1, "id": cid,
                      "source": f"libhangul-0.2.0 data/keyboards/{xml_path.name}",
                      "items": items})


TABLE_RE = re.compile(
    r"hangul_combination_table_(ahn|romaja)\[\]\s*=\s*\{(.*?)\n\};", re.S)


def extract_c_tables(header_path: Path, warn):
    """ahn/romaja combination tables exist only as C arrays in
    hangulkeyboard.h; 'first' packs (jamoA<<16|jamoB) when >0xFFFF."""
    out = []
    text = header_path.read_text(encoding="utf-8")
    for m in TABLE_RE.finditer(text):
        cid, body = m.group(1), m.group(2)
        items = []
        for row in re.finditer(r"\{\s*([^{}]+?)\s*\}", body):
            parts = [p.strip() for p in row.group(1).split(",")]
            if len(parts) == 2:
                first, res = (int(x, 16) for x in parts)
                a, b = first >> 16, first & 0xFFFF
                if not a:  # single-jamo first? shouldn't happen in ahn/romaja
                    warn(f"{cid}: odd row {parts}")
                    continue
            elif len(parts) == 3:
                a, b, res = (int(x, 16) for x in parts)
            else:
                warn(f"{cid}: unhandled row {parts}")
                continue
            items.append({"a": chr(a), "b": chr(b), "r": chr(res)})
        items.sort(key=lambda i: (i["a"], i["b"]))
        out.append((cid, dump({"schemaVersion": 1, "id": cid,
                               "source": "libhangul-0.2.0 hangul/hangulkeyboard.h",
                               "items": items})))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()
    warn = lambda msg: print(f"warn: {msg}", file=sys.stderr)

    (args.out / "layouts").mkdir(parents=True, exist_ok=True)
    (args.out / "compose").mkdir(parents=True, exist_ok=True)

    for x in sorted((args.orig).glob("hangul-keyboard-*.xml")):
        r = convert_keyboard(x, warn)
        if r:
            (args.out / "layouts" / f"{r[0]}.json").write_text(r[1], encoding="utf-8")
    for x in sorted((args.orig).glob("hangul-combination-*.xml")):
        cid, data = convert_combination(x)
        (args.out / "compose" / f"{cid}.json").write_text(data, encoding="utf-8")
    hdr = args.orig / "hangulkeyboard.h"
    if hdr.exists():
        for cid, data in extract_c_tables(hdr, warn):
            (args.out / "compose" / f"{cid}.json").write_text(data, encoding="utf-8")
    print(f"ok: converted to {args.out}")


if __name__ == "__main__":
    main()
