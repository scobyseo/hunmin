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
import re
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
        # Re-anchored 2026-11-01 (issue #164): the one commit point now also
        # carries the text the mode switch's own boundary produced, so the
        # guard's subject is the delivery expression, not just out.commit.
        # Re-anchored again the same day (issue #176): the engine's strings are
        # copied before the judgement runs (they are borrowed until the next
        # engine call), so the expression names the copies.
        "        if (!justDelivered.empty() || !switched.empty()) {",
        "        if ((!justDelivered.empty() || !switched.empty()) && used) {",
        False,
        True,  # #128: the adapter delivers the layer's answer, so this is no
        True,  # longer visible structurally -- but D1's boundary commit
        #        expectation and D2's panel assertion both reject it now
        #        (measured 2026-10-25; the old gap was D1 having no case that
        #        produced commit + in-flight text, and buffered delivery is
        #        exactly that case)
    ),
    (
        "I5-caps-dropped",
        'hunmin_buffered_preedit_set_option(preedit_, "caps", caps ? 1 : 0);',
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
        "        if (used) {\n            keyEvent.filterAndAccept();\n        }",
        "        keyEvent.filterAndAccept();",
        True,
        True,  # D1: a declined key would be swallowed instead of reaching the client
        False,
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
        "text.setCursor(std::strlen(shown));",
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
        "if (states.test(KeyState::Ctrl)) ev.mods |= HUNMIN_MOD_CTRL;",
        "if (states.test(KeyState::Ctrl) || states.test(KeyState::Shift_L)) ev.mods |= HUNMIN_MOD_CTRL | HUNMIN_MOD_SHIFT;",
        True,
        True,  # every shifted letter becomes a boundary and passes: D1's uppercase cases fail
        False,
    ),
    (
        # Issue #176's fourth checklist item. A rebuilt layer (config apply,
        # layout change) starts from the engine's default OFF, so the stored
        # value has to be re-applied when one is built. Without this the row can
        # say ON while the next word is no longer judged: D1 types `hello`
        # around a rebuild and the second delivery is the Hangul reading.
        "I12-autoswitch-not-reapplied",
        "engine_->recordAutoswitch(applyAutoswitch(*engine_->config().autoswitch ? 1 : 0));",
        "engine_->recordAutoswitch(applyAutoswitch(0));",
        False,  # the checker does not model the row; D1 is the guard
        True,
        False,
    ),
]


def _run(cmd: list[str], timeout: int = 180, cwd: pathlib.Path | None = None,
         stderr_path: pathlib.Path | None = None,
         env: dict[str, str] | None = None) -> int:
    proc = subprocess.run(cmd, cwd=cwd or REPO, capture_output=True, timeout=timeout, env=env)
    if stderr_path is not None:
        stderr_path.write_bytes(proc.stdout + proc.stderr)
    return proc.returncode


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
    ".git", "__pycache__", ".gradle", ".cxx", "node_modules", "Testing",
    ".mypy_cache",
)

