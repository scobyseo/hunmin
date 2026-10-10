#!/usr/bin/env python3
"""Determinism + lint self-check (docs/CODE_RULES.md rule: converters are
reproducible). Re-runs every data generator into a temp dir, diffs against
committed data/, then runs hunmin-doctor. Used by ctest/CI."""
import subprocess
import sys
import tempfile
from pathlib import Path

root = Path(sys.argv[1]).resolve()
py = sys.executable
DATA = root / "libhunmin/data"
IST = root / "data-orig/naraeseot"


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        print(r.stdout, r.stderr, file=sys.stderr)
        sys.exit(f"failed: {' '.join(map(str, cmd))}")
    return r


with tempfile.TemporaryDirectory() as td:
    steps = [
        [py, root / "tools/kbd2data.py", "--orig", root / "data-orig/libhangul", "--out", td],
        [py, root / "tools/ist2data.py", "--ist", IST / "sin3-p2.basic.xml",
         "--out", td, "--name", "sin3-p2-basic", "--alias", "3sin-p2-basic"],
        [py, root / "tools/ist2data.py", "--ist", IST / "sin3-p2.symbols.xml",
         "--out", td, "--name", "sin3-p2", "--alias", "3sin-p2"],
        [py, root / "tools/ist2data.py", "--ist", IST / "sin3-p2.yethangul.xml",
         "--out", td, "--name", "sin3-p2-yet", "--alias", "3sin-p2-yet",
         "--compose-extra", "sin3-p2-yet-ohi"],
        [py, root / "tools/ohi2compose.py", Path(td) / "compose"],
        [py, root / "tools/hanja2data.py", "--orig", root / "data-orig/hanja",
         "--unihan", root / "data-orig/unihan", "--out", td],
        [py, root / "tools/krdict2data.py", "--orig", root / "data-orig/krdict",
         "--out", td],
        [py, root / "tools/idioms2data.py", "--orig", root / "data-orig/idioms",
         "--out", td],
        # us-qwerty has no upstream XML: QWERTY is a physical keyboard and its
        # key table is generated from this repo's own key-id convention. Same
        # treatment as LICENSE.md below -- a committed artefact whose producer
        # is not a converter, so it is regenerated into the tree rather than
        # letting diff -r report it as missing.
        [py, root / "tools/gen-us-qwerty.py", "--out", str(Path(td) / "layouts/us-qwerty.json")],
    ]
    for s in steps:
        run(s)
    # Provenance docs are committed but are not converter output, so copy them
    # across rather than letting diff -r report them as missing.
    for lic in DATA.rglob("LICENSE.md"):
        dst = Path(td) / lic.relative_to(DATA)
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(lic.read_bytes())
    d = subprocess.run(["diff", "-r", str(DATA), td], capture_output=True, text=True)
    if d.returncode:
        print(d.stdout, file=sys.stderr)
        sys.exit("converter outputs diverge from committed data/ — regenerate:\n"
                 "  python3 tools/kbd2data.py --orig data-orig/libhangul --out libhunmin/data\n"
                 "  python3 tools/ist2data.py --ist data-orig/naraeseot/sin3-p2.symbols.xml "
                 "--out libhunmin/data --name sin3-p2 --alias 3sin-p2\n"
                 "  python3 tools/ist2data.py --ist data-orig/naraeseot/sin3-p2.yethangul.xml "
                 "--out libhunmin/data --name sin3-p2-yet --alias 3sin-p2-yet "
                 "--compose-extra sin3-p2-yet-ohi\n  python3 tools/ohi2compose.py")
sys.exit(subprocess.run([py, root / "tools/doctor.py", DATA]).returncode)
