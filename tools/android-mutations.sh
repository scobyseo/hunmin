#!/usr/bin/env bash
# Tier E1 mutation self-test: prove the Android adapter suite can FAIL.
#
#   tools/android-mutations.sh [--strategy in-tree|sandbox]
#
# Exit 0 pass, 1 fail, 77 skip (no JDK).
#
# A wrapper, and nothing else: the work is in tools/android-mutations.py,
# which has the InTree/Sandbox strategies the fcitx5 tier has. This file stays
# because it is the invocation recorded in AGENTS.md and wired into
# CMakeLists.txt, and a rename would invalidate both for no gain.
#
# WHY THIS TIER EXISTS is documented there, in full -- the mutations are the
# defects this adapter actually had, the one known-unplanted mutation, and the
# reason the patterns live in files.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
exec python3 "$ROOT/android-mutations.py" "$@"