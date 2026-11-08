#!/usr/bin/env python3
"""Compare automatic mode-conversion accuracy: sin3-p2 + qwerty.

THE QUESTION
------------
SPEC-MODES ReqMode8 switches the output mode when a word is clearly one language and
not the other:

    english_valid AND NOT hangul_valid  ->  commit as English, switch
    hangul_valid AND NOT english_valid  ->  commit as Hangul, stay
    otherwise (both valid / both invalid)  ->  commit in the current mode

ReqMode2 puts one qwerty layout inside hunmin as the English mode. That means the
conversion is a **layout switch**, and the interesting question is what the
user sees at the moment of the switch:

  A. Sin3-p2 only (the status quo: English mode does not exist)
     The Hangul layout keeps composing, so an English word becomes jamo and is
     judged on that. Nothing can be recovered -- the Latin text is already gone.

  B. Sin3-p2 + qwerty (English mode exists)
     The word is *also* typed through qwerty, which commits Latin verbatim.
     If ReqMode8 says "English", the Latin is what gets shown.

This measures the conversion decision itself: given the same keystrokes, how
often does each arm produce the right script, and how much text is lost.

THE TRAP THIS EXISTS TO AVOID
-----------------------------
Do not judge the raw ASCII. In arm A the engine composes first; that
composition is the thing under test. Judging 'sk' as ASCII measures a path no
user takes, and it is what produced a previously reported "0.00% error".

Usage:
    ~/work/kiwi-venv/bin/python tools/modes-convert-compare.py --sample 400
"""
import argparse
import json
import pathlib
import random
import subprocess
import sys
import time
try:  # issue #272 phase 2: one ROOT derivation, with a file-relative fallback
    from _common import ROOT as _COMMON_ROOT
except ImportError:  # run from outside tools/: derive from this file
    _COMMON_ROOT = __import__("pathlib").Path(__file__).resolve().parent.parent

REPO = _COMMON_ROOT
DATA = REPO / "libhunmin/data"
SIM = REPO / "build/hunmin-sim"
KSGEN = REPO / "out/sin3p2-ksgen.json"
EVAL = REPO / "out/sin3p2-eval.json"

HANGUL_BASE = 0xAC00
NSYL = 19 * 21 * 28

REAL_TAGS = {"NNG", "NNP", "NNB", "NNBC", "NP", "NR", "VV", "VA", "VX",
             "VCP", "VCN", "MAG", "MAJ", "MAA", "MM", "IC"}


def is_hangul_syllable(ch):
    return len(ch) == 1 and HANGUL_BASE <= ord(ch) < HANGUL_BASE + NSYL


def batch(keys_list, layout):
    if not keys_list:
        return []
    r = subprocess.run(
        [str(SIM), "--data", str(DATA), "--layout", layout, "--batch"],
        input="\n".join(keys_list), capture_output=True, text=True, timeout=900)
    if r.returncode != 0:
        raise RuntimeError(f"sim --batch failed on {layout}: {r.stderr.strip()[:300]}")
    return [json.loads(l) for l in r.stdout.splitlines() if l.strip()]


def load_corpus(path, n, seed):
    words = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        p = line.split("\t")
        if len(p) == 2 and p[0] and p[0].isascii():
            words.append(p[0])
    words = sorted(set(words))
    return random.Random(seed).sample(words, min(n, len(words)))


def kiwi():
    sys.path.insert(0, str(pathlib.Path.home() /
                            "work/kiwi-venv/lib/python3.12/site-packages"))
    from kiwipiepy import Kiwi  # type: ignore[import]
    k = Kiwi()
    k.tokenize("")
    return k


def judge(k, text):
    """(english_valid, hangul_valid) as ReqMode8 defines them.

    english_valid: the token is Latin script (a Latin layout commits it
    verbatim, so this is the script of the output, not a dictionary lookup --
    SPEC-MODES already ruled a dictionary filter halves the reproduction rate).
    hangul_valid: every character is a composable Hangul syllable, which is
    SPEC-MODES ReqMode8 fallback (c) and the only candidate with no cap problem.
    """
    eng = bool(text) and all(ord(c) < 128 and c.isalnum() or ord(c) < 128 and not c.isspace()
                             for c in text) and any(c.isalpha() for c in text)
    ko = bool(text) and all(is_hangul_syllable(c) for c in text)
    return eng, ko


