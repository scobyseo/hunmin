# SPDX-License-Identifier: LGPL-2.1-or-later
"""Mutation set for check-frontend-invariants.py — self-test.

docs/FRONTEND-CONTRACT.md §5 claims those invariants are guarded. A guard
nobody ever tries is an assumption. This file plants each known-interesting
mutation of hunminadapter.cpp and records whether the checker and/or engine-d1-fcitx5
reject it, so the claims stay checkable instead of aspirational.

Format, one record per mutation:
    (id, old_substring, new_substring, expect_checker, expect_ctest)

expect_* are booleans meaning "should be REJECTED". Both true for a mutation
that both layers catch; either can be False, and that is data — it marks a
gap, not a failure of this file.

Run:  python3 tools/frontend-mutations.py --list
      python3 tools/frontend-mutations.py                    # all, $HUNMIN_MUTATION_STRATEGY
      python3 tools/frontend-mutations.py --strategy sandbox # isolated
"""
from __future__ import annotations

import argparse
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[1]
ENGINE_REL = pathlib.Path("frontends/fcitx5/src/hunminadapter.cpp")
ENGINE = REPO / ENGINE_REL
CHECKER_REL = pathlib.Path("tools/check-frontend-invariants.py")
CHECKER = REPO / CHECKER_REL

# (id, old, new, checker_rejects, d1_rejects)
MUTATIONS: list[tuple[str, str, str, bool, bool, bool]] = [
    (
        "I1-commit-flush-both",
        "        } else {\n            // Engine declined",
        "        }\n        if (!used) {\n            // Engine declined",
        True,
        False,  # known gap: D1 has no case producing commit + in-flight syllable
        False,  # and D2 asserts panel state, not commit routing
    ),
    (
        "I5-caps-dropped",
        'hunmin_ic_set_option(hunminIc_, "caps", caps ? 1 : 0);',
        "",
        True,
        True,  # closed by de0de72: D1 pins sin3-p2-yet and replays the golden
        False,  # caps affects composition, not what is published to the panel
    ),
    # The anchor names the config accessor, so it has to be renamed whenever
    # that accessor is: 1521fad ("Add DVORAK and Colemak layouts with separate
    # Hangul/Latin pickers") renamed config().layout -> config().hangulLayout
    # and updated tools/check-frontend-invariants.py:88 in the same commit, but
    # not this entry -- which is what "I5-layout-override-ignored: anchor not
    # found (source moved)" was, on 2026-10-16, and it failed the whole
    # mutation self-test, and with it the deb build (debian/rules runs the
    # suite). The mutation text moves with the anchor: it must keep discarding
    # the per-item override, or it stops being the mutation it is named for.
    (
        "I5-layout-override-ignored",
        "        const std::string &want = layout.empty() ? *engine_->config().hangulLayout : layout;",
        "        (void)layout;\n        const std::string &want = *engine_->config().hangulLayout;",
        True,  # I7 checks the ternary and the call site structurally
        True,
        False,  # D2 pins the same layout the default does, so no difference
    ),
    # I9 -- filterAndAccept must stay UNGATED. Gating it on `used` was the
    # pre-P3 rule; A11 reversed it so F9 is interceptable, and putting the
    # guard back is now the defect. This is the inverse of the old I3 mutation,
    # which is why the old entry had to be replaced rather than kept.
    (
        "I3-accept-unguarded",
        "        updateUI();\n        // Only consume what the engine actually took. A key it declined\n        // (space, backspace on an empty buffer, an unmapped keysym) belongs to\n        // the client, and that is how it reaches it. This is FRONTEND-CONTRACT\n        // \u00a75 rule 3.\n        if (used) {\n            keyEvent.filterAndAccept();\n        }",
        "        updateUI();\n        keyEvent.filterAndAccept();",
        True,
        True,
        False,  # D2 never asserts acceptance, only what the panel shows
    ),
    # Tier D2 (InputPanel presentation) -- these were found by planting them
    # while writing testhunminui; all four compile and pass D1, so without the
    # D2 test they would have gone unnoticed.
    (
        "D2-preedit-never-set",
        "            ic_->inputPanel().setPreedit(text);",
        "",
        False,  # the checker does not model the panel; D2 is the guard
        False,  # D1 asserts commits, not the panel
        True,   # D2 asserts the preedit text
    ),
    (
        "D2-client-preedit-dropped",
        "ic_->inputPanel().setClientPreedit(text);",
        "",
        False,
        False,
        True,
    ),
    (
        "D2-cursor-at-zero",
        "text.setCursor(std::strlen(preedit));",
        "",
        False,
        False,
        True,
    ),
    (
        # An empty preedit must still be written back, or the previous
        # composition stays on screen after the client consumed it. The
        # original mutation removed inputPanel().reset(); that line no longer
        # exists, so the live form of the same defect is skipping the set.
        "D2-panel-never-cleared",
        "            if (useClient) {\n                ic_->inputPanel().setClientPreedit(text);",
        "            if (useClient) {\n                if (text.size()) ic_->inputPanel().setClientPreedit(text);",
        False,
        False,
        True,
    ),
    (
        "I4-shift-chord",
        "modifiers |= KeyState::Ctrl;",
        "modifiers |= KeyState::Shift_L;\n    modifiers |= KeyState::Ctrl;",
        True,
        False,  # does not compile; reported as a build failure below
        False,
    ),
]


