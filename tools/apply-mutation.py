#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Apply one mutation pattern to a source file, or fail loudly.

    tools/apply-mutation.py <target> <pattern-file>

A pattern file is two sections separated by a line reading exactly
`---REPLACE---`: the text to find, then the text to substitute. Only the FIRST
occurrence is replaced.

Exit 0 applied, 3 the pattern was not found (a setup error, not a result).

The not-found case is a hard error on purpose. A mutation test whose mutation
silently fails to apply reports "the suite stayed green" for a mutation that
never happened, which is indistinguishable from a mutation the suite cannot
catch -- the one result that would make this file worthless. Every mutation in
tools/android-mutations.sh is checked against real source text, so if one stops
matching the honest outcome is a red build naming the pattern.
"""
import pathlib
import sys

SEPARATOR = "\n---REPLACE---\n"


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    target = pathlib.Path(argv[1])
    pattern = pathlib.Path(argv[2])

    raw = pattern.read_text(encoding="utf-8")
    old, sep, new = raw.partition(SEPARATOR)
    if not sep:
        print(
            f"SETUP ERROR: {pattern} has no '---REPLACE---' separator line",
            file=sys.stderr,
        )
        return 3

    # Strip the single leading newline a heredoc-style file tends to carry, and
    # the single trailing one, so the patterns can be written as plain text.
    if old.startswith("\n"):
        old = old[1:]
    if new.endswith("\n"):
        new = new[:-1]

    src = target.read_text(encoding="utf-8")
    if old not in src:
        print(
            f"SETUP ERROR: pattern not found in {target}:\n"
            f"--- pattern begins ---\n{old}\n--- pattern ends ---",
            file=sys.stderr,
        )
        return 3

    target.write_text(src.replace(old, new, 1), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