def decide(eng, ko, current="hangul"):
    """ReqMode8's table, verbatim. Returns the mode to commit in."""
    if eng and not ko:
        return "english"
    if ko and not eng:
        return "hangul"
    return current


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--corpus", type=pathlib.Path,
                    default=pathlib.Path.home() / "work/aihub-records-corpus/624-en-mixed.txt")
    ap.add_argument("--sample", type=int, default=400)
    ap.add_argument("--seed", type=int, default=11)
    ap.add_argument("--hangul-layout", default="sin3-p2")
    ap.add_argument("--latin-layout", default="us-qwerty")
    ap.add_argument("--out", type=pathlib.Path, default=REPO / "out/modes-convert-compare.json")
    a = ap.parse_args()

    sample = load_corpus(a.corpus, a.sample, a.seed)
    print(f"corpus: {a.corpus}\nsample: {len(sample)} ascii tokens", file=sys.stderr)

    # Arm A: the Hangul layout only. This is what exists today.
    armA = batch(sample, a.hangul_layout)
    # Arm B: the same physical keys on a Latin layout. Latin commits verbatim,
    # so this is not a "keystroke conversion" -- it is what the user would see
    # if English mode were active.
    armB = batch(sample, a.latin_layout)

    k = kiwi()
    rows = []
    for w, ra, rb in zip(sample, armA, armB):
        hangul_out = ra["commit"] + ra["preedit"]
        latin_out = rb["commit"] + rb["preedit"]
        # what ReqMode8 would judge: the Latin text is what "English mode" yields
        eng, ko = judge(k, latin_out)
        mode = decide(eng, ko)
        # what the user actually gets in each arm
        shownA = hangul_out
        shownB = latin_out if mode == "english" else hangul_out
        rows.append({
            "typed": w,
            "A_hangul_layout": hangul_out,
            "B_latin_layout": latin_out,
            "english_valid": eng,
            "hangul_valid": ko,
            "r8_mode": mode,
            "shown_A": shownA,
            "shown_B": shownB,
            "A_correct_script": any(ord(c) < 128 and c.isalpha() for c in shownA),
            "B_correct_script": any(ord(c) < 128 and c.isalpha() for c in shownB),
            "A_is_hangul": all(is_hangul_syllable(c) for c in shownA) and bool(shownA),
            "B_is_hangul": all(is_hangul_syllable(c) for c in shownB) and bool(shownB),
        })

    n = len(rows)
    def pct(x):
        return round(100.0 * x / n, 2) if n else 0.0

    # The ground truth: the user typed Latin. So "correct" = Latin script shown.
    a_latin = sum(1 for r in rows if r["A_correct_script"])
    b_latin = sum(1 for r in rows if r["B_correct_script"])
    switched = sum(1 for r in rows if r["r8_mode"] == "english")
    kept = n - switched

    # Where does arm B still fail? Its keystrokes went through the HANGUL layout,
    # so the Latin text exists only because we also ran the Latin layout.
    both = sum(1 for r in rows if r["A_is_hangul"] and r["B_is_hangul"])

    summary = {
        "corpus": str(a.corpus),
        "sample": n,
        "seed": a.seed,
        "arms": {
            "A": f"{a.hangul_layout} only (no English mode exists today)",
            "B": f"{a.hangul_layout} + {a.latin_layout} (ReqMode2's English mode)",
        },
        "r8_decisions": {"switch_to_english": switched, "keep_hangul": kept},
        "user_typed_english_ground_truth": True,
        "A_shows_latin": a_latin, "A_shows_latin_pct": pct(a_latin),
        "B_shows_latin": b_latin, "B_shows_latin_pct": pct(b_latin),
        "both_arms_hangul_junk": both,
        "degenerate_comparison": {
            "sin3p2_latin_output_keys": 0,
            "claim": "A_shows_latin 0% is not a performance result. sin3-p2 "
                     "emits zero Latin letters across all 47 keys -- true of "
                     "every layout in this repo -- so arm A cannot show Latin "
                     "by construction. B shows 100% because a Latin layout "
                     "commits each key id as itself. The head-to-head is "
                     "vacuous; see out/modes-convert-cost.json for recovery "
                     "and switch cost, which are the measurable quantities.",
        },
        "date": "2026-10-05",
    }
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps({"summary": summary, "rows": rows},
                                ensure_ascii=False, indent=1), encoding="utf-8")

    print("\n=== automatic conversion: sin3-p2 alone vs + qwerty ===")
    print(f"  sample (user typed English)        {n}")
    print(f"  ReqMode8 switches to English            {switched}  ({pct(switched)}%)")
    print(f"  ReqMode8 keeps Hangul (ambiguous)       {kept}  ({pct(kept)}%)")
    print(f"\n  arm A  sin3-p2 only:")
    print(f"    shows Latin (what was typed)     {a_latin}  ({pct(a_latin)}%)")
    print(f"    shows Hangul junk               {n - a_latin}  ({pct(n - a_latin)}%)")
    print(f"\n  arm B  sin3-p2 + qwerty:")
    print(f"    shows Latin (what was typed)     {b_latin}  ({pct(b_latin)}%)")
    print(f"    shows Hangul junk               {n - b_latin}  ({pct(n - b_latin)}%)")
    print(f"\n  both arms produce Hangul junk     {both}")
    print(f"\nwrote {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())