def _run(cmd: list[str], timeout: int = 180, cwd: pathlib.Path | None = None) -> int:
    return subprocess.run(cmd, cwd=cwd or REPO, capture_output=True, timeout=timeout).returncode


def _apply(text: str, old: str, new: str) -> str | None:
    if old not in text:
        return None
    return text.replace(old, new, 1)


# The three targets a mutation can change the verdict of: the shared library
# the addon links, and the two test binaries that exercise the addon. Naming
# them keeps the sandbox build off hunmin-sim, the goldens and the D3 harness,
# which no mutation here can affect.
MUT_TARGETS = ["hunmin", "testhunmin", "testhunminui"]

# -j on every build. Measured at 2451fdc: rebuilding the two test binaries
# after touching hunminadapter.cpp takes 3.2 s serially and 1.5 s with -j16, and this
# runs nine times, so the missing flag was most of a third of the tier. With
# Makefiles -- which is what this project configures -- ninja's default
# parallelism does not apply and cmake --build inherits nothing.
#
# Capped at the CPU count because two of these may run at once (that is the
# point of the sandbox strategy) and two unconstrained -j16 builds on a 16-core
# host oversubscribe into slower, not faster.
JOBS = str(os.cpu_count() or 4)


def _build_cmd(target_dir: pathlib.Path) -> list[str]:
    return ["cmake", "--build", str(target_dir), "--target", *MUT_TARGETS,
            "-j", JOBS]

_COPY_IGNORE_BASENAMES = (
    ".git", "build", "build-android", "__pycache__", ".gradle", ".cxx",
    "node_modules", "Testing", ".mypy_cache",
)

# Whole directories, named by their path RELATIVE TO THE REPO ROOT.
#
# These cannot go in _COPY_IGNORE_BASENAMES. shutil.ignore_patterns applies
# fnmatch to the list of entry NAMES at each level, and a name is a single path
# component, so a pattern containing a slash matches nothing at any depth.
# Measured on master 2026-10-10 (base 5380754): adding "data-orig/krdict" to
# the pattern tuple copied the same 467 MB / 2949 files as not adding it, with
# data-orig/krdict present in the sandbox both times. Excluding it by path
# instead copies 99 MB / 2938 files and halves the copy, 0.47 s -> 0.25 s.
#
# It matters more than the 0.2 s, because /tmp is a tmpfs: the sandbox is RAM.
# 368 MB that no mutation build reads (gitignored source XML, not in the repo)
# is 368 MB of this host's memory held for the life of the tier.
_COPY_IGNORE_PATHS = frozenset({"data-orig/krdict"})

