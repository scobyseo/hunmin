#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Fetch the NIKE 한국어기초사전 dump into data-orig/krdict/.
# 368 MB, so unlike every other converter input this is NOT vendored --
# libhunmin/data/krdict/ holds the extracted table instead. See
# libhunmin/data/krdict/LICENSE.md (CC-BY-SA 2.0 KR).
#
# usage: tools/fetch-krdict.sh [OUTDIR]     default OUTDIR=data-orig/krdict
set -eu
OUT=${1:-"$(dirname "$0")/../data-orig/krdict"}
BASE=https://raw.githubusercontent.com/spellcheck-ko/korean-dict-nikl/master/krdict
mkdir -p "$OUT"
# Files are sharded by target_code; 011 is the tail (jamo). Verify the count so
# a partial fetch cannot pass silently.
n=0
for i in $(seq -w 1 11); do
  f="$OUT/$i.xml"
  [ -s "$f" ] || curl -fsSL --retry 3 -o "$f" "$BASE/$i.xml"
  n=$((n + 1))
done
echo "fetch-krdict: $n file(s) in $OUT"
for f in "$OUT"/*.xml; do
  grep -q '<LexicalResource' "$f" || { echo "fetch-krdict: $f is not a lexicon" >&2; exit 1; }
done
