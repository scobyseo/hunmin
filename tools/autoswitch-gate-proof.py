#!/usr/bin/env python3
"""Measure that the autoswitch data gate is what refuses the option.

docs/SPEC-MODES.md §R13-정정 (B3) says the feature is enable-able only where its
data is present. Two refusals implement that (issue #169): the option writer in
hunmin_ic_set_option() and the chord in hunmin_buffered_preedit_special_peek().
This script MEASURES that each refusal is load-bearing, by removing it alone and
re-running the tier-A cases that assert it -- a green suite after the removal
would mean the test pins nothing.

Each mutation is restored byte-for-byte in a finally block (and its mtime bumped:
copy2 preserves the old timestamp, which lets ninja keep the mutated object --
measured here, the chord stayed refused after a "restored" rebuild until the
mtimes were bumped).

Usage:
    python3 tools/autoswitch-gate-proof.py [--build-dir build] [--test-exe PATH]

Exit code 0 = every mutation turned at least one named test red AND the tree is
green again after the restore. Nothing is left modified on either path.
"""
import argparse
import os
import pathlib
try:
    from _common import ROOT
except ImportError:
    ROOT = pathlib.Path(__file__).resolve().parents[1]
import re
import shutil
import subprocess
import sys

SRC = ROOT / "libhunmin" / "src"
DATA = ROOT / "libhunmin" / "data"

MUTATIONS = [
    ("option writer", "ic.c",
     'if (value && !hunmin_ctx_autoswitch_data_present(ic->ctx)) {',
     'if (0) {'),
    ("chord", "buffered_preedit.c",
     'if (fn == HUNMIN_SPECIAL_AUTOSWITCH && hm_modes_data_present(l->ctx) != 1) return -1;',
     '/* mutation: chord gate removed */'),
    ("presence probe", "wordset.c",
     'if (ctx->modes_data) return ctx->modes_data; /* asked once per context */',
     'return 1; /* mutation: presence always true */'),
]

FILTER = ("AutoswitchDataGateTest.*:"
          "KeyRuleTest.EnablingItWhereTheDataIsPresentIsAccepted")


def run(build_dir, test_exe):
    """Build, then run the gate cases. Returns (rc, [failed test names])."""
    b = subprocess.run(["cmake", "--build", str(build_dir)], cwd=ROOT,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       text=True, errors="replace")
    if b.returncode != 0:
        errs = [l for l in (b.stdout or "").splitlines() if "error" in l.lower()]
        print("  BUILD FAILED")
        print("\n".join(errs[:5]))
        return 1, []
    r = subprocess.run([str(test_exe), "--gtest_filter=" + FILTER], cwd=ROOT,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       text=True, errors="replace",
                       env=dict(os.environ, HUNMIN_DATA_PATH=str(DATA).replace("\\", "/")))
    failed = sorted({re.sub(r"\s*\(\d+ ms\)$", "", l.split("]", 1)[1].strip())
                     for l in (r.stdout or "").splitlines()
                     if l.startswith("[  FAILED  ]") and "listed below" not in l})
    return r.returncode, failed


def restore(path, content):
    path.write_bytes(content)
    os.utime(path, None)  # a stale mtime leaves the mutated object compiled in


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--build-dir", default="build", help="configured build tree (default: build)")
    ap.add_argument("--test-exe", default=None, help="the engine-key-rule binary")
    args = ap.parse_args()
    build_dir = (ROOT / args.build_dir).resolve()
    test_exe = pathlib.Path(args.test_exe) if args.test_exe else build_dir / "key-rule-test.exe"
    if not test_exe.exists():
        print(f"missing runner: {test_exe} -- build the tier first")
        return 2

    originals = {f: (SRC / f).read_bytes() for _, f, _, _ in MUTATIONS}
    ok = True
    try:
        rc, failed = run(build_dir, test_exe)
        print(f"baseline: rc={rc} failed={failed}")
        if rc != 0:
            print("baseline is not green; nothing to prove")
            return 2
        for name, f, old, new in MUTATIONS:
            text = originals[f].decode("utf-8")
            count = text.count(old)
            assert count == 1, f"{name}: anchor found {count} times in {f}"
            restore(SRC / f, text.replace(old, new).encode("utf-8"))
            rc, failed = run(build_dir, test_exe)
            print(f"without the {name}: rc={rc} failed={failed}")
            if rc == 0 or not failed:
                print(f"  FINDING: removing the {name} refusal leaves the gate cases green")
                ok = False
            restore(SRC / f, originals[f])
    finally:
        for f, content in originals.items():
            restore(SRC / f, content)
    rc, failed = run(build_dir, test_exe)
    print(f"restored: rc={rc} failed={failed}")
    return 0 if (ok and rc == 0) else 1


if __name__ == "__main__":
    sys.exit(main())
