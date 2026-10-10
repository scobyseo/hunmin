#!/bin/sh
# Differential parity sweep: generate fresh libhangul traces (random seeds)
# and require hunmin-sim to reproduce them exactly. $1=build $2=src [N seeds]
set -e
BIN=$1
SRC=$2
SEEDS=${3:-25}
TMP=$BIN/sweep
rm -rf "$TMP"; mkdir -p "$TMP"
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
echo "parity-sweep: $SEEDS seeds x 7 layouts OK"
