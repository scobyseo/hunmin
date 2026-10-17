#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Compose whole-word tier-A goldens from per-syllable keystrokes (issue #80).

Input: tools/sin3p2-ksgen.py's output JSON (per-syllable keystrokes, BFS over
the layout automaton with hunmin-sim --batch as the oracle) plus a word list.
For each word the script concatenates the syllable keystrokes, appends the
boundary space, and re-verifies the composition through hunmin-sim --batch --
the engine, not the script, decides whether the concatenation composes the
word; a word whose concatenation renders anything else is recorded and
skipped, never silently reinterpreted.

The golden it writes follows the tier-A reference-trace format
(docs/SPEC-SCHEMA.md): a meta header, then per word a case separator and the
events hunmin-sim --trace produced for those keys. Expectations are the sim's
own output; the script only observes.

Why a space after every word: the trace case then ends at a boundary, which is
the event at which the commit-only adapter (docs/WINDOWS-TSF-BUFFERING.md) has
to insert the whole buffered word. A composed word alone would end in an open
syllable and never exercise insertion.
"""
import argparse
import json
import pathlib
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent


def run(a, mode, keys=None):
    argv = [str(a.sim), "--data", str(a.data), "--layout", a.layout]
    stdin = None
    if mode == "--batch":
        # --batch reads one keystroke string per stdin line; --keys is the
        # single-run form and --batch ignores it.
        argv += ["--batch"]
        stdin = keys + "\n" if keys is not None else None
    else:
        if keys is not None:
            argv += ["--keys", keys]
        if mode:
            argv += [mode]
    r = subprocess.run(argv, input=stdin, capture_output=True, encoding="utf-8",
                       errors="replace", timeout=600)
    if r.returncode != 0:
        raise SystemExit(f"hunmin-sim {mode} failed: {r.stderr.strip()[:400]}")
    return r.stdout


def rendered(rec):
    return rec["commit"] + rec["preedit"]


def strip_leading_signals(keys):
    """Per-syllable ksgen paths carry leading signal keys needed to reach the
    CHO from an empty state ('k/'mfc -> mfsc...). Inside a word the previous
    syllable leaves the state ready, and the same leading key then types a
    stray jamo instead (measured: 'mfc'kx renders ㅌ학ㅌ교, mfckx renders
    학교). A syllable contributes its path either as found or signal-stripped;
    the sim decides which composes."""
    i = 0
    while i < len(keys) and keys[i] in "'`":
        i += 1
    return keys[i:]


def word_candidates(syl_keys):
    """Cartesian product of keep/strip per syllable; the sim decides which
    candidate composes the word cleanly."""
    from itertools import product
    out = []
    for masks in product((0, 1), repeat=len(syl_keys)):
        out.append("".join(strip_leading_signals(k) if m else k
                    for k, m in zip(syl_keys, masks)))
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--words-file", type=pathlib.Path, required=True,
                    help="one word per line, UTF-8")
    ap.add_argument("--ksgen", type=pathlib.Path, required=True,
                    help="sin3p2-ksgen.py output with per-syllable keystrokes")
    ap.add_argument("--layout", default="sin3-p2")
    ap.add_argument("--sim", default=REPO / "build/hunmin-sim")
    ap.add_argument("--data", default=REPO / "libhunmin/data")
    ap.add_argument("--out", type=pathlib.Path, required=True)
    a = ap.parse_args()

    ksgen = json.loads(a.ksgen.read_text(encoding="utf-8"))
    keys_by_syllable = ksgen["keystrokes"]
    words = [w for w in a.words_file.read_text(encoding="utf-8").splitlines() if w]

    composed, skipped = {}, []
    for word in words:
        syl_keys, miss = [], []
        for syl in word:
            k = keys_by_syllable.get(syl)
            if k is None:
                miss.append(syl)
            else:
                syl_keys.append(k)
        if miss:
            skipped.append(f"{word} (no keystroke for {''.join(miss)})")
            continue
        found_keys = None
        cands = word_candidates(syl_keys)
        for cand in cands:
            keys = cand + " "  # boundary: the event the word is inserted at
            out = json.loads(run(a, "--batch", keys))
            if rendered(out) == word:
                found_keys = keys
                break
        if found_keys is None:
            best = max(
                (rendered(json.loads(run(a, "--batch", c + " ")))
                 for c in cands), key=len)
            skipped.append(f"{word} (composition renders {best!r})")
            continue
        composed[word] = found_keys

    if not composed:
        raise SystemExit("no word composed; nothing to write")

    lines = [json.dumps({
        "meta": {
            "layout": a.layout,
            "source": f"{a.words_file.name} words; keys per syllable from "
                      "tools/sin3p2-ksgen.py (BFS, hunmin-sim --batch oracle)",
            "tool": "tools/wordlist2golden.py",
            "note": "expectations are hunmin-sim --trace output over the "
                    "composed keys; the space after each word is the boundary "
                    "event at which a commit-only adapter inserts the word",
        }})]
    total = 0
    for word, keys in composed.items():
        lines.append(json.dumps({"meta": {"case": word, "keys": keys}},
                                ensure_ascii=False))
        for ev in run(a, "--trace", keys).splitlines():
            rec = json.loads(ev)
            rec.pop("consumed", None)
            lines.append(json.dumps(
                {"in": rec["in"], "commit": rec.get("commit", ""),
                 "preedit": rec.get("preedit", "")},
                ensure_ascii=False))
            total += 1

    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"{a.out}: {len(composed)} word cases "
          f"({total} events); skipped: {'; '.join(skipped) or 'none'}",
          file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
