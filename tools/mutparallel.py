#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""EXPERIMENT (task/MUT-options) — parallel sandboxes for the mutation tier.

Runs the nine mutations across N independent sandboxes and reports the wall
clock, so the cost of the extra setup each parallel worker needs is measured
rather than assumed.

Each worker is a full Sandbox: its own copytree, its own cmake configure, its
own first build of MUT_TARGETS. The nine mutations are dealt round-robin, so
the worker count and the per-worker mutation count are both visible in the
output.

Verdicts are collected and compared against the tier's own expectation
columns, exactly as the tier does, so a parallel run that quietly stopped
testing something shows up as a mismatch rather than as a fast pass.

    python3 tools/mutparallel.py --workers 3
    python3 tools/mutparallel.py --workers 3 --jobs 5
"""
from __future__ import annotations

import argparse
import importlib.util
import pathlib
import queue
import subprocess
import sys
import threading
import time
from types import ModuleType

REPO = pathlib.Path(__file__).resolve().parents[1]

# Assigned by main() before any Worker exists; the Worker methods need it
# to read the tier's own MUT_TARGETS / MUTATIONS / _apply rather than
# restating them.
fm: ModuleType = None  # type: ignore[assignment]


def load(name: str, rel: str) -> ModuleType:
    """Import the tier's module from its path rather than by name.

    It is a script with a hyphen in the filename, so it cannot be imported
    normally; this is how the harness reaches the tier's own table and helpers.
    """
    spec = importlib.util.spec_from_file_location(name, REPO / rel)
    if spec is None or spec.loader is None:
        raise SystemExit(f"cannot load {rel}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class Worker:
    """One sandbox, owned by one thread for the whole run."""

    def __init__(self, wid: int, fm, jobs: str, verbose: bool) -> None:
        self.wid = wid
        self.jobs = jobs
        self.verbose = verbose
        self.lock = threading.Lock()
        self.setup: dict[str, float] = {}
        self.results: list[tuple] = []
        fm.JOBS = jobs
        self.sandbox = fm.Sandbox()
        # Sandbox.__enter__ owns mkdtemp; this harness makes its own temp dir so
        # N sandboxes can be prepared CONCURRENTLY rather than one after another.
        import tempfile
        self.dir = pathlib.Path(tempfile.mkdtemp(prefix="hunmin-mutpar.")) / "src"
        self.build_dir = self.dir / "build"
        self.mutations = 0

    def log(self, *a) -> None:
        if self.verbose:
            with self.lock:
                print(f"  [w{self.wid}]", *a, file=sys.stderr, flush=True)

    def setup_sandbox(self) -> None:
        import shutil
        t0 = time.perf_counter()
        # copytree creates self.dir itself; only its PARENT was made in __init__.
        shutil.copytree(fm.REPO, self.dir, symlinks=True, ignore=fm._COPY_IGNORE,
                        ignore_dangling_symlinks=True)
        self.setup["copytree"] = time.perf_counter() - t0
        t0 = time.perf_counter()
        rc = subprocess.run(["cmake", "-B", "build", f"-DCMAKE_BUILD_TYPE={self.sandbox._build_type()}"],
                            cwd=self.dir, capture_output=True)
        self.setup["configure"] = time.perf_counter() - t0
        if rc.returncode:
            raise SystemExit(f"worker {self.wid}: configure failed")
        t0 = time.perf_counter()
        rc = subprocess.run(["cmake", "--build", str(self.build_dir),
                             "--target", *fm.MUT_TARGETS, "-j", self.jobs],
                            cwd=self.dir, capture_output=True)
        self.setup["first-build"] = time.perf_counter() - t0
        if rc.returncode:
            raise SystemExit(f"worker {self.wid}: first build failed")
        self.setup["TOTAL"] = sum(self.setup.values())
        self.log("ready in " + f"{self.setup['TOTAL']:.2f}s "
                 f"(copy {self.setup['copytree']:.2f} + conf "
                 f"{self.setup['configure']:.2f} + build "
                 f"{self.setup['first-build']:.2f})")

    def run_one(self, mid, old, new, want_chk, want_d1, want_d2, original):
        t0 = time.perf_counter()
        mutated = fm._apply(original, old, new)
        if mutated is None:
            self.results.append((mid, "ANCHOR-NOT-FOUND", None, None, None))
            return
        engine = self.dir / fm.ENGINE_REL
        engine.write_text(mutated)
        rc = subprocess.run(["cmake", "--build", str(self.build_dir),
                             "--target", *fm.MUT_TARGETS, "-j", self.jobs],
                            cwd=self.dir, capture_output=True).returncode
        if rc != 0:
            self.results.append((mid, "build-rejects", None, None, None))
            self.log(f"{mid}: build rejects (expected)")
            return
        chk = subprocess.run([sys.executable, str(self.dir / fm.CHECKER_REL)],
                             cwd=self.dir, capture_output=True).returncode
        d1 = subprocess.run(["ctest", "--test-dir", str(self.build_dir),
                             "-R", "engine-d1-fcitx5"],
                            cwd=self.dir, capture_output=True).returncode
        d2 = subprocess.run(["ctest", "--test-dir", str(self.build_dir),
                             "-R", "engine-d2-ui"],
                            cwd=self.dir, capture_output=True).returncode
        self.mutations += 1
        self.results.append((mid, "ok", chk != 0, d1 != 0, d2 != 0))
        self.log(f"{mid}: chk={chk!=0} d1={d1!=0} d2={d2!=0} "
                 f"({time.perf_counter()-t0:.2f}s)")

    def teardown(self):
        # self.sandbox was never entered (this harness makes its own temp dir),
        # so only self.dir's parent exists to remove.
        import shutil
        shutil.rmtree(self.dir.parent, ignore_errors=True)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--workers", type=int, default=3)
    ap.add_argument("--jobs", default=None,
                    help="cmake -j per worker (default: cpu_count/workers)")
    ap.add_argument("--out")
    args = ap.parse_args()

    global fm
    fm = load("frontend_mutations", "tools/frontend-mutations.py")
    import os
    jobs = args.jobs or str(max(1, (os.cpu_count() or 4) // args.workers))

    original = (REPO / fm.ENGINE_REL).read_text()

    t_start = time.perf_counter()
    workers = [Worker(i, fm, jobs, verbose=True) for i in range(args.workers)]
    # All setups run concurrently -- that is the point: each worker pays its own.
    threads = [threading.Thread(target=w.setup_sandbox) for w in workers]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    t_ready = time.perf_counter()

    q: queue.Queue = queue.Queue()
    for m in fm.MUTATIONS:
        q.put(m)
    lock = threading.Lock()

    def drain(w):
        while True:
            try:
                m = q.get_nowait()
            except queue.Empty:
                return
            with lock:
                pass
            w.run_one(*m, original)

    threads = [threading.Thread(target=drain, args=(w,)) for w in workers]
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    wall = time.perf_counter() - t_start

    seen = {}
    for w in workers:
        for r in w.results:
            seen.setdefault(r[0], r)
    gaps = []
    print(f"\n{'mutation':30} {'build':14} {'checker':9} {'d1':6} {'d2':6} verdict")
    for mid, _old, _new, want_chk, want_d1, want_d2 in fm.MUTATIONS:
        got = seen.get(mid)
        if got is None:
            gaps.append(f"{mid}: no result")
            print(f"{mid:30} MISSING")
            continue
        _, kind, c, a, b = got
        if kind == "ANCHOR-NOT-FOUND":
            gaps.append(f"{mid}: anchor not found")
            print(f"{mid:30} ANCHOR NOT FOUND")
            continue
        if kind == "build-rejects":
            print(f"{mid:30} {'fail':14} {'-':9} {'-':6} {'-':6} build rejects")
            continue
        if c != want_chk:
            gaps.append(f"{mid}: checker_rejects={c}, expected {want_chk}")
        if a != want_d1:
            gaps.append(f"{mid}: d1_rejects={a}, expected {want_d1}")
        if b != want_d2:
            gaps.append(f"{mid}: d2_rejects={b}, expected {want_d2}")
        guarded = (c and a) or (c and b) or b
        print(f"{mid:30} {'ok':14} {str(c):9} {str(a):6} {str(b):6} "
              f"{'guarded' if guarded else 'GAP'}")

    print(f"\nworkers={args.workers}  -j{jobs} each")
    for w in workers:
        print(f"  w{w.wid}: setup {w.setup['TOTAL']:6.2f}s "
              f"(copy {w.setup['copytree']:.2f} conf {w.setup['configure']:.2f} "
              f"build {w.setup['first-build']:.2f}), {w.mutations} mutations")
    print(f"  all workers ready at  {t_ready - t_start:6.2f}s")
    print(f"  mutation phase        {wall - (t_ready - t_start):6.2f}s")
    print(f"  WALL                  {wall:6.2f}s")

    for w in workers:
        w.teardown()

    if gaps:
        print(f"\n{len(gaps)} expectation mismatch(es):")
        for g in gaps:
            print("  -", g)
        return 1
    print("\nmutation set matches its recorded expectations (mutparallel)")
    return 0


if __name__ == "__main__":
    sys.exit(main())