#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 hunmin contributors
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Generate tests/golden/judgement-r8.jsonl (SPEC-MODES R8, issue #178).

The hand-authored half of a case is the INPUT a person replays: which layout,
which mode to start in, whether the automatic switch is on, and the keys. The
EXPECTED half — the delivered text, the judgements taken, the mode after the
boundary, and the per-key delivery timing — is read back from the engine through
`hunmin-sim --judge`, never typed here. That is the rule for every corpus in
tests/golden/: a case says what a person did, the engine says what happened, and
the committed file is the reviewed record of both.

    python3 tools/gen-judgement-golden.py --sim build/hunmin-sim   # rewrite
    python3 tools/gen-judgement-golden.py --sim build/hunmin-sim --check

--check is the reproducible-golden gate: it regenerates in memory and fails when
the committed file differs, so an engine change that moves a judgement shows up
as a diff to review instead of quietly rewriting the corpus.

Every case here is a line of SPEC-MODES R8's formula or a guard around it:

    english_valid AND NOT hangul_valid   -> switch to English   (line 1)
    hangul_valid AND NOT english_valid   -> switch to Hangul    (line 2, E1)
    otherwise                            -> keep the current mode

plus R9's three-key guard (both sides of it), R6's digit run (which is why the
delivery TIMING is asserted as well as the text), R4's "judged once, at the
boundary", R13's off-is-the-identity state, and R8-정정-3's widened shape rule on
a layout that composes archaic jamo.
"""

import argparse
import json
import os
import shlex
import subprocess
import sys
from typing import Any

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(REPO, "libhunmin", "data")
OUT = os.path.join(REPO, "tests", "golden", "judgement-r8.jsonl")

# The INPUT of each case. `keys` uses hunmin-sim's escape language ("\s<char>"
# is Shift held, "\b" backspace). Nothing here states an expectation.
CASES = [
    # --- R8 line 1: only the Latin reading is valid -> switch to English ----
    dict(case="r8-english-word-switches", req="R8", layout="dubeolsik", mode="hangul",
         autoswitch=1, keys="hello ",
         note="line 1 of the formula: the Latin reading is a word and the Hangul reading is not"),
    # --- R8 line 2 in Hangul mode: the no-op half of "stay Hangul" ---------
    dict(case="r8-hangul-word-keeps-hangul", req="R8", layout="dubeolsik", mode="hangul",
         autoswitch=1, keys="gksrmf ",
         note="the Hangul reading is well-formed and the Latin one is not: no switch is needed"),
    # --- R8 "otherwise": both valid, both invalid --------------------------
    dict(case="r8-both-valid-keeps-the-mode", req="R8", layout="dubeolsik", mode="hangul",
         autoswitch=1, keys="ahem ",
         note="both readings are valid ('ahem' is English, 모드 is well-formed): keep the mode"),
    dict(case="r8-both-invalid-keeps-the-mode", req="R8", layout="dubeolsik", mode="hangul",
         autoswitch=1, keys="dkssudg ",
         note="the maintainer's own example 안녕ㅎ: neither a Hangul word nor an English one"),
    # --- R8 line 2 as a real switch: E1's symmetric direction --------------
    dict(case="r8-symmetric-hangul-in-latin-mode", req="R8", layout="dubeolsik", mode="latin",
         autoswitch=1, keys="dlqslek ",
         note="E1 (ruled 2026-11-01): typed in Latin mode, reads 입니다 in Hangul, so it switches"),
    dict(case="r8-symmetric-english-in-latin-mode", req="R8", layout="dubeolsik", mode="latin",
         autoswitch=1, keys="hello ",
         note="the same direction where nothing should happen: already the right language"),
    # --- R9's three-key guard, both sides --------------------------------
    dict(case="r9-two-keys-are-not-judged", req="R9", layout="dubeolsik", mode="hangul",
         autoswitch=1, keys="on ",
         note="'on' is English, so without the guard this would switch; two keys is under it"),
    dict(case="r9-three-keys-are-judged", req="R9", layout="dubeolsik", mode="hangul",
         autoswitch=1, keys="key ",
         note="the control for the line above: three key inputs is inside the guard"),
    # --- R4: one judgement, at the boundary ------------------------------
    dict(case="r4-nothing-is-judged-in-flight", req="R4", layout="dubeolsik", mode="hangul",
         autoswitch=1, keys="hello",
         note="no delimiter, so no boundary: the word is still in the buffer and undecided"),
    # --- R6: a digit run is part of the buffer, not a per-key commit -----
    dict(case="r6-digit-run-is-held-to-the-boundary", req="R6", layout="sin3-p2", mode="hangul",
         autoswitch=1, keys="2024 ",
         note="the engine commits each digit; the shared layer must hold the run (issue #171)"),
    # --- R6 on the layout R6 is about: 세벌식 최종's digit row is JAMO ---
    dict(case="r6-jamo-digit-row-held-to-the-boundary", req="R6", layout="sebeolsik-final",
         mode="hangul", autoswitch=1, keys="123 ",
         note="세벌식 최종 keys 1 2 3 are the jongseong jamo ᇂ ᆻ ᆸ: R6's own case -- committing a digit key on its own would break the run"),
    dict(case="r6-literal-digit-row-held-to-the-boundary", req="R6", layout="sebeolsik-final",
         mode="hangul", autoswitch=1, keys="\\sj\\sk\\sl ",
         note="literal digits on the shifted letter row (Shift+j..l = 1,2,3): held as one run as well, so R6's condition is not limited to keys that compose jamo. This is not the unshifted digit row, which is jamo (docs/OPTIONS-MEASURED.md §BND-7). Corrected 2026-11-01: the first cut of this case put a delimiter after each digit and recorded its own spaces as a per-key commit"),
    # --- R8 on 세벌식 최종, whose keys are jamo rather than syllables ------
    dict(case="r8-word-keeps-hangul-on-sebeolsik-final", req="R8", layout="sebeolsik-final",
         mode="hangul", autoswitch=1, keys="jfshea ",
         note="안녕 on 세벌식 최종 (j f s h e a): the Hangul reading is a word and the Latin one is not, so the mode stays"),
    dict(case="r8-english-word-switches-on-sebeolsik-final", req="R8", layout="sebeolsik-final",
         mode="hangul", autoswitch=1, keys="hello ",
         note="line 1 on a jamo layout: the Latin reading of the same keys is still looked up, so it switches"),
    # --- R8-정정-3: an 옛한글 layout is judged by the widened shape rule ----
    dict(case="r8-old-hangul-widened-rule-keeps-the-mode", req="R8-정정-3",
         layout="dubeolsik-yet", mode="hangul", autoswitch=1, keys="\\sahem ",
         note="the Hangul reading is ᅀᅩ드 (archaic ᅀ): valid only under the widened sets, and its Latin reading 'Ahem' is an English word. Both valid, so the mode stays; the modern rule would call it invalid and switch to English"),
    # --- R13-정정 / B3: off is the identity path -------------------------
    dict(case="r13-autoswitch-off-is-the-identity", req="R13", layout="dubeolsik", mode="hangul",
         autoswitch=0, keys="hello ",
         note="with the switch off nothing is looked up and the Hangul reading is delivered"),
    # --- E1's 0.3 gate: the maintainer's mixed sentence, one stream ------
    dict(case="r8-mixed-sentence-gate", req="R8", layout="dubeolsik", mode="hangul",
         autoswitch=1, keys="\\senglish sms whgdms \\slanguage dlqslek ",
         note="'English는 좋은 Language입니다' typed as one stream (E1's release gate)"),
    # --- R4: a symbol is a termination condition too ---------------------
    dict(case="r4-symbol-ends-the-word-in-latin-mode", req="R4", layout="sin3-p2", mode="latin",
         autoswitch=1, keys="jgz.",
         note="R4: 'A symbol is also a termination condition.' A period is a mapped key, so the engine consumes it and the layer has to end the word there; without this the buffer holds 'jgz.' and the judgement abstains (measured on a device 2026-11-07)"),
    dict(case="r4-symbol-ends-the-word-in-hangul-mode", req="R4", layout="sin3-p2", mode="hangul",
         autoswitch=1, keys="test.",
         note="the same rule in the other direction: the Latin reading 'test' is a word, so the switch happens at the period"),
    dict(case="r4-symbol-does-not-split-a-digit-run", req="R6", layout="sin3-p2", mode="hangul",
         autoswitch=1, keys="2024.",
         note="R4 and R6 together: the period ends the buffer, and a digit run is not a word in either language, so nothing is judged and nothing switches"),
]


def sim_path(explicit):
    if explicit:
        return explicit
    for cand in (os.path.join(REPO, "build", "hunmin-sim"),
                 os.path.join(REPO, "build", "tools", "hunmin-sim")):
        if os.access(cand, os.X_OK):
            return cand
    sys.exit("no hunmin-sim found; pass --sim PATH (or build first)")


# The probe a frontend lane uses to ask "which mode am I in now?". This tool
# cannot ask the layer -- a lane runs the real addon, not the layer -- so the
# corpus carries the answer for each layout: the same keys, delivered in Hangul
# mode and in Latin mode. A probe whose two readings coincide tells a lane
# nothing, so the first one that differs is chosen per layout.
PROBES = ["k ", "s ", "a ", "1 "]


def probe_for(sim, layout):
    """The probe keys and what each mode delivers for them, from the engine."""
    for keys in PROBES:
        out = {}
        for mode in ("hangul", "latin"):
            cmd = [sim, "--data", DATA, "--layout", layout, "--latin", "us-qwerty",
                   "--judge", "--autoswitch", "1"]
            if mode == "latin":
                cmd.append("--start-latin")
            cmd += ["--keys", keys]
            res = subprocess.run(cmd, capture_output=True, text=True)
            if res.returncode != 0:
                sys.exit("%s: probe run failed for %r: %s" % (layout, keys, res.stderr.strip()))
            rows = [json.loads(l) for l in res.stdout.splitlines() if l.startswith("{")]
            out[mode] = rows[-1]["delivered"] if rows else None
        if out["hangul"] != out["latin"]:
            return {"keys": keys, "hangul": out["hangul"], "latin": out["latin"]}
    return None


def trace(sim, case: dict[str, Any]) -> dict[str, Any]:
    """Run one case through the layer and return what the engine observed."""
    cmd = [sim, "--data", DATA, "--layout", case["layout"],
           "--latin", "us-qwerty", "--judge",
           "--autoswitch", str(case["autoswitch"])]
    if case.get("mode") == "latin":
        cmd.append("--start-latin")
    cmd += ["--keys", case["keys"], "--trace"]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        sys.exit("%s: hunmin-sim failed (%d): %s" % (case["case"], res.returncode, res.stderr.strip()))
    rows = [json.loads(l) for l in res.stdout.splitlines() if l.startswith("{")]
    if not rows:
        sys.exit("%s: no output from hunmin-sim" % case["case"])
    summary, steps = rows[-1], rows[:-1]
    if "delivered" not in summary:
        sys.exit("%s: last line is not a case summary: %r" % (case["case"], summary))
    case["commit"] = summary["delivered"]
    case["judged"] = summary["judged"]
    case["mode_after"] = summary["mode_after"]
    case["keys_count"] = len(steps)
    per_key = [r.get("commit", "") for r in steps]
    # The timing is only asserted where the per-key commits add up to the
    # delivered text; a language toggle can deliver a syllable of its own, and a
    # case where that happens says so rather than carrying an unexplained gap.
    case["step_commits"] = per_key if "".join(per_key) == case["commit"] else None
    return case


def render(cases):
    head = {
        "meta": {
            "corpus": "judgement",
            "req": "R8; R9; R10; R4; R6 (issue #178)",
            "spec": "docs/SPEC-MODES.md R8 formula and its guards",
            "tool": "tools/gen-judgement-golden.py",
            "sim": "hunmin-sim --judge --autoswitch N --trace",
            "note": "expected values are the engine's own answers, never hand-typed; "
                    "delivered text excludes delimiter keys, which the client receives",
        }
    }
    lines = [json.dumps(head, ensure_ascii=False)]
    for c in cases:
        row: dict[str, Any] = {
            "case": c["case"],
            "req": c["req"],
            "layout": c["layout"],
            "latin": "us-qwerty",
            "autoswitch": c["autoswitch"],
            "mode": c.get("mode", "hangul"),
            "keys": c["keys"],
            "keys_count": c["keys_count"],
            "commit": c["commit"],
            "judged": c["judged"],
            "mode_after": c["mode_after"],
            "note": c["note"],
        }
        # Optional fields are spread in rather than assigned: a dict literal
        # narrowed from its own contents does not accept a later setitem for a
        # differently typed key, and pyright is part of the gate here.
        if c.get("step_commits") is not None:
            row = {**row, "step_commits": c["step_commits"]}
        if c.get("probe"):
            row = {**row, "probe": c["probe"]}
        lines.append(json.dumps(row, ensure_ascii=False))
    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sim", help="path to hunmin-sim (default: build/hunmin-sim)")
    ap.add_argument("--check", action="store_true",
                    help="do not write; fail when the committed corpus differs")
    args = ap.parse_args()
    sim = sim_path(args.sim)

    cases: list[dict[str, Any]] = [dict(c) for c in CASES]
    for c in cases:
        trace(sim, c)
        # A probe asks "which mode is the layer in NOW", so it only means
        # something after a boundary: a case that ends mid-word leaves the word
        # composing, and a probe would append to it rather than stand alone.
        c["probe"] = probe_for(sim, c["layout"]) if c["keys"].endswith(" ") else None
    body = render(cases)

    if args.check:
        try:
            with open(OUT, encoding="utf-8") as f:
                committed = f.read()
        except FileNotFoundError:
            sys.exit("%s does not exist; run without --check" % OUT)
        if committed != body:
            sys.exit("%s is stale: re-run tools/gen-judgement-golden.py and review the diff\n"
                     "  regenerate: python3 tools/gen-judgement-golden.py --sim %s"
                     % (OUT, shlex.quote(sim)))
        print("OK  %s (%d cases)" % (OUT, len(cases)))
        return 0

    with open(OUT, "w", encoding="utf-8") as f:
        f.write(body)
    print("wrote %s (%d cases)" % (OUT, len(cases)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
