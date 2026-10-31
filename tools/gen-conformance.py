#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Generate tier-C conformance traces from tier-A goldens (docs/trace-spec.md).

Tier C reuses tier A's *sequences* but expresses the input in a platform's native
event vocabulary. Generating the corpus from the goldens is what keeps the two
tiers from drifting: a hand-typed corpus would encode a second, unchecked copy of
the same expectations, and the day they disagreed nobody could say which was right.

    python3 tools/gen-conformance.py --golden tests/golden/p2-2-1-basic.jsonl \\
        --platform x11 --layout sin3-p2-basic --case basic-syllables

Expectations are copied verbatim from tier A. This script never computes what the
answer *should* be -- that is tier A's job, and inventing it here would defeat the
purpose of the tier.

Only the keys the layouts actually use are mapped. A key with no known native encoding
is an error, not a skip: silently dropping it would produce a corpus that passes
while testing less than it appears to.
"""
import argparse
import json
import pathlib
import sys

# US-QWERTY character -> X11 keysym. Only printable ASCII plus the control keys the
# goldens use; XKB keysym values for those are their ASCII codes by definition
# (X11 keysyms 0x20-0x7e are the Latin-1/ASCII range), so this is not a table of
# magic numbers -- it is the identity, spelled out so an unmapped key is loud.
ASCII_KEYSYMS = {chr(c): c for c in range(0x20, 0x7F)}

NAMED = {
    "backspace": 0xFF08,
    "return": 0xFF0D,
    "tab": 0xFF09,
    "space": 0x0020,
    "left": 0xFF51,
    "up": 0xFF52,
    "right": 0xFF53,
    "down": 0xFF54,
}

# Android KEYCODE_* for the same set. Only what a Korean layout needs; the point is
# that the mapping is explicit and testable, not that it is complete.
ANDROID_KEYCODES = {
    "backspace": 67, "return": 66, "tab": 61, "space": 62,
    "left": 21, "up": 22, "right": 23, "down": 20,
}
for _c in range(0x20, 0x7F):
    ANDROID_KEYCODES[chr(_c)] = _c  # KEYCODE_A..KEYCODE_Z are their ASCII values


def x11_event(ch: str) -> dict:
    if ch == "\\b":
        return {"keysym": NAMED["backspace"], "mods": 0}
    low = ch.lower()
    if low in NAMED:
        return {"keysym": NAMED[low], "mods": 0}
    if ch in ASCII_KEYSYMS:
        return {"keysym": ASCII_KEYSYMS[ch], "mods": 0}
    raise SystemExit(f"no X11 keysym known for {ch!r} (U+{ord(ch):04X})")


# Win32 virtual-key codes + US-keyboard set-1 scan codes. VK codes are the
# US keyboard's layout-independent codes (VK_A..VK_Z are their uppercase
# ASCII values); the scan code is what the adapter receives in lParam and
# today ignores -- carried anyway so the corpus models a real WM_KEYDOWN.
_WIN32_LETTER_SCAN = {
    "q": 0x10, "w": 0x11, "e": 0x12, "r": 0x13, "t": 0x14, "y": 0x15,
    "u": 0x16, "i": 0x17, "o": 0x18, "p": 0x19,
    "a": 0x1E, "s": 0x1F, "d": 0x20, "f": 0x21, "g": 0x22, "h": 0x23,
    "j": 0x24, "k": 0x25, "l": 0x26,
    "z": 0x2C, "x": 0x2D, "c": 0x2E, "v": 0x2F, "b": 0x30, "n": 0x31, "m": 0x32,
}
_WIN32_DIGIT_SCAN = {"1": 0x02, "2": 0x03, "3": 0x04, "4": 0x05, "5": 0x06,
                     "6": 0x07, "7": 0x08, "8": 0x09, "9": 0x0A, "0": 0x0B}
_WIN32_OEM = {  # char -> (VK, scan); shifted forms share the unshifted VK
    ";": (0xBA, 0x27), "=": (0xBB, 0x0D), ",": (0xBC, 0x33), "-": (0xBD, 0x0C),
    ".": (0xBE, 0x34), "/": (0xBF, 0x35), "`": (0xC0, 0x29), "[": (0xDB, 0x1A),
    "\\": (0xDC, 0x2B), "]": (0xDD, 0x1B), "'": (0xDE, 0x28),
}
_WIN32_VK_NAMED = {
    "backspace": 0x08, "return": 0x0D, "tab": 0x09, "space": 0x20,
    "left": 0x25, "up": 0x26, "right": 0x27, "down": 0x28,
}
_WIN32_SCAN_NAMED = {
    "backspace": 0x0E, "return": 0x1C, "tab": 0x0F, "space": 0x39,
    "left": 0x4B, "up": 0x48, "right": 0x4D, "down": 0x50,
}
# mods bit 0x01 = Shift, i.e. GetKeyState(VK_SHIFT) < 0 at delivery time
# (docs/trace-spec.md). Win32 has no CapsLock encoding here: a caps case is
# refused below, because the adapter models Caps Lock as a mode toggle, not
# as engine input.
WIN32_SHIFT = 0x01


def _win32_vk(ch: str) -> tuple[int, int, int]:
    """(vk, scan, mods) for one ASCII character, or an error."""
    low = ch.lower()
    if low in _WIN32_LETTER_SCAN:
        return (ord(low) - 32, _WIN32_LETTER_SCAN[low], WIN32_SHIFT if ch.isupper() else 0)
    if ch in _WIN32_DIGIT_SCAN:
        return (0x30 + int(ch), _WIN32_DIGIT_SCAN[ch], 0)
    if ch in _WIN32_OEM:
        vk, scan = _WIN32_OEM[ch]
        return (vk, scan, 0)
    base = _SHIFTED.get(ch)  # '!': "1", ':': ";", '|': "\\", ...
    if base:
        vk, scan, _ = _win32_vk(base)
        return (vk, scan, WIN32_SHIFT)
    if ch == " ":
        return (0x20, 0x39, 0)
    raise SystemExit(f"no Win32 VK known for {ch!r} (U+{ord(ch):04X})")


def win32_event(ch: str) -> dict:
    if ch == "\\b":
        return {"vk": _WIN32_VK_NAMED["backspace"], "scan": _WIN32_SCAN_NAMED["backspace"], "mods": 0}
    vk, scan, mods = _win32_vk(ch)
    return {"vk": vk, "scan": scan, "mods": mods}


def android_event(ch: str) -> dict:
    if ch == "\\b":
        return {"keycode": ANDROID_KEYCODES["backspace"], "mods": 0}
    low = ch.lower()
    if low in ANDROID_KEYCODES:
        return {"keycode": ANDROID_KEYCODES[low], "mods": 0}
    raise SystemExit(f"no Android keycode known for {ch!r} (U+{ord(ch):04X})")


_KEYTABLES: dict[str, set] = {}

# US-QWERTY shifted punctuation -> unshifted. Only the pairs a Korean layout can
# reference; a character absent here is simply not in the table either way.
_SHIFTED = {
    "!": "1", "@": "2", "#": "3", "$": "4", "%": "5", "^": "6", "&": "7",
    "*": "8", "(": "9", ")": "0", "_": "-", "+": "=", "{": "[", "}": "]",
    "|": "\\", ":": ";", '"': "'", "<": ",", ">": ".", "?": "/", "~": "`",
}



def _layout_maps(layout: str, ch: str | None) -> bool:
    """True when the layout's key table has an entry for `ch`.

    ch is None for a named key (backspace), which the adapter routes but the
    engine declines on an empty buffer -- so False, i.e. reaches the client.
    """
    if ch is None:
        return False
    if layout not in _KEYTABLES:
        import json as _json

        path = pathlib.Path(__file__).resolve().parents[1] / "libhunmin/data/layouts" / f"{layout}.json"
        if not path.is_file():
            raise SystemExit(f"unknown layout {layout!r} (no {path})")
        _KEYTABLES[layout] = set(_json.loads(path.read_text(encoding="utf-8"))["keys"])
    keys = _KEYTABLES[layout]
    if ch in keys:
        return True
    # Shifted input arrives uppercased ("C" for c, ":" for ;) but the layout
    # table is keyed by the unshifted character -- the engine's shift row is a
    # property of the layout, not a second key. Same normalisation the adapter
    # does before calling hunmin_ic_process().
    if "A" <= ch <= "Z":
        return ch.lower() in keys
    shifted = _SHIFTED.get(ch)
    return shifted in keys if shifted else False


def x11_named(key: str) -> dict:
    if key not in NAMED:
        raise SystemExit(f"no X11 keysym known for named key {key!r}")
    return {"keysym": NAMED[key], "mods": 0}


def win32_named(key: str) -> dict:
    if key not in _WIN32_VK_NAMED:
        raise SystemExit(f"no Win32 VK known for named key {key!r}")
    return {"vk": _WIN32_VK_NAMED[key], "scan": _WIN32_SCAN_NAMED[key], "mods": 0}


def android_named(key: str) -> dict:
    if key not in ANDROID_KEYCODES:
        raise SystemExit(f"no Android keycode known for named key {key!r}")
    return {"keycode": ANDROID_KEYCODES[key], "mods": 0}


NAMED_BUILDERS = {"x11": x11_named, "android": android_named, "win32": win32_named}

BUILDERS = {"x11": x11_event, "android": android_event, "win32": win32_event}


def parse_golden(golden: pathlib.Path, a, build, build_named):
    """One golden -> (cases, unsupported, head, excluded).

    cases are (name, body_lines); each body line carries the platform-native
    event plus the expectation copied verbatim from tier A. `excluded` names the
    cases this platform cannot encode (with the offending state), which the
    caller writes into the trace's meta so the omission is visible in the
    artefact rather than only in a terminal.
    """
    src = golden.read_text(encoding="utf-8").splitlines()
    head = json.loads(src[0])
    if "meta" not in head:
        raise SystemExit(f"{golden}: line 1 is not a meta object")

    # Per-case state that tier A carries in `{"meta": {...}}` lines. These are
    # engine options, not decoration: a caps=1 case and a caps=0 case send the
    # SAME keys and expect different text, so a generator that drops them emits
    # a trace asserting the wrong thing. Each maps to the trace's native
    # vocabulary: caps -> X LockMask, everything else -> hunmin option names,
    # which have no native encoding and therefore must be replayed by the
    # runner rather than smuggled through `mods`.
    #
    # p2-2-4-3-caps.jsonl is the corpus that exercises this; a case with any of
    # these keys is refused below unless the caller opted in, because the
    # current runner cannot reproduce a hunmin-option case from a trace alone.
    OPTION_KEYS = ("caps", "w", "W", "z", "h", "H", "mode")
    X_LOCK_MASK = 0x02

    current: dict = {}
    unsupported: set[str] = set()
    cases: list[tuple[str, list[str]]] = []
    excluded: list[dict] = []  # cases this platform cannot encode, and why
    body: list[str] = []
    name = "case"
    refused = ""  # option state that makes the CURRENT case unencodable here
    previous_preedit = ""
    for line in src[1:]:
        rec = json.loads(line)
        if "meta" in rec and "case" in rec["meta"]:  # case separator
            if refused:
                excluded.append({"case": name, "state": [refused]})
            elif body:
                cases.append((name, body))
            body = []
            previous_preedit = ""
            refused = ""
            name = rec["meta"]["case"]
            # per-case engine state, minus the label itself
            current = {k: v for k, v in rec["meta"].items() if k != "case"}
            for k in current:
                if k in OPTION_KEYS:
                    unsupported.add(k)
            # win32 has no caps encoding: its adapter models the key as a mode
            # toggle, so a trace cannot say "this case ran with Caps Lock on".
            # Emitting the case at mods=0 anyway asserts the caps-OFF text against
            # a caps-ON expectation -- measured on manual-sin3-p2-yet's `k/`
            # case, which fails at its second key: the adapter composes 고 where
            # tier A recorded commit ㄱ + preedit ㅋ. So the case is EXCLUDED and
            # the exclusion is written into the emitted meta, rather than
            # silently emitting a trace that asserts the wrong thing.
            if a.platform == "win32" and current.get("caps"):
                refused = "caps"
                unsupported.discard("caps")
            continue
        # tier A writes inputs two ways: {"ch": "q"} for a character and
        # {"key": "backspace"} for a named key (docs/SPEC-SCHEMA.md "Reference
        # trace format"). Only "backspace" occurs in the shipped corpora, but a
        # generator that assumed "ch" would raise KeyError on the other 30 of
        # every ref-* file rather than doing the obvious thing.
        spec = rec["in"]
        named = spec.get("key")
        event = build_named(named) if named else build(spec["ch"])
        # `consumed` means the engine took the key, so it is a property of the
        # LAYOUT, not of this file: the adapter consumes exactly when
        # hunmin_ic_process() returns true, and that happens only for a
        # character the layout's key table maps (FRONTEND-CONTRACT.md
        # invariant 3). tier A cannot supply it -- it records only commit and
        # preedit -- so it is read from the same table the engine reads.
        #
        # Two earlier versions guessed wrong, and the .in/.gold consistency
        # check in trace-check.py caught both: one exempted space and named
        # keys, the next exempted nothing because the adapter briefly consumed
        # everything (A11, since reverted). A guess here is a silent lie, so
        # the table is consulted instead.
        if named:
            # Backspace is consumed only when it has something to delete:
            # hunmin_ic_process() pops from a non-empty buffer and declines
            # otherwise, so an empty buffer passes it to the client. tier A
            # records the buffer state, so the rule reads it off the PREVIOUS
            # line's preedit rather than guessing.
            consumed = bool(previous_preedit)
        else:
            consumed = _layout_maps(a.layout, spec["ch"])
        if a.platform == "x11" and current.get("caps"):
            # X11 is the only platform that encodes Caps Lock in `mods`
            # (LockMask). Android applies the same bit below for historical
            # corpus; win32 has no encoding at all, so a caps case is excluded
            # at the separator above rather than emitted at mods=0.
            event["mods"] = event.get("mods", 0) | X_LOCK_MASK
        previous_preedit = rec.get("preedit", "")
        line = json.dumps(
                {
                    "in": event,
                    "commit": rec.get("commit", ""),
                    "preedit": rec.get("preedit", ""),
                    "consumed": consumed,
                },
                ensure_ascii=False,
            )
        if refused:
            continue  # excluded case: its events are not emitted at all
        body.append(line)

    if refused:
        excluded.append({"case": name, "state": [refused]})
    elif body:
        cases.append((name, body))
    unsupported -= {"caps"}  # encoded in x11 mods; win32 excludes the case instead
    return cases, unsupported, head, excluded


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--golden", required=True, type=pathlib.Path)
    ap.add_argument("--extra-golden", action="append", type=pathlib.Path, default=[],
                    help="further goldens whose cases join the SAME output; "
                         "requires --batch")
    ap.add_argument("--platform", required=True, choices=sorted(BUILDERS))
    ap.add_argument("--layout", required=True)
    ap.add_argument("--case", required=True)
    ap.add_argument("--out", required=True, type=pathlib.Path)
    ap.add_argument("--split-cases", action="store_true",
                    help="one trace per tier A case (default when a golden has cases)")
    ap.add_argument("--batch", action="store_true",
                    help="one trace file carrying every case as an in-file "
                         '{"meta": {"case": ...}} separator; the runner must '
                         "reset the adapter at each separator (docs/trace-spec.md "
                         '"Multi-case batch files"). For corpora that would '
                         "otherwise flood ctest with one test per 2-event case.")
    a = ap.parse_args()

    if a.extra_golden and not a.batch:
        raise SystemExit("--extra-golden requires --batch: merged cases must "
                         "land in one file, not be re-split")

    build = BUILDERS[a.platform]
    build_named = NAMED_BUILDERS[a.platform]

    all_cases, unsupported, head, excluded = parse_golden(a.golden, a, build, build_named)
    for extra in a.extra_golden:
        cases_e, unsupported_e, _, excluded_e = parse_golden(extra, a, build, build_named)
        all_cases += cases_e
        unsupported |= unsupported_e
        excluded += excluded_e
    cases = all_cases

    meta = {
        "platform": a.platform,
        "layout": a.layout,
        "case": a.case,
        "source": f"{a.golden.name} ({head['meta'].get('source', 'gate')})",
    }
    if excluded:
        # Provenance for what this trace does NOT cover. A corpus without it
        # reads as "every tier-A case of this golden is replayed here", which is
        # the silent omission AGENTS.md's provenance rule exists to prevent.
        meta["excluded"] = excluded
    out_lines = [json.dumps({"meta": meta}, ensure_ascii=False)]
    if excluded:
        print(f"{a.out}: excluded {len(excluded)} case(s) with no {a.platform} "
              "encoding: "
              + ", ".join(f"{e['case']} ({'+'.join(e['state'])})" for e in excluded),
              file=sys.stderr)

    if unsupported:
        raise SystemExit(
            f"{a.golden}: case state {sorted(unsupported)} has no "
            "native encoding in the trace format; replaying it needs an engine-"
            "option channel that tools/trace-check.py cannot yet drive. Refusing "
            "to emit a trace that would assert the wrong expectation."
        )

    # A golden's {"meta": {"case": ...}} lines separate INDEPENDENT cases: each
    # starts from a clean engine (docs/SPEC-P2.md "P2 gate corpora" -- a case is
    # replayed in isolation). Emitting them as one stream carries case N's
    # composition into case N+1, which is not what tier A recorded. So a
    # multi-case golden becomes one trace file per case, each replayed from a
    # fresh context -- the same isolation tier A has -- unless --batch is
    # given, which keeps the separators in the file and pushes the per-case
    # reset onto the runner (trace-check skips meta lines on both sides of the
    # diff, so a batch file needs no runner-side change to stay comparable).
    a.out.parent.mkdir(parents=True, exist_ok=True)
    if a.batch:
        batch = [out_lines[0]]
        for i, (name, body) in enumerate(cases, 1):
            batch.append(json.dumps({"meta": {"case": name, "case_index": i}},
                                    ensure_ascii=False))
            batch.extend(body)
        a.out.write_text("\n".join(batch) + "\n", encoding="utf-8")
        print(f"{a.out}: {len(cases)} cases, "
              f"{sum(len(b) for _, b in cases)} events", file=sys.stderr)
        return 0

    if len(cases) > 1 or (a.split_cases and cases):
        if not cases:
            print(f"{a.golden}: no cases to split", file=sys.stderr)
            return 1
        stem = (a.out.name[:-len(".trace.in")] if a.out.name.endswith(".trace.in")
                else a.out.stem)
        # Case labels are Hangul ("가", "나") and may contain ';' (a key that is
        # part of a tier id). Neither survives a CMake test name: Hangul
        # sanitises to the same placeholder for every case and collides, and a
        # ';' splits CMake's list. So the label stays in `meta.case` for humans
        # and the FILENAME gets a stable ASCII index instead. Deterministic
        # because cases are emitted in file order.
        for i, (name, body) in enumerate(cases, 1):
            f = a.out.with_name(f"{stem}-case{i:02d}.trace.in")
            # manual-sin3-p2-yet.jsonl has two cases both labelled "k/" -- one
            # caps:0, one caps:1 -- so a label alone does not identify a case.
            # case_index is authoritative; the label is for humans.
            head = json.dumps(
                {"meta": {**json.loads(out_lines[0])["meta"], "case": name,
                          "case_index": i, "case_id": f"{a.case}#{i:02d}"}},
                ensure_ascii=False)
            f.write_text("\n".join([head] + body) + "\n", encoding="utf-8")
            print(f"{f}: {len(body)} events", file=sys.stderr)
        return 0

    # Single-case golden: the events live in `body`, not `out_lines` (which
    # holds only the header). Writing out_lines here produced a header-only file,
    # i.e. a trace that asserts nothing.
    a.out.write_text("\n".join([out_lines[0]] + cases[0][1]) + "\n", encoding="utf-8")
    print(f"{a.out}: {len(cases[0][1])} events", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
