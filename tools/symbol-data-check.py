#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Data contract checker for the Android keypad's app-read symbol screen.

    python3 tools/symbol-data-check.py [path-to-symbols.json]

Exit != 0 on any error. Registered as ctest `keypad-symbol-data` (tier-B).

Contract owner: docs/SPEC-ANDROID-KEYPAD.md — KP6 (the ruled three-row
arrangement and nav order), KP9 (no space/enter on the symbol screen), KP11
(app-read data file, engine uninvolved; issue #298 ruling, implementation #297).
The expected rows below are the maintainer's ruled arrangement quoted from KP6,
not a copy of the data file's own claim: the file is the artefact, this is the
requirement it must satisfy. The Kotlin side (SymbolScreen.kt /
SymbolScreenTest.kt, tier E1) pins the parse and the commit path; this checker
pins the content contract and runs on hosts with no Android SDK.

The variants section (issue #312, KP10) is checked for STRUCTURE AND SCOPE,
not for the assignment: which key carries which variant is provisional
(docs/DESIGN-ANDROID-SYMBOL-VARIANTS.md §8) and unruled, so pinning the
assignment here would write a verdict into the contract. Negative fixtures
for both halves run as a built-in selfcheck on every invocation, so the
failure paths are asserted without a second test registration.
"""
import json
import string
import sys
import unicodedata
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

# Issue #312's named Tier 2 scope: every one of these must be reachable via
# some host's popup. Superset is allowed (extending the inventory is a scope
# decision, docs/DESIGN-ANDROID-SYMBOL-VARIANTS.md §7); a miss is an error.
TIER2_SCOPE = set("·…《》〈〉「」『』※₩")

# KP8-derived popup cap, proposed (design doc §6), enforced so growth is a
# deliberate contract change: a popup is read at a glance, not searched.
POPUP_CAP = 4

TOP_KEYS = {"schemaVersion", "id", "name", "source", "x-notes", "rows", "variants"}


def validate(data):
    """Return a list of contract errors for a parsed symbols document."""
    errors = []

    def err(msg):
        errors.append(msg)

    if not isinstance(data, dict):
        return ["top level is not an object"]
    extra = set(data) - TOP_KEYS
    if extra:
        err(f"unknown top-level keys: {sorted(extra)}")
    for req in ("schemaVersion", "id", "name", "source", "rows", "variants"):
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

    # --- variants (issue #312): structure and scope, never the assignment ---
    variants = data.get("variants")
    if not isinstance(variants, dict) or not variants:
        err("variants must be a non-empty object (issue #312 scope)")
        return errors
    seen_variants = {}
    for host, spec in variants.items():
        where = f"variants[{host!r}]"
        if host not in flat:
            err(f"{where}: host is not a Tier 1 symbol key of the ruled rows")
        if not isinstance(spec, dict) or set(spec) != {"up", "popup"}:
            err(f"{where}: entry must have exactly 'up' and 'popup'")
            continue
        up, popup = spec["up"], spec["popup"]
        if not isinstance(popup, list) or not popup:
            err(f"{where}: popup must be a non-empty list")
            continue
        if len(popup) > POPUP_CAP:
            err(f"{where}: popup has {len(popup)} members, cap is {POPUP_CAP} (KP8)")
        for ch in popup:
            if not (isinstance(ch, str) and len(ch) == 1 and ord(ch) > 0x7F
                    and unicodedata.category(ch)[0] in ("P", "S")):
                err(f"{where}: {ch!r} is not a single non-ASCII punctuation/symbol "
                    "character (Tier 2 is what a 104-key cannot type)")
            if ch in seen_variants:
                err(f"{where}: variant {ch!r} already lives under "
                    f"{seen_variants[ch]!r} — one home per variant")
            seen_variants[ch] = host
        if up != popup[0]:
            err(f"{where}: up must be popup[0] (one frequency order, no second ranking)")
    missing_scope = TIER2_SCOPE - set(seen_variants)
    if missing_scope:
        err(f"issue #312 scope: unreachable Tier 2 characters {sorted(missing_scope)}")
    return errors


def selfcheck():
    """Negative fixtures for the contract, run on every invocation.

    A minimal valid document is mutated one contract rule at a time; each
    mutation must produce at least one error, and the base must produce none.
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
            "keys": ["language-toggle", "left", "up", "down", "right"]}],
        "variants": {
            "$": {"up": "₩", "popup": ["₩"]},
            "*": {"up": "※", "popup": ["※"]},
            "[": {"up": "「", "popup": ["「", "『"]},
            "]": {"up": "」", "popup": ["」", "』"]},
            ".": {"up": "·", "popup": ["·", "…"]},
            "<": {"up": "《", "popup": ["《", "〈"]},
            ">": {"up": "》", "popup": ["》", "〉"]},
        },
    }
    failures = []
    if validate(base):
        failures.append(f"minimal valid document rejected: {validate(base)}")

    def mutate(name, fn):
        doc = json.loads(json.dumps(base))
        fn(doc)
        if not validate(doc):
            failures.append(f"negative fixture accepted: {name}")

    mutate("variants section absent", lambda d: d.pop("variants"))
    mutate("host not a Tier 1 key",
           lambda d: d["variants"].__setitem__("a", d["variants"].pop("$")))
    mutate("up is not popup[0]",
           lambda d: d["variants"]["."].__setitem__("up", "…"))
    mutate("popup over the KP8 cap",
           lambda d: d["variants"]["."].__setitem__("popup", ["·", "…", "‥", "•", "‧"]))
    mutate("ASCII variant character",
           lambda d: d["variants"]["$"].__setitem__("popup", ["$"]))
    mutate("non-symbol variant character",
           lambda d: d["variants"]["$"].__setitem__("popup", ["가"]))
    mutate("multi-character variant",
           lambda d: d["variants"]["."].__setitem__("popup", ["··"]))
    mutate("variant under two hosts",
           lambda d: d["variants"]["*"].__setitem__("popup", ["※", "·"]))
    mutate("issue scope character unreachable",
           lambda d: d["variants"].pop("$"))
    mutate("entry with an extra field",
           lambda d: d["variants"]["."].__setitem__("down", "…"))
    mutate("ruled row reordered",
           lambda d: d["rows"][0].__setitem__("keys", list("@!#$%^&*()")))
    mutate("engine-layout field smuggled in",
           lambda d: d.__setitem__("engine", "std"))
    return failures


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT

    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as e:
        print(f"{path}: cannot read/parse: {e}", file=sys.stderr)
        return 1

    errors = [f"{path}: {e}" for e in validate(data)]

    sc = selfcheck()
    errors.extend(f"selfcheck: {f}" for f in sc)

    if errors:
        for e in errors:
            print(e, file=sys.stderr)
        return 1
    nhosts = len(data["variants"])
    nvars = sum(len(v["popup"]) for v in data["variants"].values())
    print(f"ok: {path} — 32 symbols in the ruled arrangement, "
          f"{nvars} Tier 2 variants under {nhosts} hosts, "
          f"contract clean, selfcheck clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())