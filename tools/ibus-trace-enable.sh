#!/bin/sh
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Turn the IBus personal key log ON for THIS desktop, and off again.
#
#   tools/ibus-trace-enable.sh status
#   sudo tools/ibus-trace-enable.sh enable      # then: ibus restart
#   sudo tools/ibus-trace-enable.sh disable     # then: ibus restart
#
# Why a system file at all: IBus spawns the engine itself, so the engine's
# environment is the DAEMON's, and a trace build only logs when HUNMIN_TRACE_OUT
# is set (docs/trace-spec.md §"Personal key log"). There is no per-user
# component directory that a running daemon re-reads, and no way to inject an
# environment variable into a daemon this script did not start. So `enable`
# installs the trace engine under /usr/local and replaces /usr/bin/hunmin-ibus
# with a wrapper that sets the log path and execs it.
#
# What that costs, stated because it is the user's input method:
#   - until `disable`, every key of theirs goes through a LOCALLY BUILT engine,
#     not the packaged one;
#   - the packaged binary is kept at /usr/bin/hunmin-ibus.pkg and `disable`
#     restores it, so the rollback is one command;
#   - the log is theirs: mode 600 under ~/.local/state, never in the repository.
#
# The zero-risk alternative, no root and no package touched, is a private
# session: cmake -B build-trace -DHUNMIN_TRACE=ON, then
# tools/ibus-real-use.sh portable — it copies the trace engine itself and sets
# the same variables.
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
TRACE_BUILD="$ROOT/build-trace/frontends/ibus/hunmin-ibus"
INSTALL_BIN=/usr/local/lib/hunmin/hunmin-ibus-trace
ENGINE=/usr/bin/hunmin-ibus
BACKUP=/usr/bin/hunmin-ibus.pkg
LOG_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/hunmin"
LOG="$LOG_DIR/key-log.jsonl"
MARK="# hunmin trace wrapper"

say() { printf '%s\n' "$*"; }
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

is_wrapper() { [ -f "$ENGINE" ] && grep -q -- "$MARK" "$ENGINE" 2>/dev/null; }

case "${1:-}" in
status)
    say "engine binary: $ENGINE"
    if is_wrapper; then
        say "  state: TRACE WRAPPER installed"
        say "  log:   $LOG"
        [ -f "$LOG" ] && say "  log size: $(wc -c <"$LOG") bytes, $(wc -l <"$LOG") lines" \
                      || say "  log: not created yet (type something, or check that the engine is active)"
        say "  last line: $(tail -n 1 "$LOG" 2>/dev/null || echo '(none)')"
    else
        say "  state: the packaged engine (no personal key log)"
    fi
    say "trace build: $TRACE_BUILD"
    [ -x "$TRACE_BUILD" ] && say "  built: yes" || say "  built: NO — cmake -B build-trace -DHUNMIN_TRACE=ON && cmake --build build-trace"
    say "packaged binary kept at: $BACKUP $([ -f "$BACKUP" ] && echo '(present)' || echo '(none yet)')"
    exit 0
    ;;
enable)
    [ "$(id -u)" -eq 0 ] || die "run as root: sudo $0 enable"
    [ -x "$TRACE_BUILD" ] || die "no trace build at $TRACE_BUILD — build it first (see status)"
    is_wrapper && { say "already enabled"; exit 0; }
    [ -f "$ENGINE" ] || die "$ENGINE is missing; is hunmin-ibus installed?"
    if [ ! -f "$BACKUP" ]; then
        cp -p "$ENGINE" "$BACKUP"
        say "kept the packaged engine at $BACKUP"
    fi
    install -D -m 0755 "$TRACE_BUILD" "$INSTALL_BIN"
    install -d -m 0700 -o "${SUDO_USER:-$USER}" -g "$(id -gn "${SUDO_USER:-$USER}")" "$LOG_DIR" 2>/dev/null \
        || install -d -m 0700 "$LOG_DIR" || true
    # The wrapper is the only way to give the engine an environment: IBus starts
    # it with the daemon's, and the daemon cannot be re-inherited after the fact.
    cat >"$ENGINE" <<WRAPPER
#!/bin/sh
$MARK — installed by tools/ibus-trace-enable.sh; 'disable' restores the packaged
# engine from $BACKUP. The log is personal: mode 600 under \$HOME/.local/state.
: "\${HUNMIN_TRACE_OUT:=$LOG}"
export HUNMIN_TRACE_OUT
export HUNMIN_TRACE_RAW=1
umask 077
exec $INSTALL_BIN "\$@"
WRAPPER
    chmod 0755 "$ENGINE"
    say "installed $INSTALL_BIN and wrapped $ENGINE"
    say "log: $LOG (mode 600, created on the first key)"
    say ""
    say "NEXT, as your own user, once:  ibus restart"
    say "  (engines are spawned per input context; without the restart the old"
    say "   process keeps running and nothing is logged)"
    exit 0
    ;;
disable)
    [ "$(id -u)" -eq 0 ] || die "run as root: sudo $0 disable"
    if is_wrapper; then
        [ -f "$BACKUP" ] || die "$BACKUP is missing — reinstall the package instead: apt-get install --reinstall hunmin-ibus"
        install -m 0755 "$BACKUP" "$ENGINE"
        say "restored the packaged engine from $BACKUP"
    else
        say "not enabled; nothing to restore"
    fi
    say "NEXT, as your own user, once:  ibus restart"
    say "the log stays at $LOG (delete it yourself if you do not want it)"
    exit 0
    ;;
*)
    say "usage: $0 status | enable | disable"
    exit 2
    ;;
esac