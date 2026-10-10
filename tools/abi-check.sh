#!/bin/sh
# ABI guard: exported hunmin_* symbols must match the committed list exactly.
set -e
nm --defined-only -g "$1" | awk '{print $NF}' | grep '^hunmin_' | sort -u > "$2.tmp"
if ! diff -u "$2" "$2.tmp"; then
  echo "ABI changed. Review, then: cp $2.tmp $2 (append-only policy!)"
  rm "$2.tmp"; exit 1
fi
rm "$2.tmp"; echo "abi ok"
