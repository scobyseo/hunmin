#!/bin/sh
# Regenerate the cultural-text goldens and their tier-C traces.
#
# A text file in tests/golden/texts/ is typed through one layout by
# tools/gen-cultural-golden.py and becomes one tier-A golden plus one tier-C
# trace. Nothing is typed by hand: the keys come from a search or a probed
# table, the expectations from hunmin-sim --trace.
#
# HOW THE KEYS ARE FOUND, and why it differs by layout
#   dubeolsik and the Latin layouts -- tools/sin3p2-ksgen.py's BFS over the
#     layout's own key table (hunmin-sim --batch as the oracle).
#   sin3-p2 -- the BFS cannot do it: a 세벌식 automaton makes a key's meaning
#     depend on the slots already filled, and depth-4 search on this text found
#     81 of 124 targets, reaching neither 없 nor 워. tools/jamo-keytable.py
#     probes each cho/jung/jong in its own context (linear), and a syllable is
#     then a lookup, so that is what this uses there.
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

SEOSI_LAYOUTS="dubeolsik sin3-p2"
PANGRAM_LAYOUTS="us-qwerty colemak dvorak"

WORK="${TMPDIR:-/tmp}/hunmin-cultural.$$"
mkdir -p "$WORK"
trap 'rm -rf "$WORK"' EXIT INT TERM

generate() {  # <text file> <layout> <name> [extra gen-cultural-golden args...]
  text="$1"; layout="$2"; name="$3"; shift 3
  python3 "$ROOT/tools/gen-cultural-golden.py" \
    --text-file "$ROOT/tests/golden/texts/$text" --layout "$layout" --sim "$SIM" \
    "$@" --out "$ROOT/tests/golden/cultural-$name-$layout.jsonl"
  python3 "$ROOT/tools/gen-conformance.py" \
    --golden "$ROOT/tests/golden/cultural-$name-$layout.jsonl" \
    --platform android --layout "$layout" --case "$name" --batch \
    --out "$ROOT/tests/conformance/android/cultural-$name-$layout.trace.in"
}

for layout in $SEOSI_LAYOUTS; do
  if [ "$layout" = "sin3-p2" ]; then
    python3 "$ROOT/tools/jamo-keytable.py" --layout "$layout" \
      --table-out "$WORK/kt-$layout.json" >/dev/null 2>&1
    generate seosi.txt "$layout" seosi --jamo-table "$WORK/kt-$layout.json"
  else
    generate seosi.txt "$layout" seosi
  fi
done
for layout in $PANGRAM_LAYOUTS; do
  generate pangram.txt "$layout" pangram
done

echo "wrote goldens and traces; capture each .gold with trace-check.py --emit and review it"