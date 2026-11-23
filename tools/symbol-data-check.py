#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Data contract checker for the Android keypad's app-read symbol pages.

    python3 tools/symbol-data-check.py [symbols.json [symbols-extended.json]]

No args: both shipped files. Exit != 0 on any error. Registered as ctest
`keypad-symbol-data` (tier-B).

Contract owner: docs/SPEC-ANDROID-KEYPAD.md — KP6 (the ruled three-row
arrangement and nav order), KP9 (no space/enter on the symbol screen; the
designed Tier 2 panel: 10 per row, groups kept, no editing keys), KP10 (the
기호 mode-key gesture opens the page picker; the panel is content, not a
button), KP11 (app-read data files, engine uninvolved; issue #298 ruling,
implementation #297, pages #312). The expected rows below are the maintainer's
ruled arrangement quoted from KP6/KP9, not a copy of the data files' own
claims: the files are the artefacts, this is the requirement they must
satisfy. The Kotlin side (SymbolScreen.kt / SymbolScreenTest.kt, tier E1) pins
the parse and the commit path; this checker pins the content contract and runs
on hosts with no Android SDK.

Negative fixtures for both contracts run as a built-in selfcheck on every
invocation, so the failure paths are asserted without a second test
registration.
"""
import json
import string
import sys
import unicodedata
from pathlib import Path

# Default paths derived from this script's location, never hardcoded to a host.
DEFAULT = (Path(__file__).resolve().parent.parent
           / "frontends/android/app/src/main/assets/keypad/symbols.json")
DEFAULT_EXTENDED = DEFAULT.with_name("symbols-extended.json")

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
    # KP9's mode-key slot on the symbol screen, restored 2026-11-23 (#312):
    # the 기호 key sits beside 한/영 as in the text modes; its gesture opens
    # the page picker (KP10). Its absence was a drift artefact.
    "symbols-page",
}

# KP6 nav order ruling: "home pgup pgdn end 순서로 변경."
NAV_ORDER = ["home", "page-up", "page-down", "end"]

# The designed Tier 2 panel (KP6/KP9, maintainer 2026-11-22): 10 keys per row,
# the original 8-key category groups kept with 2 symbols moved up into each,
# no editing keys. Verbatim from docs/ANDROID-KEYPAD-MOCKUP.md §Symbol mode
# Tier 2; the entry is the 기호 mode-key gesture (KP10), not a Tier 2 button.
RULED_EXTENDED_ROWS = [
    list("·…–—‘’“”₩€"),
    list("「」『』《》〈〉£¥"),
    list("○●□■△▲※★©®"),
    list("→←↑↓↔°±×™♡"),
]
EXTENDED_FUNCTION_IDS = {"language-toggle", "symbols-tier1"}

TOP_KEYS = {"schemaVersion", "id", "name", "source", "x-notes", "rows"}


def validate(data):
    """Contract errors for the Tier 1 symbol screen (symbols.json)."""
    errors = []

    def err(msg):
        errors.append(msg)

    if not isinstance(data, dict):
        return ["top level is not an object"]
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
    for banned in ("engine", "mode", "compose", "keys", "libhangulType", "variants"):
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
    # KP9's mode-key slot (#312, 2026-11-23): the 기호 key sits beside 한/영
    # on the symbol screen as in the text modes; its gesture opens the picker.
    if "symbols-page" not in function_keys:
        err("KP9: the 기호 mode-key slot (symbols-page) must be present on the symbol screen")
    return errors


def validate_extended(data):
    """Contract errors for the extended page (the designed Tier 2 panel)."""
    errors = []

    def err(msg):
        errors.append(msg)

    if not isinstance(data, dict):
        return ["top level is not an object"]
    extra = set(data) - TOP_KEYS
    if extra:
        err(f"unknown top-level keys: {sorted(extra)}")
    for req in ("schemaVersion", "id", "name", "source", "rows"):
        if req not in data:
            err(f"missing required key {req!r}")
    if data.get("schemaVersion") != 1:
        err(f"schemaVersion must be 1, got {data.get('schemaVersion')!r}")
    if data.get("id") != "keypad-symbols-extended":
        err(f"id must be 'keypad-symbols-extended', got {data.get('id')!r}")
    for banned in ("engine", "mode", "compose", "keys", "libhangulType", "variants"):
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
                if not (isinstance(k, str) and len(k) == 1 and ord(k) > 0x7F
                        and unicodedata.category(k)[0] in ("P", "S")):
                    err(f"{where}: {k!r} is not a single non-ASCII punctuation/symbol character")
            symbol_rows.append(keys)
        elif kind == "function":
            for k in keys:
                if k not in EXTENDED_FUNCTION_IDS:
                    err(f"{where}: unknown function id {k!r}")
            function_keys.extend(keys)
        else:
            err(f"{where}: unknown kind {kind!r}")
    if symbol_rows != RULED_EXTENDED_ROWS:
        err("symbol rows do not match the designed Tier 2 panel "
            f"(expected {RULED_EXTENDED_ROWS!r}, got {symbol_rows!r})")
    flat = [k for r in symbol_rows for k in r if isinstance(k, str)]
    if len(flat) != len(set(flat)):
        err("duplicate symbol keys")
    if len(function_keys) != len(set(function_keys)):
        err("duplicate function ids")
    for banned in ("space", "enter", "backspace"):
        if banned in function_keys:
            err(f"KP9: {banned!r} must not appear on the extended page (no editing keys)")
    return errors


def selfcheck():
    """Negative fixtures for both contracts, run on every invocation.

    A minimal valid document is mutated one contract rule at a time; each
    mutation must produce at least one error, and the bases must produce none.
    Returns a list of selfcheck failures (empty when the checker itself is sane).
    """
    base = {
        "schemaVersion": 1,
        "id": "keypad-symbols",
        "name": {"en": "fixture"},
        "source": "selfcheck fixture, not shipped data",
        "rows": [{"kind": "symbols", "keys": list(r)} for r in RULED_SYMBOL_ROWS]
        + [{"kind": "function",
            "keys": ["tab", "home", "page-up", "page-down", "end", "backspace"]},
           {"kind": "function",
            "keys": ["language-toggle", "symbols-page", "left", "up", "down", "right"]}],
    }
    failures = []
    if validate(base):
        failures.append(f"minimal valid document rejected: {validate(base)}")

    def mutate(name, fn):
        doc = json.loads(json.dumps(base))
        fn(doc)
        if not validate(doc):
            failures.append(f"negative fixture accepted: {name}")

    mutate("ruled row reordered",
           lambda d: d["rows"][0].__setitem__("keys", list("@!#$%^&*()")))
    mutate("engine-layout field smuggled in",
           lambda d: d.__setitem__("engine", "std"))
    mutate("variant mapping smuggled back in (the #312 drift)",
           lambda d: d.__setitem__("variants", {".": {"up": "·"}}))
    mutate("기호 mode-key slot dropped from the symbol screen",
           lambda d: d["rows"][4].__setitem__("keys",
                   ["language-toggle", "left", "up", "down", "right"]))
    mutate("space on the symbol screen",
           lambda d: d["rows"][4]["keys"].append("space"))
    mutate("nav order broken",
           lambda d: d["rows"][3].__setitem__("keys",
                   ["tab", "end", "page-up", "page-down", "home", "backspace"]))

    ext = {
        "schemaVersion": 1,
        "id": "keypad-symbols-extended",
        "name": {"en": "fixture"},
        "source": "selfcheck fixture, not shipped data",
        "rows": [{"kind": "symbols", "keys": list(r)} for r in RULED_EXTENDED_ROWS]
        + [{"kind": "function", "keys": ["language-toggle", "symbols-tier1"]}],
    }
    if validate_extended(ext):
        failures.append(f"minimal extended document rejected: {validate_extended(ext)}")

    def mutate_ext(name, fn):
        doc = json.loads(json.dumps(ext))
        fn(doc)
        if not validate_extended(doc):
            failures.append(f"negative extended fixture accepted: {name}")

    mutate_ext("panel row reordered",
               lambda d: d["rows"][0].__setitem__("keys", list("…·–—‘’“”₩€")))
    mutate_ext("ASCII character on the extended page",
               lambda d: d["rows"][0].__setitem__("keys", list("!…–—‘’“”₩€")))
    mutate_ext("editing key on the extended page",
               lambda d: d["rows"][4].__setitem__("keys",
                       ["language-toggle", "symbols-tier1", "backspace"]))
    mutate_ext("unknown function id",
               lambda d: d["rows"][4].__setitem__("keys", ["language-toggle", "tier3"]))
    mutate_ext("duplicate symbol across rows",
               lambda d: d["rows"][1].__setitem__("keys", list("·」『』《》〈〉£¥")))
    return failures


def main():
    # No args: both shipped files. One arg: the Tier 1 screen only (the
    # historical CLI); two: both, explicitly.
    targets = [(DEFAULT, validate), (DEFAULT_EXTENDED, validate_extended)]
    if len(sys.argv) == 2:
        targets = [(Path(sys.argv[1]), validate)]
    elif len(sys.argv) == 3:
        targets = [(Path(sys.argv[1]), validate),
                   (Path(sys.argv[2]), validate_extended)]

    errors = []
    loaded = []
    for path, validator in targets:
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as e:
            print(f"{path}: cannot read/parse: {e}", file=sys.stderr)
            return 1
        errors.extend(f"{path}: {e}" for e in validator(data))
        loaded.append((path, data))

    errors.extend(f"selfcheck: {f}" for f in selfcheck())

    if errors:
        for e in errors:
            print(e, file=sys.stderr)
        return 1
    parts = []
    for path, data in loaded:
        n = sum(len(r["keys"]) for r in data["rows"] if r["kind"] == "symbols")
        parts.append(f"{path.name}: {n} symbols in the ruled arrangement")
    print("ok: " + "; ".join(parts) + "; contract clean, selfcheck clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())