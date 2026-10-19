#!/usr/bin/env python3
"""The tier-F1 rc-77 skip contract, decided OUTSIDE the test binary.

A gtest skip reports exit 0, which ctest reads as a pass -- the
"green-but-meaningless report" the suite's skip contract exists to prevent
(docs/TESTING.md "skip is exit 77"). gtest cannot express a nonzero exit for
a skip, so the data-presence probes that the runners used to carry in a
hand-written main() run here instead: the wrapper checks the data, prints a
reason and exits 77 on absence, and otherwise runs the test binary and passes
its exit code through.

Probe modes (both are run-time file checks):
  --data DIR   the repo tree's engine data (tsf-config/tsf-settings): DIR
               must hold layouts/*.json. A tree that is present but does not
               PARSE is not this probe's business -- the binary's own SetUp
               asserts the context and reads red, the same class the golden
               tiers use for converter regressions.
  --staged     the POST_BUILD-staged copy beside the test executable
               (tsf-datadir): <exe-dir>/data/layouts must exist.

Everything after the executable path is passed through unchanged
(--gtest_filter selects the registered entry's case group).
"""
import argparse
import os
import subprocess
import sys


def probe_data(data_dir):
    layouts = os.path.join(data_dir, "layouts")
    if not os.path.isdir(layouts):
        return f"skip: engine data unavailable at {layouts}"
    if not any(f.endswith(".json") for f in os.listdir(layouts)):
        return f"skip: no layout json under {layouts}"
    return None


def probe_staged(exe):
    exe_dir = os.path.dirname(os.path.abspath(exe))
    staged = os.path.join(exe_dir, "data", "layouts")
    if not os.path.isdir(staged):
        return (f"skip: staged data missing at {staged} "
                "(POST_BUILD staging did not run)")
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data")
    ap.add_argument("--staged", action="store_true")
    ap.add_argument("exe")
    ap.add_argument("passthrough", nargs=argparse.REMAINDER)
    args = ap.parse_args()

    if args.staged:
        reason = probe_staged(args.exe)
        if reason:
            print(reason, file=sys.stderr)
            return 77
    if args.data:
        reason = probe_data(args.data)
        if reason:
            print(reason, file=sys.stderr)
            return 77

    run = subprocess.run([args.exe] + args.passthrough)
    return run.returncode


if __name__ == "__main__":
    sys.exit(main())
