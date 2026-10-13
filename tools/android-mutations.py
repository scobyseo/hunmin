#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Tier E1 mutation self-test: prove the Android adapter suite can FAIL.

    tools/android-mutations.py [--strategy in-tree|sandbox]
    tools/android-mutations.sh          # the documented entry point, a wrapper

Exit 0 pass, 1 fail, 77 skip (no JDK) -- 77 so that ctest's SKIP_RETURN_CODE
on this tier can actually fire. It could not before: the shell version treated
every non-zero exit from tools/android-unit.sh as "caught", so a host with no
JDK reported 9/9 caught and exit 0 in 0.31 s. Measured on this base, that is
the entire defect class docs/D3-ROOT-CAUSE.md is about, one level up from the
one android-unit.sh was written to close.

WHY THIS FILE, GIVEN THE REPO ALREADY HAS tools/frontend-mutations.py:

  That file plants mutations in the fcitx5 adapter and checks D1/D2 catch
  them. It exists because "the suite is green" proved nothing there: five
  mutations in the F9 path all survived a green run (docs/FRONTEND-CONTRACT.md
  section 3). The same reasoning applies with more force to a brand-new suite,
  so this plants the mutations that MATTER for this adapter and asserts each
  one is caught.

  Every mutation below is a defect this adapter actually had while being
  written, or one whose absence no tier can see:

    1. the caps option is never fed to the engine -- silent for sin3-p2, so
       only a test pinned to sin3-p2-yet can see it.
    2. a declined key is consumed anyway -- the IME eats space.
    3. Ctrl is dropped from the chord mask -- Ctrl+key reaches the engine as a
       plain letter. This is a REAL bug this adapter had: the mask was copied
       from fcitx5's 0xf0000000 KeyState range, which matches no AOSP bit.
    4. Shift is treated as a chord -- every symbol tier breaks.
    5. external text is fed to the engine -- invariant 5, the entire reason
       hunmin_ic_commit_text() exists.

  Mutations 1 and 5 are the important ones: both are invisible in every other
  tier, which is precisely why they are here.

  ONE MUTATION IS KNOWN NOT TO BE CAUGHT, and its absence is deliberate: see
  the comment at its call site below. A harness that claims full coverage
  while quietly omitting an uncaught mutation is worse than one that names
  it.

