#!/bin/sh
# ABI guard: exported hunmin_* symbols must match the committed list exactly.
#
# The scratch file is mktemp'd, not "$2.tmp".  That name sat beside the
# COMMITTED list, so two suites running against one checkout collided on it
# (measured 2026-11-22: 3 of 3 parallel rounds failed with
# `rm: libhunmin/tests/abi/symbols.txt.tmp: No such file or directory`), and an
# interrupted run left the file in a tracked directory.
set -e
TMP=$(mktemp "${TMPDIR:-/tmp}/hunmin-abi.XXXXXX")
trap 'rm -f "$TMP"' EXIT
nm --defined-only -g "$1" | awk '{print $NF}' | grep '^hunmin_' | sort -u > "$TMP"
if ! diff -u "$2" "$TMP"; then
  echo "ABI changed. Review, then append the new symbols to $2 (append-only policy!)."
  exit 1
fi
echo "abi ok"