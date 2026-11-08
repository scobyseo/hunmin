#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Assert that the English word list ships apart from the base engine data
(ctest: data-split; issue #172, docs/SPEC-MODES.md ReqMode8).

B3's ruling -- "자동변환에 필요한 사전이든 뭐든 추가 데이터는 별도 배포를 가정하고 그
데이터가 있을때만 기능을 켤 수 있게 하면 transition이 용이할 거 같아" (quoted in
wiki/decisions/2026-10-11-auto-switch-off-by-default.md) -- makes "the words are
not installed" a state the engine has to answer, not an accident. That state is
only reachable if some installation leaves `modes/` out, so this script asserts
the two places that decide it agree, and that the engine still works where it
has been left out:

1. the CMake components: `--component data` installs every data directory EXCEPT
   `modes/`, and `--component modes-data` installs `modes/` and nothing else;
2. the Debian partition: every directory under libhunmin/data/ is named by
   exactly one of the two `debian/*.install` files, and `modes/` by the words
   package only;
3. the declaration: `debian/control` carries the words package, and the base
   package Recommends it -- the maintainer's ruling of 2026-11-01
   ("Recommends.") on issue #172: one dictionary arrives with a plain install,
   and the words package stays a package of its own so that an installation
   without it is still possible and still answerable;
4. the engine: hunmin-sim run against a base-only install tree delivers exactly
   the text it delivers against the full data tree. Equality against the
   identity-path run rather than a literal, because the literal would encode one
   build's spelling of the layout and pass while the base tree was broken.

