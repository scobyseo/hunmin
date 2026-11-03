#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Regression guard for the two host situations that turned Linux tiers red on
a CRLF worktree (issue #202).

1. Inputs the Linux build compares byte-for-byte must be LF in the working
   tree even when core.autocrlf=true. `.gitattributes` now defaults every text
   file to `text=auto eol=lf`, with the byte-faithful data directories and the
   Windows batch files overriding it. Check the effective attributes rather
   than trusting the file: a later `-text` or `*.bat` rule, or a future edit
   that drops the default, must fail here.

2. tools/frontend-mutations.py must not borrow the toolchain of another host
   from a build*/CMakeCache.txt. A Windows build dir on a WSL checkout named
   C:/llvm-mingw-*/gcc.exe, and the sandbox died at configure. It now skips a
   cache whose compilers are not on this host, and HUNMIN_MUTATION_CACHE names
   the right one explicitly (ctest sets it to the running build dir).

Run:  python3 tools/host-guard-check.py
"""

import importlib.util
import os
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent

# Files the Linux build/tests read as text and compare byte-for-byte (or run
# with an LF-sensitive interpreter). None may check out CRLF.
LF = [
    "libhunmin/tests/abi/symbols.txt",
    "tests/fixtures/krdict/expected/krdict.json",
    "tests/fixtures/krdict/01.xml",
    "CMakeLists.txt",
    "tools/abi-check.sh",
    "tools/krdict-fixture-check.py",
    "tools/frontend-mutations.py",
]

# Byte-faithful and binary: the default must NOT normalize these. A CRLF here
# is data (win32 traces, the golden words file) or corruption (the jar).
BYTE_FAITHFUL = [
    "libhunmin/data/layouts/dubeolsik.json",
    "tests/golden/words-sin3-p2.jsonl",
    "tests/conformance/win32/dkssud-combo.trace.in",
    "frontends/android/gradle/wrapper/gradle-wrapper.jar",
]

CRLF = ["tools/build-msvc.bat"]


def check_attributes() -> list[str]:
    paths = LF + BYTE_FAITHFUL + CRLF
    missing = [p for p in paths if not (ROOT / p).exists()]
    if missing:
        return [f"attribute check names a missing path: {p}" for p in missing]

    out = subprocess.run(
        ["git", "-C", str(ROOT), "check-attr", "text", "eol", "--", *paths],
        capture_output=True,
        text=True,
    )
    if out.returncode:
        return [f"git check-attr failed: {out.stderr.strip()}"]

    attrs: dict[tuple[str, str], str] = {}
    for line in out.stdout.splitlines():
        path, _, rest = line.partition(": ")
        attr, _, value = rest.partition(": ")
        attrs[(path, attr)] = value

    problems: list[str] = []
    for p in LF:
        eol = attrs.get((p, "eol"))
        text = attrs.get((p, "text"))
        if eol != "lf" or text not in ("auto", "set"):
            problems.append(
                f"{p}: text={text!r} eol={eol!r}; the default text=auto eol=lf "
                f"must apply (a CRLF checkout breaks the Linux guard)"
            )
    for p in BYTE_FAITHFUL:
        text = attrs.get((p, "text"))
        if text != "unset":
            problems.append(
                f"{p}: text={text!r}; byte-faithful data and binaries must be "
                f"`-text` so no EOL normalization touches them"
            )
    for p in CRLF:
        eol = attrs.get((p, "eol"))
        if eol != "crlf":
            problems.append(f"{p}: eol={eol!r}; batch files must check out CRLF")
    return problems


def load_mutations():
    spec = importlib.util.spec_from_file_location(
        "frontend_mutations", ROOT / "tools" / "frontend-mutations.py"
    )
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def write_cache(path: pathlib.Path, body: str) -> None:
    path.mkdir(parents=True)
    (path / "CMakeCache.txt").write_text(body, encoding="utf-8")


def check_toolchain_pick() -> list[str]:
    mod = load_mutations()
    saved = os.environ.pop("HUNMIN_MUTATION_CACHE", None)
    try:
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            # Sorts before build-native, so it is the first glob hit -- the
            # exact way the Windows tree won on WSL.
            write_cache(
                root / "build-a-foreign",
                "CMAKE_GENERATOR:INTERNAL=Ninja\n"
                "CMAKE_C_COMPILER:FILEPATH=C:/llvm-mingw/gcc.exe\n"
                "CMAKE_CXX_COMPILER:FILEPATH=C:/llvm-mingw/g++.exe\n",
            )
            write_cache(
                root / "build-b-native",
                "CMAKE_GENERATOR:INTERNAL=Ninja\n"
                "CMAKE_C_COMPILER:FILEPATH=cc\n"
                "CMAKE_CXX_COMPILER:FILEPATH=c++\n",
            )
            got = mod.Sandbox._parent_toolchain(root)
            want = [
                "-G",
                "Ninja",
                "-DCMAKE_C_COMPILER=cc",
                "-DCMAKE_CXX_COMPILER=c++",
            ]
            problems = []
            if got != want:
                problems.append(
                    f"_parent_toolchain picked {got!r}; expected the native "
                    f"cache's {want!r} (the foreign Windows cache must be skipped)"
                )
            native = (root / "build-b-native" / "CMakeCache.txt").resolve()
            os.environ["HUNMIN_MUTATION_CACHE"] = str(native)
            got = mod.Sandbox._parent_toolchain(root)
            if got != want:
                problems.append(
                    f"HUNMIN_MUTATION_CACHE={native} gave {got!r}; expected {want!r}"
                )
            return problems
    finally:
        os.environ.pop("HUNMIN_MUTATION_CACHE", None)
        if saved is not None:
            os.environ["HUNMIN_MUTATION_CACHE"] = saved


def main() -> int:
    if not (ROOT / ".git").exists():
        print(
            "host-guards: skipped (no .git; attributes cannot be read from an "
            "archive export)",
            file=sys.stderr,
        )
        return 77
    problems = check_attributes() + check_toolchain_pick()
    if problems:
        for p in problems:
            print(f"host-guards: {p}", file=sys.stderr)
        return 1
    print(
        "host-guards: LF defaults and the mutation sandbox's toolchain pick are intact"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
