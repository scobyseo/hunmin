#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# d3-gate-failure-test.sh -- the tier-D3 startup gate's FAILURE path, exercised.
#
# WHAT THIS IS FOR. issue #253: `start_fcitx` in tests/e2e/run-d3.sh waits for
# the root-window `@server=fcitx` property and for "Loaded addon hunmin" in the
# daemon's log, and when that gate fails the tier has to leave behind enough to
# say WHICH half was missing -- the first occurrence left a log whose tail did
# not even carry the xim addon line, and diagnosing it cost a re-instrumented
# copy of the script. The failure path is now a full evidence dump, and it only
# ever runs when a real run flakes, so nothing else would ever notice it
# breaking.
#
# The functions are EXTRACTED FROM tests/e2e/run-d3.sh, not retyped: a copy of
# the logic would keep passing after the shipped one changed. The daemon is a
# stub that writes the two "Loaded addon" lines and never opens an X connection,
# which is exactly the shape the two caught occurrences had (see
# docs/D3-PERFORMANCE.md, 2026-11-05).
#
# NOT REGISTERED in ctest: it costs the gate's own 20 s, which the slow tier's
# budget cannot afford for a path that is not a regression guard (the tier
# passing is). Run it by hand after touching start_fcitx:
#
#   tools/d3-gate-failure-test.sh
#
# Exit 0 pass, 1 fail, 77 skip (no Xvfb).
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
F="$ROOT/tests/e2e/run-d3.sh"
[ -f "$F" ] || { echo "FAIL: $F not found" >&2; exit 1; }
for t in Xvfb xwininfo xprop; do
    command -v "$t" >/dev/null 2>&1 ||
        { echo "SKIP: no $t -- tier D3 needs it (docs/ENVIRONMENT.md)" >&2; exit 77; }
done

PROBE=$(mktemp -d "${TMPDIR:-/tmp}/d3-gate-probe.XXXXXX")
export PATH="$PROBE/bin:$PATH"
mkdir -p "$PROBE/bin"
# Stub daemon: the addon lines a real fcitx5 writes, no X connection, stays up
# so the dump's process-state section has something live to report.
cat >"$PROBE/bin/fcitx5" <<'EOF'
#!/bin/bash
{
    echo "I 2026-11-05 00:00:00.000000 addonmanager.cpp:204] Loaded addon xim     (+0.02 s)"
    echo "I 2026-11-05 00:00:01.090000 addonmanager.cpp:204] Loaded addon hunmin  (+1.09 s)"
} >"$STAGE/fcitx5.log"
sleep 120
EOF
cat >"$PROBE/bin/fcitx5-remote" <<'EOF'
#!/bin/bash
echo "stub fcitx5-remote $*"
EOF
chmod +x "$PROBE/bin/fcitx5" "$PROBE/bin/fcitx5-remote"

PANEL=":93"   # outside the pool the tiers reserve (99..94)
Xvfb "$PANEL" -screen 0 320x240x24 -nolisten tcp >"$PROBE/xvfb.log" 2>&1 &
XVFB=$!
cleanup() {
    [ -n "${FCITX_PID:-}" ] && kill "$FCITX_PID" 2>/dev/null
    kill "$XVFB" 2>/dev/null
    wait 2>/dev/null
    rm -rf "$PROBE"
}
trap cleanup EXIT
for _ in $(seq 1 50); do
    xwininfo -display "$PANEL" -root >/dev/null 2>&1 && break
    sleep 0.1
done
xwininfo -display "$PANEL" -root >/dev/null 2>&1 ||
    { echo "SKIP: Xvfb $PANEL did not come up" >&2; exit 77; }

eval "$(sed -n '/^gate_ms()/,/^}/p' "$F")"
eval "$(sed -n '/^start_fcitx()/,/^}/p' "$F")"
for fn in gate_ms start_fcitx; do
    [ "$(type -t "$fn" 2>/dev/null)" = "function" ] ||
        { echo "FAIL: $fn could not be extracted from run-d3.sh" >&2; exit 1; }
done

export DISP="$PANEL" DISPLAY="$PANEL"
STAGE="$PROBE/stage"
export STAGE             # the stub daemon writes $STAGE/fcitx5.log
mkdir -p "$STAGE/config"   # start_fcitx writes $STAGE/config/profile
FCITX_PID=""
rc=0
out=$(start_fcitx sin3-p2 2>&1) || rc=$?
echo "$out"

fail=0
ok() { printf 'ok   %s\n' "$1"; }
no() { printf 'FAIL %s\n' "$1"; fail=1; }
[ "$rc" -eq 1 ] && ok "the gate returns 1" || no "the gate returned $rc, want 1"
grep -q '@server=fcitx never appeared' <<<"$out" &&
    ok "the message names the missing half" ||
    no "the message does not name the missing half"
grep -q 'never opened an X connection' <<<"$out" &&
    ok "the message names the missing X connection" ||
    no "the message does not name the missing X connection"
grep -q "the gate's own xprop reached" <<<"$out" &&
    ok "the gate's own reachability is counted" ||
    no "no xprop reachability count"
grep -q -- '--- root window properties' <<<"$out" &&
    ok "root-window properties dumped" || no "no xprop -root dump"
grep -q -- '--- environment the daemon was started with' <<<"$out" &&
    ok "daemon environment dumped" || no "no environment dump"
grep -q -- '--- fcitx5.log, complete' <<<"$out" &&
    ok "the complete log is dumped, not its tail" || no "no complete log"
grep -q '=== end d3 gate failure evidence ===' <<<"$out" &&
    ok "the evidence block is complete" || no "the evidence block is truncated"
# The dump must never carry an xauth cookie: a hex secret in a test log.
if grep -qE 'MIT-MAGIC-COOKIE-1 [0-9a-f]{32}' <<<"$out"; then
    no "an xauth cookie value reached the dump"
else
    ok "no cookie value in the dump"
fi

if [ "$fail" -eq 0 ]; then echo "PASS: the gate failure path leaves evidence"; fi
exit "$fail"