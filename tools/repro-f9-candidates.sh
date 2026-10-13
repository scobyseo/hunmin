#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Repro: F9 on a syllable with many Hanja/word candidates, through the real
# fcitx5 stack (X server -> xim -> hunmin addon -> libhunmin -> client) with
# fcitx5's own classicui loaded, so the candidate window is actually drawn.
#
#   tools/repro-f9-candidates.sh [keys] [after]
#
#   keys    sin3-p2 keys for the syllable (default kfs = 간, 67 candidates)
#   after   comma-separated keys sent AFTER F9, to show the window still works:
#           Down,Return / Page_Down,Return / Escape
#
# Exit 0 = fcitx5 is still running (no abort). Exit 1 = fcitx5 died; the log
# tail and the stage directory are printed. Exit 77 = the stack could not be
# brought up here, which is a skip and says nothing either way.
#
# WHY THIS EXISTS AND WHY IT IS NOT A CTEST. Two tiers already cover the
# hunmin half of this path and neither can reach the crash:
#
#   * engine-d2-ui drives HunminState through fcitx5's testfrontend and asserts
#     the InputPanel, and it DOES open the F9 window on 간 (67 candidates, see
#     frontends/fcitx5/test/testhunminui.cpp). testfrontend has no UI addon, so
#     nothing ever reads candidateList->label().
#   * engine-d3-* runs the whole stack but tests/e2e/run-d3.sh lists classicui
#     under DisabledAddons, so the candidate window is never drawn there
#     either.
#
# The abort happens in the code between those two: fcitx5's classicui walks
# every candidate on the page and calls label() on each one. So the tier that
# would catch it is "D2's engine, D3's stack, and classicui enabled", and no
# existing tier is that. Run this after changing anything about the F9
# candidate list.
#
# Measured threshold, not remembered: a page of 10 or fewer survives, 11 or
# more aborts fcitx5. See
# wiki/raw/measurements/2026-10-13-f9-candidate-window-abort.md.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${HUNMIN_BUILD_DIR:-$ROOT/build}"
KEYS="${1:-kfs}"
AFTER="${2:-}"

for tool in Xvfb xwininfo xprop fcitx5 fcitx5-remote dbus-run-session python3; do
    command -v "$tool" >/dev/null 2>&1 || { echo "skip: $tool not available" >&2; exit 77; }
done
[ -x "$BUILD/xim-harness" ] || { echo "skip: $BUILD/xim-harness not built" >&2; exit 77; }
[ -x "$BUILD/fcitx5-addon/libhunmin.so" ] || {
    echo "skip: $BUILD/fcitx5-addon/libhunmin.so not built" >&2; exit 77; }
[ -r "$BUILD/frontends/fcitx5/test/inputmethod/hunmin.conf" ] || {
    echo "skip: staged hunmin.conf not generated" >&2; exit 77; }
[ -r "$BUILD/frontends/fcitx5/test/addon/hunmin.conf" ] || {
    echo "skip: staged hunmin.conf not generated" >&2; exit 77; }

# A private bus, and the freedesktop portal shadowed on it. classicui queries
# org.freedesktop.portal.Desktop on startup; activating the real one spawns
# xdg-desktop-portal* on this bus, those daemons inherit our stdout, and ctest
# would then block forever waiting for EOF. The shadow has to be built BEFORE
# dbus-run-session, because dbus-daemon reads XDG_DATA_DIRS at startup.
if [ -z "${HUNMIN_F9_BUS:-}" ]; then
    SHADOW=$(mktemp -d /tmp/hunmin-f9-bus.XXXXXX)
    mkdir -p "$SHADOW/share/dbus-1/services" "$SHADOW/share/dbus-1/system-services"
    for svc in org.freedesktop.portal.Desktop org.freedesktop.portal.Documents \
               org.freedesktop.impl.portal.PermissionStore \
               org.freedesktop.impl.portal.desktop.gtk \
               org.freedesktop.impl.portal.desktop.kde \
               org.freedesktop.impl.portal.desktop.kwallet \
               org.kde.kwalletd5 org.kde.kwalletd6; do
        printf '[D-BUS Service]\nName=%s\nExec=/bin/true\n' "$svc" \
            >"$SHADOW/share/dbus-1/services/$svc.service"
    done
    export XDG_DATA_DIRS="$SHADOW/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
    exec dbus-run-session -- env HUNMIN_F9_BUS=1 "$0" "$@"
fi

STAGE=$(mktemp -d /tmp/hunmin-f9.XXXXXX)
PIDS=""
cleanup() {
    for p in $PIDS; do kill -KILL "$p" >/dev/null 2>&1 || true; done
    echo "stage kept at: $STAGE"
}
trap cleanup EXIT INT TERM