What this does NOT do: build a .deb. debhelper is not installed on every host
that runs this suite, and a .deb build is the deb job's (ci.yml). So the
partition is checked as the packaging files state it; the files themselves
landing in the right package is verified where dpkg-buildpackage runs.
"""
import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Optional

# The one place the proposed package name appears in code: B3's decision page
# rules the name "design work, not ruled", so a rename should be one edit here,
# one in debian/control, and a `git mv` of the .install file.
WORDS_PACKAGE = "hunmin-en-us"
BASE_PACKAGE = "hunmin"
DATA_PREFIX = "usr/share/hunmin"  # CMAKE_INSTALL_DATADIR's default


def fail(msg: str) -> None:
    sys.exit(f"data-split: FAILED -- {msg}")


def ok(msg: str) -> None:
    print(f"data-split: {msg}")


def install_component(cmake: str, build: Path, component: str, dest: Path,
                      config: Optional[str]) -> Path:
    cmd = [cmake, "--install", str(build), "--component", component,
           "--prefix", str(dest)]
    if config:  # multi-config generators need --config; single-config ones reject it
        cmd += ["--config", config]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        fail(f"cmake --install --component {component} failed:\n{r.stdout}{r.stderr}")
    return dest


def check_components(cmake: str, build: Path, work: Path,
                     config: Optional[str]) -> None:
    base = install_component(cmake, build, "data", work / "base", config)
    words = install_component(cmake, build, "modes-data", work / "words", config)

    base_data = base / "share" / "hunmin"
    if not base_data.is_dir():
        fail(f"the base component installed no share/hunmin at all ({base_data})")
    if (base_data / "modes").exists():
        fail("the base component installed modes/ -- the word list is back in "
             "the base distribution, which is the one thing B3 forbids")
    shipped = sorted(p.name for p in base_data.iterdir() if p.is_dir())
    if "layouts" not in shipped:
        fail(f"the base component installed no layouts/ (it shipped {shipped})")

    words_data = words / "share" / "hunmin"
    modes = words_data / "modes"
    if not modes.is_dir():
        fail(f"the modes-data component installed no {modes}")
    missing = [n for n in ("en-US.json", "meta.json", "LICENSE.md")
               if not (modes / n).is_file()]
    if missing:
        fail(f"the word list package is incomplete: {missing} missing from {modes}")
    extra = sorted(p.name for p in words_data.iterdir() if p.name != "modes")
    if extra:
        fail(f"the modes-data component installed more than the word list: {extra}")

    ok(f"components: base ships {shipped} without modes/; modes-data ships "
       f"{sorted(p.name for p in modes.iterdir())}")


def read_install_file(path: Path) -> list[str]:
    """debhelper .install entries, comments and blanks dropped."""
    if not path.is_file():
        fail(f"{path} does not exist -- a second binary package without an "
             ".install file makes dh_install stage the WHOLE tree into every "
             "package, so the split would silently not happen")
    out = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            out.append(line)
    return out


def check_debian_partition(root: Path) -> None:
    data = root / "libhunmin" / "data"
    dirs = sorted(p.name for p in data.iterdir() if p.is_dir())
    if not dirs:
        fail(f"no data directories under {data}")
    base_entries = read_install_file(root / "debian" / f"{BASE_PACKAGE}.install")
    words_entries = read_install_file(root / "debian" / f"{WORDS_PACKAGE}.install")

    def named_by(entries: list[str], name: str) -> bool:
        want = f"{DATA_PREFIX}/{name}"
        return any(e.rstrip("/") == want or e.rstrip("/").startswith(want + "/")
                   for e in entries)

    for name in dirs:
        in_base, in_words = named_by(base_entries, name), named_by(words_entries, name)
        if in_base and in_words:
            fail(f"{DATA_PREFIX}/{name} is claimed by BOTH packages; dh_install "
                 f"would put it in both or fail")
        if not (in_base or in_words):
            fail(f"{DATA_PREFIX}/{name} is claimed by NEITHER package -- it "
                 f"would be dropped from the .deb with no error. Add it to one "
                 f"of debian/{BASE_PACKAGE}.install and debian/{WORDS_PACKAGE}.install")
        if name == "modes" and not in_words:
            fail(f"{DATA_PREFIX}/modes is in the base package; the split is gone")

    ok(f"debian: {len(dirs)} data directories partitioned, modes/ -> "
       f"{WORDS_PACKAGE}")


def check_control(root: Path) -> None:
    text = (root / "debian" / "control").read_text(encoding="utf-8")
    stanzas = {}
    for block in text.split("\n\n"):
        fields = {}
        for line in block.splitlines():
            if line[:1] in (" ", "\t") or ":" not in line:
                continue
            key, _, value = line.partition(":")
            fields[key.strip()] = value.strip()
        if "Package" in fields:
            stanzas[fields["Package"]] = fields
    for name in (BASE_PACKAGE, WORDS_PACKAGE):
        if name not in stanzas:
            fail(f"debian/control has no Package: {name} stanza")
    if stanzas[WORDS_PACKAGE].get("Architecture") != "all":
        fail(f"{WORDS_PACKAGE} is not Architecture: all -- it carries data only")
    # Recommends, not Suggests and not Depends: the maintainer's ruling of
    # 2026-11-01 on issue #172, verbatim "Recommends." -- a plain install has one
    # dictionary, and the package stays removable so that the absent state B3
    # makes answerable is still reachable (--no-install-recommends, a distro
    # that drops it, a machine that uninstalls it).
    pointers = stanzas[BASE_PACKAGE].get("Recommends", "")
    if WORDS_PACKAGE not in pointers:
        fail(f"debian/control's {BASE_PACKAGE} stanza must Recommend "
             f"{WORDS_PACKAGE} (ruling 2026-11-01, issue #172); it says "
             f"Recommends: {pointers!r}")
    if WORDS_PACKAGE in stanzas[BASE_PACKAGE].get("Depends", ""):
        fail(f"{WORDS_PACKAGE} is a hard Depends -- then the words cannot be "
             f"absent, and B3's gate has no state to answer")
    ok(f"control: {WORDS_PACKAGE} declared, named by {BASE_PACKAGE}")


def check_engine(sim: Path, data: Path, base: Path) -> None:
    if not sim.is_file():
        fail(f"no hunmin-sim at {sim}")

    def run(d: Path) -> str:
        r = subprocess.run([str(sim), "--data", str(d), "--layout", "dubeolsik",
                            "--keys", "dkssud gksrmf "],
                           capture_output=True, text=True)
        if r.returncode:
            fail(f"hunmin-sim failed against {d} (rc={r.returncode}):\n{r.stderr}")
        return r.stdout

    full = run(data)
    base_only = run(base / "share" / "hunmin")
    if not full.strip():
        fail("hunmin-sim delivered nothing against the full data tree")
    if base_only != full:
        fail("the base-only install tree composes differently from the full "
             "data tree -- an install without the words package is not the "
             "same input method:\n"
             f"--- full data\n{full}\n--- base only\n{base_only}")
    ok("engine: the base-only tree delivers the same text as the full tree")


def main() -> None:
    ap = argparse.ArgumentParser(
        description="Assert that the English word list ships apart from the "
                    "base engine data (issue #172).")
    ap.add_argument("--build-dir", required=True, help="a configured build directory")
    ap.add_argument("--cmake", default=shutil.which("cmake") or "cmake")
    ap.add_argument("--data", default=None, help="the full data dir (default: <repo>/libhunmin/data)")
    ap.add_argument("--sim", default=None,
                    help="the hunmin-sim binary to drive (default: <build>/hunmin-sim)")
    ap.add_argument("--config", default="",
                    help="$<CONFIG> from the build system; empty for single-config")
    args = ap.parse_args()

    root = Path(__file__).resolve().parent.parent
    build = Path(args.build_dir).resolve()
    data = Path(args.data).resolve() if args.data else root / "libhunmin" / "data"
    if not (build / "CMakeCache.txt").is_file():
        fail(f"{build} is not a configured build directory")

    with tempfile.TemporaryDirectory(prefix="data-split-") as td:
        work = Path(td)
        check_components(args.cmake, build, work, args.config.strip() or None)
        sim = Path(args.sim) if args.sim else build / "hunmin-sim"
        check_engine(sim, data, work / "base")
    check_debian_partition(root)
    check_control(root)
    print("data-split: OK -- the word list is a separate distribution on the "
          "CMake and Debian surfaces, and the engine is healthy without it")


if __name__ == "__main__":
    main()
