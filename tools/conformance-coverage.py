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
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
CORPUS = ROOT / "tests/conformance/x11"
GOLDEN = ROOT / "tests/golden"
GEN = ROOT / "tools/gen-conformance.py"

# (golden, layout, case stem). The layout is not always the golden's own -- the
# dkssud-combo run replays p2-2-1-basic's cases through the sin3-p2 alias -- so
# it is written out rather than derived.
# Each family: the golden, the layout the run configures, the case stem, and
# optionally `cases` (the subset this corpus takes, when the golden carries more
# than the corpus wants) plus `args` (generator flags the family needs, e.g.
# --delivery). `source` is what the trace's meta must name.
FAMILIES: list[dict] = [
    {"golden": "p2-2-1-basic.jsonl", "layout": "sin3-p2-basic", "stem": "p2-2-1-basic"},
    {"golden": "p2-2-1-basic.jsonl", "layout": "sin3-p2", "stem": "dkssud-combo"},
    {"golden": "p2-2-2-tiers.jsonl", "layout": "sin3-p2", "stem": "p2-2-2-tiers"},
    {"golden": "p2-2-3-chosung.jsonl", "layout": "sin3-p2", "stem": "p2-2-3-chosung"},
    {"golden": "p2-2-4-1-yetrules.jsonl", "layout": "sin3-p2-yet", "stem": "p2-2-4-1-yetrules"},
    {"golden": "p2-2-4-2-eung.jsonl", "layout": "sin3-p2-yet", "stem": "p2-2-4-2-eung"},
    {"golden": "p2-3-swapped.jsonl", "layout": "sin3-p2", "stem": "p2-3-swapped"},
    {"golden": "manual-sin3-p2.jsonl", "layout": "sin3-p2", "stem": "manual-sin3-p2"},
    {"golden": "manual-sin3-p2-yet.jsonl", "layout": "sin3-p2-yet", "stem": "manual-sin3-p2-yet"},
    # 세벌식 최종, all ten designed cases merged into ONE case (2026-11-03, the
    # one-file-per-family policy), under the word-buffered model: the layout's
    # identity is its key arrangement (three-key assembly, the number row as
    # jongseong, 겹받침 on shifted keys), which no p2/manual golden states.
    # #220 took two of these for its boundary assertion; the rest had no tier-C
    # trace at all until 2026-11-03.
    {"golden": "case-sebeolsik-final.jsonl", "layout": "sebeolsik-final",
     "stem": "case-sebeolsik-final",
     "args": ["--delivery", "word-buffered"]},
    # 두벌식, the arrangement the 세벌식 families are read against: one jamo per
    # key, Shift = 된소리 or ㅒ/ㅖ, 겹받침 from two jongseong keys, and a number row
    # that is NOT the layout's (a digit is DECLINED and reaches the client, which is
    # the pass-through half of the contract on a character rather than a space).
    {"golden": "case-dubeolsik.jsonl", "layout": "dubeolsik", "stem": "case-dubeolsik"},
    # 신세벌식 P2, the normative pat.im/1136 arrangement (docs/SPEC-P2.md): three
    # keys per syllable, 종성 on its own letter key, the 갈마들이 pairs and the 2024
    # 받침 rule, and a number row spent on literal digits.
    {"golden": "case-sin3-p2.jsonl", "layout": "sin3-p2", "stem": "case-sin3-p2"},
    # Not a layout family: the schema's shifted-key spelling (`"shift": true`,
    # docs/SPEC-SCHEMA.md). It rides through tier C as well, so a converter that
    # stops interpreting the field reads red on the second row of each pair.
    {"golden": "shift-field.jsonl", "layout": "dubeolsik", "stem": "shift-field"},
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
    """A family's traces: one per case, or ONE file when the golden is one case.

    A single-case golden takes gen-conformance's unsplit path, which writes
    `<stem>.trace.in` with no `-caseNN` (p2-2-2-tiers after the 2026-11-03
    merge), so both spellings are legitimate.
    """
    single = CORPUS / f"{stem}.trace.in"
    if single.is_file():
        return [single]
    return sorted(CORPUS.glob(f"{stem}-case*.trace.in"))


def check() -> int:
    problems: list[str] = []
    claimed: set[str] = set()
    for fam in FAMILIES:
        golden_name, layout, stem = fam["golden"], fam["layout"], fam["stem"]
        golden = GOLDEN / golden_name
        if not golden.is_file():
            problems.append(f"{stem}: no golden {golden.relative_to(ROOT)}")
            continue
        # A subset family takes exactly the cases it names; otherwise every case
        # of the golden belongs in the corpus.
        want = len(fam["cases"]) if fam.get("cases") else max(golden_case_count(golden), 1)
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
            if fam.get("cases"):
                if meta.get("case") != fam["cases"][i - 1]:
                    problems.append(f"{f.name}: case {meta.get('case')!r} is not "
                                    f"{fam['cases'][i - 1]!r} (the subset this family takes)")
            elif want > 1 and meta.get("case_index") != i:
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
    for fam in FAMILIES:
        out = CORPUS / f"{fam['stem']}.trace.in"
        # A subset family takes exactly the cases it names -- passing the whole
        # golden would emit the other eight (--only-case is repeatable).
        subset = [x for c in fam.get("cases", []) for x in ("--only-case", c)]
        rc = subprocess.run(
            [sys.executable, str(GEN), "--golden", str(GOLDEN / fam["golden"]),
             "--platform", "x11", "--layout", fam["layout"], "--case", fam["stem"],
             "--out", str(out), *subset, *fam.get("args", [])]).returncode
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