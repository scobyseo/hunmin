#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Data contract checker for the Android keypad's app-read symbol screen.

    python3 tools/symbol-data-check.py [path-to-symbols.json]

Exit != 0 on any error. Registered as ctest `keypad-symbol-data` (tier-A).

Contract owner: docs/SPEC-ANDROID-KEYPAD.md — KP6 (the ruled three-row
arrangement and nav order), KP9 (no space/enter on the symbol screen), KP11
(app-read data file, engine uninvolved; issue #298 ruling, implementation #297).
The expected rows below are the maintainer's ruled arrangement quoted from KP6,
not a copy of the data file's own claim: the file is the artefact, this is the
requirement it must satisfy. The Kotlin side (SymbolScreen.kt /
SymbolScreenTest.kt, tier E1) pins the parse and the commit path; this checker
pins the content contract and runs on hosts with no Android SDK.
"""
import json
import string
import sys
from pathlib import Path

# Default path derived from this script's location, never hardcoded to a host.
DEFAULT = (Path(__file__).resolve().parent.parent
           / "frontends/android/app/src/main/assets/keypad/symbols.json")

# KP6, ruled 2026-11-22 (verbatim arrangement; grouped []{} and ,.<>, ` first).
RULED_SYMBOL_ROWS = [
    list("!@#$%^&*()"),
    list("`~-_=+[]{}\\|"),
    list(";:'\",.<>/?"),
]

# KP9: the symbol screen drops space/enter; KP6/KP9 fix the functional slots.
# The vocabulary is closed: a new id is a contract change, made here first.
FUNCTION_IDS = {
    "tab", "home", "page-up", "page-down", "end", "backspace",
    "language-toggle", "left", "up", "down", "right",
}

# KP6 nav order ruling: "home pgup pgdn end 순서로 변경."
NAV_ORDER = ["home", "page-up", "page-down", "end"]

TOP_KEYS = {"schemaVersion", "id", "name", "source", "x-notes", "rows"}


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT
    errors = []

    def err(msg):
        errors.append(f"{path}: {msg}")

    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as e:
        print(f"{path}: cannot read/parse: {e}", file=sys.stderr)
        return 1

    if not isinstance(data, dict):
        err("top level is not an object")
        data = {}
    extra = set(data) - TOP_KEYS
    if extra:
        err(f"unknown top-level keys: {sorted(extra)}")
    for req in ("schemaVersion", "id", "name", "source", "rows"):
        if req not in data:
            err(f"missing required key {req!r}")
    if data.get("schemaVersion") != 1:
        err(f"schemaVersion must be 1, got {data.get('schemaVersion')!r}")
    if data.get("id") != "keypad-symbols":
        err(f"id must be 'keypad-symbols', got {data.get('id')!r}")
    # KP11: engine uninvolved. Engine-layout fields must not appear here.
    for banned in ("engine", "mode", "compose", "keys", "libhangulType"):
        if banned in data:
            err(f"engine-layout field {banned!r} must not appear in app-read data (KP11)")

    rows = data.get("rows")
    if not isinstance(rows, list) or not rows:
        err("rows must be a non-empty list")
        rows = []

    symbol_rows, function_keys = [], []
    for i, row in enumerate(rows):
        where = f"rows[{i}]"
        if not isinstance(row, dict) or set(row) != {"kind", "keys"}:
            err(f"{where}: row must have exactly 'kind' and 'keys'")
            continue
        kind, keys = row["kind"], row["keys"]
        if not isinstance(keys, list) or not keys:
            err(f"{where}: keys must be a non-empty list")
            continue
        if kind == "symbols":
            for k in keys:
                if not (isinstance(k, str) and len(k) == 1
                        and k in string.punctuation):
                    err(f"{where}: {k!r} is not a single ASCII punctuation character")
            symbol_rows.append(keys)
        elif kind == "function":
            for k in keys:
                if k not in FUNCTION_IDS:
                    err(f"{where}: unknown function id {k!r}")
            function_keys.extend(keys)
        else:
            err(f"{where}: unknown kind {kind!r}")

    if symbol_rows != RULED_SYMBOL_ROWS:
        err("symbol rows do not match the KP6 ruled arrangement "
            f"(expected {RULED_SYMBOL_ROWS!r}, got {symbol_rows!r})")
    flat = [k for r in symbol_rows for k in r if isinstance(k, str)]
    if len(flat) != len(set(flat)):
        err("duplicate symbol keys")
    if set(flat) != set(string.punctuation):
        missing = sorted(set(string.punctuation) - set(flat))
        err(f"symbol inventory must be all 32 ASCII punctuation; missing {missing}")
    if len(function_keys) != len(set(function_keys)):
        err("duplicate function ids")
    for banned in ("space", "enter"):
        if banned in function_keys:
            err(f"KP9: {banned!r} must not appear on the symbol screen")
    nav_present = [k for k in function_keys if k in NAV_ORDER]
    if nav_present != [k for k in NAV_ORDER if k in nav_present]:
        err(f"KP6 nav order must be {' '.join(NAV_ORDER)}; got {nav_present}")

    if errors:
        for e in errors:
            print(e, file=sys.stderr)
        return 1
    print(f"ok: {path} — 32 symbols in the ruled arrangement, "
          f"{len(function_keys)} function keys, contract clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
