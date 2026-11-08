#!/bin/bash
# ime-host-probe.sh -- can this host deliver a key through a given input-method
# host mechanism to a real application, and does the repository's own client
# harness see the committed text?
#
# WHY THIS EXISTS
# ---------------
# docs/OPTIONS-MEASURED.md §PLT prices four ways of putting a second Linux
# frontend into this tree. Three of them are claims about what a host can do, and
# a claim about a host is worth nothing until something is driven through it. This
# script drives the cheapest one end to end -- an out-of-tree IBus engine -- and
# prints the evidence lines the pricing section quotes:
#
#   engine listed | engine spawned by the daemon | commit reached the client
#
# The engine it builds is a 40-line probe that commits a fixed string on every
# key. It is NOT a frontend, it does not read layout data and it is not part of
# the tree's build; it exists so that "the text arrived" discriminates
# engine-reached from engine-not-reached (an unhandled key would type 'r').
#
# WHAT IT MEASURES (and what it does not)
# ---------------------------------------
# Measured: an engine outside /usr/share/ibus/component registers, the daemon
# spawns it, `process_key_event` runs, and its `commit_text` lands in a GtkEntry
# buffer read through tests/e2e/gtk-harness.py -- the repository's own D3 client.
#
# NOT measured: preedit, candidate/lookup-table drawing, the Wayland half,
# multiple engines, or anything about fcitx5. Those are named as unmeasured in
# §PLT-6 rather than implied by a green run here.
#
# USAGE
#   tools/ime-host-probe.sh                 # needs ibus-1.0 headers via pkg-config
#   HUNMIN_IBUS_PREFIX=~/toolchains/ibus/usr tools/ime-host-probe.sh
#   KEEP=1 tools/ime-host-probe.sh          # keep the scratch dir for inspection
#
# Requirements: Xvfb, dbus-daemon, xdotool, python3 with GObject Introspection and
# GTK 3, ibus, and the ibus-1.0 development headers. Exit codes: 0 measured and
# passed, 1 measured and failed, 77 a prerequisite is absent (skip).
set -u

REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
HARNESS=$REPO/tests/e2e/gtk-harness.py
SCRATCH=${HUNMIN_SCRATCH:-$(mktemp -d "${TMPDIR:-/tmp}/ime-host-probe.XXXXXX")}
DISPLAY_NUM=${HUNMIN_PROBE_DISPLAY:-97}

say() { printf '%s\n' "$*"; }
skip() { say "SKIP: $*"; exit 77; }
fail() { say "FAIL: $*"; exit 1; }

for tool in Xvfb dbus-daemon xdotool ibus-daemon ibus; do
    command -v "$tool" >/dev/null || skip "missing $tool"
done
[ -x /usr/bin/python3 ] || skip "missing /usr/bin/python3"
/usr/bin/python3 -c "import gi; gi.require_version('Gtk','3.0')" 2>/dev/null ||
    skip "python3 GObject Introspection with GTK 3 is not installed"

# --- engine: headers ------------------------------------------------------
# pkg-config first; a hand-extracted dev prefix second, because no root is needed
# to unpack libibus-1.0-dev and the probe should still be runnable on a host
# where nobody wants to install it system-wide.
CFLAGS_IBUS=""
LIBS_IBUS=""
if pkg-config --exists ibus-1.0 2>/dev/null; then
    CFLAGS_IBUS=$(pkg-config --cflags ibus-1.0)
    LIBS_IBUS=$(pkg-config --libs ibus-1.0)
elif [ -n "${HUNMIN_IBUS_PREFIX:-}" ]; then
    P=$HUNMIN_IBUS_PREFIX
    CFLAGS_IBUS="-I$P/include/ibus-1.0 -I$P/include -I$P/include/glib-2.0 -I$P/lib/x86_64-linux-gnu/glib-2.0/include"
    LIBS_IBUS="-L$P/lib/x86_64-linux-gnu -libus-1.0 -lgobject-2.0 -lglib-2.0"
else
    skip "ibus-1.0 headers absent; install libibus-1.0-dev or set HUNMIN_IBUS_PREFIX"
fi

ENGINE_SRC=$SCRATCH/probe-engine.c
ENGINE_BIN=$SCRATCH/probe-engine
cat >"$ENGINE_SRC" <<'C'
/* Probe engine: commits a fixed string on every key. Not a frontend. */
#include <ibus.h>
typedef struct { IBusEngine parent; } ProbeEngine;
typedef struct { IBusEngineClass parent; } ProbeEngineClass;
GType probe_engine_get_type(void);
G_DEFINE_TYPE(ProbeEngine, probe_engine, IBUS_TYPE_ENGINE)
static gboolean probe_process_key_event(IBusEngine *engine, guint keyval,
                                       guint keycode, guint state) {
    (void)keycode; (void)state;
    g_printerr("PROBE keyval=0x%x\n", keyval);
    ibus_engine_commit_text(engine, ibus_text_new_from_static_string("\xea\xb0\x80"));
    return TRUE;
}
static void probe_engine_class_init(ProbeEngineClass *klass) {
    IBUS_ENGINE_CLASS(klass)->process_key_event = probe_process_key_event;
}
static void probe_engine_init(ProbeEngine *self) { (void)self; }
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    ibus_init();
    IBusBus *bus = ibus_bus_new();
    if (!ibus_bus_is_connected(bus)) return 1;
    g_object_ref_sink(bus);
    IBusFactory *factory = ibus_factory_new(ibus_bus_get_connection(bus));
    ibus_factory_add_engine(factory, "probe", probe_engine_get_type());
    ibus_bus_request_name(bus, "org.freedesktop.IBus.Probe", 0);
    ibus_main();
    return 0;
}
C
# shellcheck disable=SC2086
cc -o "$ENGINE_BIN" "$ENGINE_SRC" $CFLAGS_IBUS $LIBS_IBUS || fail "engine did not build"

