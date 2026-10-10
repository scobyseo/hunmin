#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Assert the packaged addon carries no tier-C trace code.

The trace sink is #ifdef'd out unless -DHUNMIN_TRACE=ON, so a released addon
should contain neither the env-var name it reads nor the literal it writes.
A "compiled out" claim that is never checked is the same kind of prose rule
FRONTEND-CONTRACT.md §5 was written to eliminate, and this one is cheap to
verify: the strings are either in the binary or they are not.

    tools/check-trace-optout.py <path/to/libhunmin.so> [...]
"""
import pathlib
import subprocess
import sys

MARKERS = (b"HUNMIN_TRACE_OUT", b'{"kind":"')
failures = []


def main(argv: list[str]) -> int:
    if not argv:
        print("usage: check-trace-optout.py <libhunmin.so> [...]", file=sys.stderr)
        return 2
    for arg in argv:
        so = pathlib.Path(arg)
        if not so.is_file():
            print(f"FAIL: {so} does not exist", file=sys.stderr)
            failures.append(str(so))
            continue
        blob = so.read_bytes()
        hits = [m.decode() for m in MARKERS if m in blob]
        if hits:
            print(f"FAIL: {so.name} contains trace code: {hits}", file=sys.stderr)
            failures.append(str(so))
        else:
            print(f"ok  : {so.name} carries no trace code")
    if failures:
        print(f"{len(failures)} FAILED: {failures}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
