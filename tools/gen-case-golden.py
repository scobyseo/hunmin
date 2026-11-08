#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 hunmin contributors
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Generate tests/golden/case-<layout>.jsonl: designed cases for one layout.

The `ref-*.jsonl` corpora are parity walks — several hundred seeded events
replayed against libhangul. They catch a data change, but they cannot say WHAT
they assert: a diff in 400 random lines says "something moved", not "the number
row stopped being jongseong jamo".

This generator writes the other kind, for layouts whose identity is an ARRANGEMENT
rather than a word list: `세벌식 최종` (sebeolsik-final, alias 3f), `두벌식`
(dubeolsik, alias 2) and `신세벌식 P2` (sin3-p2, normative pat.im/1136). The
hand-authored half is the INPUT — a case name, the feature it pins, and the keys,
in the same `hunmin-sim --keys` alphabet the other tier-A goldens use (an
uppercase letter or a shifted symbol is that key with Shift held, so `!` is
Shift+1). The EXPECTED half — the commit and preedit after every key — is read
back from the engine. `HEAD_SPEC` names the provenance each layout's `meta`
carries: two-beolsik ships beside libhangul, P2 is normative from pat.im.

Schema: the existing tier-A one, so `hunmin-sim --check` runs it unchanged.

    python3 tools/gen-case-golden.py --sim build/hunmin-sim            # rewrite
    python3 tools/gen-case-golden.py --sim build/hunmin-sim --check

