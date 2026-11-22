#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# FE16 option A evaluation probe (issue #319): a REAL ibus-daemon WITH the
# GTK3 panel (no -p disable, unlike run-d3.sh), a real GTK client, the case
# keys injected with XTEST, and a screenshot of what the panel actually
# drew. The D3 lanes measure delivery with the panel disabled; this probe
# is the drawing evidence the gloss evaluation needs, and the procedure a
# person can re-run to judge usability themselves.
#
# usage: tools/ibus-gloss-probe.sh BUILD_DIR [LAYOUT] [KEYS] [OUT_PNG]
#   defaults: dubeolsik 'rk{F9}' build/gloss-probe.png
#
# Everything runs on a private session bus and a reserved X display; only
# PIDs this script spawned are killed. Exit 77 = missing tooling (skip).
#
# Host findings baked in (measured 2026-11-22, issue #319):
# - the daemon's default panel/config lookup fails here; -p/-c are explicit
# - the memconf panel aborts on an empty engine list; gsettings seeds it
#   under the staged HOME (dconf), off the real user config
# - IBus runs a private dbus server: monitor the address file, not the
#   session bus
# - setxkbmap needs -model pc105: the model-less default gave an evdev-style
#   keymap and the 1.5.34 immodule re-forward landed on ']'. A stray ']' can
#   still follow the commit in the probe transcript (host immodule/keymap
#   interaction, AFTER the engine's correct commit); the D3 gate lanes are
#   clean. Judge real-desktop feel with tools/ibus-real-use.sh.
set -u
BUILD=${1:?usage: ibus-gloss-probe.sh BUILD_DIR [LAYOUT] [KEYS] [OUT_PNG]}
SRC=$(cd "$(dirname "$0")/.." && pwd)
LAYOUT=${2:-dubeolsik}
KEYS=${3:-rk{F9}}
OUT=${4:-$BUILD/gloss-probe.png}
BUILD=$(cd "$BUILD" && pwd)
OUT=$(realpath -m "$OUT")

ENGINE="$BUILD/frontends/ibus/hunmin-ibus"
COMPONENT="$BUILD/frontends/ibus/hunmin.xml"
[ -x "$ENGINE" ] || { echo "SKIP: engine not built"; exit 77; }
[ -r "$COMPONENT" ] || { echo "SKIP: component not generated"; exit 77; }
for tool in Xvfb xwininfo ibus-daemon ibus python3; do
    command -v "$tool" >/dev/null 2>&1 || { echo "SKIP: $tool missing"; exit 77; }
done
python3 -c 'import gi' 2>/dev/null || { echo "SKIP: python3 without gi"; exit 77; }
MEMCONF=$(command -v ibus-memconf 2>/dev/null || echo /usr/libexec/ibus-memconf)
[ -x "$MEMCONF" ] || { echo "SKIP: ibus-memconf missing"; exit 77; }
# The daemon's DEFAULT panel lookup failed on this host ("Can not execute
# default panel program", measured 2026-11-22); name the panel explicitly.
PANEL=$(command -v ibus-ui-gtk3 2>/dev/null || echo /usr/libexec/ibus-ui-gtk3)
[ -x "$PANEL" ] || { echo "SKIP: ibus-ui-gtk3 missing"; exit 77; }

# Reserve a display the same way run-d3.sh does: a claim file with our pid,
# and no live server on the number.
DISP=""
for d in 93 92 91 90; do
    xwininfo -root -display ":$d" >/dev/null 2>&1 && continue
    f="/tmp/hunmin-gloss-probe.$d"
    if ( set -o noclobber; printf '%s\n' "$$" >"$f" ) 2>/dev/null; then
        DISP=":$d"; CLAIM="$f"; break
    fi
    owner=$(cat "$f" 2>/dev/null)
    [ -n "$owner" ] && kill -0 "$owner" 2>/dev/null && continue
    rm -f "$f"; ( set -o noclobber; printf '%s\n' "$$" >"$f" ) 2>/dev/null && { DISP=":$d"; CLAIM="$f"; break; }
done
[ -n "$DISP" ] || { echo "SKIP: no free display"; exit 77; }

STAGE=$(mktemp -d /tmp/hunmin-gloss.XXXXXX)
XVFB_PID="" DAEMON_PID="" HARNESS_PID=""
cleanup() {
    for p in "$HARNESS_PID" "$DAEMON_PID" "$XVFB_PID"; do
        [ -n "$p" ] && kill "$p" 2>/dev/null
    done
    sleep 0.3
    for p in "$HARNESS_PID" "$DAEMON_PID" "$XVFB_PID"; do
        [ -n "$p" ] && kill -KILL "$p" 2>/dev/null
    done
    wait 2>/dev/null
    if [ -n "${GLOSS_PROBE_KEEP:-}" ]; then
        echo "stage kept: $STAGE"
    else
        rm -rf "$STAGE"
    fi
    [ -n "${CLAIM:-}" ] && [ "$(cat "$CLAIM" 2>/dev/null)" = "$$" ] && rm -f "$CLAIM"
}
trap cleanup EXIT INT TERM

Xvfb "$DISP" -screen 0 1280x1024x24 -nolisten tcp >"$STAGE/xvfb.log" 2>&1 &
XVFB_PID=$!
for _ in $(seq 1 50); do
    xwininfo -root -display "$DISP" >/dev/null 2>&1 && break
    sleep 0.1
done

mkdir -p "$STAGE/component" "$STAGE/home/.config"
sed "s|<exec>.*</exec>|<exec>$ENGINE --ibus</exec>|" "$COMPONENT" >"$STAGE/component/hunmin.xml"