# A path that MUST survive the copy, so the check below cannot pass vacuously
# by a filter that excludes everything.
_COPY_IGNORE_CONTROL = "libhunmin/src"


def _copy_ignore(path: str, names: list[str]) -> set[str]:
    """The `ignore` callable for shutil.copytree.

    The contract is to return the set of entries NOT to copy -- the excluded
    names -- not the ones to keep. Getting that backwards silently empties the
    copy; see _check_copy, which exists because of how easy that is.
    """
    ignored = set(shutil.ignore_patterns(*_COPY_IGNORE_BASENAMES)(path, names))
    try:
        here = pathlib.Path(path).relative_to(REPO).as_posix()
    except ValueError:  # not under the repo; basename rules still apply
        return ignored
    if here == ".":
        here = ""
    ignored.update(n for n in names if f"{here}/{n}" in _COPY_IGNORE_PATHS)
    return ignored


_COPY_IGNORE = _copy_ignore


def _check_copy(src: pathlib.Path, dst: pathlib.Path) -> None:
    """Assert the copy filter actually did what it claims.

    A copy filter that silently excludes nothing is the defect this function
    was written for: the tier still passes, still times, and still copies the
    directory it was told to leave behind. Cheap enough to run every time
    (a few stat calls), and it turns that class of mistake into a red tier.
    """
    for rel in sorted(_COPY_IGNORE_PATHS):
        if (dst / rel).exists():
            raise SystemExit(
                f"sandbox: {rel} was copied despite being excluded -- the ignore "
                f"rule is not excluding paths (it may be matching basenames)")
    if not (dst / _COPY_IGNORE_CONTROL).is_dir():
        raise SystemExit(
            f"sandbox: {_COPY_IGNORE_CONTROL} is missing from the copy -- the "
            f"ignore rule is excluding more than it should")


class Strategy:
    """Where a mutation is planted, and what gets rebuilt to test it.

    This tier used to have exactly one behaviour -- plant the mutation in the
    working tree, rebuild the shared build/ -- and that behaviour is why it had
    to be RUN_SERIAL. Making it a strategy turns the isolation from a property
    of the test into a choice, which is what lets it run next to other tests
    at all.

      in-tree   plant in the working tree, rebuild the shared build/.
                No setup cost, and what a single CI run wants. For its whole
                duration the checkout holds a deliberately broken hunminadapter.cpp
                and build/ holds binaries built from it, so nothing else that
                uses the fcitx5 addon may be running -- including another
                session's ctest, which is why it was RUN_SERIAL.

      sandbox   copy the sources to a temp dir, configure and build there,
                and plant and test entirely inside it. Measured at 2451fdc
                with -j16: 10 s of setup (configure + first build) and ~1.5 s
                per mutation, against 24.8 s for the in-tree run of the same
                nine. So isolation costs almost nothing here, and the tier
                stops needing RUN_SERIAL.

    The copy is of the WORKING TREE, not of HEAD: a mutation self-test that
    silently ran against a commit other than the one under test would be
    measuring the wrong thing.
    """

    name = "abstract"

    def __enter__(self) -> pathlib.Path:
        raise NotImplementedError

    def engine(self) -> pathlib.Path:
        """The hunminadapter.cpp this strategy mutates."""
        raise NotImplementedError

    def build(self) -> int:
        raise NotImplementedError

    def restore(self) -> None:
        """Undo the mutation so the tree is left as it was found."""

    def __exit__(self, *exc: object) -> None:
        self.restore()


