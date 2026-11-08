#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""EXPERIMENT (task/MUT-options) — instrumented driver for frontend-mutations.py.

A measurement harness, not a proposal. It imports the tier's own module and
instruments it, so the tier's wall clock can be split into named phases:

    copytree / configure / first-build   sandbox setup
    build                               per-mutation `cmake --build`
    checker                             check-frontend-invariants.py
    d1 / d2                             the two `ctest -R ...` subprocesses

With `--mode vanilla` (the default) the tier's own `main()` runs unmodified and
its exit status is propagated verbatim; only timing bookkeeping is added. The
other modes re-implement the same loop with ONE axis changed, and are labelled
as such in every record they write.

    --mode vanilla        instrument the tier exactly as it is
    --mode targets        build only --targets (default: the tier's MUT_TARGETS)
    --mode skip-checked   for a mutation the checker rejects (expect_checker
                          True), run the checker and stop -- no build, no d1/d2
    --mode setup-only     sandbox setup and nothing else; the cost every
                          parallel worker would pay on top of its own build
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile
import time
from types import ModuleType
try:  # issue #272 phase 2: one ROOT derivation, with a file-relative fallback
    from _common import ROOT as _COMMON_ROOT
except ImportError:  # run from outside tools/: derive from this file
    _COMMON_ROOT = __import__("pathlib").Path(__file__).resolve().parent.parent

REPO = _COMMON_ROOT


def load(name: str, rel: str) -> ModuleType:
    """Import the tier's module from its path rather than by name.

    It is a script with a hyphen in the filename, so it cannot be imported
    normally; this is how the harness reaches the tier's own MUTATIONS,
    MUT_TARGETS, _apply and _run instead of restating them.
    """
    spec = importlib.util.spec_from_file_location(name, REPO / rel)
    if spec is None or spec.loader is None:
        raise SystemExit(f"cannot load {rel}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _classify(cmd: list[str]) -> str:
    joined = " ".join(cmd)
    if "--build" in cmd:
        return "build"
    if "check-frontend-invariants" in joined:
        return "checker"
    if "engine-d1-fcitx5" in joined:
        return "d1"
    if "engine-d2-ui" in joined:
        return "d2"
    return "other"


class Timer:
    """Records (phase, seconds) for every subprocess and setup step."""

    def __init__(self, quiet: bool) -> None:
        self.records: list[dict] = []
        self.quiet = quiet
        self.in_setup = False

    def add(self, phase: str, sec: float, rc: int = 0, cmd: str = "") -> None:
        self.records.append({"phase": phase, "sec": round(sec, 4), "rc": rc,
                             "cmd": cmd})
        if not self.quiet:
            print(f"  [{sec:7.3f}s rc={rc:>3}] {phase:12} {cmd}", file=sys.stderr)


def install_instrumentation(fm, timer: Timer) -> None:
    """Time every _run() the tier makes, and Sandbox.__enter__ in three parts."""
    real_run = fm._run

    def timed_run(cmd, timeout=180, cwd=None):
        label = _classify(cmd)
        if timer.in_setup and label == "build":
            label = "first-build"
        t0 = time.perf_counter()
        rc = real_run(cmd, timeout=timeout, cwd=cwd)
        timer.add(label, time.perf_counter() - t0, rc, " ".join(cmd[-4:]))
        return rc

    fm._run = timed_run

    def timed_enter(self):
        timer.in_setup = True
        self.dir = pathlib.Path(tempfile.mkdtemp(prefix="hunmin-mut-sandbox."))
        src = self.dir / "src"
        if not timer.quiet:
            print(f"mutation sandbox: {src}", file=sys.stderr)
        t0 = time.perf_counter()
        shutil.copytree(fm.REPO, src, symlinks=True, ignore=fm._COPY_IGNORE,
                        ignore_dangling_symlinks=True)
        timer.add("copytree", time.perf_counter() - t0, 0, str(src))
        self._backup = (src / fm.ENGINE_REL).read_text()
        t0 = time.perf_counter()
        rc = real_run(["cmake", "-B", "build",
                       f"-DCMAKE_BUILD_TYPE={self._build_type()}"], cwd=src)
        timer.add("configure", time.perf_counter() - t0, rc, "cmake -B build")
        if rc != 0:
            raise SystemExit("sandbox: cmake configure failed")
        # _build_in goes through fm._run, which the wrapper above has already
        # timed as "first-build". Do not time it a second time here.
        t0 = time.perf_counter()
        rc = self._build_in(src)
        timer.in_setup = False
        if rc != 0:
            raise SystemExit("sandbox: initial build failed")
        return src

    fm.Sandbox.__enter__ = timed_enter


def _ctest(fm, timer: Timer, root, pattern: str) -> tuple[int, bool]:
    """Run one ctest -R and report (rc, did_the_test_actually_run).

    ctest returns 8 both for "the test failed" and for "the test did not run"
    (missing binary, disabled, no match). The tier reads rc != 0 as "the guard
    rejected this", so a variant that stops building a binary would look like a
    pass. This wrapper looks at the output so that case is visible.
    """
    cmd = ["ctest", "--test-dir", str(root / "build"), "-R", pattern]
    t0 = time.perf_counter()
    p = subprocess.run(cmd, cwd=root, capture_output=True, text=True)
    timer.add(pattern.split("-")[-1], time.perf_counter() - t0, p.returncode,
              f"-R {pattern}")
    ran = "Not Run" not in p.stdout
    return p.returncode, ran


def loop_with_targets(fm, timer: Timer, targets: list[str], ids: list[str] | None):
    """The tier's own loop, with the build's target list replaced.

    Everything else -- the mutation table, the anchor check, the expectation
    comparison, the restore contract, the exit code -- is the tier's code, not
    this file's, so a difference in verdicts between here and --mode vanilla is
    a defect in this harness rather than a finding.

    NOTE on what this measures. Sandbox setup still builds the tier's full
    MUT_TARGETS (setup calls fm._build_cmd, which this mode does not replace),
    so both test binaries exist before the first mutation. Narrowing the
    per-mutation build therefore measures "does rebuilding fewer targets after
    the fact cost anything", NOT "can the tier get away with never building
    them" -- the latter is a different experiment and its answer is in
    docs/OPTIONS-MEASURED.md §MUT: `ctest -R` on a missing binary returns 8,
    which the tier reads as a rejection.

    Every ctest invocation is also checked for "Not Run", because rc alone
    cannot distinguish "the guard rejected this" from "the guard never ran".
    """
    def loop() -> int:
        gaps: list[str] = []
        notrun: list[str] = []
        print(f"{'mutation':30} {'build':6} {'checker':9} {'d1':6} {'d2':6} verdict")
        with fm.Sandbox() as root:
            engine = root / fm.ENGINE_REL
            checker = root / fm.CHECKER_REL
            original = engine.read_text()
            print("strategy: sandbox (mutbench: target set "
                  f"{','.join(targets)})")
            for mid, old, new, want_chk, want_d1, want_d2 in fm.MUTATIONS:
                if ids and mid not in ids:
                    continue
                mutated = fm._apply(original, old, new)
                if mutated is None:
                    print(f"{mid:30} {'-':6} {'-':9} {'-':6} ANCHOR NOT FOUND")
                    gaps.append(f"{mid}: anchor not found (source moved)")
                    continue
                engine.write_text(mutated)
                # fm._run is the instrumented wrapper, so it already records
                # this build as "build". Timing it again here would double it.
                cmd = ["cmake", "--build", str(root / "build"), "--target",
                       *targets, "-j", fm.JOBS]
                rc = fm._run(cmd, cwd=root, timeout=900)
                if rc != 0:
                    print(f"{mid:30} {'fail':<6} {'-':9} {'-':6} build rejects")
                    continue
                chk = fm._run([sys.executable, str(checker)], cwd=root)
                d1_rc, d1_saw_run = _ctest(fm, timer, root, "engine-d1-fcitx5")
                d2_rc, d2_saw_run = _ctest(fm, timer, root, "engine-d2-ui")
                if not d1_saw_run:
                    notrun.append(f"{mid}: engine-d1-fcitx5 did not run")
                if not d2_saw_run:
                    notrun.append(f"{mid}: engine-d2-ui did not run")
                chk_rej, d1_rej, d2_rej = chk != 0, d1_rc != 0, d2_rc != 0
                guarded = (chk_rej and d1_rej) or (chk_rej and d2_rej) or d2_rej
                verdict = "guarded" if guarded else "GAP"
                for got, want, col in ((chk_rej, want_chk, "checker"),
                                       (d1_rej, want_d1, "d1"),
                                       (d2_rej, want_d2, "d2")):
                    if got != want:
                        gaps.append(f"{mid}: {col}_rejects={got}, expected {want}")
                print(f"{mid:30} {'ok':<6} {str(chk_rej):9} {str(d1_rej):6} "
                      f"{str(d2_rej):6} {verdict}")
        print()
        if notrun:
            print(f"{len(notrun)} test(s) DID NOT RUN (rc alone would have read "
                  "as a rejection):")
            for n in notrun[:4]:
                print("  -", n)
            return 1
        if gaps:
            print(f"{len(gaps)} expectation mismatch(es):")
            for g in gaps:
                print("  -", g)
            return 1
        print("mutation set matches its recorded expectations (mutbench)")
        return 0

    return loop


def loop_skip_checked(fm, timer: Timer, ids: list[str] | None):
    """The tier's loop with ONE change: a mutation the checker is recorded to
    reject stops after the checker, so no build and no d1/d2 run.

    The information this discards is exactly the build-layer columns: for those
    mutations the table still records what `engine-d1-fcitx5` and
    `engine-d2-ui` do, and this loop no longer observes them. The rows it
    writes say "not measured" rather than "passes".
    """
    def loop() -> int:
        gaps: list[str] = []
        print(f"{'mutation':30} {'build':6} {'checker':9} {'d1':6} {'d2':6} verdict")
        with fm.Sandbox() as root:
            engine = root / fm.ENGINE_REL
            checker = root / fm.CHECKER_REL
            original = engine.read_text()
            print("strategy: sandbox (mutbench: checker-only for expect_checker)")
            for mid, old, new, want_chk, want_d1, want_d2 in fm.MUTATIONS:
                if ids and mid not in ids:
                    continue
                mutated = fm._apply(original, old, new)
                if mutated is None:
                    print(f"{mid:30} {'-':6} {'-':9} {'-':6} ANCHOR NOT FOUND")
                    gaps.append(f"{mid}: anchor not found (source moved)")
                    continue
                engine.write_text(mutated)
                if not want_chk:
                    # Not recorded as checker-caught, so the build layers are
                    # the only verdict there is. Falls through to the full path.
                    # fm._run is the instrumented wrapper and records the
                    # build itself; do not time it a second time.
                    rc = fm._run(fm._build_cmd(root / "build"), cwd=root,
                                 timeout=900)
                    if rc != 0:
                        print(f"{mid:30} {'fail':<6} {'-':9} {'-':6} "
                              "build rejects")
                        continue
                    chk = fm._run([sys.executable, str(checker)], cwd=root)
                    d1 = fm._run(["ctest", "--test-dir", str(root / "build"),
                                  "-R", "engine-d1-fcitx5"], cwd=root)
                    d2 = fm._run(["ctest", "--test-dir", str(root / "build"),
                                  "-R", "engine-d2-ui"], cwd=root)
                    chk_rej, d1_rej, d2_rej = chk != 0, d1 != 0, d2 != 0
                else:
                    t0 = time.perf_counter()
                    chk = fm._run([sys.executable, str(checker)], cwd=root)
                    timer.add("checker-only", time.perf_counter() - t0, chk, mid)
                    chk_rej, d1_rej, d2_rej = chk != 0, None, None
                guarded = chk_rej and (want_chk or want_d1 or want_d2)
                verdict = "guarded" if guarded else "GAP"
                if chk_rej != want_chk:
                    gaps.append(f"{mid}: checker_rejects={chk_rej}, "
                                f"expected {want_chk}")
                d1s = "n/m" if d1_rej is None else str(d1_rej)
                d2s = "n/m" if d2_rej is None else str(d2_rej)
                print(f"{mid:30} {'skip' if d1_rej is None else 'ok':<6} "
                      f"{str(chk_rej):9} {d1s:6} {d2s:6} {verdict}")
        print()
        if gaps:
            print(f"{len(gaps)} expectation mismatch(es):")
            for g in gaps:
                print("  -", g)
            return 1
        print("checker verdicts match; build-layer columns NOT measured "
              "(mutbench)")
        return 0

    return loop


def loop_setup_only(fm, timer: Timer):
    """Sandbox setup, then a clean hunminadapter.cpp and nothing else.

    This is the per-worker cost of running the nine mutations in parallel: each
    worker needs its own copy, its own configure and its own first build
    before it can do any work at all.
    """
    def loop() -> int:
        with fm.Sandbox() as root:
            engine = root / fm.ENGINE_REL
            engine.write_text((root / fm.ENGINE_REL).read_text())
        return 0
    return loop


def report(timer: Timer, wall: float, run_i: int, n: int, label: str) -> None:
    totals: dict[str, float] = {}
    counts: dict[str, int] = {}
    for r in timer.records:
        totals[r["phase"]] = totals.get(r["phase"], 0.0) + r["sec"]
        counts[r["phase"]] = counts.get(r["phase"], 0) + 1
    acc = sum(totals.values())
    print(f"\n=== run {run_i}/{n}: wall {wall:.3f}s  [{label}]", file=sys.stderr)
    for k in sorted(totals, key=lambda k: -totals[k]):
        print(f"  {k:14} {totals[k]:7.3f}  n={counts[k]}", file=sys.stderr)
    print(f"  {'unaccounted':14} {wall - acc:7.3f}", file=sys.stderr)
    print(f"  {'WALL':14} {wall:7.3f}", file=sys.stderr)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode",
                    choices=["vanilla", "targets", "skip-checked", "setup-only"],
                    default="vanilla")
    ap.add_argument("-N", "--runs", type=int, default=1)
    ap.add_argument("--id", action="append")
    ap.add_argument("--out")
    ap.add_argument("--targets",
                    help="comma-separated; --mode targets only")
    args = ap.parse_args()

    fm = load("frontend_mutations", "tools/frontend-mutations.py")
    timer = Timer(quiet=bool(args.out))
    install_instrumentation(fm, timer)

    if args.mode == "vanilla":
        step = lambda: _run_vanilla(fm, args)
    elif args.mode == "targets":
        targets = (args.targets or ",".join(fm.MUT_TARGETS)).split(",")
        step = lambda: loop_with_targets(fm, timer, targets, args.id)()
    elif args.mode == "skip-checked":
        step = lambda: loop_skip_checked(fm, timer, args.id)()
    else:
        step = lambda: loop_setup_only(fm, timer)()

    walls: list[float] = []
    rc = 0
    for i in range(args.runs):
        timer.records.clear()
        t0 = time.perf_counter()
        rc = step()
        wall = time.perf_counter() - t0
        walls.append(wall)
        if args.out:
            with open(args.out, "a", encoding="utf-8") as fh:
                fh.write(json.dumps({"mode": args.mode, "run": i + 1,
                                     "wall": round(wall, 4), "rc": rc,
                                     "targets": args.targets,
                                     "records": timer.records}) + "\n")
        else:
            report(timer, wall, i + 1, args.runs, args.mode)
        if args.mode != "vanilla" or args.runs == 1:
            break

    if len(walls) > 1:
        print("\n--- wall clock across runs ---", file=sys.stderr)
        print("  " + " ".join(f"{w:.3f}" for w in walls), file=sys.stderr)
        print(f"  min {min(walls):.3f}  max {max(walls):.3f}  "
              f"mean {sum(walls)/len(walls):.3f}", file=sys.stderr)
    return rc


def _run_vanilla(fm, args) -> int:
    argv = ["frontend-mutations.py", "--strategy", "sandbox"]
    for mid in (args.id or []):
        argv += ["--id", mid]
    sys.argv = argv
    return fm.main()


if __name__ == "__main__":
    sys.exit(main())