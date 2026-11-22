#!/bin/sh
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# "Am I using the hunmin IBus engine right now?" — answered from the system,
# not from what we believe we installed.
#
#   tools/ibus-engine-status.sh
#
# It reports, in order:
#   1. the installed packages,
#   2. the two files that make the engine exist at all (component XML, schema),
#   3. whether the LIVE daemon knows the engine — the daemon reads the component
#      registry at startup, so "installed but not listed" means it needs a
#      restart, which is the usual answer to "I installed it and nothing changed",
#   4. which engine the session has ACTIVE, and whether an engine process exists,
#   5. the five stored rows, and whether the fifth one has already moved keys out
#      of the daemon's global trigger list (docs/OPTIONS-MEASURED.md §IBC-4).
#
# Read-only: it never writes the store and never restarts anything.
set -u

say() { printf '%s\n' "$*"; }
hdr() { printf '\n== %s\n' "$*"; }

hdr "packages"
dpkg -l libhunmin hunmin-data hunmin-ibus hunmin-ibus-settings 2>/dev/null |
    awk '/^ii/ {printf "  %-22s %s\n", $2, $3}' |
    grep . || say "  none installed"

hdr "files"
for f in /usr/share/ibus/component/hunmin.xml \
         /usr/share/glib-2.0/schemas/org.freedesktop.ibus.engine.hunmin.gschema.xml \
         /usr/bin/hunmin-ibus /usr/bin/hunmin-ibus-settings; do
    [ -e "$f" ] && printf '  ok      %s\n' "$f" || printf '  MISSING %s\n' "$f"
done
# The schema is only usable once glib's dpkg trigger has compiled the database.
if gsettings list-schemas 2>/dev/null | grep -qx org.freedesktop.ibus.engine.hunmin; then
    say "  schema  compiled and visible to gsettings"
else
    say "  schema  NOT compiled (pakage files present? run: glib-compile-schemas /usr/share/glib-2.0/schemas)"
fi

hdr "daemon"
# The address file is how the ibus tools find a running daemon; it is written per
# display, so the newest one is the live session.
addrfile=$(ls -t "${XDG_CONFIG_HOME:-$HOME/.config}"/ibus/bus/* 2>/dev/null | head -1)
if [ -z "${addrfile:-}" ]; then
    say "  no ibus address file: no ibus daemon for this user"
    exit 0
fi
ibus_address=$(sed -n 's/^IBUS_ADDRESS=//p' "$addrfile")
daemon_pid=$(sed -n 's/^IBUS_DAEMON_PID=//p' "$addrfile")
say "  address file: $addrfile"
say "  daemon pid:   ${daemon_pid:-?}  (alive: $([ -n "${daemon_pid:-}" ] && kill -0 "$daemon_pid" 2>/dev/null && echo yes || echo no))"
[ -n "$ibus_address" ] && say "  address:      $ibus_address"

# The tools otherwise pick the address up from the environment or the file; both
# are set here so the answer cannot come from a different daemon.
IBUS_ADDRESS=$ibus_address
export IBUS_ADDRESS
unset DBUS_SESSION_BUS_ADDRESS

hdr "engines the LIVE daemon knows"
listed=$(ibus list-engine 2>/dev/null | grep -E '^ *hunmin ' | head -1)
if [ -n "$listed" ]; then
    say "  hunmin: LISTED"
else
    say "  hunmin: not listed — the daemon read its component registry at startup."
    say "          restart it to pick the engine up:  ibus restart"
    say "          (or log out and back in)"
fi

hdr "which engine is active"
active=$(ibus engine 2>&1 | head -1)
case "$active" in
    hunmin*) say "  ACTIVE: $active   <- keys are going to the hunmin engine" ;;
    "") say "  no engine reported" ;;
    *) say "  ACTIVE: $active   <- not hunmin" ;;
esac
if pgrep -f "^/usr/bin/hunmin-ibus --ibus" >/dev/null 2>&1; then
    say "  engine process: running (pid $(pgrep -f '^/usr/bin/hunmin-ibus --ibus' | tr '\n' ' '))"
else
    say "  engine process: none — an engine is spawned when an input context needs it"
fi

hdr "the five stored rows"
for row in hangul-layout latin-layout commit-unit autoswitch switch-keys; do
    v=$(gsettings get org.freedesktop.ibus.engine.hunmin "$row" 2>/dev/null)
    [ -n "$v" ] && printf '  %-14s %s\n' "$row" "$v"
done

hdr "the daemon's trigger list (what §IBC-4's fifth row edits)"
say "  $(gsettings get org.freedesktop.ibus.general.hotkey trigger 2>/dev/null)"
say "  taken by us: $(gsettings get org.freedesktop.ibus.engine.hunmin managed-triggers 2>/dev/null)"
say ""
say "  A key our row owns is removed from the daemon's list; a key it does not own"
say "  stays there and is consumed by the daemon before any engine sees it. So a"
say "  row holding only 'Alt_R' leaves 'Hangul' switching input methods on a kr"
say "  layout, where the physical key reports Hangul rather than Alt_R."