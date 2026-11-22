#!/bin/bash
# Install the built hunmin IBus packages and open a disposable real-app session
# for actual-use evaluation (issue #292).
#
# WHY A SEPARATE SCRIPT
# ---------------------
# CI's engine-d3-ibus drives a GTK harness with XTEST. That answers "does the
# engine deliver text", not "is it pleasant to use". This script installs the
# REAL .debs and opens gnome-text-editor in a nested X display with ibus-daemon
# and the hunmin engine, so a person types on their own keyboard into a real
# application and judges preedit, commit timing, the Hanja list and key routing.
#
# It never touches the running session bus, fcitx5, or the desktop session:
# the bus, daemon, Xephyr and editor are all private children of this script,
# killed by PID on exit. No pgrep/pkill of shared daemons.
#
# usage:
#   tools/ibus-real-use.sh portable  # extract the built .debs under the repo and
#                                    # open the real-use window (NO sudo, NO system
#                                    # install) -- the fastest way to judge the IME
#   tools/ibus-real-use.sh install   # dpkg -i the four .debs (needs sudo)
#   tools/ibus-real-use.sh session   # open the real-use window (packages installed)
#   tools/ibus-real-use.sh all       # install + session
#
# The checklists to drive are in tests/manual-checklists/ibus-real-use.md.
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
PKGDIR=$(dirname "$ROOT")   # dpkg-buildpackage writes one level above the source
# The engine reads HUNMIN_IBUS_LAYOUT at init (a D3 test seam in the source;
# there is no config-store ruling yet). Default to the layout the maintainer
# actually uses, but allow any installed id, e.g. sin3-p2-basic / sin3-p2-yet.
LAYOUT=${HUNMIN_REALUSE_LAYOUT:-sin3-p2}

say() { printf '%s\n' "$*"; }
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

do_install() {
    local -a debs=()
    local f
    # Glob WITH the path prefix: the .debs live one level above this worktree,
    # not in the current directory.
    for f in "$PKGDIR"/libhunmin_*_*.deb "$PKGDIR"/hunmin-data_*.deb \
             "$PKGDIR"/hunmin-ibus_*.deb "$PKGDIR"/hunmin-en-us_*.deb; do
        [ -f "$f" ] || die "missing $f -- run tools/build-release.sh deb first"
        debs+=("$f")
    done
    say "=== installing ${#debs[@]} packages from $PKGDIR ==="
    printf '  %s\n' "${debs[@]}"
    # Not -n: on this host sudo needs a TTY and the operator's password.
    sudo dpkg -i "${debs[@]}"
    say "=== installed state ==="
    dpkg -l hunmin-ibus libhunmin hunmin-data 2>/dev/null | grep '^ii' || true
    [ -r /usr/share/ibus/component/hunmin.xml ] ||
        die "component XML missing after install"
    say "install ok: /usr/share/ibus/component/hunmin.xml present"
}

