#!/bin/sh
# Differential parity sweep: generate fresh libhangul traces (random seeds)
# and require hunmin-sim to reproduce them exactly. $1=build $2=src [N seeds]
set -e
BIN=$1
SRC=$2
SEEDS=${3:-25}
# A unique directory per run, not a fixed "$BIN/sweep": two suites sharing one
# build directory (or two --out experiments run in parallel) each did
# `rm -rf` on the other's files, and the loser read a half-written trace --
# reproduced deterministically 2026-11-22 as `ref-sebeolsik-3-2.4.jsonl:85:
# bad JSON`, and it is why a `parity-sweep` failure under -j16 was not
# evidence about the engine.  The template lives under $BIN so the traces stay
# on disk (the host's /tmp is tmpfs, and a sweep is ~200 files).
TMP=$(mktemp -d "$BIN/sweep.XXXXXX")
ok=0
trap 'if [ "$ok" = 1 ]; then rm -rf "$TMP"; else echo "parity-sweep: artifacts kept in $TMP" >&2; fi' EXIT
run() {
  id=$1; kbd=$2; seed=$3
  f=$TMP/ref-$id.$seed.jsonl
  "$BIN/gen-golden-libhangul" --kbd "$kbd" --layout "$id" --seed "$seed" --steps 200 --out "$f"
  "$BIN/hunmin-sim" --data "$SRC/libhunmin/data" --check "$f" >/dev/null
}
i=0
while [ "$i" -lt "$SEEDS" ]; do
  i=$((i+1))
  run dubeolsik 2 $i
  run dubeolsik-yet 2y $i
  run sebeolsik-3-2 32 $i
  run sebeolsik-3-90 39 $i
  run sebeolsik-final 3f $i
  run sebeolsik-noshift 3s $i
  run sebeolsik-yet 3y $i
  run romaja ro $i
done
ok=1
echo "parity-sweep: $SEEDS seeds x 7 layouts OK"
