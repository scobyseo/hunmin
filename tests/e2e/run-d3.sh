#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Tier D3 — full-stack X11 e2e: stock distro fcitx5 + the hunmin addon
# under Xvfb, golden keys injected with XTEST (tests/e2e/xtest-type.py),
# commits collected from an XIM client (tests/e2e/xim-harness.c).
# Expectations are derived from hunmin-sim --trace (tier A stays the
# single source of truth).
#
# Environment notes (docs/TESTING.md): Xvfb has no evdev/uinput backend
# and /dev/uinput is root-only in CI, so injection enters at the X
# server via XTEST; the whole stack below X (XKB -> fcitx5 XIM -> addon
# -> libhunmin -> client buffer) is real.
#
# Exit 77 = skip (required tooling missing).
#
# usage: run-d3.sh SIM DATA HARNESS TYPER ADDON_DIR IM_CONF ADDON_CONF \
#                  SRC CASES SYS_ADDON_DIR SYS_PKGDATADIR
#
# HUNMIN_D3_CLIENT selects the CLIENT, not the stack: everything above the X
# server (staging, FCITX_* dirs, the private bus, the readiness gates) is
# identical, because the question is what the toolkit does with a key the
# engine declined, and only the client half differs.
#   xim (default) tests/e2e/xim-harness.c   -- COMMIT:/FORWARD: lines
#   gtk           tests/e2e/gtk-harness.py   -- TEXT:<buffer> lines
#   candidate     tests/e2e/xim-harness.c   -- the F9 candidate window,
#                  with classicui ENABLED (issue #53). Same XIM client, but
#                  the run's purpose is the window: fcitx5 must survive the
#                  keystrokes and classicui must actually draw the window.
#                  It reads tests/e2e/cases-candidate.tsv, whose third column
#                  is a hand-written expected commit, not a derived one.
# The GTK client additionally needs GTK_IM_MODULE=fcitx5; XMODIFIERS=@im=fcitx
# is an XIM-only setting and does not select a GTK input method module.
#
# HARNESS LIFECYCLE DIFFERS BY CLIENT, and the reason is measured rather than
# assumed. gtk gets ONE harness per layout group, reused across its cases, with
# a fresh GtkEntry (hence a fresh buffer and a fresh GtkIMContext) installed per
# case. xim gets one harness per CASE, because an XIM input context carries the
# server-side composition and the client side has no per-case reset for it here.
# See the comment at the top of run_case for the measurements.
set -u

CLIENT=${HUNMIN_D3_CLIENT:-xim}

SIM=${1:?sim} DATA=${2:?data} HARNESS=${3:?harness} TYPER=${4:?typer}
ADDON_DIR=${5:?addon_dir} IM_CONF=${6:?im_conf} ADDON_CONF=${7:?addon_conf}
SRC=${8:?src} CASES=${9:?cases}
SYS_ADDON_DIR=${10:?sys_addon_dir} SYS_PKGDATADIR=${11:?sys_pkgdatadir}

