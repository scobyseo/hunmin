#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""ctest `trace-emit-guard`: --emit must not silently re-baseline a .gold.

    python3 tools/trace-emit-guard-test.py

Issue #306: `tools/trace-check.py --emit` used to write the runner's output
over the committed expectation without comparing, so the cheapest way to make
a red tier-C case green was to regenerate the expectation -- and the gate then
asserted that the tool agrees with itself. The guard under test: a changed
expectation is printed as a unified diff and refused (exit 1, file untouched);
writing it requires `--emit --accept-changed`.

Self-contained on purpose: the fixture trace carries no `meta.delivery`, so
no hunmin-sim is asked, and the runner is a stub that prints a fixed trace --
the guard is about the WRITE path, not about adapter conformance, which the
`conformance-*` tests cover with the real emitters. Runs everywhere tier C
runs, including hosts with no frontend built.

Exit 0 pass, 1 fail.
"""
import json
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
CHECKER = ROOT / "tools" / "trace-check.py"

META = {"meta": {"platform": "x11", "layout": "dubeolsik", "case": "emit-guard"}}
EVENT = {"in": {"keysym": 0x006B, "mods": 0}, "commit": "", "preedit": "ㄱ",
         "consumed": True}
STALE = dict(EVENT, preedit="ㄴ")  # the committed expectation the runner disagrees with


def jsonl(*recs: dict) -> str:
    return "".join(json.dumps(r, ensure_ascii=False) + "\n" for r in recs)


def run_checker(work: pathlib.Path, *flags: str) -> subprocess.CompletedProcess:
    return subprocess.run(
        [sys.executable, str(CHECKER), str(work / "case.trace.in"),
         "--runner", f"{sys.executable} {work / 'stub-runner.py'}", *flags],
        capture_output=True, text=True, encoding="utf-8",
    )


def main() -> int:
    failures = []

    def expect(cond: bool, what: str) -> None:
        if not cond:
            failures.append(what)
            print(f"FAIL: {what}", file=sys.stderr)

    with tempfile.TemporaryDirectory(prefix="hunmin-emit-guard-") as tmp:
        work = pathlib.Path(tmp)
        trace_in = work / "case.trace.in"
        gold = work / "case.trace.gold"
        trace_in.write_text(jsonl(META, EVENT), encoding="utf-8")
        # The stub runner replays nothing: it prints the fixed trace the guard
        # must compare against the committed .gold before writing.
        (work / "stub-runner.py").write_text(
            "import sys\nsys.stdout.write(" + repr(jsonl(META, EVENT)) + ")\n",
            encoding="utf-8")
        gold.write_text(jsonl(META, STALE), encoding="utf-8")
        before = gold.read_bytes()

        # 1. A changed expectation: refuse, print the diff, leave the file.
        p = run_checker(work, "--emit")
        expect(p.returncode == 1, f"--emit on a changed .gold exits 1 (got {p.returncode})")
        expect(gold.read_bytes() == before, "--emit on a changed .gold leaves the file untouched")
        expect("---" in p.stderr and "+++" in p.stderr, "--emit prints a unified diff")
        expect("ㄴ" in p.stderr and "ㄱ" in p.stderr,
               "the diff shows both the committed and the runner's expectation")
        expect("--accept-changed" in p.stderr, "the refusal names the accept flag")

        # 2. The explicit act: write, and say so.
        p = run_checker(work, "--emit", "--accept-changed")
        expect(p.returncode == 0, f"--emit --accept-changed exits 0 (got {p.returncode})")
        expect(gold.read_text(encoding="utf-8") == jsonl(META, EVENT),
               "--emit --accept-changed writes the runner's output")

        # 3. An unchanged expectation: no refusal, no rewrite.
        mtime = gold.stat().st_mtime_ns
        p = run_checker(work, "--emit")
        expect(p.returncode == 0, f"--emit on an identical .gold exits 0 (got {p.returncode})")
        expect(gold.stat().st_mtime_ns == mtime, "--emit on an identical .gold does not rewrite it")

        # 4. A new expectation: nothing to re-baseline, written directly.
        gold.unlink()
        p = run_checker(work, "--emit")
        expect(p.returncode == 0, f"--emit with no .gold exits 0 (got {p.returncode})")
        expect(gold.exists() and gold.read_text(encoding="utf-8") == jsonl(META, EVENT),
               "--emit with no .gold writes it")

        # 5. The accept flag is meaningless without --emit: a usage error.
        p = run_checker(work, "--accept-changed")
        expect(p.returncode == 2, f"--accept-changed without --emit is a usage error (got {p.returncode})")

    if failures:
        print(f"{len(failures)} guard assertion(s) failed", file=sys.stderr)
        return 1
    print("ok: --emit refuses a silent re-baseline; --accept-changed is the explicit act")
    return 0


if __name__ == "__main__":
    sys.exit(main())