export DISPLAY="$DISP"
export HOME="$STAGE/home" XDG_CONFIG_HOME="$STAGE/home/.config"
export XDG_RUNTIME_DIR="$STAGE"; chmod 700 "$STAGE"
export IBUS_COMPONENT_PATH="$STAGE/component"
export HUNMIN_DATA_PATH="$SRC/libhunmin/data"
export HUNMIN_IBUS_LAYOUT="$LAYOUT"
export GTK_IM_MODULE=ibus GDK_BACKEND=x11
export LANG=C.UTF-8 LC_ALL=C.UTF-8
unset WAYLAND_DISPLAY XDG_SESSION_TYPE IBUS_ADDRESS 2>/dev/null || true

# The panel is ENABLED: no -p disable. That is the whole point of the probe.
dbus-run-session -- bash -c '
    set -u
    # Seed the panel engine list: with an empty list (memconf default) the
    # GTK3 panel ABORTS in panel_switch_engine (assertion i >= 0, measured).
    # dconf under the staged HOME keeps this off the real user config.
    gsettings set org.freedesktop.ibus.general engines-order "[\"hunmin\"]" 2>/dev/null || true
    gsettings set org.freedesktop.ibus.general preload-engines "[\"hunmin\"]" 2>/dev/null || true
    ibus-daemon -x -c /usr/libexec/ibus-dconf -p disable >"$1/daemon.log" 2>&1 &
    echo $! >"$1/daemon.pid"
    for _ in $(seq 1 100); do
        ibus list-engine 2>/dev/null | grep -q Hunmin && break
        sleep 0.1
    done
    ibus list-engine 2>/dev/null | grep -q Hunmin || { echo "ERROR: engine not registered"; echo "--- daemon.log ---"; tail -30 "$1/daemon.log"; exit 1; }
    ibus engine hunmin || { echo "ERROR: engine not selected"; exit 1; }
    # 1.5.26 runs setxkbmap itself; 1.5.34 does not, and the bare-Xvfb default
    # keymap mapped F9 to the ] keycode (measured: TEXT:가]). Load us explicitly.
    setxkbmap -model pc105 -layout us 2>/dev/null || true
    # Run the panel ourselves so its stderr is visible (the daemon-spawned
    # panel left no log and no process -- measured).
    "$8" >"$1/panel.log" 2>&1 &
    echo $! >"$1/panel.pid"
    sleep 1.0
    python3 "$2" >"$1/harness.log" 2>&1 &
    echo $! >"$1/harness.pid"
    for _ in $(seq 1 50); do grep -q READY "$1/harness.log" 2>/dev/null && break; sleep 0.1; done
    grep -q READY "$1/harness.log" || { echo "ERROR: harness not ready"; exit 1; }
    # the engine spawns lazily on the first input context
    DPID=$(cat "$1/daemon.pid")
    for _ in $(seq 1 100); do
        ps -o args= --ppid "$DPID" 2>/dev/null | grep -Fq "$3" && break
        sleep 0.1
    done
    sleep 0.5
    # Capture the real-bus engine signals: the decisive evidence that the
    # auxiliary text (the gloss) is published on the live stack, not only in
    # the D2 fake-bus lane.
    BUSFILE=$(ls "$XDG_CONFIG_HOME/ibus/bus/"* 2>/dev/null | head -1)
    IBUS_ADDR=$(grep -o "unix:[^,]*" "$BUSFILE" 2>/dev/null | head -1)
    if [ -n "$IBUS_ADDR" ]; then
        dbus-monitor --address "$IBUS_ADDR" >"$1/signals.log" 2>&1 &
    else
        dbus-monitor --session >"$1/signals.log" 2>&1 &
    fi
    MPID=$!
    python3 "$4" "$DISPLAY" "$5" || { echo "ERROR: injection failed"; exit 1; }
    sleep "${9:-2.0}"   # F9 opens asynchronously; the panel draws on the event loop
    python3 "$6" "$7"
    echo "--- panel process ---"
    ps -eo pid,args | grep -E "ibus-ui-gtk3" | grep -v grep || echo "NO PANEL PROCESS"
    echo "--- windows on $DISPLAY ---"
    xwininfo -root -children -display "$DISPLAY" 2>/dev/null | grep -E "ibus|panel|0x" | head -12
    echo "--- engine signals (lookup/aux) ---"
    grep -E "member=(UpdateLookupTable|UpdateAuxiliaryText|ShowLookupTable|HideLookupTable|CommitText)" "$1/signals.log" | head -12
    echo "--- keymap (F9 / bracketright) ---"
    xmodmap -pke 2>/dev/null | grep -E "= F9|bracketright" | head -3
    echo "--- panel.log ---"
    tail -5 "$1/panel.log"
    echo "--- client transcript ---"
    grep -E "^(TEXT|MODULE|READY):" "$1/harness.log" | head -8
    kill $MPID 2>/dev/null
    kill $(cat "$1/panel.pid") 2>/dev/null
    kill $(cat "$1/harness.pid") 2>/dev/null
    kill $(cat "$1/daemon.pid") 2>/dev/null
    wait 2>/dev/null
' "$MEMCONF" "$STAGE" "$SRC/tests/e2e/gtk-harness.py" "$ENGINE" \
  "$SRC/tests/e2e/xtest-type.py" "$KEYS" "$SRC/tools/xvfb-screenshot.py" "$OUT" "$PANEL" "${GLOSS_PROBE_SETTLE:-2.0}"
rc=$?
echo "probe rc=$rc; screenshot: $OUT"
exit "$rc"
