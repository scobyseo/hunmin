#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""ci-test-timing.py -- say where a ctest run spent its seconds, from the run.

WHY. `ctest` prints a per-test time and a label summary, but only in the middle
of a long log, and only for the run that just happened. A CI reader (or the
artefact, after the fact) needs three questions answered from the run itself:

  what configuration did this run use, which tests registered, where did the
  time go.

tools/ci-env-snapshot.sh answers the first two before the suite runs. This
answers the third, from ctest's own JUnit output, and it runs ALWAYS -- a green
run that took 3x longer than the last one is a finding, not a non-event.

usage: ci-test-timing.py [--junit build/junit.xml] [--top 10]
                         [--summary "$GITHUB_STEP_SUMMARY"]

Exit 0 always: this is a report, not a gate. Budget enforcement is
tools/ci-time-budget.sh, which is a local selection on a known core count.
"""
from __future__ import annotations

import argparse
import pathlib
import sys
import xml.etree.ElementTree as ET


def _rows(path: pathlib.Path) -> list[tuple[str, float, str]]:
    """(name, seconds, status) per test case, from ctest's JUnit output."""
    root = ET.parse(path).getroot()
    time_attr = "time" if root.tag == "testsuite" else None
    rows: list[tuple[str, float, str]] = []
    for suite in root.iter("testsuite"):
        for case in suite.iter("testcase"):
            status = "pass"
            for child in case:
                if child.tag in ("failure", "error"):
                    status = "fail"
                elif child.tag == "skipped":
                    status = "skip"
            # ctest names the suite after the build configuration
            # ("Linux-c++"), not after a test, so the case name is the whole
            # identity of the row -- prefixing it would only add noise.
            rows.append((case.get("name", "?"),
                         float(case.get("time", "0") or 0), status))
    if time_attr is not None and not rows:
        raise SystemExit(f"{path}: no <testcase> elements")
    return rows


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--junit", default="build/junit.xml",
                    help="ctest --output-junit file (default: build/junit.xml)")
    ap.add_argument("--top", type=int, default=10,
                    help="how many of the slowest tests to list (default 10)")
    ap.add_argument("--summary", default=None,
                    help="append the same report to this file as markdown "
                         "(GitHub: $GITHUB_STEP_SUMMARY)")
    args = ap.parse_args()

    path = pathlib.Path(args.junit)
    if not path.is_file():
        # Not an error: a run configured without --output-junit still has its
        # per-test times in the ctest log. Say so rather than fail the job.
        print(f"no {path}: no JUnit output to summarise "
              f"(did ctest run with --output-junit?)")
        return 0

    rows = _rows(path)
    total = sum(t for _, t, _ in rows)
    failed = [r for r in rows if r[2] == "fail"]
    skipped = [r for r in rows if r[2] == "skip"]
    slowest = sorted(rows, key=lambda r: -r[1])[: args.top]

    lines = [
        f"tests: {len(rows)}  passed: {len(rows) - len(failed) - len(skipped)}  "
        f"failed: {len(failed)}  skipped: {len(skipped)}",
        f"sum of per-test times: {total:.1f} s "
        f"(wall clock is ctest's own total, which is smaller under -j)",
        "",
        f"slowest {len(slowest)}:",
    ]
    for name, secs, status in slowest:
        lines.append(f"  {secs:7.2f} s  {status:4}  {name}")
    if failed:
        lines += ["", "failed:"] + [f"  {n}" for n, _, _ in failed]
    if skipped:
        lines += ["", "skipped (a skip is not a pass):"] + \
                 [f"  {n}" for n, _, _ in skipped]

    text = "\n".join(lines)
    print(text)
    if args.summary:   # empty outside GitHub, where the variable does not exist
        with open(args.summary, "a", encoding="utf-8") as fh:
            fh.write("\n### ctest timing\n\n```\n" + text + "\n```\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())