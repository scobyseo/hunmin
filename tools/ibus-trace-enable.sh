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
# Every file this script installs is written beside its target and renamed over
# it: a plain write fails with ETXTBSY while an engine is running (measured
# 2026-11-27 on the first real run), and the running engine keeps the old inode
# either way.
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
#   NOT       a dev build's default data dir: libhunmin compiled with prefix
#             /usr/local looks in /usr/local/share/hunmin, which has no layouts,
#             and an engine with no layouts declines EVERY key. The wrapper
#             exports HUNMIN_DATA_PATH (TRACE_DATA) for that reason, and `enable`
#             refuses to install without a data directory that has layouts/.
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
# HOME is root's under sudo/pkexec; the log belongs to the person who ran this.
if [ -n "${SUDO_USER:-}" ]; then
    TARGET_HOME=$(getent passwd "$SUDO_USER" | cut -d: -f6)
elif [ -n "${PKEXEC_UID:-}" ]; then
    TARGET_HOME=$(getent passwd "$PKEXEC_UID" | cut -d: -f6)
else
    TARGET_HOME=$HOME
fi
LOG_DIR="${XDG_STATE_HOME:-$TARGET_HOME/.local/state}/hunmin"
# Where the DATA is. A dev build's libhunmin defaults HUNMIN_DEFAULT_DATA_DIR to
# its own install prefix (/usr/local/share/hunmin), which does not exist here —
# and with no layouts the engine builds no state and declines EVERY key, i.e. the
# person cannot type at all (measured 2026-11-27, this exact failure). The
# installed data is the package's, so the wrapper points at it.
TRACE_DATA=${HUNMIN_TRACE_DATA:-/usr/share/hunmin}
MARK="# hunmin trace wrapper"

say() { printf '%s\n' "$*"; }
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

# Install a file WITHOUT writing through the target: a running engine (and a
# mapped shared library) refuses an in-place write with ETXTBSY, which is what
# `cat > /usr/bin/hunmin-ibus` did on the first real run. Writing beside the
# target and renaming over it replaces the directory entry; the process that is
# already running keeps the old inode and is unaffected.
#   put SRC MODE DEST
put() {
    src=$1; mode=$2; dest=$3
    tmp="$dest.new.$$"
    cp -f "$src" "$tmp" && chmod "$mode" "$tmp" && mv -f "$tmp" "$dest" && return 0
    rm -f "$tmp"
    return 1
}

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
    [ -d "$TRACE_DATA/layouts" ] || die "no layouts in $TRACE_DATA — install hunmin-data or pass HUNMIN_TRACE_DATA=<dir with layouts/>"
    # NOT a shortcut when a wrapper is already installed: an earlier version of
    # this script installed one too, and "already enabled" then silently kept the
    # old wrapper — which is exactly how a fixed wrapper failed to reach the
    # machine (2026-11-28). Re-installing is idempotent: put() renames and the
    # preflight validates.
    [ -f "$ENGINE" ] || die "$ENGINE is missing; is hunmin-ibus installed?"
    if is_wrapper; then
        say "replacing the installed wrapper"
    elif [ -f "$BACKUP" ]; then
        say "keeping the packaged engine already saved at $BACKUP"
    else
        cp -p "$ENGINE" "$BACKUP"
        say "kept the packaged engine at $BACKUP"
    fi
    install -d -m 0755 "$INSTALL_DIR"
    # A re-enable runs while the installed engine may still be alive: put()
    # renames instead of writing through it (see its comment).
    put "$TRACE_BUILD" 0755 "$INSTALL_BIN" || die "cannot install $INSTALL_BIN"
    # The engine's RUNPATH points into the checkout; install the matching
    # libhunmin beside it so the checkout can go away.
    put "$TRACE_LIB" 0644 "$INSTALL_LIB" || die "cannot install $INSTALL_LIB"
    install -d -m 0700 "$LOG_DIR" || true
    if [ -n "${SUDO_USER:-}" ]; then
        chown "$SUDO_USER" "$LOG_DIR" 2>/dev/null || true
    elif [ -n "${PKEXEC_UID:-}" ]; then
        chown "$PKEXEC_UID" "$LOG_DIR" 2>/dev/null || true
    fi
    # The wrapper is the only way to give the engine an environment: IBus starts
    # it with the daemon's, and a running daemon cannot be re-inherited.
    wrapper_tmp="$ENGINE.new.$$"
    cat >"$wrapper_tmp" <<WRAPPER
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
# Without this the engine finds no layouts and silently declines every key: the
# lib is a /usr/local build whose compiled default data dir is empty here.
: "\${HUNMIN_DATA_PATH:=$TRACE_DATA}"
export HUNMIN_DATA_PATH
LD_LIBRARY_PATH="$INSTALL_DIR\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH
exec $INSTALL_BIN "\$@"
WRAPPER
    chmod 0755 "$wrapper_tmp"
    mv -f "$wrapper_tmp" "$ENGINE" || die "cannot install the wrapper at $ENGINE"
    # Preflight the thing that broke the first time: the installed lib must find
    # layouts in the wrapper's environment. No bus needed -- hunmin_ctx_new is
    # the engine's own first call and returns NULL with no layouts.
    if command -v python3 >/dev/null 2>&1; then
        if HUNMIN_DATA_PATH="$TRACE_DATA" LD_LIBRARY_PATH="$INSTALL_DIR" python3 - "$INSTALL_LIB" <<'PYCHECK'
import ctypes, sys
lib = ctypes.CDLL(sys.argv[1])
lib.hunmin_ctx_new.restype = ctypes.c_void_p
lib.hunmin_ctx_new.argtypes = [ctypes.POINTER(ctypes.c_char_p), ctypes.c_int]
sys.exit(0 if lib.hunmin_ctx_new(None, 0) else 1)
PYCHECK
        then
            say "preflight: the installed engine loads layouts from $TRACE_DATA"
        else
            # Never leave a wrapper that would eat every key: put the packaged
            # engine back before failing.
            if [ -f "$BACKUP" ]; then
                put "$BACKUP" 0755 "$ENGINE" && say "preflight failed — the packaged engine was restored"
            fi
            die "preflight FAILED: the installed lib finds no layouts in $TRACE_DATA (the engine would decline every key)"
        fi
    else
        say "preflight skipped (python3 not found) — if typing stops, run: $0 disable"
    fi
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
            put "$BACKUP" 0755 "$ENGINE" || die "cannot restore $ENGINE (is it busy?)"
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