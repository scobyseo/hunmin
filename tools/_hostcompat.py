#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Host-portability helpers shared by the standalone scripts under tools/.

The encoding half of this problem is handled at the process boundary:
CMakeLists.txt sets PYTHONUTF8=1 for every test, so open(), Path.read_text()
and subprocess text mode all default to UTF-8 and child Python processes
inherit the same setting. This module holds the parts that setting cannot
express -- locating a POSIX shell or a native compiler, and writing LF bytes
on a host whose text mode would otherwise translate newlines.

Import is a plain sibling import (`from _hostcompat import ...`): when a tool
is run as `python tools/<tool>.py`, sys.path[0] is the tools directory.
"""
import os
import pathlib
import shutil

UTF8 = "utf-8"


def first_on_path(*names: str) -> str | None:
    """The first executable name that resolves on PATH, or None."""
    for name in names:
        found = shutil.which(name)
        if found:
            return found
    return None


def posix_shell() -> str | None:
    """A POSIX shell for .sh runners, preferring Git-for-Windows on Windows.

    A default Windows install puts the WSL launcher (WindowsApps\\bash.exe) on
    PATH, and that bash cannot open the drive-letter paths ctest passes it --
    measured as rc=127 for every Android tier-C trace. Git's bash understands
    both the Windows path and the MSYS tools (find/paste/realpath) the runners
    use. POSIX hosts keep the normal PATH lookup.
    """
    if os.name == "nt":
        for pf in (os.environ.get("ProgramFiles"),
                   os.environ.get("ProgramFiles(x86)")):
            if not pf:
                continue
            for sub in (pathlib.Path(pf) / "Git" / "usr" / "bin" / "bash.exe",
                        pathlib.Path(pf) / "Git" / "bin" / "bash.exe"):
                if sub.exists():
                    return str(sub)
    return shutil.which("bash")


def native_cc_cxx() -> tuple[str | None, str | None]:
    """The C/C++ compiler pair this host can actually run.

    POSIX images usually provide cc/c++; a MinGW host provides gcc/g++. Using
    a fixed pair made a native CMake cache look foreign on Windows and the
    host-guards fixture failed on the compiler it was meant to exercise.
    """
    return (first_on_path("cc", "gcc", "clang"),
            first_on_path("c++", "g++", "clang++"))


def write_lf(path: pathlib.Path, text: str) -> None:
    """Write UTF-8 with LF on every host.

    Path.write_text translates "\\n" to os.linesep on Windows, which made the
    same krdict fixture produce CRLF there and broke the byte-identical
    comparison against the committed LF expected file.
    """
    path.write_bytes(text.replace("\r\n", "\n").encode(UTF8))
