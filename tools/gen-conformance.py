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
try:  # issue #272 phase 2: one ROOT derivation, with a file-relative fallback
    from _common import ROOT as _COMMON_ROOT
except ImportError:  # run from outside tools/: derive from this file
    _COMMON_ROOT = __import__("pathlib").Path(__file__).resolve().parent.parent

# US-QWERTY character -> X11 keysym. Only printable ASCII plus the control keys the
# goldens use; XKB keysym values for those are their ASCII codes by definition
# (X11 keysyms 0x20-0x7e are the Latin-1/ASCII range), so this is not a table of
# magic numbers -- it is the identity, spelled out so an unmapped key is loud.
ASCII_KEYSYMS = {chr(c): c for c in range(0x20, 0x7F)}

# A BackSpace can reach this file three ways, and all three mean the same key:
#   {"key": "backspace"}   the reference-trace spelling (docs/SPEC-SCHEMA.md)
#   {"ch": "\\b"}          the two-character escape of the `hunmin-sim --keys`
#                          alphabet (tools/sim.c keys_step)
#   {"ch": "\b"}           what `hunmin-sim --trace` actually writes (U+0008)
# Only the `key` form was handled, so a golden the engine's own writer produced
# failed with "no X11 keysym known for '\x08'" -- measured 2026-11-03 while
# adding the empty-buffer BackSpace to the designed cases (issue #229). Accepting
# the byte tier A writes is not a convenience: every trace is generated from a
# golden, and that is how a golden spells this key.
BACKSPACE_SPELLINGS = ("\\b", "\b")

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
    if ch in BACKSPACE_SPELLINGS:
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
# mods bit 0x01 = Shift, i.e. GetKeyState(VK_SHIFT) < 0 at delivery time;
# bit 0x02 = CapsLock, i.e. GetKeyState(VK_CAPITAL) toggled on (docs/trace-spec.md).
WIN32_SHIFT = 0x01
WIN32_CAPS_LOCK = 0x02


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
    if ch in BACKSPACE_SPELLINGS:
        return {"vk": _WIN32_VK_NAMED["backspace"], "scan": _WIN32_SCAN_NAMED["backspace"], "mods": 0}
    vk, scan, mods = _win32_vk(ch)
    return {"vk": vk, "scan": scan, "mods": mods}


def android_event(ch: str) -> dict:
    """One Android event: the character's OWN code, not its lowercase.

    The engine derives Shift from the key id's case (`hunmin_ic_process("K")` is
    Shift+k), and a tier-A golden writes a shifted key that way, so lowering the
    character here replayed the unshifted key -- measured on
    tests/golden/cultural-pangram-*.jsonl, whose second line is the same pangram
    in capitals and whose android trace asserted `t` where tier A says `T`.
    `shift_char` above resolves a `"shift": true` record to its character before
    it reaches this function, so a shifted SYMBOL arrives already spelled.
    """
    if ch in BACKSPACE_SPELLINGS:
        return {"keycode": ANDROID_KEYCODES["backspace"], "mods": 0}
    if ch in ANDROID_KEYCODES:
        return {"keycode": ANDROID_KEYCODES[ch], "mods": 0}
    raise SystemExit(f"no Android keycode known for {ch!r} (U+{ord(ch):04X})")


_KEYTABLES: dict[str, set] = {}

# US-QWERTY shifted punctuation -> unshifted. Only the pairs a Korean layout can
# reference; a character absent here is simply not in the table either way.
_SHIFTED = {
    "!": "1", "@": "2", "#": "3", "$": "4", "%": "5", "^": "6", "&": "7",
    "*": "8", "(": "9", ")": "0", "_": "-", "+": "=", "{": "[", "}": "]",
    "|": "\\", ":": ";", '"': "'", "<": ",", ">": ".", "?": "/", "~": "`",
}
_UNSHIFTED = {v: k for k, v in _SHIFTED.items()}


def shift_char(ch: str) -> str:
    """What this character's key spells with Shift held.

    tier A may write a shifted key either as the resolved character (the
    corpus convention: `"X"`, `"!"`) or as the base character plus
    `"shift": true`, which is what `hunmin-sim --trace` records for a `\\s` key
    and what docs/SPEC-SCHEMA.md documents. A native event carries the resolved
    character either way -- `FcitxKey_A`, not `a` with a modifier -- so the
    conversion belongs here, once. A converter that ignored the field replayed
    the base key and measured the wrong thing (measured 2026-11-03).
    """
    if "a" <= ch <= "z":
        return ch.upper()
    return _UNSHIFTED.get(ch, ch)