# Display allocation is a reservation, not a probe -- mkdir is atomic, so two
# concurrent runs cannot both pick :97 and type into each other. Same
# reservation trick as tests/e2e/run-d3.sh.
DISP=":97"
rmdir /tmp/hunmin-d3-disp.97 >/dev/null 2>&1 || true
mkdir -p /tmp/hunmin-d3-disp.97 || { echo "skip: display :97 busy" >&2; exit 77; }
trap 'rmdir /tmp/hunmin-d3-disp.97 >/dev/null 2>&1 || true; cleanup' EXIT INT TERM

Xvfb "$DISP" -screen 0 1280x1024x24 -nolisten tcp >"$STAGE/xvfb.log" 2>&1 &
PIDS="$PIDS $!"
for _ in $(seq 1 50); do
    xwininfo -root -display "$DISP" >/dev/null 2>&1 && break
    sleep 0.1
done

mkdir -p "$STAGE/config" "$STAGE/data/fcitx5/conf" "$STAGE/pkgdata/addon" \
         "$STAGE/pkgdata/inputmethod" "$STAGE/cache" "$STAGE/runtime" \
         "$STAGE/home/.config" "$STAGE/home/.local/share"
chmod 700 "$STAGE/runtime"

cp "$BUILD/frontends/fcitx5/test/inputmethod/hunmin.conf" \
   "$STAGE/pkgdata/inputmethod/hunmin.conf"
cp "$BUILD/frontends/fcitx5/test/addon/hunmin.conf" \
   "$STAGE/pkgdata/addon/hunmin.conf"
if [ -n "${HUNMIN_F9_SYSADDON:-}" ]; then
    # The "is it only my build?" control: run the INSTALLED addon against the
    # INSTALLED data instead of the freshly built ones. Everything else about
    # the stack is identical, which is the point.
    rm -f "$STAGE/pkgdata/addon/hunmin.conf"
    export FCITX_ADDON_DIRS="/usr/lib/x86_64-linux-gnu/fcitx5"
    export HUNMIN_DATA_PATH=/usr/share/hunmin
else
    cp "$BUILD/fcitx5-addon/libhunmin.so" "$STAGE/pkgdata/addon/libhunmin.so"
    export FCITX_ADDON_DIRS="$STAGE/pkgdata/addon:/usr/lib/x86_64-linux-gnu/fcitx5"
    export HUNMIN_DATA_PATH="$ROOT/libhunmin/data"
fi

# keyboard-us first and hunmin second is load-bearing: measured 2026-10-03, with
# hunmin at Items/0 fcitx5 activates the FIRST listed item and DefaultIM is
# ignored, so every key reaches the client raw. Do not "tidy" this ordering.
cat >"$STAGE/config/profile" <<'EOF'
[Groups/0]
Name=Default
Default Layout=us
DefaultIM=hunmin

[Groups/0/Items/0]
Name=keyboard-us
Layout=

[Groups/0/Items/1]
Name=hunmin
Layout=sin3-p2

[GroupOrder]
0=Default
EOF

# classicui is deliberately NOT in DisabledAddons. Everything else here is
# copied from tests/e2e/run-d3.sh; dropping classicui from that list is the
# single difference from the tier D3 environment.
cat >"$STAGE/config/config" <<'EOF'
[Behavior]
ActiveByDefault=True
ResetStateWhenFocusIn=All
DisabledAddons=kimpanel
DisabledAddons=virtualkeyboard
DisabledAddons=notificationitem
DisabledAddons=notifications
EOF

export DISPLAY="$DISP"
unset WAYLAND_DISPLAY XDG_SESSION_TYPE XDG_CURRENT_DESKTOP XDG_SESSION_DESKTOP \
      DESKTOP_SESSION
export LANG=C.UTF-8 LC_ALL=C.UTF-8
export XMODIFIERS=@im=fcitx
export HOME="$STAGE/home" XDG_CONFIG_HOME="$STAGE/home/.config" \
       XDG_DATA_HOME="$STAGE/home/.local/share" XDG_CACHE_HOME="$STAGE/cache" \
       XDG_RUNTIME_DIR="$STAGE/runtime"
export FCITX_CONFIG_HOME="$STAGE/config" FCITX_DATA_HOME="$STAGE/data"
export FCITX_DATA_DIRS="$STAGE/pkgdata:/usr/share/fcitx5"
export LD_LIBRARY_PATH="$BUILD:${LD_LIBRARY_PATH:-}"

# -d is daemonise, not debug. The log rule is --verbose. FCITX_LOG_RULE is not
# read by fcitx5 at all -- there is no such getenv anywhere in 5.1.19 -- which
# is why tests/e2e/run-d3.sh's FCITX_LOG_RULE export has no effect and its
# logs come out at default=5.
LOG_RULE="${FCITX_LOG_RULE:-default=5,xim_key=5}"
fcitx5 -D --verbose "$LOG_RULE" >"$STAGE/fcitx5.log" 2>&1 &
FCITX_PID=$!
PIDS="$PIDS $FCITX_PID"

