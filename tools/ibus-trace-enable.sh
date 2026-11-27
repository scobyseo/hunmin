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
#   - until `disable`, every key goes through a LOCALLY BUILT engine;
#   - the packaged binary is kept at /usr/bin/hunmin-ibus.pkg and `disable`
#     restores it (and refuses to, if the package has been upgraded since);
#   - the log is theirs: mode 600 under ~/.local/state, never in the repository.
#
# WHAT SURVIVES A REBOOT, and what does not (checked 2026-11-27):
#   survives  /usr/bin/hunmin-ibus (the wrapper), /usr/local/lib/hunmin/{the
#             engine, libhunmin.so.0}, and the session's ibus start, which reads
#             the same component XML. Logging resumes by itself after a boot.
#   survives  the log directory; each engine process gets its OWN file
#             (key-log-<date>-<time>-<pid>.jsonl), so a reboot does not weld two
#             sessions into one stream.
#   NOT       an apt upgrade or reinstall of hunmin-ibus: dpkg overwrites
#             /usr/bin/hunmin-ibus, the wrapper disappears, logging stops and the
#             input method is simply the packaged one again. Re-run `enable`.
#   NOT       the build tree: the engine is installed with its own libhunmin and
#             the wrapper puts that directory first, because the build has a
#             RUNPATH into the checkout and a deleted worktree would otherwise
#             leave the user with NO input method. `status` checks both copies.
#
# The zero-risk alternative, no root and no package touched, is a private
# session: cmake -B build-trace -DHUNMIN_TRACE=ON, then
# tools/ibus-real-use.sh portable — it copies the trace engine itself and sets
# the same variables.
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
TRACE_BUILD="$ROOT/build-trace/frontends/ibus/hunmin-ibus"
TRACE_LIB="$ROOT/build-trace/libhunmin.so.0"
INSTALL_DIR=/usr/local/lib/hunmin
INSTALL_BIN="$INSTALL_DIR/hunmin-ibus-trace"
INSTALL_LIB="$INSTALL_DIR/libhunmin.so.0"
ENGINE=/usr/bin/hunmin-ibus
BACKUP=/usr/bin/hunmin-ibus.pkg
LOG_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/hunmin"
MARK="# hunmin trace wrapper"

say() { printf '%s\n' "$*"; }
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

is_wrapper() { [ -f "$ENGINE" ] && grep -q -- "$MARK" "$ENGINE" 2>/dev/null; }

# Does the backup still match what dpkg installed? False after a package upgrade
# or reinstall, and then restoring it would DOWNGRADE the engine.
package_intact() {
    sums=/var/lib/dpkg/info/hunmin-ibus.md5sums
    [ -f "$sums" ] || return 1
    wanted=$(awk '$2=="usr/bin/hunmin-ibus"{print $1}' "$sums")
    [ -n "$wanted" ] || return 1
    [ "$(md5sum <"$BACKUP" | cut -d' ' -f1)" = "$wanted" ]
}

case "${1:-}" in
status)
    say "engine binary: $ENGINE"
    if is_wrapper; then
        say "  state: TRACE WRAPPER installed"
        if package_intact; then
            say "  backup: $BACKUP matches the packaged binary"
        else
            say "  backup: $BACKUP does NOT match dpkg — reinstall the package before"
            say "          relying on 'disable': apt-get install --reinstall hunmin-ibus"
        fi
        say "  log dir: $LOG_DIR"
        if ls -1 "$LOG_DIR"/key-log-*.jsonl >/dev/null 2>&1; then
            for f in $(ls -1t "$LOG_DIR"/key-log-*.jsonl | head -3); do
                say "    $(wc -l <"$f") lines  $(basename "$f")"
            done
        else
            say "    no log yet (type something, or check that the engine is active)"
        fi
    else
        say "  state: the packaged engine (no personal key log)"
    fi
    say "trace build: $TRACE_BUILD"
    [ -x "$TRACE_BUILD" ] && say "  built: yes" \
        || say "  built: NO — cmake -B build-trace -DHUNMIN_TRACE=ON && cmake --build build-trace"
    say "  libhunmin: $TRACE_LIB $([ -f "$TRACE_LIB" ] && echo '(present)' || echo '(MISSING)')"
    say "installed copies (what a reboot will use):"
    for f in "$INSTALL_BIN" "$INSTALL_LIB"; do
        if [ -f "$f" ]; then say "  present  $f"; else say "  absent   $f"; fi
    done
    say "packaged binary kept at: $BACKUP $([ -f "$BACKUP" ] && echo '(present)' || echo '(none yet)')"
    exit 0
    ;;