--check regenerates in memory and fails on drift; the suite runs the checker, the
generator run is a review event (never a build step).
"""

import argparse
import json
import os
import subprocess
import sys

try:
    from _common import ROOT as _ROOT
    REPO = str(_ROOT)
except ImportError:  # run from elsewhere: derive from this file
    REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(REPO, "libhunmin", "data")

# The layout's defining facts, one case each. Keys only — no expectations here.
#   세벌식 최종 (3f) in one paragraph, because every case below is a piece of it:
#   choseong, jungseong and jongseong sit on separate keys, so a syllable is
#   assembled from three keystrokes; the UNSHIFTED number row carries jongseong
#   jamo (1=ᇂ 2=ᆻ 3=ᆸ …) and its shifted row carries the 된소리/겹받침 forms
#   (!=ᆩ @=ᆰ #=ᆽ $=ᆵ %=ᆴ); a LITERAL digit is Shift on the letter rows
#   (H=0 J=1 K=2 L=3, Y=5 U=6 I=7 O=8 P=9, :=4); the remaining shifted keys are
#   `=` literals (~ from Shift+0, " from Shift+H); and the layout composes modern
#   Hangul only, so a jamo pair with no precomposed form renders with the
#   conjoining filler instead of a syllable.
CASES = {
    "sebeolsik-final": [
        dict(case="word-annyeong", feature="three-key syllable assembly",
             keys="jfshea ", note="안녕: ᄋ ᅡ ᆫ + ᄂ ᅧ ᆼ, six keys, each jamo its own key"),
        dict(case="word-hangeul", feature="three-key syllable assembly",
             keys="mfskgw ", note="한글: ᄒ ᅡ ᆫ + ᄀ ᅳ ᆯ"),
        dict(case="gyeopbatchim-gap", feature="겹받침 on a shifted letter",
             keys="kfX ", note="값: ᆹ is Shift+x, the jongseong a 두벌식 keyboard has no key for"),
        dict(case="gyeopbatchim-manh", feature="겹받침 on a shifted letter",
             keys="ifS ", note="많: ᆭ is Shift+s"),
        dict(case="digit-row-is-jamo", feature="the number row is jongseong jamo",
             keys="1234567890 ",
             note="1..0 are ᇂ ᆻ ᆸ ᅭ ᅲ ᅣ ᅨ ᅴ ᅮ ᄏ: this row is why ReqMode6 exists (SPEC-MODES §ReqMode6)"),
        dict(case="shifted-digit-row", feature="된소리/겹받침 on the shifted number row",
             keys="!@#$% ", note="ᆩ ᆰ ᆽ ᆵ ᆴ: the shifted number row, where the final consonants live"),
        dict(case="literal-digits-on-the-letter-row", feature="literal digits are Shift+letter",
             keys="JKLH ", note="1 2 3 0: a person typing digits uses the shifted letter rows, not the number row"),
        dict(case="no-precomposed-form", feature="a jamo pair with no syllable",
             keys="ks ", note="초성 ᄀ + 종성 ᆫ and no 중성: rendered with the conjoining filler, not a syllable"),
        dict(case="symbol-ends-the-word", feature="a symbol is a boundary",
             keys="kf.", note="가 then '.', which the engine commits and the layout spells as itself"),
        dict(case="shifted-punctuation-is-literal", feature="`=` literals on the shifted keys",
             keys=":M)", note="Shift+; = '4', Shift+h = '\"', Shift+0 = '~': the shifted keys commit text"),
        # Issue #229: the number row in full, and the declined key after the
        # word. `digit-row-is-jamo` above presses 1..0 in this layout's own
        # order; this one presses every digit in ROW order so the sweep is the
        # same key string on every layout, then ends the word and BackSpaces
        # into an empty buffer, which the adapter must decline.
        dict(case="digit-row-and-empty-backspace", feature="the number row, and the declined key after the word",
             keys="0123456789 \\b",
             note="0-9 are ᄏ ᇂ ᆻ ᆸ ᅭ ᅲ ᅣ ᅨ ᅴ ᅮ in ROW order (ReqMode6: this row is jongseong jamo, typed without a "
                  "Shift); the space delivers the composed jamo and the BackSpace after it finds an empty "
                  "buffer and is declined, so the client receives it"),
    ],
    # 두벌식 (2). Its identity is the opposite arrangement from 세벌식: one jamo per
    # key, consonants on the left and vowels on the right, Shift turning a
    # consonant into its 된소리 and a vowel into ㅒ/ㅖ, no 겹받침 key (값 is ㄱㅏㅂㅅ
    # in sequence), and — the part no other family states — the number row and the
    # punctuation are NOT the layout's: it maps 26 letters and nothing else, so a
    # digit or symbol is declined and reaches the client untouched.
    "dubeolsik": [
        dict(case="word-annyeong", feature="one jamo per key, two syllables",
             keys="dkssud ", note="안녕: ᄋ ᅡ ᆫ + ᄂ ᅧ ᆼ, alternating consonant and vowel keys"),
        dict(case="word-hangeul", feature="one jamo per key, two syllables",
             keys="gksrmf ", note="한글: ᄒ ᅡ ᆫ + ᄀ ᅳ ᆯ"),
        dict(case="gyeopbatchim-from-two-keys", feature="겹받침 needs two keys",
             keys="rkqt ", note="값: ᆹ is ᄇ then ᄉ in the jong slot -- a 두벌식 keyboard has no 겹받침 key, "
                               "which is why 세벌식 최종 needs one"),
        dict(case="shift-is-doen-sori", feature="a shifted consonant is its 된소리",
             keys="RkQkTkWkEk ", note="까 빠 싸 짜 따: the five doubled consonants are the Shift of five keys"),
        dict(case="shift-is-yae-ye", feature="a shifted vowel is ㅒ/ㅖ",
             keys="dOdP ", note="얘 예: only two of the vowels differ under Shift"),
        dict(case="number-row-is-not-mapped", feature="the number row is not the layout's",
             keys="1230", note="두벌식 maps 26 letters and no digit, so 1 2 3 0 are DECLINED and reach the "
                               "client: the pass-through half of the contract, on a character rather than a space"),
        dict(case="shifted-number-is-not-mapped", feature="a shifted digit is not the layout's",
             keys="!@#$", note="Shift+1..4 = ! @ # $: declined for the same reason, and the client receives them"),
        dict(case="symbol-ends-the-word", feature="a symbol is a boundary",
             keys="rk.", note="가 then '.': the engine commits 가 and the layout does not own the period"),
        dict(case="no-precomposed-form", feature="a jamo pair with no syllable",
             keys="rs", note="초성 ᄀ then 종성 ᆫ with no 중성: committed as jamo, not composed"),
        dict(case="sentence-annyeonghaseyo", feature="real text, five syllables",
             keys="dkssudgktpdy ", note="안녕하세요: the everyday sentence the layout exists for, one jamo per key"),
        dict(case="sentence-hangeulimnida", feature="real text, five syllables",
             keys="gksrmfdlqslek ", note="한글입니다: 겹받침 in the middle (입니다 has ᆸ then ᆫ), no special key"),
        # Issue #229. Two halves of the frontend contract the family above does
        # not press: the number row (every digit, not the 1 2 3 0 a word needs)
        # and the key the adapter DECLINES after the word is out. Both are
        # absent from a corpus that types words, and both are layout-visible:
        # this layout maps 26 letters, so all ten digits are declined and reach
        # the client, and the trailing BackSpace finds an empty buffer and is
        # declined for the same reason (docs/DESIGN-FRONTEND.md Part B invariant 3).
        dict(case="digit-row-and-empty-backspace", feature="the number row, and the declined key after the word",
             keys="0123456789 \\b",
             note="0-9 are not this layout's (it maps 26 letters), so each is DECLINED and reaches the client; "
                  "the space ends the word and the BackSpace that follows it finds an empty buffer and is "
                  "declined too -- tier A records commit/preedit only, so consumption is asserted here"),
    ],
    # 신세벌식 P2 (normative source: pat.im/1136, docs/SPEC-P2.md). Three keys
    # assemble a syllable, but where 세벌식 최종 puts jongseong on the number row,
    # P2 gives it its own letter keys and uses the number row for literal digits
    # and symbols. The 갈마들이 core: the same letter has a 초성 and a 종성
    # spelling (`s` is ᅨ in the jung slot and ᆫ in the jong slot; `cq` is two
    # initials across a syllable boundary, `cQ` one 겹받침), a lone batchim
    # receiving an 윗글 choseong becomes its 된소리 (2024 rule), and a vowel-plus-
    # jung key can be reached differently depending on what is already in flight.
    "sin3-p2": [
        dict(case="word-annyeong", feature="three-key syllable assembly",
             keys="jfshta ", note="안녕: ᄋ ᅡ ᆫ + ᄂ ᅧ ᆼ, three keys each, jongseong on its own key"),
        dict(case="word-hangeul", feature="three-key syllable assembly",
             keys="mfskgw ", note="한글: ᄒ ᅡ ᆫ + ᄀ ᅳ ᆯ"),
        dict(case="galmadori-initials-vs-batchim", feature="갈마들이: two initials or one 겹받침",
             keys="cq cQ ", note="cq = ㄱ ㅅ across the boundary (each an 초성), cQ = ㄳ as a 겹받침: "
                                 "the Shift marks the jong reading (docs/SPEC-P2.md §disambiguation, rule 2)"),
        dict(case="galmadori-one-key-two-slots", feature="갈마들이: one key, two slots",
             keys="ks ", note="계: 's' is ᅨ while a consonant is in flight and ᆫ as a jongseong -- the key's "
                              "meaning comes from the slot, not from the key"),
        dict(case="jong-has-its-own-key", feature="종성 has a key of its own",
             keys="ifz ", note="마 then 맘: 초성 ᄆ is 'i' and 종성 ᆷ is 'z', two different keys"),
        dict(case="batchim-takes-upper-choseong", feature="a lone batchim becomes 된소리 (2024 rule)",
             keys="gG eE vV ", note="ㄷ+윗글 ᄃ = ㄸ, ᄇ+ᄈ = ㅃ, ᄌ+ᄍ = ㅉ -- the buffer is 받침-only, so the "
                                    "choseong replaces it in place (docs/SPEC-P2.md §unit-id conditions)"),
        dict(case="number-row-is-literal", feature="the number row delivers digits",
             keys="1230 ", note="1 2 3 0 are text, not jongseong: the row 세벌식 최종 spends on batchim, P2 "
                                "spends on typing digits"),
        dict(case="shifted-number-is-a-symbol", feature="the shifted number row is punctuation",
             keys="!@#$ ", note="! @ # $, delivered as text by the layout"),
        dict(case="symbol-ends-the-word", feature="a symbol is a boundary",
             keys="kf. ", note="가 then '.': the engine commits 가 and the layout spells the period itself"),
        dict(case="sentence-annyeonghaseyo", feature="real text, five syllables",
             keys="jfshtamfncjx ", note="안녕하세요: five syllables, three keys each"),
        dict(case="sentence-hangeulimnida", feature="real text, five syllables",
             keys="mfskgwjdehduf ", note="한글입니다: ᆸ then ᆫ in 입니다, each jongseong on its own key"),
        # Issue #229: the number row in full, and the declined key after the
        # word. P2 spends the row on literal digits, so this is the row ReqMode6's
        # digit run is about -- the 1 2 3 0 the other cases press are the half
        # of it a word happens to need.
        dict(case="digit-row-and-empty-backspace", feature="the number row, and the declined key after the word",
             keys="0123456789 \\b",
             note="0-9 are delivered as themselves, ten digits in ROW order; the space ends the word and the "
                  "BackSpace after it finds an empty buffer and is declined, so the client receives it"),
    ],
    # 신세벌식 P2's two siblings, added for issue #229 and nothing else: tier C
    # replays the digit row per LAYOUT, because a layout table can lose a row the
    # other four kept, and these two had no designed cases of their own. Their
    # identity is covered by the P2 gate corpora (p2-2-1-basic.jsonl and
    # manual-sin3-p2-yet.jsonl); what no corpus pressed on them is the number
    # row and a decline.
    "sin3-p2-basic": [
        dict(case="digit-row-and-empty-backspace", feature="the number row, and the declined key after the word",
             keys="0123456789 \\b",
             note="the basic tier spends the number row on literal digits; the space ends the word and the "
                  "BackSpace after it finds an empty buffer and is declined (issue #229)"),
    ],
    "sin3-p2-yet": [
        dict(case="digit-row-and-empty-backspace", feature="the number row, and the declined key after the word",
             keys="0123456789 \\b",
             note="same row as the base P2 -- the 옛한글 tables change the jamo, not the digits; the space "
                  "ends the word and the BackSpace after it is declined on the empty buffer (issue #229)"),
    ],
}

# The head line's provenance differs by layout: 세벌식 최종's identity is its own
# key arrangement, P2's is pat.im/1136, and 두벌식's is the libhangul table this
# repository ships beside. Written per layout so the trace's `meta` names where
# the case text came from, not which tool wrote it.
HEAD_SPEC = {
    "dubeolsik": ("the libhangul 0.2.0 두벌식 table this repository ships "
                  "(libhunmin/data/layouts/dubeolsik.json)"),
    "sebeolsik-final": "the layout's own key arrangement; ReqMode6 for the number row (docs/SPEC-MODES.md)",
    "sin3-p2": "pat.im/1136 (normative, docs/SPEC-P2.md); ReqMode6 for the number row (docs/SPEC-MODES.md)",
    # The two P2 siblings added for #229. Their only designed case is the digit
    # row + decline, so their identity facts stay with the gate corpora
    # (p2-2-1-basic.jsonl, manual-sin3-p2-yet.jsonl) and are NOT restated here --
    # one home per fact.
    "sin3-p2-basic": ("pat.im/1136's basic tier (docs/SPEC-P2.md, gate corpus "
                      "p2-2-1-basic.jsonl); ReqMode6 for the number row (docs/SPEC-MODES.md)"),
    "sin3-p2-yet": ("pat.im/1136's 옛한글 tier (docs/SPEC-P2.md, gate corpus "
                    "manual-sin3-p2-yet.jsonl); ReqMode6 for the number row (docs/SPEC-MODES.md)"),
}


def sim_path(explicit):
    if explicit:
        return explicit
    for cand in (os.path.join(REPO, "build", "hunmin-sim"),
                 os.path.join(REPO, "build", "tools", "hunmin-sim")):
        if os.access(cand, os.X_OK):
            return cand
    sys.exit("no hunmin-sim found; pass --sim PATH (or build first)")


def events(sim, layout, case):
    """Every key's answer, straight from the engine's own trace."""
    res = subprocess.run([sim, "--data", DATA, "--layout", layout, "--keys", case["keys"], "--trace"],
                         capture_output=True, text=True)
    if res.returncode != 0:
        sys.exit("%s: hunmin-sim failed (%d): %s" % (case["case"], res.returncode, res.stderr.strip()))
    return [json.loads(l) for l in res.stdout.splitlines() if l.startswith("{")]


def render(layout, cases):
    head = {
        "meta": {
            "source": "designed",
            "spec": HEAD_SPEC[layout],
            "layout": layout,
            "tool": "tools/gen-case-golden.py",
            "sim": "hunmin-sim --keys … --trace",
            "note": "expected commit/preedit per key are the engine's answers, never typed; "
                    "a diff here is a change to the layout or the composer and must be reviewed",
        }
    }
    lines = [json.dumps(head, ensure_ascii=False)]
    for c, evs in cases:
        lines.append(json.dumps({"meta": {"case": c["case"], "feature": c["feature"], "note": c["note"]}},
                                ensure_ascii=False))
        for e in evs:
            row = {"in": e["in"], "commit": e["commit"], "preedit": e["preedit"]}
            lines.append(json.dumps(row, ensure_ascii=False))
    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sim", help="path to hunmin-sim (default: build/hunmin-sim)")
    ap.add_argument("--layout", default="sebeolsik-final",
                    help="one of the layouts this tool carries cases for (default: sebeolsik-final)")
    ap.add_argument("--check", action="store_true", help="do not write; fail when the committed corpus differs")
    args = ap.parse_args()
    sim = sim_path(args.sim)
    cases = CASES.get(args.layout)
    if not cases:
        sys.exit("no case table for layout %r; known: %s" % (args.layout, ", ".join(sorted(CASES))))
    out = os.path.join(REPO, "tests", "golden", "case-%s.jsonl" % args.layout)

    built = [(dict(c), events(sim, args.layout, c)) for c in cases]
    body = render(args.layout, built)

    if args.check:
        try:
            with open(out, encoding="utf-8") as f:
                committed = f.read()
        except FileNotFoundError:
            sys.exit("%s does not exist; run without --check" % out)
        if committed != body:
            sys.exit("%s is stale: re-run tools/gen-case-golden.py and review the diff\n"
                     "  regenerate: python3 tools/gen-case-golden.py --sim %s --layout %s"
                     % (out, sim, args.layout))
        print("OK  %s (%d cases)" % (out, len(cases)))
        return 0

    with open(out, "w", encoding="utf-8") as f:
        f.write(body)
    print("wrote %s (%d cases)" % (out, len(cases)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
