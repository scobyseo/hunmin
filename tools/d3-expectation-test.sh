#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# d3-expectation-test.sh -- the tier-D3 IBus-host paths, exercised against the
# functions the runner actually ships.
#
# WHAT THIS IS FOR. Issue #325. The IBus lanes failed on the jammy runner with
# two defects that no fcitx5 lane can reveal, because both are ibus-host-only:
#
#   1. inject_keys turned `kfs{F9}{Return}` into `kfs{Control_R{Return}` and
#      appended a stray `}` to every case that has no {F9} at all. bash ends
#      `${k//\{F9\}/{Control_R}}` at the `}` of the REPLACEMENT, so the token
#      is truncated and the rest of the string shifts by one character. The
#      injector then reported "unknown named key {Control_R{Return}" and every
#      buffer carried one extra `}`.
#   2. expected_text dropped the trace's trailing in-flight word, so every
#      expectation stopped at the last key that produced a commit: an EMPTY
#      expectation for a case with no space in it, and `... 우리나라 ` for a
#      long one. That is exactly what run 45026960367 printed:
#      `expected: TEXT:` against `actual: TEXT:안녕ㄴ}`.
#
# Neither is reachable without a 22.04/IBus runner, and both are pure text
# transformations, so they are pinned here instead of in a tier that cannot run
# on this host. The expected values are the ones that dispatch run MEASURED
# (45026960367, job 137221976486), with defect 1 removed: actual minus the
# forged `}`. They are evidence from the real IBus adapter, not a re-derivation
# from the same simulator the fold reads.
#
# The functions are EXTRACTED FROM tests/e2e/run-d3.sh, not retyped, for the
# reason tools/d3-gate-failure-test.sh extracts its own: a copy would keep
# passing after the shipped one changed.
#
# usage: tools/d3-expectation-test.sh SIM DATA
# Exit 0 pass, 1 fail.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
F="$ROOT/tests/e2e/run-d3.sh"
SIM=${1:?usage: $0 SIM DATA}
DATA=${2:?usage: $0 SIM DATA}
[ -f "$F" ] || { echo "FAIL: $F not found" >&2; exit 1; }
[ -x "$SIM" ] || { echo "FAIL: hunmin-sim not built at $SIM" >&2; exit 1; }
[ -d "$DATA" ] || { echo "FAIL: data dir not found at $DATA" >&2; exit 1; }

for fn in sim_keys sim_layer_args expected_text inject_keys; do
    eval "$(sed -n "/^$fn()/,/^}/p" "$F")"
    [ "$(type -t "$fn" 2>/dev/null)" = "function" ] ||
        { echo "FAIL: $fn could not be extracted from run-d3.sh" >&2; exit 1; }
done

fail=0
ok() { printf 'ok   %s\n' "$1"; }
no() { printf 'FAIL %s\n' "$1"; fail=1; }

# check_fold HOST LAYOUT KEYS EXPECTED
check_fold() {
    local host=$1 layout=$2 keys=$3 want=$4 got
    got=$(IM_HOST=$host expected_text "$layout" "$keys")
    if [ "$got" = "$want" ]; then
        ok "[$host $layout/$keys] -> $want"
    else
        no "[$host $layout/$keys]"
        printf '  want: %s\n  got:  %s\n' "$want" "$got"
    fi
}

# --- the buffer fold, on both hosts ------------------------------------------
# Measured on the REAL frontends, not derived from the fold:
#   * ibus host, ubuntu-22.04, run 45026960367 (issue #325) -- the actual values
#     of the eight cases, with the `}` defect 1 forged removed;
#   * fcitx5 host, this host, engine-d3-gtk3 2026-11-22 -- the same eight
#     strings, which is the property the fold now has: the word layer only
#     delays commits to a boundary, and the barrier key flushes the rest.
# Both hosts are checked against the same value so that a change which makes
# them disagree fails here rather than only on a 22.04 runner.
for host in ibus fcitx5; do
    check_fold "$host" dubeolsik 'dkssuds' 'TEXT:안녕ㄴ'
    check_fold "$host" sin3-p2 'kfcQ' 'TEXT:각ㅒ'
    check_fold "$host" sin3-p2 'jk1' 'TEXT:ㅇµ'
    check_fold "$host" dubeolsik 'dkssuds ' 'TEXT:안녕ㄴ '
    check_fold "$host" dubeolsik \
        'ehdgoanfrhk qorentksdl akfmrhe kfgehfhr gksmsladl qhdngktk dnflskfk akstp' \
        'TEXT:동해물과 백두산이 마르곧 ㅏㅀ도록 하느님이 보우하사 우리나라 만세'
    check_fold "$host" dubeolsik \
        'wnrsms skfRkwlg ksmfdmf dnfjfj gks wja qnRmfjadl djqtrlfmf' \
        'TEXT:죽는 날까짛 ㅏ늘을 우러러 한 점 부끄럼이 없기를'
    check_fold "$host" sin3-p2 \
        'u/ameibwk/f ;ecubnfsjd ifygk/ ufwdu/y/c mfhghdzjd ;/jbmfnf jbydhfyf ifsnc' \
        'TEXT:동해물과 백두산이 마르고 닳도록 하느님이 보우하사 우리나라 만세'
    check_fold "$host" sin3-p2 \
        'lbchgs hfwkkfld mfhgwjgw jbyryr mfs lrz ;bkkgyrzjd jreqkdygw' \
        'TEXT:죽는 날까지 하늘을 우러러 한 점 부끄럼이 없기를'
done

# --- inject_keys' {F9} translation ------------------------------------------
# The stub stands in for xtest-type.py: it prints the key string it was handed,
# which is the only thing inject_keys can get wrong.
PROBE=$(mktemp -d "${TMPDIR:-/tmp}/d3-expect-probe.XXXXXX") || exit 1
trap 'rm -rf "$PROBE"' EXIT
cat >"$PROBE/typer.py" <<'EOF'
import sys
print(sys.argv[2])
EOF
TYPER="$PROBE/typer.py"
DISP="probe"
check_inject() { # $1 = case keys, $2 = what the injector must receive
    local got
    got=$(IM_HOST=ibus inject_keys "$1")
    if [ "$got" = "$2" ]; then
        ok "[inject $1] -> $2"
    else
        no "[inject $1]"
        printf '  want: %s\n  got:  %s\n' "$2" "$got"
    fi
}
check_inject 'kfs{F9}{Return}' 'kfs{Control_R}{Return}'
check_inject 'kfs{F9}{Down},{Return}' 'kfs{Control_R}{Down},{Return}'
check_inject 'kfs{F9}' 'kfs{Control_R}'
check_inject 'dkssuds' 'dkssuds'
check_inject 'dkssud^' 'dkssud^'

# The fcitx5 host must NOT translate: F9 is the catalogue key there.
got=$(IM_HOST=fcitx5 inject_keys 'kfs{F9}{Return}')
if [ "$got" = 'kfs{F9}{Return}' ]; then
    ok '[inject kfs{F9}{Return}] fcitx5 keeps {F9}'
else
    no '[inject kfs{F9}{Return}] fcitx5 translated the token'
    printf '  got: %s\n' "$got"
fi

if [ "$fail" -eq 0 ]; then
    echo "PASS: the ibus-host expectation fold and {F9} translation are correct"
fi
exit "$fail"