mkdir -p "$SCRATCH/component"
cat >"$SCRATCH/component/probe.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<component>
    <name>org.freedesktop.IBus.Probe</name>
    <description>hunmin host probe</description>
    <exec>$ENGINE_BIN --ibus</exec>
    <version>0.1</version>
    <author>probe</author>
    <license>MIT</license>
    <homepage></homepage>
    <textdomain>probe</textdomain>
    <engines>
        <engine>
            <name>probe</name>
            <language>ko</language>
            <license>MIT</license>
            <author>probe</author>
            <layout>us</layout>
            <longname>Hunmin Probe</longname>
            <description>Probe</description>
            <rank>1</rank>
        </engine>
    </engines>
</component>
XML

# --- private X server and session bus ------------------------------------
# Pick a display that is free rather than a fixed number: a leftover Xvfb from an
# earlier run makes a second run fail in a way that looks like a product bug.
if [ -z "${HUNMIN_PROBE_DISPLAY:-}" ]; then
    for n in 98 99 100 101 102 103; do
        [ -e "/tmp/.X$n-lock" ] || { DISPLAY_NUM=$n; break; }
    done
fi
Xvfb ":$DISPLAY_NUM" -screen 0 1024x768x24 >"$SCRATCH/xvfb.log" 2>&1 &
XVFB_PID=$!
dbus-daemon --session --fork --print-address=1 --print-pid=1 >"$SCRATCH/bus.addr" 2>/dev/null
# With --fork, line 1 is the address and line 2 is the pid of the bus we own --
# never pgrep for "dbus-daemon --session" here, that is how a run kills the
# workstation's own session bus on cleanup.
BUS_PID=$(sed -n 2p "$SCRATCH/bus.addr" | tr -d ' ')

DAEMON_PID=""
GTK_PID=""
cleanup() {
    [ -n "$GTK_PID" ] && kill "$GTK_PID" 2>/dev/null
    [ -n "$DAEMON_PID" ] && kill "$DAEMON_PID" 2>/dev/null
    [ -n "${BUS_PID:-}" ] && kill "$BUS_PID" 2>/dev/null
    kill "$XVFB_PID" 2>/dev/null
    [ "${KEEP:-0}" = 1 ] && say "scratch kept: $SCRATCH" || rm -rf "$SCRATCH"
}
trap cleanup EXIT

# GDK prefers a Wayland display over DISPLAY (measured on a Wayland workstation:
# Gdk.Display is a GdkWaylandDisplay with DISPLAY set), which silently sends the
# client somewhere the probe cannot see. Force X11 and drop the session vars a
# desktop would have set -- the same trap docs/FCITX5-ADDON-NOTES.md §8 records.
unset WAYLAND_DISPLAY XDG_SESSION_TYPE XDG_CURRENT_DESKTOP XDG_SESSION_DESKTOP DESKTOP_SESSION
export DISPLAY=":$DISPLAY_NUM"
export DBUS_SESSION_BUS_ADDRESS=$(head -1 "$SCRATCH/bus.addr")
export GDK_BACKEND=x11
export GTK_IM_MODULE=ibus
export IBUS_COMPONENT_PATH=$SCRATCH/component
export XDG_CACHE_HOME=$SCRATCH/cache
export XDG_CONFIG_HOME=$SCRATCH/config

MEMCONF=$(command -v ibus-memconf || echo /usr/libexec/ibus-memconf)
ibus-daemon -x -c "$MEMCONF" -p disable >"$SCRATCH/daemon.log" 2>&1 &
DAEMON_PID=$!
for _ in $(seq 1 40); do ibus list-engine >/dev/null 2>&1 && break; sleep 0.25; done

LISTED=$(ibus list-engine 2>/dev/null | grep -c 'Hunmin Probe' || true)
say "engine listed:        $LISTED (expected 1)"
[ "$LISTED" = 1 ] || { tail -5 "$SCRATCH/daemon.log"; fail "the daemon did not load the out-of-tree component"; }

xdotool key >/dev/null 2>&1
ibus engine probe >/dev/null 2>&1 || say "note: engine selection reported failure; continuing"

/usr/bin/python3 "$HARNESS" >"$SCRATCH/client.log" 2>&1 &
GTK_PID=$!
for _ in $(seq 1 80); do grep -q '^READY' "$SCRATCH/client.log" 2>/dev/null && break; sleep 0.25; done
grep -q '^READY' "$SCRATCH/client.log" || { cat "$SCRATCH/client.log"; fail "the GTK client never became ready"; }

# The daemon spawns the engine the component XML names, on the first input
# context that asks for it -- so this count is evidence that registration reached
# the point of use, not just that a file was parsed.
SPAWNED=$(pgrep -fc "$ENGINE_BIN --ibus" || true)
say "engine processes:      ${SPAWNED:-0} spawned by the daemon"

for k in r k d k; do xdotool key "$k"; sleep 0.2; done
sleep 0.8
# grep -o, not tr -cd: tr counts BYTES, so a 3-byte character would triple the
# commit count and a 2-commit run would read as 6 (measured).
COMMITS=$(grep '^TEXT:' "$SCRATCH/client.log" | tail -1 | grep -o '가' | wc -l)
say "commits in the client: $COMMITS (keys sent: 4)"

if [ "$COMMITS" -ge 4 ]; then
    say "RESULT: PASS -- an out-of-tree engine was registered, spawned and its commits"
    say "        reached a real GTK client through the host's own input module."
    exit 0
fi
say "client transcript:"; tail -10 "$SCRATCH/client.log"
fail "the engine's commit never reached the client"