#!/bin/sh
# Regenerate reference goldens from the system libhangul and (in check mode)
# diff against committed tests/golden. $1=build dir $2=source dir [$3=check]
set -e
BIN=$1
SRC=$2
GEN=$BIN/gen-golden-libhangul
[ -x "$GEN" ] || GEN=$BIN/../gen-golden-libhangul
OUT=$BIN/goldens
mkdir -p "$OUT"
i=0
for spec in "dubeolsik:2:101" "dubeolsik-yet:2y:102" "sebeolsik-3-2:32:103" \
            "sebeolsik-3-90:39:104" "sebeolsik-final:3f:105" \
            "sebeolsik-noshift:3s:106" "sebeolsik-yet:3y:107" \
            "romaja:ro:108" "ahnmatmae:ahn:109"; do
  hid=$(echo "$spec" | cut -d: -f1)
  kbd=$(echo "$spec" | cut -d: -f2)
  seed=$(echo "$spec" | cut -d: -f3)
  "$GEN" --kbd "$kbd" --layout "$hid" --seed "$seed" --steps 400 --out "$OUT/ref-$hid.jsonl"
  i=$((i+1))
done
if [ "$3" = check ]; then
  for f in "$OUT"/*.jsonl; do
    diff -q "$f" "$SRC/tests/golden/$(basename "$f")" || exit 1
  done
  echo "goldens reproducible ($i files)"
else
  cp "$OUT"/*.jsonl "$SRC/tests/golden/"
  echo "goldens written ($i files)"
fi
