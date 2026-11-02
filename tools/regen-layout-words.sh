#!/bin/sh
# Regenerate the per-layout word goldens and their tier-C traces (issue #186).
#
# The conformance corpus exercised 4 of the 15 shipped layouts. Each layout's
# tier-A parity walk (`ref-<layout>.jsonl`) cannot be replayed as a tier-C trace:
# one key there finalises a syllable and the very next key starts another, so
# tier A's per-event `commit` sits mid-word, while the fcitx5 adapter commits a
# whole buffered word at the boundary (docs/FRONTEND-EQUIVALENCE.md §6.3). A
# SHORT WORD is replayed identically on every platform, so that is what this
# script builds: one word per layout, keys found by tools/sin3p2-ksgen.py
# (BFS over the layout's OWN key table, hunmin-sim --batch as the oracle) and
# expectations read back from the engine by tools/wordlist2golden.py.
#
# Nothing here types an expectation. The keys are searched, the commit/preedit
# are the engine's answers, and the trace's `in` lines are copied from the
# golden by tools/gen-conformance.py.
#
# WHY x11 HAS NO LATIN-LAYOUT TRACE. colemak, dvorak and us-qwerty emit text on
# EVERY key, so tier A's goldens carry a commit at every event and no boundary
# word. The x11 emitter buckets tier-A commits into the buffered word and echoes
# an expectation only at the boundary, so the trace it emits cannot equal the
# `.in` it was given (measured: every event of `hello` differs). That is a
# limitation of the x11 dialect, not of these layouts, and it is recorded in
# docs/FRONTEND-EQUIVALENCE.md §6.3 rather than worked around here.
#
# Usage:
#   tools/regen-layout-words.sh [BUILD_DIR]     default: build
#
# After it runs, capture each new trace's `.gold` from the runner and REVIEW the
# diff -- `tools/trace-check.py <trace.in> --runner <runner> --emit` is the only
# way a `.gold` may be written (docs/trace-spec.md), and a blind accept is how a
# red test becomes green without anything being measured.
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$ROOT/build}"
SIM="$BUILD/hunmin-sim"
[ -x "$SIM" ] || { echo "no $SIM; cmake --build $BUILD first" >&2; exit 1; }

WORK="${TMPDIR:-/tmp}/hunmin-layout-words.$$"
mkdir -p "$WORK"
trap 'rm -rf "$WORK"' EXIT INT TERM

HANGUL="ahnmatmae dubeolsik-yet romaja sebeolsik-3-2 sebeolsik-3-90 sebeolsik-final sebeolsik-noshift sebeolsik-yet"
# No Latin block: colemak/dvorak/us-qwerty are covered by the English evaluation
# text instead (tools/regen-cultural.sh), which reaches 47 of a Latin layout's 47
# keys where a one-word probe reached five.

golden() {  # <layout> <words file>
  python3 "$ROOT/tools/sin3p2-ksgen.py" --layout "$1" \
    --text-file "$ROOT/tools/$2" --out "$WORK/ks-$1.json" >/dev/null
  python3 "$ROOT/tools/wordlist2golden.py" --layout "$1" \
    --words-file "$ROOT/tools/$2" --ksgen "$WORK/ks-$1.json" \
    --sim "$SIM" --out "$ROOT/tests/golden/words-$1.jsonl"
}

trace() {  # <golden layout> <trace layout> <platform> <case> <out>
  python3 "$ROOT/tools/gen-conformance.py" \
    --golden "$ROOT/tests/golden/words-$1.jsonl" \
    --platform "$3" --layout "$2" --case "$4" --out "$5"
}

for l in $HANGUL; do
  golden "$l" words-conformance-hangul.txt
  trace "$l" "$l" x11 "나" "$ROOT/tests/conformance/x11/words-$l.trace.in"
  trace "$l" "$l" android "나" "$ROOT/tests/conformance/android/words-$l.trace.in"
done

echo "wrote goldens and traces; capture each .gold with trace-check.py --emit and review it"