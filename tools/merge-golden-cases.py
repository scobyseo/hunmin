#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Merge a golden's cases into one case per engine-option set, joined by spaces.

A `{"meta": {"case": ...}}` line starts an INDEPENDENT case: tier A replays each
from a clean engine. That split costs a trace file per case in tier C, and for a
golden whose cases are short probes of one rule it buys only names.

What makes a merge sound is not "the buffer is empty at the end" -- measured
2026-11-03, `patim-sin3-p2-basic` ends all 37 cases empty and still diverges when
concatenated, because a repeated key doubles the consonant across the boundary.
What makes it sound is a BOUNDARY KEY: a space is declined by the engine, so it
finalises the syllable in flight and leaves the next key starting clean. Its own
expectation is mechanical, not engine-read: the syllable that was in preedit is
committed, the buffer empties, and in `total` schemas the text is unchanged.

So the merge is:

    case A events | space | case B events | space | ...

with each space line carrying `commit: <A's last preedit>, preedit: ""` (or
`total: <A's last total>`), and every other line copied verbatim. Cases whose
per-case meta differs in an ENGINE OPTION (`caps`, `w`, `W`, `z`, `h`, `H`,
`mode`) cannot share a case -- one case has one engine state -- so they are
grouped, one merged case per distinct option set.

The tool refuses to write unless `hunmin-sim --check` replays the merged file
identically, and prints the old-label to merged-offset map so the diff can be
reviewed by a human rather than trusted.

    python3 tools/merge-golden-cases.py tests/golden/p2-2-1-basic.jsonl        # report
    python3 tools/merge-golden-cases.py tests/golden/p2-2-1-basic.jsonl --write
"""
import argparse
import json
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
OPTION_KEYS = frozenset({"caps", "w", "W", "z", "h", "H", "mode"})
SPACE = {"ch": " "}


def parse(golden: pathlib.Path):
    lines = golden.read_text(encoding="utf-8").splitlines()
    head = json.loads(lines[0])
    cases: list[dict] = []
    for line in lines[1:]:
        rec = json.loads(line)
        if "meta" in rec and "case" in rec["meta"]:
            cases.append({"meta": rec["meta"], "ev": []})
            continue
        if not cases:  # a golden with no separators is one implicit case
            cases.append({"meta": {"case": "(implicit)"}, "ev": []})
        cases[-1]["ev"].append(rec)
    return head, cases


def space_line(prev: dict) -> dict:
    """The boundary between two cases: a declined key that finalises the buffer."""
    if "total" in prev:
        return {"in": dict(SPACE), "total": prev["total"]}
    return {"in": dict(SPACE), "commit": prev.get("preedit", ""), "preedit": ""}


def merge(head: dict, cases: list[dict], stem: str) -> tuple[list[str], list[str]]:
    groups: dict[tuple, list[dict]] = {}
    for c in cases:
        opt = tuple(sorted((k, v) for k, v in c["meta"].items() if k in OPTION_KEYS))
        groups.setdefault(opt, []).append(c)

    out: list[str] = [json.dumps(head, ensure_ascii=False)]
    mapping: list[str] = []
    for n, (opt, group) in enumerate(groups.items(), 1):
        label = f"{stem}-merged" if len(groups) == 1 else f"{stem}-merged-{n}"
        meta = {"case": label, **{k: v for k, v in opt}}
        out.append(json.dumps({"meta": meta}, ensure_ascii=False))
        for i, c in enumerate(group):
            if i:
                out.append(json.dumps(space_line(group[i - 1]["ev"][-1]), ensure_ascii=False))
            mapping.append(f"  {c['meta']['case']!r} -> event {len(out) - 1} of case {label!r}")
            out.extend(json.dumps(r, ensure_ascii=False) for r in c["ev"])
    return out, mapping


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("golden", type=pathlib.Path)
    ap.add_argument("--sim", default=str(ROOT / "build/hunmin-sim"))
    ap.add_argument("--write", action="store_true")
    a = ap.parse_args()

    head, cases = parse(a.golden)
    if len(cases) < 2:
        print(f"{a.golden.name}: {len(cases)} case -- nothing to merge")
        return 0

    merged, mapping = merge(head, cases, a.golden.stem)
    body = "\n".join(merged) + "\n"

    with tempfile.NamedTemporaryFile("w", suffix=".jsonl", delete=False, encoding="utf-8") as f:
        f.write(body)
        tmp = f.name
    proc = subprocess.run([a.sim, "--data", str(ROOT / "libhunmin/data"), "--check", tmp],
                          capture_output=True, text=True)
    if proc.returncode != 0:
        print(f"{a.golden.name}: merging CHANGES the expectations -- refusing", file=sys.stderr)
        print((proc.stdout + proc.stderr).strip()[:2000], file=sys.stderr)
        return 1

    groups = len({tuple(sorted((k, v) for k, v in c["meta"].items() if k in OPTION_KEYS))
                  for c in cases})
    print(f"{a.golden.name}: {len(cases)} case(s) -> {groups} merged case(s), "
          f"replay identical ({(proc.stdout + proc.stderr).strip().splitlines()[0]})")
    print("\n".join(mapping))
    if a.write:
        a.golden.write_text(body, encoding="utf-8")
        print(f"wrote {a.golden}")
    return 0


if __name__ == "__main__":
    sys.exit(main())