up=0
for _ in $(seq 1 200); do
    if xprop -root -display "$DISP" 2>/dev/null | grep -q '@server=fcitx' &&
       grep -q 'Loaded addon hunmin' "$STAGE/fcitx5.log" 2>/dev/null; then
        up=1; break
    fi
    kill -0 "$FCITX_PID" 2>/dev/null || break
    sleep 0.1
done
[ "$up" = 1 ] || { echo "fcitx5 did not come up" >&2; tail -25 "$STAGE/fcitx5.log" >&2; exit 77; }
grep -E 'Loaded addon (classicui|hunmin)' "$STAGE/fcitx5.log" || true

fcitx5-remote -o >/dev/null 2>&1 || true
sleep 1.0

# The harness must come AFTER fcitx5: xcb_xim_open() fails against a display
# with no XIM server on it yet, and a failing harness says nothing about hunmin.
"$BUILD/xim-harness" </dev/null >"$STAGE/commits.log" 2>"$STAGE/harness.log" &
HARNESS_PID=$!
PIDS="$PIDS $HARNESS_PID"
for _ in $(seq 1 80); do
    grep -q READY "$STAGE/commits.log" 2>/dev/null && break
    sleep 0.1
done
grep -q READY "$STAGE/commits.log" 2>/dev/null || {
    echo "skip: XIM harness never became ready" >&2; exit 77; }

# XTEST injection. F9 is XK_F9 = 0xFFC6. 0xFFC2 is XK_F5 and is the obvious
# mistake here: an earlier run of this script sent 0xFFC2, the addon correctly
# declined it as an unmapped keysym and flushed the composition, and the
# transcript looked like "F9 committed the syllable". The xim_key log line
# ("XIM Key Event: 2 F5") is what settled it.
python3 - "$DISP" "$KEYS" "$AFTER" <<'PY'
import ctypes, ctypes.util, subprocess, sys, time

display, keys, after = sys.argv[1], sys.argv[2], sys.argv[3]
XK_F9 = 0xFFC6
NAMED = {"Return": 0xFF0D, "Escape": 0xFF1B, "Down": 0xFF54, "Up": 0xFF52,
         "Page_Down": 0xFF56, "Page_Up": 0xFF55, "space": 0x20}

x11 = ctypes.CDLL(ctypes.util.find_library("X11"))
xtst = ctypes.CDLL(ctypes.util.find_library("Xtst"))
x11.XOpenDisplay.restype = ctypes.c_void_p
x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
x11.XKeysymToKeycode.restype = ctypes.c_ubyte
xtst.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
                                   ctypes.c_ulong]

dpy = x11.XOpenDisplay(display.encode())


def tap(sym):
    code = x11.XKeysymToKeycode(dpy, sym)
    if not code:
        print("no keycode for keysym 0x%x" % sym, file=sys.stderr)
        sys.exit(2)
    xtst.XTestFakeKeyEvent(dpy, code, 1, 0)
    x11.XFlush(dpy)
    time.sleep(0.06)
    xtst.XTestFakeKeyEvent(dpy, code, 0, 0)
    x11.XFlush(dpy)
    time.sleep(0.06)


for ch in keys:
    tap(ord(ch))
time.sleep(0.5)
tap(XK_F9)
time.sleep(1.5)

for token in (t for t in after.split(",") if t):
    tap(NAMED[token] if token in NAMED else ord(token))
    time.sleep(0.8)

# The window geometry is the oracle that the window actually opened: a page of
# N candidates in one vertical column is much taller than the one-character
# preedit, so "27x34" means no window and "336x334" means the window is up.
out = subprocess.run(["xwininfo", "-root", "-children", "-display", display],
                     capture_output=True, text=True).stdout
found = False
for line in out.splitlines():
    if "Fcitx5 Input Window" in line:
        print("input window after F9: %s" % line.strip())
        found = True
if not found:
    # fcitx5 has already closed its X connection, so the input window is gone.
    print("input window after F9: gone (fcitx5 tore down its X connection)")
PY

sleep 1
# What the client ended up holding. This is the oracle for "the window still
# works": with the page capped, candidates past the first page are reached with
# Page_Down, so `after=Page_Down,Return` should commit a candidate rather than
# flushing the source syllable.
echo "--- client transcript ---"
grep -E '^(COMMIT|FORWARD):' "$STAGE/commits.log" 2>/dev/null || true
if kill -0 "$FCITX_PID" 2>/dev/null; then
    echo "RESULT: fcitx5 still running after F9"
    exit 0
fi
set +e
wait "$FCITX_PID"
rc=$?
set -e
echo "RESULT: fcitx5 exited rc=$rc"
echo "--- fcitx5.log tail ---"
tail -30 "$STAGE/fcitx5.log"
exit 1