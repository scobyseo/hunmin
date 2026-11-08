#!/usr/bin/env python3
"""Evaluate hangul/latin mode judgement on the real sin3-p2 keystroke path.

THE PIPELINE, AND WHY IT MATTERS
--------------------------------
SPEC-MODES ReqMode8 asks for `hangul_valid`: after composing, is this a Hangul word
(keep Hangul mode) or English (switch)? The pipeline in an IME is exactly

    keystrokes -> hunmin engine -> composed Hangul -> judge

Judging the raw ASCII never happens. An earlier attempt fed 'sk' straight to
Kiwi, got 0.00% English misjudgement, and reported it as the answer. On the
real path 'sk' composes to 'ㄴ' instead. This script measures the real path and
prints the wrong-path number beside it, so the size of that mistake stays on
the record rather than being rediscovered.

MEASURED FIRST: NO LAYOUT HAS A LATIN OUTPUT PATH
-------------------------------------------------
Every ASCII letter was searched for and not found, on every layout:

    sin3-p2 / sin3-p2-basic / sin3-p2-yet : 0 latin letters (digits 0-9 only)
    sebeolsik-3-2                        : 0 latin letters
    dubeolsik, sebeolsik-3-90/final/noshift/yet : 0
    romaja                               : 0 latin letters (it transliterates)

Produced by pressing each declared key id once and reading what came out
(hunmin-sim --batch). So an English token cannot be typed on a Hangul layout
at all -- which means "does the composed text look English" is not a question
the Hangul path can raise, and the mode decision has to be made from the
keystrokes or not at all. This is the single most important structural fact
for ReqMode8 and it was invisible until it was measured.

WHAT IS MEASURED HERE
--------------------
For a sample of ASCII tokens (the real thing people type in English), each is
typed through the engine on sin3-p2, and three judges see the *composed*
output:
  combination : is every character a composable Hangul syllable (ReqMode8 fallback (c))
  kiwi        : full morphological analysis -- does it recognise a Korean word

Usage:
    ~/work/kiwi-venv/bin/python tools/sin3p2-eval.py \
        --corpus ~/work/aihub-records-corpus/624-ko.txt --sample 300
"""
import argparse
import json
import pathlib
import random
import subprocess
import sys
import time

REPO = pathlib.Path(__file__).resolve().parent.parent
DATA = REPO / "libhunmin/data"
SIM = REPO / "build/hunmin-sim"
KSGEN = REPO / "out/sin3p2-ksgen.json"

HANGUL_BASE = 0xAC00
NCHO, NJUNG, NJONG = 19, 21, 28

# A token counts as a Korean word if its FIRST morpheme is a real lexical
# category. Composition junk arrives as SW (foreign symbol) or SL (foreign
# language), both of which are excluded. Measured on real output:
#   가 -> NNG   한글 -> NNG   커피 -> NNG   크다 -> VA   빠르게 -> VA
#   서울 -> NNP  민수 -> NNP   구르미 -> NNG
#   sk -> SL    world -> SL    ㄴ -> SW   ㅋ -> SW   쇼차 -> NNG(쇼) NNB(차)
# The first-morpheme rule matters: 쇼차 is composed junk that Kiwi reads as two
# real nouns, which is why whole-output tagging would be too generous here.
REAL_TAGS = {"NNG", "NNP", "NNB", "NNBC", "NP", "NR", "VV", "VA", "VX",
             "VCP", "VCN", "MAG", "MAJ", "MAA", "MM", "IC"}


def is_hangul_syllable(ch):
    return len(ch) == 1 and HANGUL_BASE <= ord(ch) < HANGUL_BASE + NCHO * NJUNG * NJONG


def combination_judge(text):
    """(c) every character is a composable Hangul syllable.

    A combination test, not a dictionary: it accepts every form the IME could
    produce, which is why its false-accept rate on 형용사/부사/관형사/수사 is
    the open measurement in wiki/decisions/the-outstanding-decisions.md.
    """
    return bool(text) and all(is_hangul_syllable(c) for c in text)


def batch_evaluate(keys_list, layout):
    if not keys_list:
        return []
    r = subprocess.run(
        [str(SIM), "--data", str(DATA), "--layout", layout, "--batch"],
        input="\n".join(keys_list), capture_output=True, text=True, timeout=900,
    )
    if r.returncode != 0:
        raise RuntimeError(f"sim --batch failed: {r.stderr.strip()[:300]}")
    return [json.loads(l) for l in r.stdout.splitlines() if l.strip()]


