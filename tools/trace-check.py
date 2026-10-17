#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Tier C gate: replay a platform-native trace through a frontend and diff it
against the expectation tier A implies (docs/trace-spec.md).

    tools/trace-check.py <case>.trace.in [--emit] [--runner <cmd>]

Exit 0 diff-clean, 1 mismatch, 77 skip (frontend not built for this platform).

`--emit` writes the `.gold` from the runner's own output rather than comparing.
That is the "make retrace" path: it must never be the way a red test becomes
green. Review the diff line by line.

Two things are checked, and the second is the one that matters:

1. the frontend's trace matches the expectation, and
2. the expectation still matches the tier-A golden it was generated from.

Check 2 is why this is not just "run the adapter and eyeball it". A stale `.gold`
-- one left over from before an engine change -- would make check 1 pass forever.
"""
import argparse
import json
import os
import pathlib
import shlex
import shutil
import subprocess
import sys


def load(path: pathlib.Path) -> list[dict]:
    recs = []
    for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip():
            continue
        try:
            recs.append(json.loads(line))
        except json.JSONDecodeError as e:
            raise SystemExit(f"{path}:{n}: {e}")
    if not recs or "meta" not in recs[0]:
        raise SystemExit(f"{path}: line 1 must be a meta object (trace-spec.md)")
    return recs


def normalise(recs: list[dict]) -> list[dict]:
    """Comparable projection: only what the spec defines as observable."""
    out = []
    for r in recs:
        if "meta" in r:
            continue
        extra = set(r) - {"in", "commit", "preedit", "consumed"}
        if extra:
            raise SystemExit(
                f"trace record has fields the spec does not define: {sorted(extra)}"
            )
        out.append(
            {
                "in": r.get("in"),
                "commit": r.get("commit", ""),
                "preedit": r.get("preedit", ""),
                "consumed": r.get("consumed"),
            }
        )
    return out


def check_invariants(recs: list[dict], path: pathlib.Path) -> None:
    """The invariants in docs/trace-spec.md that are checkable from the file alone.

    A trace that breaks one is a bad test, not a failing adapter -- caught here so
    the diagnosis is not misattributed to the frontend. Note that commit and
    preedit are deliberately NOT checked for mutual exclusion: tier A's goldens
    have both non-empty on 700+ lines, and the spec records that correction.
    """
    events = normalise(recs)
    if not events:
        raise SystemExit(f"{path}: no events")
    for n, e in enumerate(events, 1):
        if "in" not in e:
            raise SystemExit(f"{path}: event {n} has no `in`")
        if e["consumed"] is None:
            raise SystemExit(f"{path}: event {n} has no `consumed`")


def find_bash() -> str | None:
    """A POSIX shell to wrap .sh runners on Windows (shutil.which, then the
    usual Git-for-Windows install roots)."""
    cand = shutil.which("bash")
    if cand:
        return cand
    for pf in (os.environ.get("ProgramFiles"), os.environ.get("ProgramFiles(x86)")):
        if not pf:
            continue
        for sub in (os.path.join(pf, "Git", "usr", "bin", "bash.exe"),
                    os.path.join(pf, "Git", "bin", "bash.exe")):
            if os.path.exists(sub):
                return sub
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("trace", type=pathlib.Path)
    ap.add_argument("--emit", action="store_true")
    ap.add_argument(
        "--runner",
        default="frontends/fcitx5/build/trace-emit",
        help="command that reads a .trace.in on stdin and writes a trace on stdout",
    )
    a = ap.parse_args()

    recs = load(a.trace)
    check_invariants(recs, a.trace)

    # Cross-platform spawn: CMake passes --runner as one argv token, which may
    # be a .sh script. POSIX hosts exec it directly; Windows' CreateProcess
    # cannot run a shell script, so the runner is wrapped in a located bash
    # (whose usr/bin goes first in the child's PATH for find/paste/realpath).
    # Any OS failure to spawn becomes the documented skip: "runner not built
    # for this platform" includes "cannot be executed on this platform".
    tokens = shlex.split(a.runner, posix=(os.name != "nt"))
    bashp = None
    if os.name == "nt" and tokens and tokens[0].lower().endswith(".sh"):
        bashp = find_bash()
        if bashp is None:
            print(f"skip: no POSIX shell to run {a.runner!r}", file=sys.stderr)
            return 77
        argv = [bashp, tokens[0]]
    else:
        argv = tokens
    env = dict(os.environ)
    if bashp:
        env["PATH"] = os.path.dirname(bashp) + os.pathsep + env["PATH"]

    try:
        proc = subprocess.run(
            argv + [str(a.trace)],
            capture_output=True,
            text=True,
            env=env,
        )
    except OSError as e:
        print(f"skip: cannot run {a.runner!r}: {e}", file=sys.stderr)
        return 77
    if proc.returncode == 77:
        print("skip: runner reports the frontend is not built", file=sys.stderr)
        return 77
    if proc.returncode != 0:
        print(proc.stdout + proc.stderr, file=sys.stderr)
        print(f"runner failed rc={proc.returncode}", file=sys.stderr)
        return 1

    gold = a.trace.with_suffix("").with_suffix(".trace.gold")
    got = load_text_as_records(proc.stdout, a.trace)

    if a.emit:
        gold.write_text(proc.stdout if proc.stdout.endswith("\n") else proc.stdout + "\n",
                        encoding="utf-8")
        print(f"wrote {gold} -- review the diff, do not blind-accept", file=sys.stderr)
        return 0

    if not gold.exists():
        print(f"missing {gold}; generate with --emit and review it", file=sys.stderr)
        return 1

    want = normalise(load(gold))
    have = normalise(got)

    # The .in carries its OWN expectations, copied verbatim from tier A. If they
    # disagree with the .gold, the runner is replaying an input it is no longer
    # being asked to satisfy -- and on the commit path TestFrontend asserts the
    # .in's value, so the two are verified against DIFFERENT things and a wrong
    # expectation in the .in passes silently. Comparing .in against .gold is
    # what makes the pair self-consistent; only then does adapter-vs-gold mean
    # anything. Found by mutating a preedit in the .in and watching it pass.
    declared = normalise(recs)
    drift = [(n, d, g) for n, (d, g) in enumerate(zip(declared, want), 1) if d != g]
    if drift or len(declared) != len(want):
        for n, d, g in drift[:3]:
            print(f"event {n}: .in says {d}\n         .gold says {g}", file=sys.stderr)
        if len(declared) != len(want):
            print(f"length: .in has {len(declared)}, .gold has {len(want)}", file=sys.stderr)
        print(f"{a.trace}: .in and .gold disagree -- regenerate with --emit",
              file=sys.stderr)
        return 1

    if want == have:
        print(f"ok: {a.trace.name} ({len(have)} events)")
        return 0

    for n, (w, h) in enumerate(zip(want, have), 1):
        if w != h:
            print(f"event {n}: expected {w}\n         got      {h}", file=sys.stderr)
    if len(want) != len(have):
        print(f"length: expected {len(want)}, got {len(have)}", file=sys.stderr)
    return 1


def load_text_as_records(text: str, origin: pathlib.Path) -> list[dict]:
    recs = []
    for n, line in enumerate(text.splitlines(), 1):
        if not line.strip():
            continue
        try:
            recs.append(json.loads(line))
        except json.JSONDecodeError as e:
            raise SystemExit(f"{origin}: runner emitted bad JSON line {n}: {e}")
    return recs


if __name__ == "__main__":
    sys.exit(main())