def _layout_maps(layout: str, ch: str | None) -> bool:
    """True when the layout's key table has an entry for `ch`.

    ch is None for a named key (backspace), which the adapter routes but the
    engine declines on an empty buffer -- so False, i.e. reaches the client.
    """
    if ch is None:
        return False
    if layout not in _KEYTABLES:
        import json as _json

        path = _COMMON_ROOT / "libhunmin/data/layouts" / f"{layout}.json"
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

# The delivery policy a case must be replayed under for tier A's shape to be
# observable (docs/trace-spec.md "meta"). A golden commits a syllable when the
# engine finalises it, i.e. on a keypress it ALSO consumed; a word-buffered
# adapter holds that text until a declined key or teardown. Both are correct --
# the policy is the user's choice -- but only PER_SYLLABLE delivers at the point
# tier A recorded. So a case with a mid-stream commit names the policy instead
# of being silently dropped: dropping it is the drift issue #187 measured
# (13 cases the win32 corpus replayed and the x11 corpus did not).
COMMIT_MODE_FOR_MID_STREAM = "syllable"


def parse_golden(golden: pathlib.Path, a, build, build_named):
    """One golden -> (cases, unsupported, head, excluded).

    cases are (name, body_lines, mid_stream_commit); each body line carries the
    platform-native event plus the expectation copied verbatim from tier A.
    `mid_stream_commit` is True when tier A commits while the key was still
    consumed -- the one shape whose delivery point a word-buffered adapter
    cannot reproduce (see COMMIT_MODE below). `excluded` names the cases this
    platform cannot encode (with the offending state), which the caller writes
    into the trace's meta so the omission is visible in the artefact rather
    than only in a terminal.
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
    OPTION_KEYS = ("w", "W", "z", "h", "H")
    X_LOCK_MASK = 0x02

    current: dict = {}
    unsupported: set[str] = set()
    # (name, body, total, mid_stream_commit). `total` is the case's cumulative
    # delivered text for --delivery's word-buffered model (#220); the flag is
    # #187's: a commit on a key the case also consumed, which a word-buffered
    # adapter can only reproduce under HUNMIN_COMMIT_PER_SYLLABLE. They are
    # alternative answers to the same question -- name the model, or switch the
    # policy -- so a trace carries at most one (see the writer below).
    cases: list[tuple[str, list[str], str, bool, dict]] = []
    excluded: list[dict] = []  # cases this platform cannot encode, and why
    body: list[str] = []
    # True once this case has committed a syllable on a key it also consumed:
    # that is a delivery point AFTER the keypress but BEFORE the word boundary,
    # so a word-buffered adapter puts the same text in the client later. The
    # expectations stay tier A's either way; what changes is which delivery
    # policy the trace must be replayed under to see them.
    mid_stream_commit = False
    name = "case"
    refused = ""  # option state that makes the CURRENT case unencodable here
    previous_preedit = ""
    # Cumulative delivery (docs/trace-spec.md "Word-buffered adapters"): the
    # text the case produces in total -- every tier-A commit plus the preedit
    # still composing when the keys stop. Computed here and written into the
    # trace's meta so tools/trace-check.py can compare it against the runner's
    # own sum instead of trusting a number somebody typed.
    cumulative_commits = ""
    last_preedit = ""
    for line in src[1:]:
        rec = json.loads(line)
        if "meta" in rec and "case" in rec["meta"]:  # case separator
            if refused:
                excluded.append({"case": name, "state": [refused]})
            elif body:
                _opts = {k: v for k, v in current.items() if k in OPTION_KEYS}
                cases.append((name, body, cumulative_commits + last_preedit,
                              mid_stream_commit, _opts))
            body = []
            cumulative_commits = ""
            last_preedit = ""
            mid_stream_commit = False
            previous_preedit = ""
            refused = ""
            name = rec["meta"]["case"]
            # per-case engine state, minus the label itself
            current = {k: v for k, v in rec["meta"].items() if k != "case"}
            # Engine-option flags (w/W/z/h/H) are no longer refused:
            # they are carried in the case separator's `options` field and
            # applied by the runner via hunmin_buffered_preedit_set_option.
            # Issue #184.
            # A caps case is encodable on every platform that carries the
            # bit in `mods` (x11 LockMask, win32 0x02); it is applied per
            # event in the emit loop below. Issue #184.
            continue
        # tier A writes inputs two ways: {"ch": "q"} for a character and
        # {"key": "backspace"} for a named key (docs/SPEC-SCHEMA.md "Reference
        # trace format"). Only "backspace" occurs in the shipped corpora, but a
        # generator that assumed "ch" would raise KeyError on the other 30 of
        # every ref-* file rather than doing the obvious thing.
        spec = rec["in"]
        named = spec.get("key")
        # The other spelling of the same key: the engine's own writer records a
        # BackSpace as `{"ch": "\u0008"}`, not as `{"key": "backspace"}`
        # (BACKSPACE_SPELLINGS). Normalising it here means ONE rule below decides
        # its consumption, and `spec["ch"]` is never read for a named key -- an
        # unconditional read raised KeyError on the ref-* corpora that spell
        # backspace the named way.
        if named is None and spec.get("ch") in BACKSPACE_SPELLINGS:
            named = "backspace"
        # `consumed` means the engine took the key, so it is a property of the
        # LAYOUT for a character and of the BUFFER for BackSpace: the adapter
        # consumes exactly when the layer returns true (DESIGN-FRONTEND.md Part B
        # invariant 3), and tier A cannot supply that -- it records only commit
        # and preedit. Both branches derive it below, never from a guess.
        #
        # Two earlier versions guessed wrong, and the .in/.gold consistency
        # check in trace-check.py caught both: one exempted space and named
        # keys, the next exempted nothing because the adapter briefly consumed
        # everything (A11, since reverted). A guess here is a silent lie, so
        # the table is consulted instead.
        if named:
            event = build_named(named)
            # A NAMED key is a boundary, not composition input: the shared
            # layer classifies every non-text key as HUNMIN_KEY_BOUNDARY_PASS
            # (libhunmin/src/buffered_preedit.c, "a named key with no list open
            # is a boundary"), so the adapter declines it and the client
            # receives it. BackSpace is the one exception -- it is TEXT with
            # base 0x08 and is consumed exactly when the buffer has something to
            # delete, which the previous line's preedit records. That is
            # trace-spec.md invariant 4: `consumed` is false exactly when the
            # key is unmapped, a chord, or an empty buffer.
            #
            # This read `bool(previous_preedit)` for EVERY named key, which
            # labels a declined space or arrow as consumed whenever a syllable
            # is in flight (measured 2026-11-03: the space after 안녕 has preedit
            # 녕 on the line above and the adapter declines it). The .in/.gold
            # check below would have caught it, but a generator should not emit
            # a wrong expectation for the checker to find.
            consumed = named == "backspace" and bool(previous_preedit)
        else:
            # A shifted key may arrive as the base character plus "shift": true;
            # the native event must carry the resolved character (see
            # shift_char). The tier-A text is the same either way.
            spec_ch = shift_char(spec["ch"]) if spec.get("shift") else spec["ch"]
            event = build(spec_ch)
            consumed = _layout_maps(a.layout, spec_ch)
        if current.get("caps") and a.platform in ("x11", "win32"):
            # x11 LockMask / win32 mods 0x02 (docs/trace-spec.md). A caps
            # case sets the bit on every event of the case, matching the
            # shipped TIP, which feeds the engine caps option per event.
            mask = X_LOCK_MASK if a.platform == "x11" else WIN32_CAPS_LOCK
            event["mods"] = event.get("mods", 0) | mask
        previous_preedit = rec.get("preedit", "")
        cumulative_commits += rec.get("commit", "")
        last_preedit = rec.get("preedit", "")
        line = json.dumps(
                {
                    "in": event,
                    "commit": rec.get("commit", ""),
                    "preedit": rec.get("preedit", ""),
                    "consumed": consumed,
                },
                ensure_ascii=False,
            )
        if consumed and rec.get("commit"):
            mid_stream_commit = True
        if refused:
            continue  # excluded case: its events are not emitted at all
        body.append(line)

    if refused:
        excluded.append({"case": name, "state": [refused]})
    elif body:
        _opts = {k: v for k, v in current.items() if k in OPTION_KEYS}
        cases.append((name, body, cumulative_commits + last_preedit, mid_stream_commit, _opts))
    unsupported -= {"caps"}  # encoded in x11/win32 mods
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
    ap.add_argument("--only-case", action="append", default=[],
                    metavar="LABEL",
                    help="emit only the named tier A case(s); repeatable. A label "
                         "that matches no case is an error, so a corpus cannot "
                         "silently lose the case it was meant to add.")
    ap.add_argument("--delivery", choices=["word-buffered"],
                    help="the frontend buffers a whole eojeol and delivers it at "
                         "a boundary, so tier A's per-key expectations cannot be "
                         "copied verbatim (docs/trace-spec.md \"Word-buffered "
                         "adapters\"). Writes meta.delivery with the case's "
                         "cumulative text, computed from the golden. x11 only: "
                         "the android emitter echoes the trace and win32 has its "
                         "own delivery-mode channel.")
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

    if a.delivery and a.platform != "x11":
        raise SystemExit(f"--delivery {a.delivery}: the projection is the fcitx5 "
                         "adapter's word policy; the android emitter echoes the "
                         "trace it replays and win32 carries its own --delivery channel")

    build = BUILDERS[a.platform]
    build_named = NAMED_BUILDERS[a.platform]

    all_cases, unsupported, head, excluded = parse_golden(a.golden, a, build, build_named)
    for extra in a.extra_golden:
        cases_e, unsupported_e, _, excluded_e = parse_golden(extra, a, build, build_named)
        all_cases += cases_e
        unsupported |= unsupported_e
        excluded += excluded_e
    cases = all_cases

    if a.only_case:
        seen = {name for name, *_ in cases}
        missing = [c for c in a.only_case if c not in seen]
        if missing:
            raise SystemExit(f"{a.golden}: --only-case named no case: {missing} "
                             f"(the golden carries {sorted(seen)})")
        cases = [c for c in cases if c[0] in set(a.only_case)]

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

    # All OPTION_KEYS are now carried in the trace's case separator as an
    # `options` field and applied by the runner, so `unsupported` should be
    # empty. Keep the guard for genuinely unknown keys (mode, autoswitch, ...).
    if unsupported:
        raise SystemExit(
            f"{a.golden}: case state {sorted(unsupported)} has no "
            "native encoding in the trace format."
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
        if a.delivery:
            raise SystemExit("--batch and --delivery do not combine: no x11 "
                             "emitter resets the adapter at a case separator, so a "
                             "batched word-buffered trace would carry one word into "
                             "the next (docs/trace-spec.md \"Multi-case batch files\")")
        # A batch file cannot carry a per-case policy, and it does not need one:
        # its runner (frontends/tsf/test/trace-emit.cpp) measures the committed
        # text and compares it cumulatively, so a mid-stream commit is observed
        # wherever the adapter chooses to deliver it. Say so in the terminal
        # rather than in a field the dialect does not define.
        needs = [name for name, _, _, mid, _ in cases if mid]
        if needs:
            print(f"{a.out}: {len(needs)} case(s) commit mid-stream ({', '.join(needs)}); "
                  "a batch trace leaves the delivery point to the runner",
                  file=sys.stderr)
        batch = [out_lines[0]]
        for i, (name, body, _, _, opts) in enumerate(cases, 1):
            _sep = {"case": name, "case_index": i}
            if opts:
                _sep["options"] = opts
            batch.append(json.dumps({"meta": _sep}, ensure_ascii=False))
            batch.extend(body)
        a.out.write_text("\n".join(batch) + "\n", encoding="utf-8")
        print(f"{a.out}: {len(cases)} cases, "
              f"{sum(len(b) for _, b, _, _, _ in cases)} events", file=sys.stderr)
        return 0

    def delivery_meta(total: str) -> dict:
        """What the case delivers in total, for tools/trace-check.py to check.

        A word-buffered adapter delivers a whole eojeol at the boundary, so no
        per-event `commit` of tier A is what the client receives. The total is
        the one thing both models must agree on, and trace-check compares the
        runner's own sum against it.
        """
        return {"delivery": {"model": a.delivery, "total": total}} if a.delivery else {}

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
        for i, (name, body, total, mid_stream_commit, opts) in enumerate(cases, 1):
            f = a.out.with_name(f"{stem}-case{i:02d}.trace.in")
            # manual-sin3-p2-yet.jsonl has two cases both labelled "k/" -- one
            # caps:0, one caps:1 -- so a label alone does not identify a case.
            # case_index is authoritative; the label is for humans.
            case_meta = {**json.loads(out_lines[0])["meta"], "case": name,
                         "case_index": i, "case_id": f"{a.case}#{i:02d}",
                         **({"options": opts} if opts else {}),
                         **delivery_meta(total)}
            # One answer per trace: naming the delivery model and switching the
            # addon's policy would assert two different things about one run.
            if mid_stream_commit and not a.delivery:
                case_meta["commit_mode"] = COMMIT_MODE_FOR_MID_STREAM
            head = json.dumps({"meta": case_meta}, ensure_ascii=False)
            f.write_text("\n".join([head] + body) + "\n", encoding="utf-8")
            print(f"{f}: {len(body)} events", file=sys.stderr)
        return 0

    # Single-case golden: the events live in `body`, not `out_lines` (which
    # holds only the header). Writing out_lines here produced a header-only file,
    # i.e. a trace that asserts nothing.
    name, body, total, mid_stream_commit, opts = cases[0]
    head = {**json.loads(out_lines[0])["meta"],
            **({"options": opts} if opts else {}),
            **delivery_meta(total)}
    if body and mid_stream_commit and not a.delivery:
        head["commit_mode"] = COMMIT_MODE_FOR_MID_STREAM
    a.out.write_text("\n".join([json.dumps({"meta": head}, ensure_ascii=False)] + body) + "\n",
                     encoding="utf-8")
    print(f"{a.out}: {len(body)} events", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