def keystrokes_for_latin_word(word, ksg):
    """Type a word by concatenating per-character keystrokes.

    A Latin letter has no keystroke on a Hangul layout -- measured, see the
    module docstring. The closest available thing is the ASCII key id itself
    pressed as a physical key, which is what the hardware delivers, and what
    the engine then does with it. If the layout leaves it unmapped, the engine
    passes it through and the token reaches the judge as Latin; if the layout
    maps it, it composes into Hangul. Either way the engine decides, which is
    the point: this measures what an IME would really show.
    """
    return word


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--corpus", type=pathlib.Path,
                    help="ASCII frequency list, TAB-separated word,count. "
                         "For AIHub 624 this is 624-en-mixed.txt; 624-ko.txt "
                         "holds only Hangul and yields no ASCII tokens.")
    ap.add_argument("--words", default="", help="comma-separated ASCII tokens")
    ap.add_argument("--sample", type=int, default=300)
    ap.add_argument("--seed", type=int, default=11)
    ap.add_argument("--layout", default="sin3-p2")
    ap.add_argument("--out", type=pathlib.Path, default=REPO / "out/sin3p2-eval.json")
    ap.add_argument("--no-kiwi", action="store_true")
    a = ap.parse_args()

    if a.words:
        words = [w for w in a.words.split(",") if w]
        src = "--words"
    elif a.corpus:
        words = []
        for line in a.corpus.read_text(encoding="utf-8", errors="replace").splitlines():
            p = line.split("\t")
            if len(p) == 2 and p[0] and p[0].isascii():
                words.append(p[0])
        src = str(a.corpus)
    else:
        sys.exit("need --corpus or --words")

    words = sorted(set(words))
    rng = random.Random(a.seed)
    sample = words if a.words else rng.sample(words, min(a.sample, len(words)))
    print(f"corpus: {src}\nsample: {len(sample)} ascii tokens\n", file=sys.stderr)

    # --- the real pipeline: press the physical keys, let the engine compose ---
    seqs = [keystrokes_for_latin_word(w, None) for w in sample]
    recs = batch_evaluate(seqs, a.layout)
    results = []
    for w, r in zip(sample, recs):
        out = r["commit"] + r["preedit"]
        results.append({
            "ascii": w, "keys": r["keys"], "composed": out,
            "all_hangul_syllables": combination_judge(out),
            "produced_hangul": any(is_hangul_syllable(c) for c in out),
        })

    n = len(results)
    comb = sum(1 for r in results if r["all_hangul_syllables"])
    any_ko = sum(1 for r in results if r["produced_hangul"])
    passed_through = sum(1 for r in results if not r["produced_hangul"])

    # --- the Kiwi judge, on the composed output ---
    n_kw = raw_kw = None
    kiwi_ms = None
    if not a.no_kiwi:
        try:
            sys.path.insert(0, str(pathlib.Path.home() /
                                    "work/kiwi-venv/lib/python3.12/site-packages"))
            from kiwipiepy import Kiwi  # type: ignore[import]
            t0 = time.time()
            k = Kiwi()
            k.tokenize("")
            kiwi_ms = round((time.time() - t0) * 1000)

            def judge(text):
                try:
                    toks = k.tokenize(text)
                except Exception:
                    return None, None
                if not toks:
                    return None, None
                return toks[0].tag in REAL_TAGS, toks[0].tag

            n_kw = 0
            raw_kw = 0
            for item in results:
                ok, tag = judge(item["composed"])
                item["kiwi_says_hangul"] = ok
                item["kiwi_tag"] = tag
                if ok:
                    n_kw += 1
                # the wrong path, for the record
                rok, _ = judge(item["ascii"])
                if rok:
                    raw_kw += 1
        except ImportError:
            print("kiwipiepy not importable; ran without the Kiwi judge",
                  file=sys.stderr)

    pct = lambda x: round(100.0 * x / n, 2) if n else 0.0
    summary = {
        "layout": a.layout,
        "source": src,
        "sample": n,
        "pipeline": "physical keys -> hunmin engine -> composed -> judge",
        "produced_any_hangul": any_ko,
        "produced_any_hangul_pct": pct(any_ko),
        "all_hangul_syllables": comb,
        "all_hangul_syllables_pct": pct(comb),
        "passed_through_as_latin": passed_through,
        "passed_through_pct": pct(passed_through),
        "kiwi_says_hangul": n_kw,
        "kiwi_says_hangul_pct": pct(n_kw) if n_kw is not None else None,
        "kiwi_on_raw_ascii_wrong_path": raw_kw,
        "kiwi_on_raw_ascii_pct": pct(raw_kw) if raw_kw is not None else None,
        "kiwi_load_ms": kiwi_ms,
        "date": "2026-10-05",
    }

    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps({"summary": summary, "results": results},
                                ensure_ascii=False, indent=1), encoding="utf-8")

    print("=== sin3-p2: physical keystrokes -> engine -> judge ===")
    print(f"  sample                          {n}")
    print(f"  produced Hangul at all         {any_ko}  ({pct(any_ko)}%)")
    print(f"  all Hangul syllables           {comb}  ({pct(comb)}%)")
    print(f"  passed through as Latin        {passed_through}  ({pct(passed_through)}%)")
    if n_kw is not None:
        print(f"  Kiwi: Hangul                    {n_kw}  ({pct(n_kw)}%)")
        print(f"  --- contrast, the WRONG path (raw ASCII into Kiwi) ---")
        print(f"  Kiwi on raw ASCII               {raw_kw}  ({pct(raw_kw)}%)")
    print(f"\nwrote {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())