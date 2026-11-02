#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Every tier-A case of every family in the x11 corpus must have a trace.

Issue #187 was a coverage hole nobody could see: the corpus was generated once,
cases later disappeared from it (2026-10-25, word buffering), and nothing
regenerated or compared it against the goldens it claims to replay. The win32
corpus replayed 13 case instances the x11 corpus had silently dropped. A corpus
is a claim about coverage, so the claim needs a mechanical check.

FAMILIES below is the record of what this corpus replays and how to regenerate
it -- it exists because the invocation (golden, layout, case stem) is otherwise
recoverable only by reading each trace's `meta` line, which cannot see a family
that has no traces at all (exactly how p2-2-2-tiers vanished).

    python3 tools/conformance-coverage.py --check
    python3 tools/conformance-coverage.py --regen

`--check` is ctest `conformance-coverage`. `--regen` rewrites the `.in` files
from the goldens; the `.gold` files must then be re-emitted with
`tools/trace-check.py --emit` and reviewed against tier A, which is what tier C
itself enforces (trace-check compares `.in` to `.gold` before comparing either
to the adapter).
"""
import argparse
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
CORPUS = ROOT / "tests/conformance/x11"
GOLDEN = ROOT / "tests/golden"
GEN = ROOT / "tools/gen-conformance.py"

# (golden, layout, case stem). The layout is not always the golden's own -- the
# dkssud-combo run replays p2-2-1-basic's cases through the sin3-p2 alias -- so
# it is written out rather than derived.
FAMILIES: list[tuple[str, str, str]] = [
    ("p2-2-1-basic.jsonl", "sin3-p2-basic", "p2-2-1-basic"),
    ("p2-2-1-basic.jsonl", "sin3-p2", "dkssud-combo"),
    ("p2-2-2-tiers.jsonl", "sin3-p2", "p2-2-2-tiers"),
    ("p2-2-3-chosung.jsonl", "sin3-p2", "p2-2-3-chosung"),
    ("p2-2-4-1-yetrules.jsonl", "sin3-p2-yet", "p2-2-4-1-yetrules"),
    ("p2-2-4-2-eung.jsonl", "sin3-p2-yet", "p2-2-4-2-eung"),
    ("p2-3-swapped.jsonl", "sin3-p2", "p2-3-swapped"),
    ("manual-sin3-p2.jsonl", "sin3-p2", "manual-sin3-p2"),
    ("manual-sin3-p2-yet.jsonl", "sin3-p2-yet", "manual-sin3-p2-yet"),
]


def golden_case_count(golden: pathlib.Path) -> int:
    """Cases in a golden, counted the way the generator counts them.

    A case begins at a `{"meta": {"case": ...}}` line; a golden whose cases
    carry no such line is a single implicit case, so an empty count means one.
    """
    n = 0
    for line in golden.read_text(encoding="utf-8").splitlines()[1:]:
        rec = json.loads(line)
        if "meta" in rec and "case" in rec["meta"]:
            n += 1
    return n


def traces(stem: str) -> list[pathlib.Path]:
    return sorted(CORPUS.glob(f"{stem}-case*.trace.in"))


def check() -> int:
    problems: list[str] = []
    claimed: set[str] = set()
    for golden_name, layout, stem in FAMILIES:
        golden = GOLDEN / golden_name
        if not golden.is_file():
            problems.append(f"{stem}: no golden {golden.relative_to(ROOT)}")
            continue
        want = max(golden_case_count(golden), 1)
        have = traces(stem)
        claimed.update(f.name for f in have)
        if len(have) != want:
            problems.append(
                f"{stem}: {len(have)} trace(s) for {want} tier-A case(s) "
                f"({golden_name}) -- regenerate with "
                f"tools/conformance-coverage.py --regen")
            continue
        for i, f in enumerate(have, 1):
            meta = json.loads(f.read_text(encoding="utf-8").splitlines()[0])["meta"]
            if meta.get("source", "").split(" (")[0] != golden_name:
                problems.append(f"{f.name}: source {meta.get('source')!r} is not {golden_name}")
            if meta.get("layout") != layout:
                problems.append(f"{f.name}: layout {meta.get('layout')!r} is not {layout!r}")
            if meta.get("case_index") != i:
                problems.append(f"{f.name}: case_index {meta.get('case_index')!r} is not {i}")
            # A `--emit`ed `.gold` without its `.in` counterpart (or the
            # reverse) is a half-written corpus that tier C would either skip
            # or diff against nothing.
            if not f.with_suffix("").with_suffix(".trace.gold").is_file():
                problems.append(f"{f.name}: no .trace.gold beside it")

    # A trace no family claims means a case was added to the corpus without
    # being recorded here -- the direction that lets the table drift.
    for f in sorted(CORPUS.glob("*.trace.in")):
        if f.name not in claimed:
            problems.append(f"{f.name}: not claimed by any FAMILIES entry")

    for p in problems:
        print(p, file=sys.stderr)
    if problems:
        print(f"{len(problems)} coverage problem(s)", file=sys.stderr)
        return 1
    print(f"ok: {len(claimed)} traces cover every case of "
          f"{len(FAMILIES)} famil{'y' if len(FAMILIES) == 1 else 'ies'}")
    return 0


def regen() -> int:
    CORPUS.mkdir(parents=True, exist_ok=True)
    for golden_name, layout, stem in FAMILIES:
        stem_case = stem
        out = CORPUS / f"{stem}.trace.in"
        rc = __import__("subprocess").run(
            [sys.executable, str(GEN), "--golden", str(GOLDEN / golden_name),
             "--platform", "x11", "--layout", layout, "--case", stem_case,
             "--out", str(out)]).returncode
        if rc != 0:
            return rc
    print("regenerated .in files; re-emit the .gold files with "
          "tools/trace-check.py --emit and review them against tier A",
          file=sys.stderr)
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--regen", action="store_true")
    a = ap.parse_args()
    if a.regen:
        return regen()
    return check()


if __name__ == "__main__":
    sys.exit(main())