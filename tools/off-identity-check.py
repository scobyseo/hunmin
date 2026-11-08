#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Assert the SPEC-MODES §4 invariant: OFF returns 100% of the previous behaviour.

docs/SPEC-MODES.md §4 states it as a requirement, not a preference:

    "When it is off it must return to 100% of the previous behaviour. This is
     an invariant. It is verifiable and must be verified."

The oracle is the corpus that was already there. Every tier-A golden in
`tests/golden/` was recorded before the judgement existed, and `--check`
still asserts those expectations against `hunmin_ic`, the engine the frontends
sat on then. So nothing new is generated here: the SAME corpus is driven one
level up, through `hunmin_buffered_preedit` — the layer every frontend now sits
on — with `autoswitch` written to 0 and read back, and the rest of the feature
present (the Latin layout and the shipped English list), so the option is the
only thing standing between the layer and a different delivery.

`hunmin-sim --off-check` does the comparison; this script is the census around
it. `tests/golden/judgement-r8.jsonl` is driven too, as pairs: for every case
whose corpus entry says the judgement switches, the same keys with `autoswitch`
0 must deliver what the engine delivers (R15), and the switch itself is the
control -- a case that no longer switches fails rather than being skipped. It exists because a check that runs a GLOBBED corpus has one failure mode a
green run cannot show: a file that silently stops being driven. So every globbed
file must appear in the tool's own "OK <path>" line, and the exclusions below are
named with the test that covers them instead — an exclusion without a home is
how a corpus shrinks by one file and stays green.

    tools/off-identity-check.py [--sim build/hunmin-sim] [--data libhunmin/data]
                               [--golden-dir tests/golden]
                               [--trace-dir tests/conformance/x11]

Exit 0 = every file driven and every byte equal; 1 = a mismatch or a file that
was not driven; 2 = the tool or the corpus is missing (a check that cannot read
what it guards is not a check).
"""
import argparse
import pathlib
try:
    from _common import ROOT
except ImportError:
    ROOT = pathlib.Path(__file__).resolve().parents[1]
import subprocess
import sys

# Not driven here, and each one has a home that does drive it. A golden with no
# home belongs in this list only with the ctest id that replaced it, because the
# alternative -- silently skipping the file -- is the one way this check could
# shrink without going red.
# Empty today, and kept for the reason it exists: a file that cannot be driven
# has to name the test that covers it instead, because the alternative -- a
# silent skip -- is the one way this corpus could shrink without going red. The
# judgement corpus used to be here (its cases store a key STRING, not per-key
# `in` records); it is driven now, as the pairs R15 is about.
EXCLUDED: dict[str, str] = {}


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--sim", default=str(ROOT / "build" / "hunmin-sim"),
                    help="the built hunmin-sim (default: build/hunmin-sim)")
    ap.add_argument("--data", default=str(ROOT / "libhunmin" / "data"),
                    help="the data directory to load layouts from")
    ap.add_argument("--golden-dir", default=str(ROOT / "tests" / "golden"))
    ap.add_argument("--trace-dir", default=str(ROOT / "tests" / "conformance" / "x11"))
    args = ap.parse_args(argv)

    sim = pathlib.Path(args.sim)
    data = pathlib.Path(args.data)
    if not sim.is_file():
        print(f"FAIL: {sim} is not built -- configure and build hunmin-sim first", file=sys.stderr)
        return 2
    if not data.is_dir():
        print(f"FAIL: {data} is not a data directory", file=sys.stderr)
        return 2

    goldens = sorted(pathlib.Path(args.golden_dir).glob("*.jsonl"))
    traces = sorted(pathlib.Path(args.trace_dir).glob("*.trace.in"))
    if not goldens:
        print(f"FAIL: no goldens under {args.golden_dir}", file=sys.stderr)
        return 2
    if not traces:
        print(f"FAIL: no traces under {args.trace_dir}", file=sys.stderr)
        return 2

    driven, skipped = [], []
    for f in goldens + traces:
        home = EXCLUDED.get(f.name) if f.parent == pathlib.Path(args.golden_dir) else None
        if home:
            skipped.append((f, home))
        else:
            driven.append(f)

    cmd = [str(sim), "--data", str(data), "--off-check", *map(str, driven)]
    r = subprocess.run(cmd, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       text=True, errors="replace")
    out = r.stdout or ""
    print(out, end="" if out.endswith("\n") else "\n")

    # The census: every file this script decided to drive must have been driven.
    # `hunmin-sim` prints one "OK  <path>" per file that made no comparison fail,
    # so the set difference is exactly the files that were never reached or that
    # failed. A green exit with a file missing would be the silent shrink.
    reported = set()
    for line in out.splitlines():
        if line.startswith("OK  "):
            reported.add(pathlib.Path(line[4:].rsplit(" (", 1)[0]).resolve())
    missing = [f for f in driven if f.resolve() not in reported]
    for f in skipped:
        print(f"skip: {f[0].relative_to(ROOT)} -- not driven here; covered by ctest {f[1]}")
    for f in missing:
        print(f"FAIL: {f.relative_to(ROOT)} was selected and never reported", file=sys.stderr)
    print(f"census: {len(reported)}/{len(driven)} selected files reported "
          f"({len(goldens)} goldens and {len(traces)} x11 traces in the corpus, "
          f"{len(skipped)} named here with the test that covers them)")

    if r.returncode != 0 or missing:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))