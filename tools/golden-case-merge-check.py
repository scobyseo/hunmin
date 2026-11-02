#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Is a golden's case split load-bearing, or is it only labels?

A `{"meta": {"case": ...}}` line in a golden starts an INDEPENDENT case: tier A
replays each from a clean engine. That isolation is what makes a merge safe or
unsafe, and the property that decides it is not the case's length or subject --
it is whether the case ENDS with composition in flight. If it does, the next
case must start fresh and concatenating leaks the syllable into it; if every
case ends with an empty buffer, the boundaries carry nothing but names.

Measured 2026-11-03, when `p2-2-2-tiers`' eight three-event probes were merged
into one case (docs/SPEC-P2.md "P2 gate corpora") and the question was asked
whether the same applies elsewhere: it did not. Every other family ends its
cases mid-composition, and merging them fails tier A on the first case boundary.

This tool is the measurement, not a verdict. It reports, per golden, whether
merging every case into one replays identically under `hunmin-sim --check`, and
when it does not, what the first divergence is.

    python3 tools/golden-case-merge-check.py [--sim build/hunmin-sim]
"""
import argparse
import json
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]

# The same per-case engine options tools/gen-conformance.py knows about: a case
# names these in its meta, so one merged case cannot carry two different values.
OPTION_KEYS = frozenset({"caps", "w", "W", "z", "h", "H", "mode"})


def load_cases(golden: pathlib.Path):
    """(head, [(case_meta, events)]); a golden with no separators has one case."""
    lines = golden.read_text(encoding="utf-8").splitlines()
    head = json.loads(lines[0])
    cases: list[tuple[dict, list[dict]]] = []
    current: tuple[dict, list[dict]] | None = None
    for line in lines[1:]:
        rec = json.loads(line)
        if "meta" in rec and "case" in rec["meta"]:
            current = (rec["meta"], [])
            cases.append(current)
            continue
        if current is None:
            current = ({"case": "(implicit)"}, [])
            cases.append(current)
        current[1].append(rec)
    return head, cases


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--sim", default=str(ROOT / "build/hunmin-sim"))
    ap.add_argument("--data", default=str(ROOT / "libhunmin/data"))
    ap.add_argument("--dir", default=str(ROOT / "tests/golden"),
                    help="directory of *.jsonl goldens to report on")
    a = ap.parse_args()

    if not pathlib.Path(a.sim).is_file():
        raise SystemExit(f"no {a.sim} -- build first (cmake -B build && cmake --build build)")

    rows = []
    for golden in sorted(pathlib.Path(a.dir).glob("*.jsonl")):
        head, cases = load_cases(golden)
        if len(cases) < 2:
            continue
        ends_mid = sum(1 for _, ev in cases if ev and ev[-1].get("preedit", ""))
        # Engine OPTIONS, not descriptive keys ("note", "source", "kbd" ... are
        # provenance and merge fine). An option set that differs between cases
        # cannot be expressed by one merged case at all: a case has one engine
        # state, so caps-on and caps-off cases are not mergeable in principle.
        option_sets = {tuple(sorted((k, v) for k, v in m.items() if k in OPTION_KEYS))
                       for m, _ in cases}

        merged = [head, {"meta": {"case": "merged"}}]
        for _, ev in cases:
            merged.extend(ev)
        with tempfile.NamedTemporaryFile("w", suffix=".jsonl", delete=False,
                                         encoding="utf-8") as f:
            f.write("\n".join(json.dumps(r, ensure_ascii=False) for r in merged) + "\n")
            path = f.name
        proc = subprocess.run([a.sim, "--data", a.data, "--check", path],
                              capture_output=True, text=True)
        out = (proc.stdout + proc.stderr).splitlines()
        first = next((ln for ln in out if "MISMATCH" in ln or ln.startswith("OK")), "")
        if not first:
            first = out[0] if out else ""
        rows.append({
            "golden": golden.name,
            "cases": len(cases),
            "ends_mid": ends_mid,
            "option_sets": len(option_sets),
            "merge_rc": proc.returncode,
            "detail": first.strip()[:110],
        })

    w = max(len(r["golden"]) for r in rows) + 2
    print(f"{'golden'.ljust(w)}{'cases':>6}{'ends mid':>10}{'opt sets':>10}{'merge':>8}  detail")
    for r in rows:
        verdict = "OK" if r["merge_rc"] == 0 and r["option_sets"] == 1 else "fails"
        print(f"{r['golden'].ljust(w)}{r['cases']:>6}{r['ends_mid']:>10}"
              f"{r['option_sets']:>10}{verdict:>8}  {r['detail']}")
    mergeable = [r["golden"] for r in rows if r["merge_rc"] == 0 and r["option_sets"] == 1]
    print(f"\nmergeable: {', '.join(mergeable) if mergeable else '(none)'}")
    print("A case whose last event has a non-empty preedit ends MID-composition: its "
          "boundary is the isolation, not a label.")
    return 0


if __name__ == "__main__":
    sys.exit(main())