enable)
    [ "$(id -u)" -eq 0 ] || die "run as root: sudo $0 enable"
    [ -x "$TRACE_BUILD" ] || die "no trace build at $TRACE_BUILD — build it first (see status)"
    [ -f "$TRACE_LIB" ] || die "no $TRACE_LIB — the trace build links libhunmin; build it"
    is_wrapper && { say "already enabled"; exit 0; }
    [ -f "$ENGINE" ] || die "$ENGINE is missing; is hunmin-ibus installed?"
    if [ ! -f "$BACKUP" ]; then
        cp -p "$ENGINE" "$BACKUP"
        say "kept the packaged engine at $BACKUP"
    fi
    install -d -m 0755 "$INSTALL_DIR"
    install -m 0755 "$TRACE_BUILD" "$INSTALL_BIN"
    # The engine's RUNPATH points into the checkout; install the matching
    # libhunmin beside it so the checkout can go away.
    install -m 0644 "$TRACE_LIB" "$INSTALL_LIB"
    install -d -m 0700 "$LOG_DIR" || true
    if [ -n "${SUDO_USER:-}" ]; then
        chown "$SUDO_USER" "$LOG_DIR" 2>/dev/null || true
    fi
    # The wrapper is the only way to give the engine an environment: IBus starts
    # it with the daemon's, and a running daemon cannot be re-inherited.
    cat >"$ENGINE" <<WRAPPER
#!/bin/sh
$MARK — installed by tools/ibus-trace-enable.sh; 'disable' restores the packaged
# engine from $BACKUP. One file per engine process, mode 600, under
# \$HOME/.local/state: a reboot or an 'ibus restart' starts a new process, and
# welding two sessions into one stream would let the analyser join a slot from
# one day with a slot from the next.
dir="\$HOME/.local/state/hunmin"
[ -d "\$dir" ] || mkdir -p "\$dir" || exit 1
# If the installed engine is gone (a cleaned /usr/local), fall back to the
# packaged one rather than leaving the person without an input method.
[ -x $INSTALL_BIN ] || exec $BACKUP "\$@"
umask 077
: "\${HUNMIN_TRACE_OUT:=\$dir/key-log-\$(date +%Y%m%d-%H%M%S)-\$\$.jsonl}"
export HUNMIN_TRACE_OUT
export HUNMIN_TRACE_RAW=1
LD_LIBRARY_PATH="$INSTALL_DIR\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH
exec $INSTALL_BIN "\$@"
WRAPPER
    chmod 0755 "$ENGINE"
    say "installed $INSTALL_BIN + $(basename "$INSTALL_LIB") beside it, wrapped $ENGINE"
    say "log: $LOG_DIR/key-log-<date>-<time>-<pid>.jsonl (mode 600, one file per engine process)"
    say ""
    say "survives a reboot: yes — the wrapper and the installed engine persist, and the"
    say "  session's ibus start reads the same component XML."
    say "does NOT survive: an apt upgrade/reinstall of hunmin-ibus overwrites the"
    say "  wrapper (logging stops, the packaged engine comes back) — re-run enable."
    say ""
    say "NEXT, as your own user, once:  ibus restart"
    say "  (engines are spawned per input context; without the restart the old"
    say "   process keeps running and nothing is logged)"
    exit 0
    ;;
disable)
    [ "$(id -u)" -eq 0 ] || die "run as root: sudo $0 disable"
    if is_wrapper; then
        [ -f "$BACKUP" ] || die "$BACKUP is missing — reinstall instead: apt-get install --reinstall hunmin-ibus"
        if package_intact; then
            install -m 0755 "$BACKUP" "$ENGINE"
            rm -f "$INSTALL_BIN" "$INSTALL_LIB"
            say "restored the packaged engine from $BACKUP and removed the installed copies"
        else
            say "the package changed since 'enable' (or the backup no longer matches"
            say "dpkg's md5), so restoring the backup would DOWNGRADE the engine."
            say "Restore from the package instead:"
            say "  apt-get install --reinstall hunmin-ibus"
            say "then delete $INSTALL_BIN and $INSTALL_LIB by hand."
            exit 1
        fi
    else
        say "not enabled; nothing to restore"
    fi
    say "NEXT, as your own user, once:  ibus restart"
    say "the logs stay in $LOG_DIR (delete them yourself if you do not want them)"
    exit 0
    ;;
*)
    say "usage: $0 status | enable | disable"
    exit 2
    ;;
esac