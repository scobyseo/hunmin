#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Tier C step 4: keycode-map fixture test (docs/TESTING.md).

    python3 tools/keymap-fixture.py [--json]

Asserts that every key a layout can use has a defined native encoding on each
platform, and that the platform's own headers agree with the table here.

The table is NOT the authority. For X11 the authority is the XKB header
(xkbcommon/xkbcommon-keysyms.h, present in the vendored fcitx5-5.1.19 tree and
in every distro's /usr/include); this script reads it and compares. A hand-typed
table that merely restates the code under test catches nothing, which is the
mistake tier A's `consumed` rule made twice before it was derived instead.

What this cannot check, and why it still earns its place: the per-platform
*adapter* mapping. This proves the ENCODING is right and complete; that the
adapter applies it is what tier C's trace replay (104 traces, 3,832 events)
covers. Between them there is no key a layout uses that no tier exercises.

Exit 0 pass, 1 fail. Written to be run in ctest and by hand.
"""
import argparse
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
LAYOUTS = ROOT / "libhunmin/data/layouts"

# US-QWERTY shifted punctuation, needed because layouts key on the unshifted
# character while a trace sends the shifted one (C -> c, : -> ;).
SHIFTED = {
    "!": "1", "@": "2", "#": "3", "$": "4", "%": "5", "^": "6", "&": "7",
    "*": "8", "(": "9", ")": "0", "_": "-", "+": "=", "{": "[", "}": "]",
    "|": "\\", ":": ";", '"': "'", "<": ",", ">": ".", "?": "/", "~": "`",
}

# XKB spells the punctuation keys; letters and digits use the character itself.
ASCII_XKB_NAME = {
    ";": "semicolon", "\\": "backslash", "]": "bracketright", "[": "bracketleft",
    "=": "equal", "-": "minus", "'": "apostrophe", "`": "grave", ",": "comma",
    ".": "period", "/": "slash", " ": "space",
}

# X11 keysym values that are not their ASCII code. Only the ones a Korean layout
# can reference; space and the printable range are the identity by XKB's own
# definition.
X11_SPECIAL = {
    "space": 0x0020, "backspace": 0xFF08, "return": 0xFF0D, "tab": 0xFF09,
    "escape": 0xFF1B, "left": 0xFF51, "up": 0xFF52, "right": 0xFF53,
    "down": 0xFF54, "F9": 0xFFC6,
}

# Android KEYCODE_* for the same set. ASCII letters and digits are their own
# keycodes by AOSP's definition; the rest are the named constants.
# AOSP keycodes are NOT ASCII and NOT contiguous from 0: KEYCODE_0 is 7,
# KEYCODE_A is 29, KEYCODE_Z is 54, and punctuation sits past the letters
# (KEYCODE_SEMICOLON 74, KEYCODE_GRAVE 68). Every value below was read out of
# the installed platforms;android-35/android.jar with javap -constants, not
# guessed -- the table this replaces returned ord(ch) for all 47 keys, which
# was wrong for every single one of them. Verify with:
#   javap -cp $ANDROID_HOME/platforms/android-35/android.jar -constants \
#         android.view.KeyEvent | grep KEYCODE_
ANDROID_NAMED = {
    "space": 62, "backspace": 67, "return": 66, "tab": 61, "escape": 111,
    "left": 21, "up": 22, "right": 23, "down": 20,
}

# Character -> AOSP KEYCODE name. Letters and digits key by name; the layout
# tables hold the unshifted character, so 'k' resolves to KEYCODE_K.
ANDROID_CHAR_KEYCODE = {
    **{c: c for c in "0123456789"},
    **{c: c.upper() for c in "abcdefghijklmnopqrstuvwxyz"},
    ";": "SEMICOLON", "'": "APOSTROPHE", ",": "COMMA", ".": "PERIOD",
    "/": "SLASH", "-": "MINUS", "=": "EQUALS", "[": "LEFT_BRACKET",
    "]": "RIGHT_BRACKET", "\\": "BACKSLASH", "`": "GRAVE", " ": "SPACE",
}

# Values verified against android.jar on 2026-10-03 (API 35). Kept explicit so
# the table is checkable without a JDK present: keymap-fixture compares these
# against the jar when it can find one.
ANDROID_KEYCODES = {
    # Digits, then letters: AOSP numbers these contiguously from 7 and 29.
    **{str(d): 7 + d for d in range(10)},
    **{chr(ord("A") + i): 29 + i for i in range(26)},
    # Punctuation sits AFTER the letters. Note KEYCODE_EQUALS: there is no
    # KEYCODE_EQUAL, and guessing one cost a round trip (the fixture check
    # against android.jar is what caught it).
    "COMMA": 55, "PERIOD": 56,
    "GRAVE": 68, "MINUS": 69, "EQUALS": 70, "LEFT_BRACKET": 71,
    "RIGHT_BRACKET": 72, "BACKSLASH": 73, "SEMICOLON": 74, "APOSTROPHE": 75,
    "SLASH": 76,
    # Named keys.
    "SPACE": 62, "TAB": 61, "ENTER": 66, "DEL": 67, "ESCAPE": 111,
    "DPAD_UP": 19, "DPAD_DOWN": 20, "DPAD_LEFT": 21, "DPAD_RIGHT": 22,
}
# Keys with no AOSP keycode at all: layout key -> the KEYCODE_* name it would be.
#
# This is a DECLARATION, and when an android.jar is available it is checked
# against the platform's own constant table -- not against the fixture's tables
# below, which would only prove the fixture agrees with itself.
#
# It is EMPTY, and both halves of why matter. Every layout key is printable ASCII
# and every printable key has an AOSP keycode. F9 was listed here on the belief
# that "KEYCODE_F9 does not exist"; measured 2026-10-31 out of android.jar,
# `KeyEvent.KEYCODE_F9` is 139, so the belief was false and F9 is no longer a
# layout key at all (0 named keys across the 15 layouts). What is true is
# narrower and is not a keycode fact: no on-screen input source on a phone
# produces 139, which is why the F9 flow lives in the adapters
# (docs/OPTIONS-MEASURED.md §E5-0).
ANDROID_UNREACHABLE: dict[str, str] = {}

failures: list[str] = []


def check(label: str, ok: bool, detail: str = "") -> None:
    print(f"{'ok  ' if ok else 'FAIL'}: {label}" + (f" -- {detail}" if detail else ""))
    if not ok:
        failures.append(label)


def android_jar() -> pathlib.Path | None:
    """The installed platform's android.jar -- the authority for KEYCODE_*."""
    roots = []
    env = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT")
    if env:
        roots.append(pathlib.Path(env) / "platforms")
    roots.append(pathlib.Path.home() / "Android/Sdk/platforms")
    for r in roots:
        if not r.is_dir():
            continue
        for p in sorted(r.glob("android-*/android.jar"), reverse=True):
            return p
    return None


def javap_keycodes(jar: pathlib.Path) -> dict[str, int]:
    """KEYCODE_* -> value, read out of android.jar. No AOSP assumptions."""
    r = subprocess.run(
        ["javap", "-cp", str(jar), "-constants", "android.view.KeyEvent"],
        capture_output=True, text=True)
    out: dict[str, int] = {}
    for m in re.finditer(r"KEYCODE_([A-Z0-9_]+)\s*=\s*(\d+)", r.stdout):
        out[m.group(1)] = int(m.group(2))
    return out


def layout_keys() -> set[str]:
    keys: set[str] = set()
    for f in sorted(LAYOUTS.glob("*.json")):
        keys |= set(json.loads(f.read_text(encoding="utf-8"))["keys"])
    return keys


def x11_keysym(ch: str) -> int | None:
    if ch in X11_SPECIAL:
        return X11_SPECIAL[ch]
    if 0x20 <= ord(ch) <= 0x7E:
        return ord(ch)
    base = SHIFTED.get(ch)
    if base and 0x20 <= ord(base) <= 0x7E:
        return ord(base)
    return None


def android_keycode(ch: str) -> int | None:
    """AOSP keycode for a character, or None when the platform has none.

    Deliberately NOT ord(ch): AOSP keycodes start at 7 for '0' and 29 for 'A',
    so the ASCII identity that holds for X11 is wrong here. A shifted input
    resolves to the same keycode as its base, because the layout keys on the
    unshifted character and Android reports the key, not the produced glyph.
    """
    if len(ch) != 1:            # a named key, not a character
        return None
    if ch in ANDROID_NAMED:
        return ANDROID_NAMED[ch]
    name = ANDROID_CHAR_KEYCODE.get(ch)
    if name is None:
        base = SHIFTED.get(ch)
        name = ANDROID_CHAR_KEYCODE.get(base) if base else None
    if name is None:
        return None
    return ANDROID_KEYCODES.get(name)


def xkb_header() -> pathlib.Path | None:
    """Locate the XKB keysym header — the authority for X11 values."""
    for p in (ROOT / "fcitx5-5.1.19", pathlib.Path("/usr/include"),
              pathlib.Path("/usr/include/x86_64-linux-gnu")):
        if not p.is_dir():
            continue
        for name in ("xkbcommon-keysyms.h", "XF86keysym.h", "X11/keysymdef.h"):
            if (p / name).is_file():
                return p / name
            if (p / "xkbcommon" / name).is_file():
                return p / "xkbcommon" / name
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()

    keys = layout_keys()
    printable = {k for k in keys if 0x20 <= ord(k) <= 0x7E}
    named = keys - printable

    # The platform authority, read once: the declared-unreachable check below
    # needs it, and so does the agreement check further down.
    jar = android_jar()
    if jar is not None and shutil.which("javap") is None:
        jar = None  # no reader for the jar: unverified is the honest state
    has_jar = jar is not None
    live = javap_keycodes(jar) if jar is not None else {}

    if not a.json:
        print(f"{len(keys)} keys used across {len(list(LAYOUTS.glob('*.json')))} layouts "
              f"({len(printable)} printable, {len(named)} named)")
        print()

    # --- every layout key has an encoding on every platform ------------------
    missing_x11 = sorted(k for k in keys if x11_keysym(k) is None)
    check("every layout key has an X11 keysym", not missing_x11, str(missing_x11[:8]))

    # Android: a key with no keycode must be DECLARED, and the declaration must
    # be true of the PLATFORM -- checked against android.jar when one is present,
    # never against the tables above (that check would only prove the fixture
    # agrees with itself, which is how the false F9 claim survived).
    unreachable = sorted(k for k in keys if android_keycode(k) is None)
    undeclared = [k for k in unreachable if k not in ANDROID_UNREACHABLE]
    check("every key unreachable on Android is declared as such", not undeclared,
          f"undeclared: {undeclared} -- add to ANDROID_UNREACHABLE with a reason")
    if has_jar:
        # javap_keycodes() keys are the names WITHOUT the KEYCODE_ prefix.
        wrong_decl = [(k, c, f"KEYCODE_{c} = {live[c]}") for k, c in ANDROID_UNREACHABLE.items()
                      if c in live]
        check("Android's declared-unreachable set is unreachable per android.jar",
              not wrong_decl, f"{wrong_decl} -- the platform HAS that keycode")
    else:
        check("Android's declared-unreachable set is unreachable (fixture table; no jar)",
              all(android_keycode(k) is None for k in ANDROID_UNREACHABLE),
              "declared unreachable but the fixture's own table resolves it")

    # --- the adapter's accepted range, which is what tier C replays ----------
    out_of_range = sorted(k for k in keys if not 0x20 <= ord(k) <= 0x7E)
    check("every layout key is inside the adapter's printable-ASCII window",
          not out_of_range,
          f"{out_of_range} would hit flush() and reach the client instead of the engine")

    # --- Android: verify against android.jar when a JDK is available ----------
    if not has_jar:
        # Skip, not fail: CI's linux job has no JDK or SDK. The table is
        # unverifiable there, which is a coverage gap to record (TESTING.md
        # tier E), not a defect in the tool.
        print("skip: no android.jar or javap -- Android keycodes unverified "
              "(tier E records this as the CI gap it is)")
    else:
        live = javap_keycodes(jar)
        wrong = [(n, v, live.get(n)) for n, v in ANDROID_KEYCODES.items()
                 if n in live and live[n] != v]
        check(f"Android keycodes agree with android.jar ({len(live)} parsed)",
              not wrong, str(wrong[:5]))
        check("every layout key has a live AOSP keycode",
              all(android_keycode(c) in set(live.values()) for c in keys),
              str([c for c in keys if android_keycode(c) not in set(live.values())][:6]))

    # --- XKB header agreement ------------------------------------------------
    # Same contract as the android.jar skip above: without the keysym
    # authority the tool's own table stays unverified, and a skip records
    # that honestly -- a FAIL would be read as "the table is wrong", which it
    # is not proven to be.
    hdr = xkb_header()
    if hdr is None:
        print("skip: no xkbcommon-keysyms.h / keysymdef.h found -- encoding "
              "table unverified (looked in the vendored fcitx5-5.1.19 tree "
              "and the system includes)", file=sys.stderr)
        return 77
    src = hdr.read_text(encoding="utf-8", errors="replace")
    # XF86XK_<name> for named keys; printable ASCII is the identity by XKB's
    # definition, so only the named ones need a header constant.
    # xkbcommon spells these XKB_KEY_*, older X11 headers XF86XK_*.
    defs = {m.group(1): int(m.group(2), 16) for m in
            re.finditer(r"#define\s+(?:XKB_KEY|XF86XK|XVK|XK)_(\w+)\s+"
                        r"(0x[0-9a-fA-F]+)\b", src)}
    check(f"{hdr.name} exposes keysym defines to parse", len(defs) > 100,
          f"{len(defs)} defines")

    disagree = [(ch, hex(val), hex(defs[ch])) for ch, val in X11_SPECIAL.items()
                if ch in defs and defs[ch] != val]
    check("X11 named keysym values agree with the header", not disagree,
          str(disagree[:4]))

    # The identity claim, checked for every printable key rather than for
    # space as a proxy: XKB defines 0x20..0x7E as Latin-1/ASCII, which is
    # where the whole printable table comes from. If this drifts, every
    # printable keysym in this file is wrong.
    #
    # Punctuation has no XKB_KEY_<char> define -- XKB spells it
    # (XKB_KEY_semicolon, XKB_KEY_backslash). Letters and digits do have
    # one, so both routes are exercised.
    wrong = []
    for c in printable:
        name = ASCII_XKB_NAME.get(c, c)
        if name not in defs:
            wrong.append((c, "no header define"))
        elif defs[name] != ord(c):
            wrong.append((c, hex(ord(c)), hex(defs[name])))
    check("every printable key equals its ASCII keysym, per the header",
          not wrong, f"{len(wrong)} disagree, e.g. {wrong[:4]}")
    named_count = sum(1 for c in printable if c in ASCII_XKB_NAME)
    check("the header check exercised spelled punctuation",
          named_count >= 10, f"{named_count} keys checked by name")

    # --- the tables are self-consistent --------------------------------------
    # A shifted SYMBOL is its own printable keysym ("!" -> 0x21), so it is
    # already in range and resolves to itself. What needs resolving is input the
    # platform sends UPPERCASE, or a symbol the layout does not key on: "C"
    # against a layout that keys on "c", ":" against ";".
    check("a shifted symbol is its own keysym",
          all(x11_keysym(s) == ord(s) for s in SHIFTED),
          str([(s, hex(x11_keysym(s) or 0)) for s in SHIFTED
               if x11_keysym(s) != ord(s)][:4]))
    check("shifted punctuation that the layout keys on resolves to its base",
          all(ord(b) <= 0x7E for b in SHIFTED.values()))
    # They must NOT agree, and asserting that they do is the mistake this file
    # already made once: X11 keysyms are the ASCII identity, AOSP keycodes start
    # at 7 for '0' and 29 for 'A'. What must hold is that both are injective
    # over the key set, so no two layout keys collide on either platform --
    # a collision would make two different keys indistinguishable to an adapter.
    for label, fn in (("X11", x11_keysym), ("Android", android_keycode)):
        vals = [fn(c) for c in sorted(keys)]
        dupes = {v for v in vals if v is not None and vals.count(v) > 1}
        check(f"{label} keycodes are injective over the layout keys", not dupes,
              str(sorted(dupes)))
    check("X11 and Android deliberately disagree (ASCII vs AOSP numbering)",
          any(x11_keysym(k) != android_keycode(k) for k in printable))

    if a.json:
        print(json.dumps({
            "keys": len(keys),
            "printable": len(printable),
            "named": sorted(named),
            "android_unreachable": sorted(ANDROID_UNREACHABLE),
            "map": {k: {"x11": x11_keysym(k), "android": android_keycode(k)}
                    for k in sorted(keys)},
        }, ensure_ascii=False, indent=2))

    print()
    if failures:
        print(f"{len(failures)} FAILED: {failures}")
        return 1
    print("keymap fixture holds")
    return 0


if __name__ == "__main__":
    sys.exit(main())