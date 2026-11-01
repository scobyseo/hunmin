#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 hunmin contributors
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Generate tests/golden/case-<layout>.jsonl: designed cases for one layout.

The `ref-*.jsonl` corpora are parity walks — several hundred seeded events
replayed against libhangul. They catch a data change, but they cannot say WHAT
they assert: a diff in 400 random lines says "something moved", not "the number
row stopped being jongseong jamo".

This generator writes the other kind, for the layout whose identity is its own
key arrangement (`세벌식 최종`, `sebeolsik-final`, alias `3f`). The hand-authored
half is the INPUT — a case name, the feature it pins, and the keys, in the same
`hunmin-sim --keys` alphabet the other tier-A goldens use (an uppercase letter or
a shifted symbol is that key with Shift held, so `!` is Shift+1). The EXPECTED
half — the commit and preedit after every key — is read back from the engine.

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
             note="1..0 are ᇂ ᆻ ᆸ ᅭ ᅲ ᅣ ᅨ ᅴ ᅮ ᄏ: this row is why R6 exists (SPEC-MODES §R6)"),
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
    ],
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
            "spec": "the layout's own key arrangement; R6 for the number row (docs/SPEC-MODES.md)",
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
