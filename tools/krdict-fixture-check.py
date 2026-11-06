#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Self-contained determinism check for tools/krdict2data.py (ctest: krdict-fixture).

The real 국립국어원 dump is 368 MB and gitignored (tools/fetch-krdict.sh), so
data-selfcheck cannot run the krdict converter in a fresh worktree — it skips
(rc 77) when the dump is absent, and the dump-dependent full run is the
maintainer-only tools/selfcheck-krdict.sh (NOT a ctest test). This script is
the self-contained replacement: it converts the committed fixture
tests/fixtures/krdict/ into a temp dir and diffs byte-identically against the
committed expected output, proving the converter's logic with no dump present.

Any diff means either the converter changed or the committed expected output is
stale. Regenerate the expected output deliberately:

    python3 tools/krdict2data.py --orig tests/fixtures/krdict \
        --out /tmp/kf && cp /tmp/kf/krdict/krdict.json \
        tests/fixtures/krdict/expected/krdict.json
"""
import difflib
import filecmp
import subprocess
import sys
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parent.parent
fixture = root / "tests/fixtures/krdict"
expected = fixture / "expected" / "krdict.json"

with tempfile.TemporaryDirectory() as td:
    out = Path(td)
    r = subprocess.run(
        [sys.executable, root / "tools/krdict2data.py",
         "--orig", str(fixture), "--out", str(out)],
        capture_output=True, text=True)
    if r.stdout:
        print(r.stdout, end="")
    if r.returncode:
        print(r.stderr, file=sys.stderr)
        sys.exit(f"failed: krdict2data.py on the committed fixture (rc={r.returncode})")
    got = out / "krdict" / "krdict.json"
    if not got.exists():
        sys.exit(f"krdict2data.py produced no {got}")
    if not filecmp.cmp(expected, got, shallow=False):
        print(f"--- expected ({expected})", file=sys.stderr)
        print(f"+++ produced ({got})", file=sys.stderr)
        # difflib, not an external `diff`: Git-for-Windows does not put one on
        # PATH, so the failure path itself crashed with FileNotFoundError.
        exp = expected.read_text(encoding="utf-8", errors="replace").splitlines(keepends=True)
        prod = got.read_text(encoding="utf-8", errors="replace").splitlines(keepends=True)
        sys.stderr.writelines(difflib.unified_diff(exp, prod, fromfile=str(expected), tofile=str(got)))
        sys.exit("krdict-fixture: committed fixture output diverges from the "
                 "committed expected krdict.json — either the converter "
                 "changed or the expected output is stale; see the docstring.")
print("krdict-fixture: committed fixture matches the committed expected output")