# Extract the built packages into a private prefix. This tests the EXACT
# artefacts from dpkg-buildpackage without apt/dpkg state or sudo. The
# component XML's <exec> is rewritten from /usr/bin to the extracted binary,
# the same out-of-tree route run-d3.sh uses via IBUS_COMPONENT_PATH.
do_portable() {
    local prefix="$ROOT/.ibus-realuse-prefix" f
    local -a debs=()
    for f in "$PKGDIR"/libhunmin_*_*.deb "$PKGDIR"/hunmin-data_*.deb \
             "$PKGDIR"/hunmin-ibus_*.deb "$PKGDIR"/hunmin-en-us_*.deb; do
        [ -f "$f" ] || die "missing $f -- run tools/build-release.sh deb first"
        debs+=("$f")
    done
    rm -rf "$prefix"
    mkdir -p "$prefix"
    for f in "${debs[@]}"; do
        dpkg-deb -x "$f" "$prefix"
    done
    ENGINE_BIN="$prefix/usr/bin/hunmin-ibus"
    COMPONENT_DIR="$prefix/usr/share/ibus/component"
    [ -x "$ENGINE_BIN" ] || die "portable engine missing at $ENGINE_BIN"
    [ -r "$COMPONENT_DIR/hunmin.xml" ] || die "portable component XML missing"
    sed -i "s|<exec>.*</exec>|<exec>$ENGINE_BIN --ibus</exec>|" \
        "$COMPONENT_DIR/hunmin.xml"
    # If a trace-enabled engine was built (cmake -B build-trace -DHUNMIN_TRACE=ON),
    # use it: every key then writes a personal-key line with the resolved layout
    # in meta, which is the evidence that hunmin actually handled the key.
    TRACE_LOG=""
    if [ -x "$ROOT/build-trace/frontends/ibus/hunmin-ibus" ]; then
        cp "$ROOT/build-trace/frontends/ibus/hunmin-ibus" "$ENGINE_BIN"
        TRACE_LOG="$prefix/key-log.jsonl"
        : >"$TRACE_LOG"
    fi
    cat >"$prefix/env.sh" <<EOF
export IBUS_COMPONENT_PATH="$COMPONENT_DIR"
export LD_LIBRARY_PATH="$prefix/usr/lib/x86_64-linux-gnu\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
export HUNMIN_DATA_PATH="$prefix/usr/share/hunmin"
export HUNMIN_IBUS_LAYOUT="$LAYOUT"
# Evaluation-only: turn on the D1-proven autoswitch path and ship its word list
# in this prefix. No config-store decision is implied (that ruling is open).
export HUNMIN_IBUS_AUTOSWITCH=1
# Add the raw IBus keyval to each trace line: base is 0 for every unmapped key,
# so this is the only way to tell a Hangul press (0xff31) from Right Alt
# (0xffea), which is what the physical Korean key maps to under XKB "us".
export HUNMIN_TRACE_RAW=1
EOF
    if [ -n "$TRACE_LOG" ]; then
        printf 'export HUNMIN_TRACE_OUT=%q\n' "$TRACE_LOG" >>"$prefix/env.sh"
    fi
    # shellcheck disable=SC1090
    source "$prefix/env.sh"
    say "portable prefix: $prefix (no system install, no sudo)"
    say "layout: $LAYOUT (override: HUNMIN_REALUSE_LAYOUT=<id> $0 ...)"
    if [ -n "$TRACE_LOG" ]; then
        say "key log: $TRACE_LOG (live feed below proves hunmin handled each key)"
    else
        say "key log: OFF -- build with 'cmake -B build-trace -DHUNMIN_TRACE=ON' for per-key evidence"
    fi
}