class InTree(Strategy):
    name = "in-tree"

    def __enter__(self) -> pathlib.Path:
        self._backup = ENGINE.read_text()
        return REPO

    def engine(self) -> pathlib.Path:
        return ENGINE

    def build(self) -> int:
        return _run(_build_cmd(REPO / "build"), timeout=900)

    def restore(self) -> None:
        # Restore AND rebuild: leaving build/ holding binaries compiled from a
        # mutation would make every later ctest in this checkout meaningless.
        ENGINE.write_text(self._backup)
        self.build()


class Sandbox(Strategy):
    name = "sandbox"

    @staticmethod
    def _parent_toolchain() -> list[str]:
        """The sandbox must configure like the parent tree did. A bare
        `cmake -B build` picks the platform default generator -- Visual Studio
        on a Windows host, which cannot build this POSIX-shaped tree -- and
        the build fails before the first mutation lands (measured: "sandbox:
        initial build failed" on the llvm-mingw host). The parent's
        CMakeCache is the truthful record of what this host actually can
        build with; reuse its generator, platform and compilers verbatim. No
        cache (the CI layout configures into build/) behaves as before."""
        import re
        overrides: list[str] = []
        for cache in REPO.glob("build*/CMakeCache.txt"):
            vals: dict[str, str] = {}
            for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
                if ":INTERNAL=" in line or ":FILEPATH=" in line:
                    key, _, val = line.partition("=")
                    vals[key] = val
            if not vals.get("CMAKE_GENERATOR:INTERNAL"):
                continue
            overrides = ["-G", vals["CMAKE_GENERATOR:INTERNAL"].strip()]
            if vals.get("CMAKE_GENERATOR_PLATFORM:INTERNAL"):
                overrides += ["-A", vals["CMAKE_GENERATOR_PLATFORM:INTERNAL"].strip()]
            for key, opt in (("CMAKE_C_COMPILER:FILEPATH", "-DCMAKE_C_COMPILER="),
                             ("CMAKE_CXX_COMPILER:FILEPATH", "-DCMAKE_CXX_COMPILER=")):
                if vals.get(key):
                    overrides.append(opt + vals[key].strip())
            break
        return overrides

    def __enter__(self) -> pathlib.Path:
        self.dir = pathlib.Path(tempfile.mkdtemp(prefix="hunmin-mut-sandbox."))
        src = self.dir / "src"
        print(f"mutation sandbox: {src}", file=sys.stderr)
        shutil.copytree(REPO, src, symlinks=True, ignore=_COPY_IGNORE,
                        ignore_dangling_symlinks=True)
        _check_copy(REPO, src)
        self._backup = (src / ENGINE_REL).read_text()
        rc = _run(["cmake", "-B", "build", *self._parent_toolchain(),
                   f"-DCMAKE_BUILD_TYPE={self._build_type()}"],
                  cwd=src)
        if rc != 0:
            raise SystemExit("sandbox: cmake configure failed")
        if self._build_in(src) != 0:
            raise SystemExit("sandbox: initial build failed")
        return src

    @staticmethod
    def _build_type() -> str:
        # Deliberately NOT the main build's type. This tier asks two questions
        # -- does the mutation compile, and do the guards reject it -- and
        # neither depends on optimisation. Measured at 2451fdc: inheriting
        # RelWithDebInfo made every hunminadapter.cpp rebuild 3.28 s instead of
        # 1.5 s, which over nine mutations is the difference between a 41 s
        # tier and a 17 s one. Override with $HUNMIN_MUTATION_BUILD_TYPE if a
        # future mutation turns out to need the real flags.
        return os.environ.get("HUNMIN_MUTATION_BUILD_TYPE", "")

    @staticmethod
    def _build_in(src: pathlib.Path) -> int:
        return _run(_build_cmd(src / "build"), cwd=src, timeout=900)

    def engine(self) -> pathlib.Path:
        return self.dir / "src" / ENGINE_REL

    def build(self) -> int:
        return self._build_in(self.dir / "src")

    def restore(self) -> None:
        # Nothing to restore: the whole tree is about to be deleted, and
        # rebuilding it first would be 1.5 s spent proving nothing.
        return

    def __exit__(self, *exc: object) -> None:
        shutil.rmtree(self.dir, ignore_errors=True)


