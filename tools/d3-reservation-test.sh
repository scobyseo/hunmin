#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# d3-reservation-test.sh -- the tier-D3 display claim, tested directly.
#
# WHAT THIS IS FOR. issue #250: the D3 tiers reserve X display numbers so three
# concurrent runs cannot type into each other. The reservation used `mkdir`,
# which is NOT atomic on this host (/usr/bin/mkdir is uutils coreutils -- see
# docs/ENVIRONMENT.md; three concurrent mkdir on one path produced more than one
# "winner" in 12 of 20 rounds). Two instances then claimed :99 0.6 ms apart, X
# refused the loser, and the winner's fcitx5 died mid-run.
#
# The functions are EXTRACTED FROM tests/e2e/run-d3.sh, not retyped: a copy of
# the logic would keep passing after the shipped one changed.
#
# Exit 0 pass, 1 fail. Runs in a second or two; nothing is left in /tmp.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
F="$ROOT/tests/e2e/run-d3.sh"
[ -f "$F" ] || { echo "FAIL: $F not found" >&2; exit 1; }
for t in Xvfb xwininfo; do
    command -v "$t" >/dev/null 2>&1 ||
        { echo "SKIP: no $t -- tier D3 needs it (docs/ENVIRONMENT.md)" >&2; exit 77; }
done

eval "$(sed -n '/^x_display_up()/,/^}/p' "$F")"
eval "$(sed -n '/^claim_file()/,/^}/p' "$F")"
eval "$(sed -n '/^reserve_display()/,/^}/p' "$F")"
for fn in x_display_up claim_file reserve_display; do
    [ "$(type -t "$fn" 2>/dev/null)" = "function" ] ||
        { echo "FAIL: $fn could not be extracted from run-d3.sh" >&2; exit 1; }
done

fail=0
ok() { printf 'ok   %s\n' "$1"; }
no() { printf 'FAIL %s\n' "$1"; fail=1; }
# The numbers this tool claims: NOT 99..94, which the real tiers reserve. A
# tool that cleaned up /tmp/hunmin-d3-disp.9* would delete a concurrent ctest's
# live claims, so the glob is bounded to what this file creates.
POOL="93 92 91"
cleanup() {
    local n
    for n in $POOL; do rm -f "/tmp/hunmin-d3-disp.$n"; done
}
trap cleanup EXIT
cleanup

# 1. A number with a live server on it is not available.
Xvfb :93 -screen 0 320x240x24 -nolisten tcp >/dev/null 2>&1 &
xvfb=$!
for _ in $(seq 1 50); do x_display_up :93 && break; sleep 0.1; done
RESERVED=""; RES_DISP=""
reserve_display "93 92"
[ "$RES_DISP" = ":92" ] && ok "a live server makes its number unavailable" ||
    no "reserve_display took '$RES_DISP' where :93 has a live server"
kill "$xvfb" 2>/dev/null; wait "$xvfb" 2>/dev/null
for _ in $(seq 1 50); do x_display_up :92 || break; sleep 0.1; done
cleanup

# 2. Concurrent claimers never share a number, and the pool is what bounds them.
: >/tmp/hunmin-d3-claim-out.$$
for _ in $(seq 1 12); do
    ( RESERVED=""; RES_DISP=""
      if reserve_display "93 92 91"; then echo "ok $RES_DISP" >>"/tmp/hunmin-d3-claim-out.$$"
      else echo "none -" >>"/tmp/hunmin-d3-claim-out.$$"; fi ) &
done
wait
claims=$(grep -c '^ok' "/tmp/hunmin-d3-claim-out.$$" || true)
unique=$(awk '/^ok/{print $2}' "/tmp/hunmin-d3-claim-out.$$" | sort -u | wc -l)
[ "$claims" = "3" ] && [ "$unique" = "3" ] &&
    ok "12 concurrent claimers on a three-number pool: 3 claims, 3 distinct" ||
    no "12 concurrent claimers produced $claims claims / $unique distinct (want 3/3)"
rm -f "/tmp/hunmin-d3-claim-out.$$"
cleanup

# 3. A claim whose owner is gone is taken over -- a killed run must not wedge
#    the pool until someone removes files by hand.
echo 999999 >/tmp/hunmin-d3-disp.92      # 999999 is not a running pid here
RESERVED=""; RES_DISP=""
reserve_display "92"
[ "$RES_DISP" = ":92" ] && ok "a dead owner's claim is taken over" ||
    no "a dead owner's claim wedged :92 (got '${RES_DISP:-none}')"
cleanup

# 4. A claim whose owner is ALIVE is honoured -- this is the whole point.
bash -c 'sleep 30' &
live=$!
echo "$live" >/tmp/hunmin-d3-disp.91
RESERVED=""; RES_DISP=""
reserve_display "91"
[ -z "$RES_DISP" ] && ok "a live owner's claim is honoured" ||
    no "reserve_display stole :91 from a running pid $live"
kill "$live" 2>/dev/null; wait "$live" 2>/dev/null
cleanup

# 5. A release from another pid leaves the claim alone. This is the failure
#    that started #250: an empty directory had no owner, so a late release
#    deleted the next run's claim.
RESERVED=""; RES_DISP=""
reserve_display "93"
claim="${RESERVED# }"
if [ -n "$claim" ] && [ "$(cat "$claim" 2>/dev/null)" = "$$" ]; then
    stale=999999
    [ "$(cat "$claim" 2>/dev/null)" = "$stale" ] && rm -f "$claim"
    [ -f "$claim" ] && ok "a release from another pid leaves the claim alone" ||
        no "a release from another pid deleted a live claim"
else
    no "could not claim :93 for the release test (got '$claim')"
fi
cleanup

[ "$fail" -eq 0 ] && echo "PASS: the display claim behaves as #250 requires"
exit "$fail"