do_session() {
    for t in Xvfb Xephyr xwininfo ibus-daemon ibus gnome-text-editor; do
        command -v "$t" >/dev/null 2>&1 || die "missing $t"
    done
    ENGINE_BIN=${ENGINE_BIN:-/usr/bin/hunmin-ibus}
    COMPONENT_DIR=${COMPONENT_DIR:-/usr/share/ibus/component}
    [ -x "$ENGINE_BIN" ] || die "engine missing at $ENGINE_BIN -- run: $0 portable | install"
    [ -r "$COMPONENT_DIR/hunmin.xml" ] || die "component XML missing at $COMPONENT_DIR"

    local stage disp n
    stage=$(mktemp -d /tmp/hunmin-realuse.XXXXXX)
    for n in 90 91 92 93; do
        [ -e "/tmp/.X$n-lock" ] || { disp=":$n"; break; }
    done
    [ -n "${disp:-}" ] || die "no free nested display (90-93)"
    say "nested display: $disp on $DISPLAY"

    # Global (not local): the EXIT trap runs after this function returns, so
    # locals would be unbound there and the cleanup itself would fail.
    IBUS_PID=""; XEPHYR_PID=""; BUS_PID=""; EDITOR_PID=""
    cleanup() {
        [ -n "$EDITOR_PID" ] && kill "$EDITOR_PID" 2>/dev/null
        [ -n "$IBUS_PID" ] && kill "$IBUS_PID" 2>/dev/null
        [ -n "$XEPHYR_PID" ] && kill "$XEPHYR_PID" 2>/dev/null
        [ -n "$BUS_PID" ] && kill "$BUS_PID" 2>/dev/null
        for _ in $(seq 1 20); do
            local alive=0 p
            for p in "$EDITOR_PID" "$IBUS_PID" "$XEPHYR_PID" "$BUS_PID"; do
                [ -n "$p" ] && kill -0 "$p" 2>/dev/null && alive=1
            done
            [ "$alive" -eq 0 ] && break
            sleep 0.1
        done
        for p in "$EDITOR_PID" "$IBUS_PID" "$XEPHYR_PID" "$BUS_PID"; do
            [ -n "$p" ] && kill -KILL "$p" 2>/dev/null
        done
        # Never rm -rf an operator directory by accident: only our mktemp path.
        [ -n "$stage" ] && rm -rf "$stage"
    }
    trap cleanup EXIT INT TERM

    # A PRIVATE session bus. The desktop's bus is never touched; --fork gives
    # us the exact pid to kill, so no pgrep of a shared dbus-daemon.
    dbus-daemon --session --fork --print-address=1 --print-pid=1 \
        >"$stage/bus" 2>/dev/null
    BUS_PID=$(sed -n 2p "$stage/bus" | tr -d ' ')
    export DBUS_SESSION_BUS_ADDRESS=$(sed -n 1p "$stage/bus")

    Xephyr "$disp" -screen 1280x800x24 -ac -nolisten tcp \
        >"$stage/xephyr.log" 2>&1 &
    XEPHYR_PID=$!
    for _ in $(seq 1 60); do
        xwininfo -root -display "$disp" >/dev/null 2>&1 && break
        sleep 0.1
    done
    xwininfo -root -display "$disp" >/dev/null 2>&1 ||
        { cat "$stage/xephyr.log"; die "Xephyr did not come up"; }

    # Inside the nested display. GDK must be X11: the Wayland compositor is
    # left outside on purpose, because IBus on this host has no ibus-wayland.
    export DISPLAY="$disp" GDK_BACKEND=x11 GTK_IM_MODULE=ibus HUNMIN_IBUS_LAYOUT="$LAYOUT"
    unset WAYLAND_DISPLAY XDG_SESSION_TYPE XDG_CURRENT_DESKTOP XDG_SESSION_DESKTOP DESKTOP_SESSION
    export XDG_CONFIG_HOME="$stage/config" XDG_DATA_HOME="$stage/data" XDG_CACHE_HOME="$stage/cache"
    mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME"

    # Panel ENABLED on purpose: the engine publishes a lookup table, but with
    # -p disable nothing draws it and Hanja looks "unimplemented". The default
    # panel is not on PATH on this host ("Can not execute default panel
    # program"), so name the packaged binary explicitly.
    local panel
    panel=$(command -v ibus-ui-gtk3 2>/dev/null || echo /usr/libexec/ibus-ui-gtk3)
    [ -x "$panel" ] || die "no IBus panel binary at $panel"
    ibus-daemon -c /usr/libexec/ibus-memconf -p "$panel" \
        >"$stage/ibus.log" 2>&1 &
    IBUS_PID=$!
    for _ in $(seq 1 80); do
        ibus list-engine 2>/dev/null | grep -qi hunmin && break
        sleep 0.1
    done
    ibus list-engine 2>/dev/null | grep -qi hunmin ||
        { tail -30 "$stage/ibus.log"; die "the installed hunmin engine did not register"; }
    ibus engine hunmin >/dev/null 2>&1 || die "could not select hunmin"
    say "IBus engine: $(ibus engine 2>/dev/null)  |  layout: $LAYOUT"

    gnome-text-editor >"$stage/editor.log" 2>&1 &
    EDITOR_PID=$!
    say ""
    say "=== a real editor is open in the nested window ==="
    say "  click into the editor, then type on YOUR keyboard:"
    say "    dkssuds<space>   -> 안녕ㄴ (Hangul composition + declined space)"
    say "    kfs<F9>          -> Hanja lookup table for 간"
    say "    <Hangul key>     -> switch Hangul/Latin (FE13 on this frontend)"
    say "  See tests/manual-checklists/ibus-real-use.md for the full checklist."
    say "  Ctrl+C here closes the window and removes the private bus/daemon."
    wait "$EDITOR_PID"
}