# The GTK harness is a Python program (see tests/e2e/gtk-harness.py for why it
# is not C: no GTK development headers on this host, see the measurement page).
# `python3` on PATH is not necessarily one that can import the GObject
# introspection bindings -- measured here: the first python3 on PATH is a venv
# without `gi`, so the harness died with "No module named 'gi'" and the runner
# reported "harness never became ready", which says nothing about GTK. Probe
# for a python that can actually import gi rather than trusting PATH.
pick_python() {
    local p
    for p in ${HUNMIN_D3_PYTHON:-} python3 /usr/bin/python3; do
        [ -n "$p" ] || continue
        if command -v "$p" >/dev/null 2>&1 &&
           "$p" -c 'import gi' >/dev/null 2>&1; then
            echo "$p"
            return 0
        fi
    done
    return 1
}
if [ "$CLIENT" = "gtk" ]; then
    HARNESS_PY=$(pick_python) || {
        echo "SKIP: no python3 with gobject-introspection (gi) bindings"
        exit 77
    }
    GTK_VER=${HUNMIN_GTK:-3.0}
    # The GTK input method module itself is a RUNTIME package here: the GTK
    # development headers are not installed and cannot be (sudo -n fails on
    # this host), so the client is built through GObject introspection against
    # the real libgtk + the real fcitx5 IM module rather than compiled against
    # gtk/gtk.h. That is a modelling choice with a consequence: a host with the
    # runtime but no immodule must SKIP, not fail, because there is nothing to
    # measure there -- see wiki/raw/measurements/2026-10-09-gtk-forward-path.md.
    case "$GTK_VER" in
        4.0) GTK_IMMODULES=/usr/lib/*/gtk-4.0/*/immodules ;;
        *) GTK_IMMODULES=/usr/lib/*/gtk-3.0/*/immodules ;;
    esac
    ls $GTK_IMMODULES/*im-fcitx5.so >/dev/null 2>&1 || {
        echo "SKIP: fcitx5's GTK${GTK_VER%%.*} input method module not installed"
        exit 77
    }
    if [ "$GTK_VER" = "4.0" ]; then
        # GTK 4 has no Gtk.Window.focus(); the harness takes the X focus with
        # xdotool (see gtk-harness.py). Without it the keys go nowhere.
        command -v xdotool >/dev/null 2>&1 || {
            echo "SKIP: xdotool needed to focus a GTK 4 toplevel"
            exit 77
        }
    fi
    # Recorded on the run: which interpreter produced the measurement.
    echo "GTK harness python: $HARNESS_PY ($("$HARNESS_PY" -c 'import gi,sys;
print(sys.version.split()[0])' 2>/dev/null))"
    HARNESS_CMD=("$HARNESS_PY" "$HARNESS")
else
    HARNESS_CMD=("$HARNESS")
fi
if [ "$CLIENT" = "candidate" ]; then
    # classicui is the addon that draws the window; every other D3 mode
    # disables it. The empty value is substituted verbatim into the
    # DisabledAddons line, leaving classicui enabled.
    CANDIDATE_CLASSICUI=""
else
    CANDIDATE_CLASSICUI="DisabledAddons=classicui"
fi
export CANDIDATE_CLASSICUI

for tool in Xvfb xwininfo xprop fcitx5 python3 dbus-run-session; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "SKIP: $tool not available"
        exit 77
    }
done
[ -x "$HARNESS" ] || { echo "SKIP: harness not built"; exit 77; }
[ -r "$CASES" ] || { echo "SKIP: no cases"; exit 77; }
[ -f "$IM_CONF" ] && [ -f "$ADDON_CONF" ] || {
    echo "SKIP: fcitx conf files not generated"
    exit 77
}
[ -d "$SYS_ADDON_DIR" ] && [ -d "$SYS_PKGDATADIR" ] || {
    echo "SKIP: fcitx5 system dirs not found"
    exit 77
}

# Private session bus, always: the outer environment may already carry a
# DBus address whose bus has a conflicting fcitx name owner.
#
# The bus gets a datadir that shadows the freedesktop portal + KDE kwallet
# .service files. fcitx5 core queries org.freedesktop.portal.Desktop on
# startup; activating it spawns xdg-desktop-portal and ksecretd *on this bus*,
# and those daemons inherit this script's stdout/stderr. They then hold the
# pipe open long after the script exits, so ctest blocks forever waiting for
# EOF and never applies its TIMEOUT property. With the services shadowed the
# request fails harmlessly and no fd-inheriting daemon is ever spawned.
if [ -z "${HUNMIN_D3_BUS:-}" ]; then
    d3_shadow=$(mktemp -d /tmp/hunmin-d3-bus.XXXXXX)
    mkdir -p "$d3_shadow/share/dbus-1/services" "$d3_shadow/share/dbus-1/system-services"
    # An empty .service with a harmless Exec makes activation succeed trivially
    # without a real daemon, instead of pulling in the KDE portal stack.
    for svc in org.freedesktop.portal.Desktop \
                org.freedesktop.portal.Documents \
                org.freedesktop.impl.portal.PermissionStore \
                org.freedesktop.impl.portal.desktop.gtk \
                org.freedesktop.impl.portal.desktop.kde \
                org.freedesktop.impl.portal.desktop.kwallet \
                org.kde.kwalletd5 org.kde.kwalletd6; do
        cat >"$d3_shadow/share/dbus-1/services/$svc.service" <<'EOS'
[D-BUS Service]
Name=PLACEHOLDER
Exec=/bin/true
EOS
        sed -i "s/^Name=PLACEHOLDER/Name=$svc/" \
            "$d3_shadow/share/dbus-1/services/$svc.service"
    done
    export XDG_DATA_DIRS="$d3_shadow/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
    # exec replaces this shell, so the shadow dir cannot be removed on exit;
    # hand it to the inner instance via HUNMIN_D3_SHADOW and let cleanup reap it.
    exec dbus-run-session -- env HUNMIN_D3_BUS=1 HUNMIN_D3_SHADOW="$d3_shadow" \
        "$0" "$@"
fi

STAGE=$(mktemp -d /tmp/hunmin-d3.XXXXXX)
# HUNMIN_D3_KEEP=1 leaves $STAGE on disk and prints its path. This harness
# fails with a diff of two text streams and nothing else -- no window, no
# screenshot -- so when a case fails for a reason you cannot see, the only way
# to find out why is to read what fcitx5, dkit and the injector each wrote.
# Opt-in, because the default has to clean up after itself.
if [ -n "${HUNMIN_D3_KEEP:-}" ]; then echo "D3 stage: $STAGE"; fi
HARNESS_PID=""
FCITX_PID=""
XVFB_PID=""
fail=0

XEPHYR_PID=""
Xephyr=""

cleanup() {
    local rc=$?
    for p in "$HARNESS_PID" "$FCITX_PID" "$XEPHYR_PID" "$XVFB_PID"; do
        [ -n "$p" ] && kill "$p" 2>/dev/null
    done
    for _ in $(seq 1 30); do
        left=0
        for p in "$HARNESS_PID" "$FCITX_PID" "$XEPHYR_PID" "$XVFB_PID"; do
            [ -n "$p" ] && kill -0 "$p" 2>/dev/null && left=1
        done
        [ "$left" -eq 0 ] && break
        sleep 0.1
    done
    for p in "$HARNESS_PID" "$FCITX_PID" "$XEPHYR_PID" "$XVFB_PID"; do
        [ -n "$p" ] && kill -KILL "$p" 2>/dev/null
    done
    wait 2>/dev/null
    if [ "$fail" -ne 0 ]; then
        rc=$fail
        echo "=== d3 debug: fcitx5.log ==="
        tail -40 "$STAGE/fcitx5.log" 2>/dev/null
        echo "=== d3 debug: harness.log ==="
        cat "$STAGE/harness.log" 2>/dev/null
    fi
    [ -n "${HUNMIN_D3_KEEP:-}" ] || rm -rf "$STAGE"
    # Release only OUR claims, and only once their server has stopped answering.
    # The pid in the claim file keeps a late release from deleting the next run's
    # claim; the liveness wait keeps a number from being handed on while its old
    # server is still shutting down. A claim left behind by a killed run is stale
    # but harmless -- reserve_display() takes it over after checking its owner.
    local r d
    for r in $RESERVED; do
        [ "$(cat "$r" 2>/dev/null)" = "$$" ] || continue
        d=":${r##*.}"
        for _ in $(seq 1 50); do
            x_display_up "$d" || break
            sleep 0.1
        done
        rm -f "$r" 2>/dev/null
    done
    # reaped only by the inner instance: the outer shell exec'd away
    [ -n "${HUNMIN_D3_SHADOW:-}" ] && rm -rf "$HUNMIN_D3_SHADOW"
    exit "$rc"
}
trap cleanup EXIT INT TERM

# ---------------------------------------------------------------- X servers
# A nested pair, per the fcitx5 wiki's debug recipe: Xvfb as the outer host
# and Xephyr as the display fcitx5 and the harness actually talk to. DISP is
# the Xephyr; HOST is its parent and is never used for anything else.
#
# Note the diagnosis this replaced. The open failure was that the XIM filter
# never sees the injected key, and the tempting reading -- "no window manager,
# so no focus, therefore nest it" -- was measured wrong first: a standalone
# probe under plain Xvfb gets XSetInputFocus applied and the key delivered.
# So nesting is not a fix for that, it is a variable elimination: it removes
# "a bare X server without a compositor" from the set of explanations.
#
# Display allocation is a RESERVATION, not a probe. The version this replaces
# listed candidates and returned the first with no /tmp/.X<n>-lock -- a TOCTOU
# race that is invisible when one D3 test runs at a time and corrupts the tier
# when two do. Measured under `ctest -j16` at 2451fdc: engine-d3-gtk3 and
# engine-d3-gtk4 both picked :99, both started an Xvfb, an Xephyr, an fcitx5 and
# a harness on it, and the two servers typed into each other's client. The
# failures were not crashes but interleaved keystrokes -- `dkssuds` came back as
# "ㅇㄴ" from both -- which is the worst shape of failure, because each test's
# diff looks like a real engine regression rather than a collision.
#
# `mkdir` alone is NOT enough, measured 2026-11-04 (issue #250) -- and on this
# host mkdir is not even atomic:
#
#   /usr/bin/mkdir -> /usr/lib/cargo/bin/coreutils/mkdir   (uutils coreutils)
#   three concurrent `mkdir` on one fresh path, 20 rounds:
#     uutils mkdir              12/20 rounds had MORE THAN ONE "winner"
#     python3 os.mkdir           0/20
#     bash `set -C; : >FILE`     0/20
#
# So the reservation's *design* was right and its *primitive* was not: two
# instances claimed :99 0.6 ms apart, X refused the loser ("Server is already
# active for display 99"), and the winner's fcitx5 died with `XCB connection
# ":99" got error: 1` / `All display connections are gone, exit now` -- which
# surfaced as `harness never became ready` and `fcitx5 did not come up` in half
# of the -j16 runs.
#
# The claim is therefore a FILE holding the claiming pid, created with bash's
# noclobber (O_CREAT|O_EXCL, a kernel-level guarantee, and no external binary to
# be wrong about it), and it is checked against a live server:
#   - a number is only free when no X server answers on it (`x_display_up`);
#   - a claim whose owner is still alive is honoured; one whose owner is gone is
#     stale and may be taken over;
#   - a release acts only on its own claim, and waits until the server has
#     stopped answering first.
x_display_up() { # $1 = display WITH colon, e.g. :99
    xwininfo -root -display "$1" >/dev/null 2>&1
}
claim_file() { # $1 = path; succeeds only when THIS process created it
    ( set -o noclobber; printf '%s\n' "$$" >"$1" ) 2>/dev/null
}
RESERVED=""   # claim files to release; a space-separated list, appended
RES_DISP=""   # the display the last successful reserve_display() claimed
reserve_display() { # $1 = candidates, highest preference first; sets RES_DISP
    local d f owner
    for d in $1; do
        f="/tmp/hunmin-d3-disp.$d"
        # A live server owns this number whatever the claim says -- this is what
        # makes a stale claim harmless: the number is skipped, not stolen.
        x_display_up ":$d" && continue
        # A DIRECTORY at this path is a reservation from the mkdir era, before
        # the primitive changed. They were always empty, so an empty one is
        # simply stale: clear it and claim the number as a file.
        if [ -d "$f" ]; then
            rmdir "$f" 2>/dev/null || continue
        fi
        if ! claim_file "$f"; then
            owner=$(cat "$f" 2>/dev/null)
            [ -n "$owner" ] && kill -0 "$owner" 2>/dev/null && continue
            rm -f "$f" 2>/dev/null
            claim_file "$f" || continue
        fi
        RES_DISP=":$d"
        RESERVED="$RESERVED $f"
        return 0
    done
    return 1
}
# Not $(reserve_display ...): command substitution runs in a subshell, so the
# RESERVED bookkeeping would be discarded and every reservation would leak.
reserve_display "99 98 97" || {
    echo "SKIP: no free X display for the host server"
    fail=0
    exit 77
}
HOST="$RES_DISP"
reserve_display "96 95 94" || {
    echo "SKIP: no free X display for Xephyr"
    fail=0
    exit 77
}
DISP="$RES_DISP"
Xvfb "$HOST" -screen 0 1280x1024x24 -nolisten tcp >"$STAGE/xvfb.log" 2>&1 &
XVFB_PID=$!
server_up() { x_display_up "$1"; }
xvfb_up=0
for _ in $(seq 1 50); do
    if server_up "$HOST"; then
        xvfb_up=1
        break
    fi
    sleep 0.1
done
[ "$xvfb_up" -eq 1 ] || {
    echo "SKIP: Xvfb did not come up"
    fail=0
    exit 77
}
# Assign the resolved path, do not just test it: `command -v X >/dev/null || X=""`
# leaves X empty precisely when X exists, because the || branch is what runs
# the assignment and the found case falls through doing nothing.
Xephyr="$(command -v Xephyr 2>/dev/null || true)"
if [ -n "$Xephyr" ]; then
    DISPLAY="$HOST" "$Xephyr" "$DISP" -screen 1280x900x24 -nolisten tcp -ac \
        >"$STAGE/xephyr.log" 2>&1 &
    XEPHYR_PID=$!
    xephyr_up=0
    for _ in $(seq 1 60); do
        if server_up "$DISP"; then
            xephyr_up=1
            break
        fi
        sleep 0.1
    done
    [ "$xephyr_up" -eq 1 ] || {
        echo "SKIP: Xephyr did not come up"
        fail=0
        exit 77
    }
else
    echo "NOTE: Xephyr not installed; running on a bare Xvfb display"
fi

# ---------------------------------------------------------------- env
# StandardPaths semantics that matter here (fcitx-utils/standardpath.cpp):
#   StandardPathsPrivate::defaultPaths() (fcitx-utils/standardpath.cpp:259)
#   splits FCITX_DATA_DIRS on ':' and uses each entry VERBATIM -- packageName
#   is appended only to the *fallback*, never to an environment value. So an
#   entry must already contain the fcitx5/ level itself: the staged tree is
#   $STAGE/pkgdata/addon and $STAGE/pkgdata/inputmethod, and FCITX_DATA_DIRS is
#   $STAGE/pkgdata. Staging it one level deeper (pkgdata/fcitx5/addon) is why
#   hunmin was silently not loaded while every system addon was.
#   FCITX_ADDON_DIRS is a library path list and is used verbatim.
#   FCITX_CONFIG_HOME is used verbatim for profile / PkgConfig lookups.
# $STAGE/config must exist: it is FCITX_CONFIG_HOME, so the global `config`
# file and `profile` below land here. Without it the heredocs fail with
# "No such file or directory" on stderr, and because the failing command is
# not the last one in the script the failure does not change the exit status
# seen by ctest -- fcitx5 then starts with no profile, falls back to a
# keyboard-us group, and every key passes through raw.
mkdir -p "$STAGE/config" "$STAGE/data/fcitx5/conf" "$STAGE/pkgdata/addon" \
    "$STAGE/pkgdata/inputmethod" \
    "$STAGE/cache" "$STAGE/runtime" \
    "$STAGE/home/.config" "$STAGE/home/.local/share"
chmod 700 "$STAGE/runtime"
cp "$IM_CONF" "$STAGE/pkgdata/inputmethod/hunmin.conf"
cp "$ADDON_CONF" "$STAGE/pkgdata/addon/hunmin.conf"
# fcitx5 loads the addon library from the same directory as the addon.conf
cp "$ADDON_DIR/libhunmin.so" "$STAGE/pkgdata/addon/libhunmin.so"

# Item order is load-bearing. DefaultIM=hunmin is what selects the engine, but
# fcitx5 orders the group's list and, measured on 2026-10-03, only ever activated
# the FIRST listed item: with hunmin at Items/0 the active IM was still
# keyboard-us at injection time and every case failed. With keyboard-us first
# and hunmin second, DefaultIM is honoured and the cases pass. Do not "tidy"
# this ordering back.
cat >"$STAGE/config/profile" <<'EOF'
[Groups/0]
# Group Name
Name=Default
# Layout
Default Layout=us
# Default Input Method
DefaultIM=hunmin

[Groups/0/Items/0]
# Name
Name=keyboard-us
# Layout
Layout=

[Groups/0/Items/1]
# Name
Name=hunmin
# Layout
Layout=

[GroupOrder]
0=Default
EOF

# xim frontend config: tell it to use hunmin for XICs
# NOTE: xim.conf has no DefaultInputMethod option — the XIM module never
# reads one (grep DefaultInputMethod src/frontend/xim/xim.cpp -> 0 hits).
# The IM is chosen by the profile's DefaultIM plus the focus-in policy.

# fcitx starts contexts INACTIVE (keyboard-us passthrough) unless
# ActiveByDefault — without this every key would go to the client raw.
#
# DisabledAddons: the GUI/portal addons are irrelevant to D3 (no candidate
# window is asserted on) and actively harmful. Loading them makes fcitx5
# D-Bus-activate xdg-desktop-portal/kwallet; those daemons are then spawned
# by the private D3 bus while inheriting its stdout/stderr, and they keep
# that pipe open long after this script exits. ctest blocks forever waiting
# for EOF and never applies the TIMEOUT property. Disable them so the bus
# stays free of fd-inheriting children.
# classicui is the ONE exception and only in candidate mode: it is the addon
# that draws the candidate window, and tier D3 is otherwise the tier that
# can never draw it. CANDIDATE_CLASSICUI is substituted verbatim into the
# line above, so an empty value means "leave classicui enabled" and the
# default (unset) keeps it disabled for every other mode.
# The context is only activated on focus-in when resetStateWhenFocusIn is not
# the default PropertyPropagatePolicy::No — Instance's InputContextFocusIn
# watcher (instance.cpp:1060) only calls activate()/deactivate() inside that
# policy check. ActiveByDefault alone is therefore not enough: without this the
# context stays inactive, fcitx5-remote -n reports nothing, and every key is
# translated by the client's own keymap instead of reaching libhunmin.
# EnabledAddons/DisabledAddons live inside BehaviorConfig, not a section of
# their own (globalconfig.cpp) -- so they belong under [Behavior]. Placing them
# under [Addon] parses as nothing and fcitx5 logs an empty override set.
cat >"$STAGE/config/config" <<EOF
[Behavior]
ActiveByDefault=True
ResetStateWhenFocusIn=All
${CANDIDATE_CLASSICUI:-DisabledAddons=classicui}
DisabledAddons=kimpanel
DisabledAddons=virtualkeyboard
DisabledAddons=notificationitem
DisabledAddons=notifications
EOF

export DISPLAY="$DISP"
# Detach from the host desktop session. On a Wayland workstation fcitx5
# prefers $WAYLAND_DISPLAY over $DISPLAY and silently attaches to the real
# compositor, so the Xvfb instance is never used: XIM never registers and
# every key reaches the client raw. Unset the whole family.
unset WAYLAND_DISPLAY XDG_SESSION_TYPE XDG_CURRENT_DESKTOP XDG_SESSION_DESKTOP \
      DESKTOP_SESSION GNOME_SHELL_SESSION_MODE MUTTER_SESSION_DBUS_DISPLAY
export LANG=C.UTF-8 LC_ALL=C.UTF-8
export XMODIFIERS=@im=fcitx
# GTK selects its input method module from GTK_IM_MODULE, not from XMODIFIERS.
# Without this a GTK client loads its own built-in fallback and the run would
# measure GTK's default key handling rather than fcitx5's GTK frontend --
# which is exactly the thing being measured. The harness prints MODULE: lines
# naming the im-fcitx5.so mapped into its process, and run_case REFUSES a run
# where it is absent, so this cannot pass while measuring another input method.
if [ "$CLIENT" = "gtk" ]; then
    export GTK_IM_MODULE=fcitx5
    # X11 explicitly: a GTK client that falls back to the Wayland backend
    # would need a compositor, and there is none under Xvfb. Spelling it out
    # means a missing backend is an error, not a silent fallback.
    export GDK_BACKEND=x11
fi
export HOME="$STAGE/home"
export XDG_CONFIG_HOME="$HOME/.config"
export XDG_DATA_HOME="$HOME/.local/share"
export XDG_CACHE_HOME="$STAGE/cache"
export XDG_RUNTIME_DIR="$STAGE/runtime"
export FCITX_CONFIG_HOME="$STAGE/config"
export FCITX_DATA_HOME="$STAGE/data"
export FCITX_ADDON_DIRS="$ADDON_DIR:$SYS_ADDON_DIR"
export FCITX_DATA_DIRS="$STAGE/pkgdata:$SYS_PKGDATADIR"
export HUNMIN_DATA_PATH="$DATA"
export LD_LIBRARY_PATH="$(dirname "$(realpath "$SIM")"):${LD_LIBRARY_PATH:-}"
export FCITX_LOG_RULE="default=5,xim=5,hunmin=5"
# The harness reads its case id from here when SIGUSR1'd; run_case rewrites it
# per case. One harness serves a whole layout group (see run_case), so the id
# cannot be a start argument.
CASE_ID_FILE="$STAGE/case-id"
: >"$CASE_ID_FILE"
export HUNMIN_CASE_ID_FILE="$CASE_ID_FILE"

# -------------------------------------------------------- fcitx control
# Elapsed milliseconds from $2 (a gate start, in EPOCHREALTIME's microseconds)
# to $1, or "never" when the fact was not observed. Bash builtins only: this is
# called for every gate fact on every start, not only on a failure, so a fork
# per call would be paid 200 times a start.
gate_ms() { # $1 = timestamp in us (or ""), $2 = start in us
    if [ -z "$1" ]; then
        printf 'never'
    else
        printf '%d ms' "$(( ($1 - $2) / 1000 ))"
    fi
}

start_fcitx() { # $1 = layout
    # NOTE: never probe with fcitx5-remote before the instance owns the
    # bus name — dbus *activates* org.fcitx.Fcitx5 from its .service file,
    # racing a second instance against ours (observed: addon unload storm).
    # The layout must reach the addon, and the addon reads it from the PROFILE's
    # per-item Layout= field -- HunminAdapter::layoutFor() calls
    # InputMethodGroup::layoutFor(), which reads item.layout() out of
    # [Groups/0/Items/N]. Writing it to data/fcitx5/conf/hunmin.conf (an
    # fcitx5 *addon* config file) does nothing: fcitx5 never hands an addon the
    # layout, and the addon's own hunmin.conf has no Layout key at all.
    # Measured with the layout left empty: the engine then composes with
    # sin3-p2 regardless of the case, so dkssuds committed ㅎ|곈 instead of
    # 안|녕 while the two sin3-p2 cases passed -- right answer, wrong reason.
    rewrite_profile_layout() { # $1 = layout
        local layout=$1
        # DefaultIM must stay set: fcitx5 only reads DefaultIM= on first load
        # and an empty per-item Layout is not a valid fallback.
        cat >"$STAGE/config/profile" <<EOF
[Groups/0]
# Group Name
Name=Default
# Layout
Default Layout=us
# Default Input Method
DefaultIM=hunmin

[Groups/0/Items/0]
# Name
Name=keyboard-us
# Layout
Layout=

[Groups/0/Items/1]
# Name
Name=hunmin
# Layout
Layout=$layout

[GroupOrder]
0=Default
EOF
    }
    rewrite_profile_layout "$1"
    local gate_layout=$1
    fcitx5 >"$STAGE/fcitx5.log" 2>&1 &
    FCITX_PID=$!
    # Two gates, not one, and the order matters. The XIM server registration
    # is necessary but NOT sufficient: the xim addon loads early and hunmin
    # loads a second later (measured 2026-10-03: "Loaded addon xim" at t+0.02s,
    # "Loaded addon hunmin" at t+1.08s). Returning as soon as @server=fcitx
    # appears let the first keys be injected while no input method engine was
    # loaded, so they were forwarded to the server and dropped: the case failed
    # with an empty commit stream and the loss was invisible in the output.
    #
    # The hunmin addon must be loaded before any key is injected. The xim
    # addon loads early and hunmin loads a second later (measured 2026-10-03:
    # "Loaded addon xim" at t+0.02s, "Loaded addon hunmin" at t+1.08s), and
    # returning as soon as @server=fcitx appears let the first keys be
    # injected while no engine was loaded: they were forwarded to the server
    # and dropped, and the case failed with an empty commit stream.
    #
    # Do NOT gate on fcitx5-remote answering. Measured here: it reports state
    # 0 and an empty current-IM name for this whole run, including long after
    # fcitx5 owns its bus name and fcitx5-diagnose reports a live InputContext.
    # Gating on it makes the test fail to start for a reason unrelated to the
    # engine, so its report is not treated as a readiness signal here.
    # The gate ALSO records when each of its three facts becomes true, not only
    # that they eventually are. Issue #253: one start in eight parallel runs had
    # "Loaded addon hunmin" at +1.09 s and no @server=fcitx for the rest of the
    # 19 s gate, and the shipped poll kept nothing that said which half was
    # missing -- the log's tail did not even carry "Loaded addon xim". Timing
    # the facts costs three greps and one xprop per 0.1 s poll, and the result
    # is appended to $STAGE/gate.log on every start, so a passing run's timings
    # are readable afterwards (HUNMIN_D3_KEEP=1) instead of needing an
    # instrumented copy of this script.
    local start_us=${EPOCHREALTIME/./}
    local now_us xim_us="" hunmin_us="" prop_us="" polls=0 xprop_ok=0 xprop_bad=0
    local xprop_out
    for _ in $(seq 1 200); do
        polls=$((polls + 1))
        now_us=${EPOCHREALTIME/./}
        # Named separately because the xim addon is what publishes
        # @server=fcitx, and the gate does not otherwise ask whether it loaded
        # at all. Recorded, never gated on: a new condition would change what
        # the tier waits for.
        if [ -z "$xim_us" ] &&
            grep -q 'Loaded addon xim' "$STAGE/fcitx5.log" 2>/dev/null; then
            xim_us=$now_us
        fi
        if [ -z "$hunmin_us" ] &&
            grep -q 'Loaded addon hunmin' "$STAGE/fcitx5.log" 2>/dev/null; then
            hunmin_us=$now_us
        fi
        # One xprop per poll, kept: whether the GATE could reach the display at
        # all is the fact that separates "the server never published" from "the
        # server was unreachable from here too", and it is otherwise thrown
        # away with stderr.
        if xprop_out=$(xprop -root -display "$DISP" 2>/dev/null); then
            xprop_ok=$((xprop_ok + 1))
            if [ -z "$prop_us" ] && printf '%s' "$xprop_out" | grep -q '@server=fcitx'; then
                prop_us=$now_us
            fi
        else
            xprop_bad=$((xprop_bad + 1))
        fi
        if [ -n "$prop_us" ] && [ -n "$hunmin_us" ]; then
            printf 'layout=%s prop=%s xim_addon=%s hunmin=%s polls=%s\n' \
                "$gate_layout" \
                "$(gate_ms "$prop_us" "$start_us")" \
                "$(gate_ms "$xim_us" "$start_us")" \
                "$(gate_ms "$hunmin_us" "$start_us")" \
                "$polls" >>"$STAGE/gate.log"
            # Activate the hunmin IM: -s takes the IM name and switches to it.
            # (-o is a bare flag meaning "activate the application"; passing
            # it an imname argument silently does nothing but activate, which
            # left the context on keyboard-us and every key passing through raw.)
            # NOTE: no `fcitx5-remote -s hunmin` here. Measured: it returns
            # rc=0 (success) and yet the active IM stays keyboard-us, because
            # Instance::setCurrentInputMethod looks the name up in the current
            # group's list and the context has none focused at this point. It
            # looks like it works and is not doing anything; DefaultIM in the
            # profile above is what actually selects the engine.
            echo "Activating the input method via fcitx5-remote -o..."
            fcitx5-remote -o 2>&1 | tee -a "$STAGE/fcitx5.log"
            fcitx5-remote 2>&1 | tee -a "$STAGE/fcitx5.log"
            # The gate above already proves readiness: measured 2026-11-04, the
            # root-window @server=fcitx property and "Loaded addon xim" become
            # true in the same poll at 1.20-1.35 s (24 starts), 1.44-1.76 s under
            # 16 CPU hogs, and "Loaded addon hunmin" is there 0.1 s earlier. So
            # this is a settling allowance, not a wait for a signal -- 0.1 s
            # instead of 0.3 s, which the post-gate dead time used to spend
            # three times a tier (docs/D3-PERFORMANCE.md).
            sleep 0.1
            return 0
        fi
        kill -0 "$FCITX_PID" 2>/dev/null || break
        sleep 0.1
    done
    # The gate now names WHICH half was missing, because "did not come up" is
    # the wrong sentence for a daemon that is running and serving (#253), and
    # dumps what the poll saw while the daemon is still alive -- start_fcitx
    # returns before stop_fcitx runs, so the display, the process and the log
    # are all still there. The dump goes to the ctest log (stdout) and to
    # $STAGE/gate-failure.log, so it survives the stage being removed unless
    # HUNMIN_D3_KEEP=1 was set, and the next occurrence is diagnosable without
    # re-instrumenting a copy of this script.
    local why loglines
    if ! kill -0 "$FCITX_PID" 2>/dev/null; then
        why="fcitx5 (pid $FCITX_PID) exited before the gate was satisfied"
    elif [ -n "$hunmin_us" ] && [ -z "$prop_us" ]; then
        why="the hunmin addon loaded but @server=fcitx never appeared on $DISP"
    elif [ -n "$prop_us" ] && [ -z "$hunmin_us" ]; then
        why="@server=fcitx appeared but the hunmin addon never loaded"
    else
        why="neither @server=fcitx nor the hunmin addon appeared"
    fi
    # Measured 2026-11-05: an X connection that fails is silent -- fcitx5 loads
    # the xcb addon and logs no "Connecting to X11 display" line at all, with
    # DISPLAY pointing at nothing and with DISPLAY unset alike (probe in
    # docs/D3-PERFORMANCE.md). A start that has no such line therefore never
    # had a display to register an XIM server on, which is a different defect
    # from an addon that loaded and failed to publish.
    if ! grep -q 'Connecting to X11 display' "$STAGE/fcitx5.log" 2>/dev/null; then
        why="$why; fcitx5 never opened an X connection (no 'Connecting to X11 display' line)"
    fi
    echo "ERROR: fcitx5 did not come up (layout=$1): $why"
    loglines=$(grep -c '' "$STAGE/fcitx5.log" 2>/dev/null || true)
    {
        echo "=== d3 gate failure evidence (issue #253) ==="
        echo "layout=$1 display=$DISP pid=$FCITX_PID polls=$polls of 200" \
            "elapsed=$(gate_ms "$now_us" "$start_us")"
        echo "--- when each gate fact was first observed ---"
        printf '  Loaded addon xim    : %s\n' "$(gate_ms "$xim_us" "$start_us")"
        printf '  Loaded addon hunmin : %s\n' "$(gate_ms "$hunmin_us" "$start_us")"
        printf '  @server=fcitx       : %s\n' "$(gate_ms "$prop_us" "$start_us")"
        echo "  the gate's own xprop reached $DISP in $xprop_ok of $polls polls" \
            "($xprop_bad failed)"
        echo "--- root window properties on $DISP (xprop -root) ---"
        xprop -root -display "$DISP" 2>&1 || echo "  xprop failed"
        echo "--- fcitx5 process state ---"
        ps -o pid,ppid,stat,etime,wchan:28,args -p "$FCITX_PID" 2>&1 ||
            echo "  pid $FCITX_PID is not running"
        ps -o pid,stat,wchan:28,args --ppid "$FCITX_PID" 2>&1 |
            sed 's/^/  child: /'
        echo "--- environment the daemon was started with (no cookie values) ---"
        tr '\0' '\n' <"/proc/$FCITX_PID/environ" 2>/dev/null |
            grep -E '^(DISPLAY|XAUTHORITY|WAYLAND_DISPLAY|XDG_RUNTIME_DIR|DBUS_SESSION_BUS_ADDRESS)=' ||
            echo "  (cannot read /proc/$FCITX_PID/environ)"
        echo "--- X sockets, and auth entries by display name only ---"
        ls -l /tmp/.X11-unix/ 2>&1
        # Names only. An xauth cookie is a credential and never belongs in a
        # test log, on the console or in an uploaded artefact.
        if [ -n "${XAUTHORITY:-}" ]; then
            xauth -f "$XAUTHORITY" list 2>/dev/null | awk '{print "  " $1 " " $2}'
        else
            echo "  XAUTHORITY is unset"
        fi
        echo "--- addon, XIM and X-connection lines from the fcitx5 log ---"
        grep -nE 'Loaded addon|xim|Connecting to X11' "$STAGE/fcitx5.log" 2>/dev/null |
            tail -60 || echo "  (no addon, xim or X-connection line in the log)"
        echo "--- every start's gate timings in this run ($STAGE/gate.log) ---"
        cat "$STAGE/gate.log" 2>/dev/null || echo "  (this was the first start)"
        echo "--- Xephyr and Xvfb logs (tail 20 each) ---"
        tail -20 "$STAGE/xephyr.log" 2>/dev/null | sed 's/^/  xephyr: /' ||
            echo "  (no xephyr.log)"
        tail -20 "$STAGE/xvfb.log" 2>/dev/null | sed 's/^/  xvfb: /' ||
            echo "  (no xvfb.log)"
        echo "--- fcitx5.log, complete (${loglines:-0} lines) ---"
        cat "$STAGE/fcitx5.log" 2>/dev/null
        echo "=== end d3 gate failure evidence ==="
        echo "stage: $STAGE (kept on exit only with HUNMIN_D3_KEEP=1)"
    } >"$STAGE/gate-failure.log" 2>&1 || true
    cat "$STAGE/gate-failure.log"
    return 1
}

stop_fcitx() {
    [ -n "$FCITX_PID" ] || return 0
    kill "$FCITX_PID" 2>/dev/null
    for _ in $(seq 1 30); do
        kill -0 "$FCITX_PID" 2>/dev/null || break
        sleep 0.1
    done
    kill -KILL "$FCITX_PID" 2>/dev/null
    wait "$FCITX_PID" 2>/dev/null
    FCITX_PID=""
}

# ------------------------------------------------------------- a case
# The expected stream is what the CLIENT should end up holding, in order:
# every string the engine committed, plus -- for each key the engine DECLINED
# -- the character the input method forwarded back to us (docs/trace-spec.md
# `consumed`). Both halves come from hunmin-sim --trace, so tier A stays the
# single source of truth; the `consumed` field it now emits is the engine's own
# answer, not a re-derivation from the layout's key table.
#
# A declined key contributes its character even when the engine also committed
# something on the way (space flushes the in-flight syllable AND is declined),
# because both reach the client: the commit as COMMIT, the character as
# FORWARD. Hence one trace line can produce two expected lines.
sim_keys() { # $1 = case-file keys string; prints hunmin-sim-compatible keys
    printf '%s' "$1" | python3 -c '
import sys
s = sys.stdin.read()
s = s.replace("{BackSpace}", "\x08")
s = s.replace("{Return}", "\x0d")
s = s.replace("{Escape}", "")   # frontend dismisses the candidate window; sim cannot model this
s = s.replace("{F9}", "")
s = s.replace("{,}", ",")
sys.stdout.write(s)
'
}

expected_stream() { # $1=layout $2=keys
    local keys
    keys=$(sim_keys "$2")
    "$SIM" --data "$DATA" --layout "$1" --keys "$keys" --trace |
        python3 -c '
import json, sys
word = ""
for line in sys.stdin:
    o = json.loads(line)
    if o.get("consumed", True):
        word += o.get("commit", "")
        continue
    boundary = word + o.get("commit", "")
    word = ""
    if boundary:
        print("COMMIT:" + boundary)
    ch = (o.get("in") or {}).get("ch")
    if ch:
        print("FORWARD:" + ch)
'
}

# The GTK client's observable is a GtkEntry's buffer, not a stream of events:
# it prints TEXT:<whole buffer> on every change, and the last such line is the
# final state. So the expectation is the CONCATENATION of everything the engine
# committed plus the character of every key it declined -- which is the same
# information expected_stream carries, folded into the order a widget's buffer
# has instead of the order a callback fires in.
#
# It is a strictly different assertion from the XIM one and a weaker one in one
# specific respect: a client that inserted the space TWICE and then removed it
# would print the same final line as one that inserted it once. Nothing here
# can see the intermediate TEXT: lines; they are printed for the failure dump
# only. That is a property of measuring a buffer, not a shortcut.
expected_text() { # $1=layout $2=keys
    local keys
    keys=$(sim_keys "$2")
    "$SIM" --data "$DATA" --layout "$1" --keys "$keys" --trace | \
        python3 -c '
import json, sys
buf = []
word = ""
for line in sys.stdin:
    o = json.loads(line)
    if o.get("consumed", True):
        word += o.get("commit", "")
        continue
    done = word + o.get("commit", "")
    word = ""
    if done:
        buf.append(done)
    ch = (o.get("in") or {}).get("ch")
    if ch:
        buf.append(ch)
# Include trailing in-flight word, because the client buffer holds it.
if word:
    buf.append(word)
print("TEXT:" + "".join(buf))
'
}

# Kept as the COMMIT-only view for the cases that predate forwarded keys; the
# pass-through cases are compared with expected_stream instead.
expected_commits() { # $1=layout $2=keys
    "$SIM" --data "$DATA" --layout "$1" --keys "$2" --trace |
        python3 -c '
import json, sys
for line in sys.stdin:
    o = json.loads(line)
    if o.get("commit"):
        print("COMMIT:" + o["commit"])
'
}

run_case() { # $1=layout $2=keys $3=expectation-fn (default expected_stream)
    local layout=$1 keys=$2 expect_fn=${3:-expected_stream}
    local expected actual case_lines hpid i case_ready case_id=$CASE_SEQ
    CASE_SEQ=$((CASE_SEQ + 1))

    if [ "$CLIENT" = "gtk" ]; then
        # The buffer is the whole assertion for a GTK client; there is no
        # COMMIT-only variant of it. Defaulting to expected_stream here would
        # silently compare the wrong shape, so force it.
        expect_fn=expected_text
    fi
    expected=$("$expect_fn" "$layout" "$keys")

    # HARNESS LIFECYCLE, AND WHY THE TWO CLIENTS DIFFER AGAIN
    # ------------------------------------------------------
    # gtk: ONE harness per layout group, reused across its cases. The group
    #   harness is started here if none is alive and torn down when the layout
    #   changes; every case is announced over the case-id file plus SIGUSR1.
    #
    # xim: ONE harness per CASE, which is the arrangement before 8de61b1 and
    #   which measured all eight cases green (2026-10-10, 40.65s). Reuse was
    #   tried and measured unsound: the XIM client keeps ONE input context for
    #   the group and an XIC carries the server-side composition with it.
    #   Four of the eight cases end with a preedit still in flight (measured
    #   with hunmin-sim --trace: ㄴ, ㅒ, 세, 를), so the next case in the group
    #   starts on top of it and commits a syllable the expectation never asked
    #   for -- `sin3-p2 jk1` came back COMMIT:ㅒ|COMMIT:ㅇµ where tier A says
    #   COMMIT:ㅇµ. An IC focus cycle (xcb_xim_unset_ic_focus/set_ic_focus) was
    #   tried as the reset and did NOT clear the preedit; a per-case IC is the
    #   obvious remaining option and is NOT attempted here.
    #
    #   What reuse would have bought, measured: this harness starts in 0.14s,
    #   not the ~5s its own comment claimed, so per-case start costs about 1.1s
    #   over the whole tier. Correctness is not worth that, and the honest
    #   eight-case X11 run costs ~40s whether or not it is reused.
    #
    # stdin is closed to /dev/null for the harness deliberately: a client has
    # no business reading the runner's streams, and the runner reads its case
    # list on fd 3 (below) so that nothing it starts can eat it by accident.
    reuse_harness=0
    [ "$CLIENT" = "gtk" ] && reuse_harness=1

    if [ "$reuse_harness" = "1" ] && [ -n "$HPID" ]; then
        hpid=$HPID
    else
        hpid=""
        for _ in 1 2 3; do
            "${HARNESS_CMD[@]}" </dev/null >"$STAGE/commits.log" 2>"$STAGE/harness.log" &
            hpid=$!
            for i in $(seq 1 50); do
                grep -q READY "$STAGE/commits.log" 2>/dev/null && break
                kill -0 "$hpid" 2>/dev/null || break
                sleep 0.1
            done
            grep -q READY "$STAGE/commits.log" 2>/dev/null && break
            kill "$hpid" 2>/dev/null
            wait "$hpid" 2>/dev/null
            sleep 0.3
            hpid=""
        done
        if [ -z "$hpid" ]; then
            echo "FAIL [$layout/$keys]: harness never became ready"
            fail=1
            return
        fi
        [ "$reuse_harness" = "1" ] && HPID=$hpid
    fi

    case_ready=1
    if [ "$reuse_harness" = "1" ]; then
        # Stage this case's id where the harness will read it, then SIGUSR1 it.
        # The signal is process-to-process, so no keystroke crosses the X
        # server and the engine cannot consume or forward it -- that is why a
        # marker is not a key. See xim-harness.c on_case_marker.
        printf '%s' "$case_id" >"$CASE_ID_FILE"
        kill -USR1 "$hpid" 2>/dev/null
        # Wait for the harness to confirm it is ON THIS CASE and ready for
        # keys. READY only covers the first case of a group; CASESTART:<id> is
        # the per-case gate, and the GTK harness prints it only once the case's
        # fresh GtkEntry holds the X focus -- which is the fact the fixed "let
        # fcitx focus round-trips settle" sleep used to be standing in for.
        case_ready=0
        for i in $(seq 1 100); do
            if grep -qx "CASESTART:$case_id" "$STAGE/commits.log" 2>/dev/null; then
                case_ready=1
                break
            fi
            kill -0 "$hpid" 2>/dev/null || break
            sleep 0.1
        done
        if [ "$case_ready" -ne 1 ]; then
            if grep -qx "CASEFAIL:$case_id" "$STAGE/commits.log" 2>/dev/null; then
                echo "FAIL [$layout/$keys]: the case's fresh input widget never took the X focus"
            else
                echo "FAIL [$layout/$keys]: harness never reached case $case_id"
            fi
            kill "$hpid" 2>/dev/null
            wait "$hpid" 2>/dev/null
            HPID=""
            fail=1
            return
        fi
    fi
    # Kept, and kept unconditional, because the case marker is NOT a substitute
    # for it. The marker proves the widget holds the X focus; it does not prove
    # the toolkit has finished connecting that widget's GtkIMContext to fcitx5,
    # and there is no cheap signal for the second: fcitx5-remote answers in 3ms
    # but reports the same thing for every case (measured, 2026-10-10), and
    # fcitx5-diagnose does report contexts but costs 3.0s a call.
    #
    # Dropping this sleep was tried, on the reasoning that the marker subsumes
    # it, and it is not safe: over six GTK 4 runs at zero settle, one case came
    # back as TEXT:dㅏㄴ녕ㄴ where tier A says TEXT:안녕ㄴ -- the leading `d`
    # reached the widget raw, with no engine behind it. It was the first case of
    # a layout group, i.e. exactly the case whose fcitx5 daemon was one second
    # old. Five further runs were clean, so the window is narrow and rare; it is
    # still a green-test-that-lies failure mode and it is not worth 3 seconds.
    sleep 0.4 # let fcitx focus round-trips settle

    # The IM is already active (start_fcitx did fcitx5-remote -o), so do NOT
    # send the Ctrl+space trigger key here: it is a toggle and would deactivate
    # the context, sending every key to the client raw.
    if ! python3 "$TYPER" "$DISP" "$keys" </dev/null; then
        echo "FAIL [$layout/$keys]: key injection failed"
        kill "$hpid" 2>/dev/null
        wait "$hpid" 2>/dev/null
        HPID=""
        fail=1
        return
    fi
    # How long to wait for the client to finish taking the keys. The two paths
    # need different answers, and each gets ONE block and ONE reason.
    if [ "$reuse_harness" = "1" ]; then
        # GTK: one harness serves the whole layout group, so a quiet log is not a
        # completion signal -- the settle is clock, and it is load-bearing (at
        # zero settle a case came back TEXT:dㅏㄴ녕ㄴ, docs/TESTING.md). The two
        # consecutive sleeps this replaces, 0.5 then 0.3 with nothing between
        # them, were the same wait written twice.
        sleep 0.8
    else
        # XIM: a fresh client process per case, writing each line as it arrives,
        # so a log that stops growing IS the completion signal. Measured
        # 2026-11-04 over 27 cases: quiet after 0.119-0.213 s (median 0.122),
        # against the 0.8 s of clock this replaces -- so the cap below is the old
        # settle and a slow client behaves exactly as it did.
        settle_step=0.03     # poll interval, seconds
        settle_quiet=3       # consecutive quiet polls that mean "done" (~90 ms)
        settle_cap=27        # 27 x 0.03 = 0.81 s, the fixed settle this replaces
        settle_lines=$(grep -c '' "$STAGE/commits.log" 2>/dev/null) || settle_lines=0
        settle_stable=0
        settle_capped=1
        for _ in $(seq 1 "$settle_cap"); do
            sleep "$settle_step"
            settle_now=$(grep -c '' "$STAGE/commits.log" 2>/dev/null) || settle_now=0
            if [ "$settle_now" = "$settle_lines" ]; then
                settle_stable=$((settle_stable + 1))
            else
                settle_stable=0
                settle_lines=$settle_now
            fi
            if [ "$settle_stable" -ge "$settle_quiet" ]; then settle_capped=0; break; fi
        done
        # Not an assertion -- the comparison below is. This only says the client
        # was still talking at the cap, which the old fixed settle would have
        # cut off too.
        if [ "$settle_capped" -eq 1 ]; then
            echo "note: [$layout/$keys] still writing at the settle cap" >&2
        fi
    fi
    if [ "$reuse_harness" = "1" ]; then
        # The harness is deliberately NOT killed and NOT waited on here: the
        # layout group's next case reuses it. It is torn down when the layout
        # changes and at end of run, where fcitx5 goes down with it.
        :
    else
        kill -TERM "$hpid" 2>/dev/null
        wait "$hpid" 2>/dev/null
    fi

    # Both line kinds in the order the harness printed them, prefix intact: the
    # ORDER is the measurement (a space arrives after the syllable it ended),
    # so this must be one pass over the log, not one pass per prefix --
    # concatenating the two would silently sort every commit ahead of every
    # forwarded key and still compare equal for most sequences.
    if [ "$reuse_harness" = "1" ]; then
        # A reused GTK harness writes every case of the group into one log,
        # separated by CASESTART:<id> markers. Take only the lines between this
        # case's marker and the next one. awk keeps the ordering the harness
        # printed, which is the measurement.
        case_lines=$(awk -v id="$case_id" '
            $0 == "CASESTART:" id   { on = 1; next }
            /^CASESTART:/           { on = 0 }
            on { print }
        ' "$STAGE/commits.log" || true)
        # A GtkEntry has one buffer and its history is not a stream: the earlier
        # TEXT: lines in this case's segment are the intermediate states and
        # the measurement is the state the user is left looking at. `tail -1`
        # rather than a stream diff, for that reason.
        actual=$(printf '%s\n' "$case_lines" | grep -E '^TEXT:' | tail -1 || true)
        # The fcitx5 GTK IM module must actually be in this process, or the
        # run measures something else and says nothing about the GTK frontend.
        # This is a real hazard here, not a hypothetical. Measured: with
        # GTK_IM_MODULE=xim in the environment instead of fcitx5, every case
        # still PASSES -- GTK loads im-xim.so, the XMODIFIERS=@im=fcitx set
        # above for the XIM server points it at the same fcitx5, and the buffer
        # fills with the right text by the XIM path this measurement exists to
        # distinguish from the GTK one. A result alone cannot tell the two
        # apart; this check can.
        if ! grep -q '^MODULE:im-fcitx5.so=' "$STAGE/commits.log" 2>/dev/null; then
            echo "FAIL [$layout/$keys]: fcitx5's GTK IM module is not loaded;"
            echo "  the buffer came from some other input method, so this run"
            echo "  is not a GTK-frontend measurement."
            grep -E '^(MODULE|GTK_IM_MODULE):' "$STAGE/commits.log" 2>/dev/null |
                sed 's/^/  /'
            fail=1
            return
        fi
    else
        # A fresh XIM harness per case means a fresh log per case, so the whole
        # file is this case's stream.
        actual=$(grep -E '^(COMMIT|FORWARD):' "$STAGE/commits.log" || true)
    fi
    if [ "$actual" != "$expected" ]; then
        echo "FAIL [$layout/$keys]"
        echo "  expected: $(printf '%s' "$expected" | paste -sd'|')"
        echo "  actual:   $(printf '%s' "$actual" | paste -sd'|')"
        echo "  --- client transcript ---"
        grep -E '^(TEXT|COMMIT|FORWARD|CASESTART|CASEFAIL|MODULE|GTK_IM_MODULE|READY):' \
            "$STAGE/commits.log" 2>/dev/null | sed 's/^/  /'
        fail=1
    else
        echo "ok: [$layout/$keys] -> $(printf '%s' "$expected" | paste -sd'|')"
        # A GTK run is only evidence about the fcitx5 GTK frontend if that
        # module was in fact loaded, so the module line is printed on success
        # too -- a green run that never loaded im-fcitx5.so would otherwise
        # read as a measurement of fcitx5 when it measured GTK's fallback.
        if [ "$CLIENT" = "gtk" ]; then
            grep -E '^(MODULE|GTK_IM_MODULE):' "$STAGE/commits.log" |
                sed 's/^/  /'
        fi
    fi
}

# ------------------------------------------------- a candidate-window case
# CANDIDATE MODE: the F9 path, measured where it actually draws. This is the
# tier D3 could not be: classicui loaded, so the candidate window is really
# drawn, and the abort in issue #45 -- classicui calling label() past the
# CommonCandidateList's 10-label vector -- is reachable here. Every other
# tier goes green with the cap reverted; this one must not.
#
# WHAT IS ASSERTED, and why each piece:
#   1. fcitx5 survives the keystrokes. This is the #45 abort: a page past the
#      label ceiling aborts fcitx5 with "CommonCandidateList: invalid label
#      idx" (SIGABRT). kill -0 after injection is the whole oracle for that.
#   2. The input window EXISTS and is drawn. A composition without a candidate
#      list is a one-character preedit window (measured ~27px tall); the F9
#      window with candidates is a tall column (measured 300-706px). The
#      geometry check is what makes "fcitx5 survived" mean "the window opened",
#      rather than "nothing crashed because nothing was drawn".
#   3. The client transcript matches the expectation, when the case carries
#      one. The candidate window's behaviour after opening -- Return commits
#      the selected Hanja, Page_Down pages, Escape closes -- is the same
#      COMMIT stream the ordinary D3 expectation describes, so an expectation
#      column in the candidate case file is compared exactly as in the XIM
#      cases. Absent expectation = geometry + survival only.
#
# The expectation is derived from hunmin-sim like every D3 case, BUT the
# candidate file's keys include {F9} and the sim does not know F9. So the
# expectation column must be written by the case author against the intended
# commit, and the injection derives nothing: the file's own mode column
# selects whether an expectation is checked. This is deliberate -- a sim
# trace for a sequence the sim cannot type would be a fiction.
run_candidate_case() { # $1=layout $2=keys $3=expectation text or empty
    local layout=$1 keys=$2 expect=${3:-}
    local hpid i window actual case_id=$CASE_SEQ
    CASE_SEQ=$((CASE_SEQ + 1))

    for _ in 1 2 3; do
        "${HARNESS_CMD[@]}" </dev/null >"$STAGE/commits.log" 2>"$STAGE/harness.log" &
        hpid=$!
        for i in $(seq 1 50); do
            grep -q READY "$STAGE/commits.log" 2>/dev/null && break
            kill -0 "$hpid" 2>/dev/null || break
            sleep 0.1
        done
        grep -q READY "$STAGE/commits.log" 2>/dev/null && break
        kill "$hpid" 2>/dev/null
        wait "$hpid" 2>/dev/null
        sleep 0.3
        hpid=""
    done
    if [ -z "$hpid" ]; then
        echo "FAIL [candidate/$layout/$keys]: harness never became ready"
        fail=1
        return
    fi
    sleep 0.4

    if ! python3 "$TYPER" "$DISP" "$keys" </dev/null; then
        echo "FAIL [candidate/$layout/$keys]: key injection failed"
        kill "$hpid" 2>/dev/null
        wait "$hpid" 2>/dev/null
        fail=1
        return
    fi
    # F9 opens the window asynchronously (fcitx5 -> addon -> libhunmin ->
    # classicui draws); give it the same settle the repro script measured.
    sleep 1.5

    # The window geometry oracle: xwininfo -root -children lists every
    # toplevel, and classicui's window is named "Fcitx5 Input Window".
    # One-character preedit vs. a candidate column differ by an order of
    # magnitude in height, so "is there a window at all" is answerable.
    window=$(xwininfo -root -children -display "$DISP" 2>/dev/null |
        grep -o '"[^"]*Input Window[^"]*"[^+]*' | head -1 || true)

    # THE ABORT ORACLE. With the #45 cap reverted and a page past the label
    # ceiling, fcitx5 has already died by this point: classicui called
    # label() past the vector, std::invalid_argument went uncaught, SIGABRT.
    # kill -0 is therefore the assertion the whole tier exists for, and it is
    # checked BEFORE anything reads the window: a dead fcitx5 has torn down
    # its X connection, so xwininfo reports nothing either way.
    if ! kill -0 "$FCITX_PID" 2>/dev/null; then
        echo "FAIL [candidate/$layout/$keys]: fcitx5 DIED during the F9 path"
        echo "  --- fcitx5.log tail ---"
        tail -30 "$STAGE/fcitx5.log" 2>/dev/null | sed 's/^/  /'
        wait "$hpid" 2>/dev/null
        fail=1
        return
    fi

    if [ -z "$window" ]; then
        echo "FAIL [candidate/$layout/$keys]: fcitx5 is alive but no input"
        echo "  window was drawn after the candidate keys -- the window is the"
        echo "  thing this tier exists to draw, so 'alive but nothing drawn'"
        echo "  means the F9 path was not exercised."
        echo "  --- fcitx5.log tail ---"
        tail -30 "$STAGE/fcitx5.log" 2>/dev/null | sed 's/^/  /'
        kill "$hpid" 2>/dev/null
        wait "$hpid" 2>/dev/null
        fail=1
        return
    fi

    kill -TERM "$hpid" 2>/dev/null
    wait "$hpid" 2>/dev/null
    actual=$(grep -E '^(COMMIT|FORWARD):' "$STAGE/commits.log" || true)

    if [ -n "$expect" ]; then
        if [ "$actual" != "$expect" ]; then
            echo "FAIL [candidate/$layout/$keys]"
            echo "  expected: $(printf '%s' "$expect" | paste -sd'|')"
            echo "  actual:   $(printf '%s' "$actual" | paste -sd'|')"
            fail=1
            return
        fi
    fi
    echo "ok: [candidate/$layout/$keys] window drawn ($window), fcitx5 alive${actual:+ -> $(printf '%s' "$actual" | paste -sd'|')}"
}

run_edit_handwritten_case() { # $1=layout $2=keys $3=expected transcript
    local layout=$1 keys=$2 expect=$3 hpid i actual case_id=$CASE_SEQ
    CASE_SEQ=$((CASE_SEQ + 1))

    for _ in 1 2 3; do
        "${HARNESS_CMD[@]}" </dev/null >"$STAGE/commits.log" 2>"$STAGE/harness.log" &
        hpid=$!
        for i in $(seq 1 50); do
            grep -q READY "$STAGE/commits.log" 2>/dev/null && break
            kill -0 "$hpid" 2>/dev/null || break
            sleep 0.1
        done
        grep -q READY "$STAGE/commits.log" 2>/dev/null && break
        kill "$hpid" 2>/dev/null
        wait "$hpid" 2>/dev/null
        sleep 0.3
        hpid=""
    done
    if [ -z "$hpid" ]; then
        echo "FAIL [edit/$layout/$keys]: harness never became ready"
        fail=1
        return
    fi
    if ! python3 "$TYPER" "$DISP" "$keys" </dev/null; then
        echo "FAIL [edit/$layout/$keys]: key injection failed"
        kill "$hpid" 2>/dev/null
        wait "$hpid" 2>/dev/null
        fail=1
        return
    fi
    sleep 0.2
    actual=$(grep -E '^(COMMIT|FORWARD):' "$STAGE/commits.log" || true)
    if [ "$actual" != "$expect" ]; then
        echo "FAIL [edit/$layout/$keys]"
        echo "  expected: $(printf '%s' "$expect" | paste -sd'|')"
        echo "  actual:   $(printf '%s' "$actual" | paste -sd'|')"
        echo "  --- client transcript ---"
        cat "$STAGE/commits.log" | sed 's/^/  /'
        fail=1
    else
        echo "ok: [edit/$layout/$keys] -> $actual"
    fi
    kill -TERM "$hpid" 2>/dev/null
    wait "$hpid" 2>/dev/null
}

# ---------------------------------------------------------------- run
cur_layout=""
HPID=""          # live GTK harness pid for the current layout group; empty = none
CASE_SEQ=0       # monotonic case id, used as the harness marker
# The third column selects the expectation. Empty means the original
# COMMIT-only rule, which is the stronger claim when it holds: the client sees
# exactly what the engine committed and nothing else. `stream` is the weaker
# one, used only where a pass-through key is part of the case -- it expects the
# forwarded character too, so it can no longer detect a pass-through key that
# SHOULD NOT have arrived. Both are checked; they differ in what they are able
# to fail on, which is why a pass-through case is not simply the default.
#
# The case list is read on fd 3, NOT on stdin. Everything this loop starts --
# fcitx5, hunmin-sim, the client harness, the injector -- inherits fd 0, and any
# process that reads it takes a bite out of the case list, which ends the loop
# early with no error anywhere: the tier reports green having run one case.
# That is not hypothetical. engine-d3-x11 ran exactly one of its eight cases
# from 8de61b1 until 2026-10-10: xim-harness.c polled stdin and drained it 256
# bytes at a time, and cases.tsv was on stdin because the loop redirected it
# there. See xim-harness.c and the harness note in gtk-harness.py.
#
# EDIT CASES (HUNMIN_D3_EDIT_CASES): an additional case file for edit-key
# scenarios -- backspace, mode toggle, candidate window. Same shape as
# cases.tsv, but keys may include {BackSpace}, {Return}, {F9}, {Escape}
# and ^ (mode toggle). All edit cases use stream mode because edit keys
# are pass-through.
# After a layout change fcitx5 has just been restarted. start_fcitx's gate has
# already proved readiness (~1.2 s measured 2026-11-04: @server=fcitx and the
# xim addon appear in the same poll), so an XIM client needs no further settle;
# GTK keeps its second, because a reused harness wires the IMContext per case and
# gtk3/gtk4 skip on the host that measured this. The candidate loop below keeps
# its own 1.0 s: its window settle was not re-measured.
if [ "$CLIENT" = "gtk" ]; then layout_settle=1.0; else layout_settle=0.1; fi
exec 3<"$CASES"
if [ "$CLIENT" = "candidate" ]; then
    # CANDIDATE MODE: a dedicated case file, because its columns are not the
    # ordinary ones -- the keys contain {F9} (which hunmin-sim cannot type,
    # so no expectation is derived), the third column is the EXPECTED COMMIT
    # text written by hand against the intended behaviour, and an empty
    # expectation means "geometry + survival only".
    while IFS=$'\t' read -r -u 3 layout keys expect _; do
        case "$layout" in '' | \#*) continue ;; esac
        if [ "$layout" != "$cur_layout" ]; then
            stop_fcitx
            if ! start_fcitx "$layout"; then
                fail=1
                exec 3<&-
                exit 1
            fi
            cur_layout="$layout"
            sleep 1.0
        fi
        run_candidate_case "$layout" "$keys" "$expect"
    done
    exec 3<&-
    stop_fcitx
    exit "$fail"
fi
if [ -n "${HUNMIN_D3_EDIT_CASES:-}" ] && [ -f "${HUNMIN_D3_EDIT_CASES}" ]; then
    exec 4<"$HUNMIN_D3_EDIT_CASES"
    while IFS=$'\t' read -r -u 4 layout keys mode expect; do
        case "$layout" in '' | \#*) continue ;; esac
        if [ "$layout" != "$cur_layout" ]; then
            stop_fcitx
            if ! start_fcitx "$layout"; then
                fail=1
                exec 3<&-
                exec 4<&-
                exit 1
            fi
            cur_layout="$layout"
            sleep "$layout_settle"
        fi
        if [ -n "$expect" ]; then
            run_edit_handwritten_case "$layout" "$keys" "$expect"
        else
            run_case "$layout" "$keys" expected_stream
        fi
    done
    exec 4<&-
    # STOP HERE. The ordinary cases.tsv run below is NOT repeated in edit mode:
    # engine-d3-x11 runs that same file through this same stack and client, so
    # running it here measured 43.6s of the tier's 93.3s (2026-11-04,
    # docs/D3-PERFORMANCE.md) with no assertion the x11 tier does not already
    # make. The CMake comment for this tier says the edit cases are separate
    # "so a failure is diagnosed by its scenario, not mixed into the ordinary
    # D3 cases" -- running both here contradicted that.
    if [ -n "$HPID" ]; then
        kill "$HPID" 2>/dev/null
        wait "$HPID" 2>/dev/null
    fi
    stop_fcitx
    exit "$fail"
fi
while IFS=$'\t' read -r -u 3 layout keys mode _; do
    case "$layout" in '' | \#*) continue ;; esac
    if [ "$layout" != "$cur_layout" ]; then
        # Leaving a layout group: the group's GTK harness belongs to the old
        # fcitx5, and a restarted daemon drops its input context, so it must
        # not be reused. (HPID is only ever the GTK client's; the XIM client
        # starts a fresh harness per case and has nothing to tear down here.)
        if [ -n "$HPID" ]; then
            kill "$HPID" 2>/dev/null
            wait "$HPID" 2>/dev/null
            HPID=""
        fi
        stop_fcitx
        if ! start_fcitx "$layout"; then
            fail=1
            exec 3<&-
            exit 1
        fi
        cur_layout="$layout"
        sleep "$layout_settle"
    fi
    if [ "$mode" = "stream" ]; then
        run_case "$layout" "$keys" expected_stream
    else
        run_case "$layout" "$keys"
    fi
done
exec 3<&-

if [ -n "$HPID" ]; then
    kill "$HPID" 2>/dev/null
    wait "$HPID" 2>/dev/null
fi
stop_fcitx
exit "$fail"