STRATEGIES: dict[str, type[Strategy]] = {"in-tree": InTree, "sandbox": Sandbox}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--list", action="store_true", help="print the set and exit")
    ap.add_argument("--id", action="append", help="run only these mutation ids")
    ap.add_argument("--strategy", choices=sorted(STRATEGIES), default=None,
                    help="where to plant the mutation (default: $HUNMIN_MUTATION_STRATEGY "
                         "or sandbox)")
    args = ap.parse_args()

    if args.list:
        for mid, old, new, chk, d1, d2 in MUTATIONS:
            print(f"{mid:28} checker={chk!s:5} d1={d1!s:5} d2={d2!s:5}")
        return 0

    name = (args.strategy
            or os.environ.get("HUNMIN_MUTATION_STRATEGY")
            or "sandbox")
    if not ENGINE.is_file():
        print(f"missing {ENGINE}")
        return 1

    selected = [m for m in MUTATIONS if not args.id or m[0] in args.id]
    if args.id and len(selected) != len(args.id):
        print("unknown id(s); see --list")
        return 2

    original = ENGINE.read_text()

    print(f"{'mutation':30} {'build':6} {'checker':9} {'d1':6} {'d2':6} verdict")
    gaps: list[str] = []
    strategy = STRATEGIES[name]()
    # `with` is what restores: InTree puts hunminadapter.cpp back AND rebuilds,
    # Sandbox just deletes the tree. A separate try/finally here would be a
    # second, weaker copy of that contract.
    with strategy as root:
        # Relative to `root`, not REPO: under the sandbox these must be the
        # sandbox's copies, or the checker would read the untouched file and
        # every mutation would look caught.
        engine = root / ENGINE_REL
        checker = root / CHECKER_REL
        original = engine.read_text()
        print(f"strategy: {name}")
        for mid, old, new, want_chk, want_d1, want_d2 in selected:
            mutated = _apply(original, old, new)
            if mutated is None:
                print(f"{mid:30} {'-':6} {'-':9} {'-':6} ANCHOR NOT FOUND")
                gaps.append(f"{mid}: anchor not found (source moved)")
                continue
            engine.write_text(mutated)
            if strategy.build() != 0:
                print(f"{mid:30} {'fail':<6} {'-':9} {'-':6} build rejects")
                continue
            chk = _run([sys.executable, str(checker)], cwd=root)
            d1 = _run(["ctest", "--test-dir", str(root / "build"),
                       "-R", "engine-d1-fcitx5"], cwd=root)
            d2 = _run(["ctest", "--test-dir", str(root / "build"),
                       "-R", "engine-d2-ui"], cwd=root)
            chk_rej, d1_rej, d2_rej = chk != 0, d1 != 0, d2 != 0
            guarded = (chk_rej and d1_rej) or (chk_rej and d2_rej) or d2_rej
            verdict = "guarded" if guarded else "GAP"
            if chk_rej != want_chk:
                gaps.append(f"{mid}: checker_rejects={chk_rej}, expected {want_chk}")
            if d1_rej != want_d1:
                gaps.append(f"{mid}: d1_rejects={d1_rej}, expected {want_d1}")
            if d2_rej != want_d2:
                gaps.append(f"{mid}: d2_rejects={d2_rej}, expected {want_d2}")
            print(f"{mid:30} {'ok':<6} {str(chk_rej):9} {str(d1_rej):6} "
                  f"{str(d2_rej):6} {verdict}")

    print()
    if gaps:
        print(f"{len(gaps)} expectation mismatch(es) — update MUTATIONS or fix:")
        for g in gaps:
            print("  -", g)
        return 1
    print(f"mutation set matches its recorded expectations; hunminadapter.cpp restored "
          f"({name})")
    return 0


if __name__ == "__main__":
    sys.exit(main())