# Like do_session but WITHOUT Xephyr: the app opens as a normal window on the
# operator's current display, so they type in their own desktop. It still uses
# a private bus/daemon and the portable prefix, so nothing in the live session
# (fcitx5, session bus, desktop settings) is changed.
do_desktop() {
    for t in xwininfo ibus-daemon ibus; do
        command -v "$t" >/dev/null 2>&1 || die "missing $t"
    done
    local app=${2:-gnome-text-editor}
    command -v "$app" >/dev/null 2>&1 || die "missing $app"
    local host_display=${DISPLAY:-:0}

    local stage
    stage=$(mktemp -d /tmp/hunmin-desktop.XXXXXX)
    IBUS_PID=""; BUS_PID=""; APP_PID=""; TAIL_PID=""   # globals: EXIT trap runs after return
    cleanup() {
        [ -n "$TAIL_PID" ] && kill "$TAIL_PID" 2>/dev/null
        [ -n "$APP_PID" ] && kill "$APP_PID" 2>/dev/null
        [ -n "$IBUS_PID" ] && kill "$IBUS_PID" 2>/dev/null
        [ -n "$BUS_PID" ] && kill "$BUS_PID" 2>/dev/null
        sleep 0.3
        for p in "$TAIL_PID" "$APP_PID" "$IBUS_PID" "$BUS_PID"; do
            [ -n "$p" ] && kill -KILL "$p" 2>/dev/null
        done
        [ -n "$stage" ] && rm -rf "$stage"
    }
    trap cleanup EXIT INT TERM

    dbus-daemon --session --fork --print-address=1 --print-pid=1 \
        >"$stage/bus" 2>/dev/null
    BUS_PID=$(sed -n 2p "$stage/bus" | tr -d ' ')
    export DBUS_SESSION_BUS_ADDRESS=$(sed -n 1p "$stage/bus")

    # X11 backend: on Wayland a GTK app would use the compositor's IM protocol
    # (fcitx5 here) and never see this IBus daemon.
    export DISPLAY="$host_display" GDK_BACKEND=x11 GTK_IM_MODULE=ibus HUNMIN_IBUS_LAYOUT="$LAYOUT"
    export XDG_CONFIG_HOME="$stage/config" XDG_DATA_HOME="$stage/data" XDG_CACHE_HOME="$stage/cache"
    mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME"

    local panel
    panel=$(command -v ibus-ui-gtk3 2>/dev/null || echo /usr/libexec/ibus-ui-gtk3)
    [ -x "$panel" ] || die "no IBus panel binary at $panel"
    ibus-daemon -c /usr/libexec/ibus-memconf -p "$panel" >"$stage/ibus.log" 2>&1 &
    IBUS_PID=$!
    for _ in $(seq 1 80); do
        ibus list-engine 2>/dev/null | grep -qi hunmin && break
        sleep 0.1
    done
    ibus list-engine 2>/dev/null | grep -qi hunmin ||
        { tail -30 "$stage/ibus.log"; die "hunmin did not register"; }
    ibus engine hunmin >/dev/null 2>&1 || die "could not select hunmin"

    "$app" >"$stage/app.log" 2>&1 &
    APP_PID=$!
    say "=== $app is open on your desktop: IBus engine $(ibus engine 2>/dev/null), layout $LAYOUT ==="
    if [ -n "${TRACE_LOG:-}" ]; then
        say "  live key log (hunmin's own shared events -- proof it handled the key):"
        ( tail -n +1 -f "$TRACE_LOG" 2>/dev/null | python3 -u -c '
import json, subprocess, sys
for line in sys.stdin:
    sys.stdout.write("    " + line)
    sys.stdout.flush()
    if not line.startswith("{"):
        continue
    try:
        o = json.loads(line)
    except ValueError:
        continue
    ev = o.get("in", {})
    # The language key is the only consumed event with base=0 and id=0;
    # releases are consumed:false and Enter/F9 carry a key id.
    if ev.get("base") == 0 and ev.get("id") == 0 and o.get("consumed"):
        subprocess.run(["notify-send", "hunmin", "한/영 전환"],
                       check=False, capture_output=True)
' ) &
        TAIL_PID=$!
    fi
    say "  Click into the window and type with your own keyboard."
    say "  한/영: both Hangul (0xff31) and the physical key's Alt_R (0xffea"
    say "    under XKB 'us') are language keys. A press logs base=0,"
    say "    raw=65514, consumed=true and toggles the language."
    say "  Ctrl+C here closes the app and the private daemon."
    wait "$APP_PID"
}

case "${1:-portable}" in
    portable) do_portable; do_session ;;
    desktop)  do_portable; do_desktop "$@" ;;
    install) do_install ;;
    session) do_session ;;
    all) do_install; do_session ;;
    *) printf 'usage: %s [portable|desktop [APP]|install|session|all]\n' "$0" >&2; exit 2 ;;
esac