Patterns live in FILES under tools/android-mutations/ rather than inline in
the script. Inline multi-line patterns inside $(...) mangle their own quoting,
and a mangled pattern silently matches nothing -- which reads as "the mutation
did not apply" for all six, exactly as it did on the first run of the shell
version.
"""
from __future__ import annotations

import argparse
import importlib.util
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
from types import ModuleType

REPO = pathlib.Path(__file__).resolve().parents[1]

KOTLIN = pathlib.Path("frontends/android/app/src/main/java/org/hunmin/ime")
ENGINE = "HunminEngine.kt"
KEYS = "HunminKeys.kt"
VOICE = "VoiceInput.kt"

UNIT = pathlib.Path("tools/android-unit.sh")
APPLY = pathlib.Path("tools/apply-mutation.py")
PATTERNS = pathlib.Path("tools/android-mutations")

# Where android-unit.sh looks for the report it audits. Wiped before every run
# by this driver; see run_unit().
RESULTS = pathlib.Path(
    "frontends/android/app/build/test-results/testDebugUnitTest")

SKIP = 77

# (label, Kotlin file, pattern name, expect_caught). Order is the order the
# shell version planted them in, which is the order the defects above are
# listed in. The fourth field is the same model tools/frontend-mutations.py
# uses for its own expect_* columns: True means tier E1 must reject it, False
# means it is a MEASURED gap and surviving is the expected result. A mutation
# that starts being caught breaks its expectation and forces the question to be
# re-asked, which is the whole reason to record the gap rather than leave the
# pattern file lying on disk unreferenced.
MUTATIONS: list[tuple[str, str, str, bool]] = [
    ("the caps option is never fed to the engine", ENGINE, "no-caps", True),
    ("a declined key is consumed anyway", ENGINE, "always-consume", True),
    ("Ctrl is dropped from the chord mask", KEYS, "no-ctrl", True),
    ("Shift is treated as a chord", KEYS, "shift-is-chord", True),
    ("external text goes through the jamo pipeline", ENGINE,
     "voice-through-engine", True),
    ("recognised text is not trimmed before commit", VOICE, "voice-no-trim", True),
    ("a throwing recogniser is not contained", VOICE, "voice-throw-escapes", True),
    ("two callbacks commit the text twice", VOICE, "voice-double-report", True),
    ("permission decision bypasses the gate", VOICE, "voice-permission-bypass", True),
    # EXPECTED TO SURVIVE. This is not a hypothetical defect: 7914ee3 shipped
    # exactly this -- two startInProcess() calls in a row inside start(), so
    # every tap built two SpeechRecognizers, orphaned the first, and stopped
    # only the second. The regression test written for it
    # (VoiceSessionTest."one tap starts the recogniser exactly once") reaches
    # the route through launchOn(), because start() needs a Context and tier E1
    # is a plain JVM suite with no mocking library -- Robolectric was
    # considered and rejected on the record (VoiceSessionTest.kt:15). The test's
    # own comment says so, and says re-planting the call still gives green.
    #
    # Measured again 2026-10-10 on this branch: 44/44 with the mutation
    # applied, exit 0. Closing it needs either a Context fake in tier E1 or a
    # production change to start()'s Context coupling -- a dependency and a
    # design decision, both of which belong to the maintainer. Until one
    # happens it is named here instead of being silently absent.
    ("one tap starts two recognisers (re-planted 7914ee3)", VOICE,
     "voice-double-start", False),
]

# NOT a mutation: swapping flush for commit on the declined path. It was
# planted, measured, and survives -- see the note in HunminEngineTest and
# FRONTEND-CONTRACT section 4. Unlike the entry above this one is NOT planted,
# so nothing here fails if the two branches later stop differing.


def _load(name: str, rel: str) -> ModuleType:
    """Import a tool by path. They are scripts with hyphens in the name, so
    they cannot be imported normally; tools/mutparallel.py loads the sibling
    the same way."""
    spec = importlib.util.spec_from_file_location(name, REPO / rel)
    if spec is None or spec.loader is None:
        raise SystemExit(f"cannot load {rel}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


# The copy filter lives in the sibling tier, not here, for the reason the fix
# in 5d2756b was made at all: a filter that silently excludes nothing still
# passes, still times, and still copies what it was told to leave behind. Two
# copies of one is how the 368 MB came to be copied twice over.
fm = _load("frontend_mutations", "tools/frontend-mutations.py")


def _engine_chain(build: pathlib.Path) -> dict[str, pathlib.Path]:
    """Every name in the libhunmin.so link chain, mapped to what it resolves to.

    build-host-jni.sh links with -L$ROOT/build -lhunmin and bakes that same
    directory into the shim's RUNPATH, so the sandbox needs libhunmin.so for
    the link AND the SONAME (libhunmin.so.0) for the run, both inside its own
    build/. Missing either, the shim silently falls back to the main checkout's
    engine -- which is the isolation this strategy exists to provide.
    """
    out: dict[str, pathlib.Path] = {}
    link = build / "libhunmin.so"
    while True:
        out[link.name] = link.resolve()
        if not link.is_symlink():
            break
        # os.readlink, not resolve(): resolve() collapses to the real file and
        # loses the SONAME name on the way, so libhunmin.so.0 never got copied
        # and the loader silently found a DIFFERENT libhunmin in /usr/lib. The
        # check below caught that, which is what it is for.
        nxt = build / os.readlink(link)
        if nxt.parent != build.resolve() or nxt.name in out:
            break
        link = nxt
    return out


class Strategy:
    """Where a mutation is planted, and what gets tested against it.

    Same shape and the same trade as tools/frontend-mutations.py: making the
    isolation a choice is what lets the tier run next to anything else.

      in-tree   plant in the working tree, as the shell version did. No setup
                cost, and for its whole duration the checkout holds three
                deliberately broken Kotlin files. The restore copies back a
                snapshot taken at the START, so any edit made to those files
                while the tier runs -- by a person, or by another session's
                editor -- is overwritten and gone. That is the maintainer's
                description: "a concurrent build gets poisoned on both sides
                and blocks parallel development".

      sandbox   copy the tree to a temp dir and do all of it there. Nothing
                outside the temp dir is written, so there is nothing to
                poison and nothing to lose.
    """

    name = "abstract"

    def __enter__(self) -> pathlib.Path:
        raise NotImplementedError

    def source(self, filename: str) -> pathlib.Path:
        """The Kotlin file this strategy mutates."""
        raise NotImplementedError

    def restore(self) -> None:
        """Undo the mutation so the tree is left as it was found."""

    def __exit__(self, *exc: object) -> None:
        self.restore()


class InTree(Strategy):
    name = "in-tree"

    def __enter__(self) -> pathlib.Path:
        self._backup = {f: (REPO / KOTLIN / f).read_text() for _, f, _, _ in MUTATIONS}
        return REPO

    def source(self, filename: str) -> pathlib.Path:
        return REPO / KOTLIN / filename

    def restore(self) -> None:
        for name, text in self._backup.items():
            (REPO / KOTLIN / name).write_text(text)


class Sandbox(Strategy):
    name = "sandbox"

    def __enter__(self) -> pathlib.Path:
        self.dir = pathlib.Path(tempfile.mkdtemp(prefix="hunmin-mut-android."))
        self.src = self.dir / "src"
        print(f"mutation sandbox: {self.src}", file=sys.stderr)
        shutil.copytree(REPO, self.src, symlinks=True, ignore=fm._COPY_IGNORE,
                        ignore_dangling_symlinks=True)
        fm._check_copy(REPO, self.src)

        # The engine binary, copied rather than referenced, for the same reason
        # the whole tree is: a file another process is rewriting under us is
        # worse than one we took a copy of. See _verify_shim_isolated for what
        # this does and does not guarantee.
        #
        # HUNMIN_BUILD_DIR defaults to build/ (plain `cmake -B build`, the linux
        # CI job); a packaging build (debian/rules) exports it to obj-x86_64-
        # linux-gnu, where the engine actually is -- issue #8, measured 2026-10-13.
        build_dir = pathlib.Path(os.environ.get("HUNMIN_BUILD_DIR", REPO / "build"))
        chain = _engine_chain(build_dir)
        out = self.src / "build"
        out.mkdir(exist_ok=True)
        for name, real in chain.items():
            shutil.copy2(real, out / name)
        print(f"  engine copied in: {', '.join(sorted(chain))}", file=sys.stderr)
        return self.src

    def source(self, filename: str) -> pathlib.Path:
        return self.src / KOTLIN / filename

    def restore(self) -> None:
        # Nothing to restore: the whole tree is about to be deleted.
        return

    def __exit__(self, *exc: object) -> None:
        shutil.rmtree(self.dir, ignore_errors=True)


STRATEGIES: dict[str, type[Strategy]] = {"in-tree": InTree, "sandbox": Sandbox}


def _run_unit(root: pathlib.Path) -> tuple[int, str]:
    """Run tier E1 inside `root`, after deleting the report it will audit.

    android-unit.sh audits app/build/test-results/testDebugUnitTest, and its
    whole value is that it FAILS when zero tests ran or any were skipped. That
    check is only worth something if the directory holds the run that just
    happened. Deleting it first makes "green because it read a stale report"
    impossible: with the directory gone, a run that produces nothing fails with
    "no test results" instead of auditing whatever was there before.

    Costs a few milliseconds and one Gradle task's worth of re-writing.
    """
    shutil.rmtree(root / RESULTS, ignore_errors=True)
    p = subprocess.run([str(root / UNIT)], cwd=root, capture_output=True,
                       text=True)
    return p.returncode, (p.stdout + p.stderr).strip()


def _verify_shim_isolated(root: pathlib.Path) -> str | None:
    """Confirm the shim the sandbox will load resolves INSIDE the sandbox.

    build-host-jni.sh bakes -Wl,-rpath,$ROOT/build, where ROOT is wherever it
    ran. If it ran in the main checkout the shim loads the main checkout's
    engine, and a sandbox that quietly tests the wrong binary is the same
    green-but-measuring-nothing shape as a stale results directory. Cheap to
    check, so it is checked.
    """
    shim = root / "build/libhunjni-host.so"
    if not shim.is_file():
        return None  # not built yet; checked on the run after it is
    out = subprocess.run(["ldd", str(shim)], capture_output=True,
                         text=True).stdout
    for line in out.splitlines():
        if "libhunmin" not in line:
            continue
        resolved = line.split("=>")[-1].strip().split(" ")[0]
        if not resolved.startswith(str(root)):
            return f"{shim} loads libhunmin from {resolved}, outside {root}"
    return None


def say(line: str = "") -> None:
    """Progress goes to stdout, diagnostics to stderr, and ctest merges both
    into one log. Block-buffered stdout puts every diagnostic above the header
    it belongs under, so this flushes."""
    print(line, flush=True)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--strategy", choices=sorted(STRATEGIES), default=None,
                    help="where to plant the mutation (default: "
                         "$HUNMIN_MUTATION_STRATEGY or sandbox)")
    args = ap.parse_args()

    name = (args.strategy
            or os.environ.get("HUNMIN_MUTATION_STRATEGY")
            or "sandbox")
    for _, filename, _, _ in MUTATIONS:
        if not (REPO / KOTLIN / filename).is_file():
            say(f"missing {REPO / KOTLIN / filename}")
            return 1
    if not (REPO / PATTERNS).is_dir():
        say(f"missing {REPO / PATTERNS} pattern directory")
        return 1

    say("== tier E1 mutation self-test")
    strategy = STRATEGIES[name]()
    with strategy as root:
        say(f"strategy: {name}")

        # An unmutated run first. Two reasons, both about what a later green
        # means: a checkout whose suite is already red would report all nine
        # mutations "caught" for the wrong reason, and the shim check below
        # needs the build to exist. Costs one run -- 3.3 s in the sandbox --
        # and it is the only place a red baseline can be caught.
        rc, out = _run_unit(root)
        if rc == SKIP:
            print("skip: tier E1 has no JDK", file=sys.stderr)
            return SKIP
        if rc != 0:
            print("FAIL: tier E1 is already red with no mutation planted -- "
                  "every 'caught' below would be meaningless", file=sys.stderr)
            print(out, file=sys.stderr)
            return 1
        say(f"  baseline: {out.splitlines()[-1]}")
        leaked = _verify_shim_isolated(root)
        if leaked:
            print(f"FAIL: {leaked}", file=sys.stderr)
            return 1

        sources = {f: strategy.source(f) for _, f, _, _ in MUTATIONS}
        pristine = {f: p.read_text() for f, p in sources.items()}
        passed = failed = known = 0
        for label, filename, pattern, expect_caught in MUTATIONS:
            target = sources[filename]
            pattern_file = root / PATTERNS / pattern
            rc = subprocess.run([sys.executable, str(root / APPLY), str(target),
                                 str(pattern_file)], cwd=root).returncode
            if rc != 0:
                # Refuses to run if the pattern is absent -- a mutation that
                # silently does not apply is the exact failure mode this tier
                # exists to avoid, and tools/apply-mutation.py exits 3 there.
                print(f"  NOT APPLIED (pattern moved?): {label}", file=sys.stderr)
                failed += 1
                continue
            rc, out = _run_unit(root)
            if rc == SKIP:
                print("skip: tier E1 lost its JDK mid-run", file=sys.stderr)
                return SKIP
            caught = rc != 0
            if caught == expect_caught:
                if caught:
                    say(f"  caught: {label}")
                    passed += 1
                else:
                    say(f"  known gap (expected to survive): {label}")
                    known += 1
            else:
                # Either a guard went missing, or a recorded gap was closed and
                # the expectation now lies. Both are the harness refusing to
                # drift silently, which is its entire reason to exist.
                # Keyed on what actually happened, not on which way the
                # expectation was wrong: an earlier version branched on
                # `if caught` and so told a surviving mutation it had started
                # being caught, which is the one message that would make this
                # harness actively misleading.
                if not caught:
                    print(f"  SURVIVED (BAD): {label} -- tier E1 stayed green "
                          f"but it was expected to be caught", file=sys.stderr)
                else:
                    print(f"  NOW CAUGHT (expectation is stale): {label} -- tier E1 "
                          f"rejects it, so it is no longer a gap. Move it out of "
                          f"the expect_caught=False list and say what closed it.",
                          file=sys.stderr)
                failed += 1
            target.write_text(pristine[filename])

    say()
    if failed:
        print(f"FAIL: {failed} of {passed + known + failed} planted mutations "
              f"did not match their recorded expectation", file=sys.stderr)
        return 1
    say(f"PASS: {passed} caught by tier E1, {known} recorded as known gaps, "
        f"all as expected")
    return 0


if __name__ == "__main__":
    sys.exit(main())