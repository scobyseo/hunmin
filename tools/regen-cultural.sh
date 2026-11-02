#!/bin/sh
# Regenerate the cultural-text goldens and their tier-C traces.
#
# A text file in tests/golden/texts/ is typed through one layout by
# tools/gen-cultural-golden.py (keys per character from tools/sin3p2-ksgen.py's
# BFS over that layout's own key table, expectations from hunmin-sim --trace),
# and the result becomes one tier-A golden plus one tier-C trace.
#
# WHY THE TEXTS ARE THE SIZE THEY ARE
#   seosi.txt   — 서시, the Korean cultural text: 104 syllables in nine lines,
#                 title and imprint included. Hanja (序詩) is carried as a
#                 `hanja_segments` run, not dropped.
#   pangram.txt — the English evaluation text: a Latin layout's key table is 47
#                 keys, and one sentence cannot reach the digit or punctuation
#                 rows (measured: the classic pangram covers 27 of them), so the
#                 file carries the sentence in both cases plus those rows.
#                 Measured coverage on all three Latin layouts: 47/47 keys.
#
# WHY ANDROID ONLY
#   The three Latin layouts have no x11 form at all — a Latin layout commits on
#   every key and the x11 emitter carries a commit only at a boundary event
#   (docs/trace-spec.md "What a case can express on each platform"). win32 can
#   express them, but its `.gold` may only be written by the runner on a Windows
#   host: that trace is a Windows round's work, not this script's.
#
# Usage:
#   tools/regen-cultural.sh [BUILD_DIR]      default: build
#
# Then capture each `.gold` from the runner and REVIEW it:
#   python3 tools/trace-check.py <trace.in> --runner tools/android-trace-emit.sh --emit
# A blind accept is how a red test becomes green without anything being measured.
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$ROOT/build}"
SIM="$BUILD/hunmin-sim"
[ -x "$SIM" ] || { echo "no $SIM; cmake --build $BUILD first" >&2; exit 1; }

SEOSI_LAYOUTS="dubeolsik"
PANGRAM_LAYOUTS="us-qwerty colemak dvorak"

generate() {  # <text file> <layout> <name>
  python3 "$ROOT/tools/gen-cultural-golden.py" \
    --text-file "$ROOT/tests/golden/texts/$1" --layout "$2" --sim "$SIM" \
    --out "$ROOT/tests/golden/cultural-$3-$2.jsonl"
  python3 "$ROOT/tools/gen-conformance.py" \
    --golden "$ROOT/tests/golden/cultural-$3-$2.jsonl" \
    --platform android --layout "$2" --case "$3" --batch \
    --out "$ROOT/tests/conformance/android/cultural-$3-$2.trace.in"
}

for layout in $SEOSI_LAYOUTS; do
  generate seosi.txt "$layout" seosi
done
for layout in $PANGRAM_LAYOUTS; do
  generate pangram.txt "$layout" pangram
done

echo "wrote goldens and traces; capture each .gold with trace-check.py --emit and review it"