# Build output, matched by PATTERN and by TYPE: any DIRECTORY named `build`,
# `obj`, or one of their `-suffixed` variants (`build-trace`, `build-mingw`,
# `build-msvc`, `build-tsf-devhost`, debhelper's `obj-x86_64-linux-gnu`, .NET's
# `obj`). The isdir test is not decoration: `build*` as a plain fnmatch pattern
# would also drop real sources, because ten tracked files carry such a basename
# (tools/build-release.sh, build-android.sh, build-host-jni.sh,
# build-deb-wsl.sh, build-modes-corpus.py, frontends/.../build.gradle.kts, ...).
#
# `.gitignore` already treats `build*/` and `obj-*/` as build state at any
# depth, and this tier builds in the repo, so a copy that keeps them copies
# gigabytes -- and copies them while another build is deleting from them.
# Measured 2026-10-31 inside `dpkg-buildpackage` (issue #154):
# frontend-mutations died at shutil.copytree with
#   [Errno 2] No such file or directory:
#     .../obj-x86_64-linux-gnu/CMakeFiles/Progress
# because `cmake --build` removed the entry between listdir and stat. Excluding
# the directory removes the writer together with the bytes.
_BUILD_STATE_DIR = re.compile(r"^(?:build|obj)(?:-.+)?$")

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
    # Type-aware: a DIRECTORY that is build output, whatever it is called.
    for name in names:
        if name not in ignored and _BUILD_STATE_DIR.match(name) \
                and os.path.isdir(os.path.join(path, name)):
            ignored.add(name)
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
    # Both sides of the type-aware build-state rule, checked on the top level
    # (where a build tree in this repo lives -- `build`/`obj-*` beside the
    # CMakeLists). A directory must be gone; a FILE whose name only looks like
    # build output must survive, because a basename pattern would have dropped
    # it (tools/build-release.sh and nine siblings, issue #154).
    for name in sorted(os.listdir(src)):
        if not _BUILD_STATE_DIR.match(name):
            continue
        here, there = src / name, dst / name
        if here.is_dir() and there.exists():
            raise SystemExit(
                f"sandbox: the build tree {name}/ was copied -- build state must "
                f"not enter the sandbox (a concurrent build is writing in it)")
        if here.is_file() and not there.exists():
            raise SystemExit(
                f"sandbox: {name} was excluded by a build-output pattern but it "
                f"is a tracked file -- the rule must test the entry's type")


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
    def _cache_values(cache: pathlib.Path) -> dict[str, str]:
        vals: dict[str, str] = {}
        for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
            if ":INTERNAL=" in line or ":FILEPATH=" in line:
                key, _, val = line.partition("=")
                vals[key] = val
        return vals

    @staticmethod
    def _compilers_on_host(vals: dict[str, str]) -> bool:
        """A cache belongs to the host that configured it. A Windows checkout
        beside a Linux one (or run from WSL) leaves build-mingw/CMakeCache.txt
        naming C:/llvm-mingw-*/gcc.exe; handing that to a POSIX configure fails
        before the first mutation lands. A compiler that exists, or is on PATH,
        is this host's; one that does not means the cache is another host's and
        must be skipped (issue #202)."""
        for key in ("CMAKE_C_COMPILER:FILEPATH", "CMAKE_CXX_COMPILER:FILEPATH"):
            value = vals.get(key, "").strip()
            if value and not (pathlib.Path(value).exists() or shutil.which(value)):
                return False
        return True

    @classmethod
    def _selected_cache(cls, root: pathlib.Path = REPO) -> pathlib.Path | None:
        """The parent cache to configure the sandbox from.

        HUNMIN_MUTATION_CACHE names it explicitly: the ctest registration sets
        it to the build dir running this test, which removes the guess for the
        normal path. Without it, take the first build*/ cache whose compiler
        exists here, so a foreign Windows tree cannot win the glob."""
        override = os.environ.get("HUNMIN_MUTATION_CACHE")
        if override:
            return pathlib.Path(override)
        for cache in sorted(root.glob("build*/CMakeCache.txt")):
            vals = cls._cache_values(cache)
            if not vals.get("CMAKE_GENERATOR:INTERNAL"):
                continue
            if not cls._compilers_on_host(vals):
                print(f"mutation sandbox: ignoring {cache} (its compilers are "
                      f"not on this host)", file=sys.stderr)
                continue
            return cache
        return None

    @classmethod
    def _parent_prefixes(cls) -> list[str]:
        """The prefixes this host actually built against, from the parent
        cache. A dependency installed outside the system prefix (fcitx5 5.1.13
        in $HOME on the WSL host) is reachable two ways -- CMake config files
        through CMAKE_PREFIX_PATH, pkg-config files through PKG_CONFIG_PATH --
        and a tree that needs the second fails its INITIAL BUILD in the sandbox
        while the parent built fine. Both are derived from the same value."""
        cache = cls._selected_cache()
        if cache is not None:
            for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
                if line.startswith("CMAKE_PREFIX_PATH:"):
                    value = line.partition("=")[2]
                    return [p for p in value.split(";") if p.strip()]
        # A build tree OUTSIDE the repo (the WSL layout keeps one in $HOME, so
        # the glob above finds nothing) leaves only the caller's environment.
        # Not an error: the harness then builds exactly like the shell that
        # invoked it, which is what a pre-2026-10-25 run did.
        return [p for p in os.environ.get("CMAKE_PREFIX_PATH", "").split(os.pathsep) if p.strip()]

    @classmethod
    def _sandbox_env(cls) -> dict[str, str]:
        env = dict(os.environ)
        pcs: list[str] = []
        for prefix in cls._parent_prefixes():
            for sub in ("lib/pkgconfig", "lib/x86_64-linux-gnu/pkgconfig", "share/pkgconfig"):
                candidate = pathlib.Path(prefix) / sub
                if candidate.is_dir():
                    pcs.append(str(candidate))
        if pcs:
            existing = env.get("PKG_CONFIG_PATH", "")
            env["PKG_CONFIG_PATH"] = os.pathsep.join(pcs + ([existing] if existing else []))
        return env

    @classmethod
    def _parent_toolchain(cls, root: pathlib.Path = REPO) -> list[str]:
        """The sandbox must configure like the parent tree did. A bare
        `cmake -B build` picks the platform default generator -- Visual Studio
        on a Windows host, which cannot build this POSIX-shaped tree -- and
        the build fails before the first mutation lands (measured: "sandbox:
        initial build failed" on the llvm-mingw host). The parent's
        CMakeCache is the truthful record of what this host actually can
        build with; reuse its generator, platform and compilers verbatim. A
        cache from another host is skipped by _selected_cache(), and no cache
        at all (the CI layout configures into build/) behaves as before."""
        cache = cls._selected_cache(root)
        if cache is None:
            return []
        vals = cls._cache_values(cache)
        overrides = ["-G", vals["CMAKE_GENERATOR:INTERNAL"].strip()]
        if vals.get("CMAKE_GENERATOR_PLATFORM:INTERNAL"):
            overrides += ["-A", vals["CMAKE_GENERATOR_PLATFORM:INTERNAL"].strip()]
        for key, opt in (("CMAKE_C_COMPILER:FILEPATH", "-DCMAKE_C_COMPILER="),
                         ("CMAKE_CXX_COMPILER:FILEPATH", "-DCMAKE_CXX_COMPILER="),
                         # A dependency installed OUTSIDE the system prefix
                         # (fcitx5 5.1.13 in $HOME here) is found only
                         # through this cache value. Without it the sandbox
                         # configures, then fails its INITIAL BUILD, which
                         # reads as a code failure and is not one -- the
                         # parent tree built only because the shell that
                         # configured it had CMAKE_PREFIX_PATH exported.
                         ("CMAKE_PREFIX_PATH:UNINITIALIZED", "-DCMAKE_PREFIX_PATH="),
                         ("CMAKE_PREFIX_PATH:STRING", "-DCMAKE_PREFIX_PATH=")):
            if vals.get(key):
                overrides.append(opt + vals[key].strip())
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
        log = self.dir / "initial-build.log"
        if self._build_in(src, stderr_path=log) != 0:
            tail = log.read_text(encoding="utf-8", errors="replace").splitlines()[-15:]
            raise SystemExit("sandbox: initial build failed; last lines of "
                             f"{log}:\n" + "\n".join(tail))
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

    @classmethod
    def _build_in(cls, src: pathlib.Path, stderr_path: pathlib.Path | None = None) -> int:
        return _run(_build_cmd(src / "build"), cwd=src, timeout=900,
                    stderr_path=stderr_path, env=cls._sandbox_env())

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