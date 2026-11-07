#!/usr/bin/env bash
# host-env.sh — the host facts the build scripts kept re-deriving, in one
# place. Source it; do not execute it.
#
#   . "$(dirname "$0")/host-env.sh"
#
# What it owns, and why each part exists (all measured 2026-11-07 on the
# Windows host, see PLAN-0.3-CHECK.md §"Host findings"):
#
#  1. hunmin_load_env <file>   — `~/.hunmin-env` is plain KEY=value, so a
#     bare `source` leaves the variables unexported and every child process
#     (tools/build-android.sh, dpkg-buildpackage, gradlew) sees an empty
#     ANDROID_NDK. Loading it with `set -a` is the difference between "looks
#     configured" and "is configured".
#  2. hunmin_native_path <p>   — MSYS does not convert an argument for a
#     native program when the calling shell disabled it (MSYS_NO_PATHCONV=1,
#     MSYS2_ARG_CONV_EXCL=*). adb/python3/cmake/ninja then receive `/c/...`
#     verbatim and fail with "cannot stat" or `C:\c\Workspace\...`. cygpath
#     -m is what tools/build-android.sh already does for cmake/ninja.
#  3. hunmin_jdk_bin            — JAVA_HOME is stored in C:/ form, which MSYS
#     cannot use as a PATH entry. Native JVM consumers want C:/..., the shell
#     wants /c/.../bin.
#  4. hunmin_linux_host / hunmin_remote_*  — the dedicated Linux box. Unset
#     HUNMIN_LINUX_HOST keeps every caller on its previous path.
#
# Nothing here installs anything, and nothing writes outside the repo except
# the dirs the caller names.

# --- 1. environment ---------------------------------------------------------------

# Load KEY=value files with export semantics, in order — later files win.
# No-op for a file that is absent.
#
#   hunmin_load_env                          # ~/.hunmin-env only
#   hunmin_load_env "$HOME/.hunmin-env" "$ROOT/.env"
#
# The second form is what the build scripts use: `~/.hunmin-env` carries the
# host's toolchain paths, and an optional, gitignored `.env` in the checkout
# carries the per-worktree choice (`HUNMIN_LINUX_HOST`). Host facts stay out
# of the tree; the choice of box stays with the checkout.
hunmin_load_env() {
    local f
    for f in "$@"; do
        [ -n "$f" ] || continue
        [ -f "$f" ] || continue
        set -a
        # shellcheck disable=SC1090  # path is the caller's, by design
        . "$f"
        set +a
    done
    if [ "$#" -eq 0 ] && [ -f "$HOME/.hunmin-env" ]; then
        set -a
        # shellcheck disable=SC1091
        . "$HOME/.hunmin-env"
        set +a
    fi
}

# --- 2/3. platform ---------------------------------------------------------------

hunmin_is_msys() {
    case "$(uname -s 2>/dev/null || echo unknown)" in
        MSYS*|MINGW*|CYGWIN*) return 0 ;;
        *) return 1 ;;
    esac
}

# Native form of a path for a program that cannot read /c/... — identity
# everywhere cygpath does not exist.
hunmin_native_path() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -m "$1"
    else
        printf '%s\n' "$1"
    fi
}

# The same path as the WSL guest sees it. wsl.exe cannot use C:/… and the
# guest has no cygpath for the Windows side, so the drive letter is mapped
# directly: C:/Workspace/x -> /mnt/c/Workspace/x.
hunmin_wsl_path() {
    local p=$1
    command -v cygpath >/dev/null 2>&1 && p=$(cygpath -m "$p")
    printf '%s\n' "$p" | sed -e 's|^\([A-Za-z]\):|/mnt/\L\1|'
}

# Directory holding java/keytool as the SHELL can reach it, or empty.
# Measured: a `C:/…/bin` entry in PATH is not resolvable at all — bash treats
# the string as a pattern, not a mount, so `command -v java` reports nothing
# while `[ -x C:/…/bin/java ]` succeeds. The POSIX form is the only one that
# works in PATH; the C:/ form stays for the JVM's own config.
hunmin_jdk_bin() {
    local jh=${JAVA_HOME:-} p
    [ -n "$jh" ] || return 0
    p=$jh
    if command -v cygpath >/dev/null 2>&1; then
        p=$(cygpath -u "$jh" 2>/dev/null) || p=$jh
    fi
    if [ -x "$p/bin/java" ]; then
        printf '%s\n' "$p/bin"
    elif [ -x "$jh/bin/java" ]; then
        printf '%s\n' "$jh/bin"
    fi
}

# Reptile-proof PATH entry for the JDK: idempotent, and silent when there is
# no JDK to add.
hunmin_use_jdk() {
    local bin
    bin=$(hunmin_jdk_bin) || true
    [ -n "$bin" ] || return 0
    case ":$PATH:" in
        *":$bin:"*) ;;
        *) PATH="$bin:$PATH" ;;
    esac
    export PATH
}

# --- 4. the dedicated Linux box ---------------------------------------------------

# SSH target of the box, or empty. HUNMIN_LINUX_HOST=scoby@192.168.219.112
hunmin_linux_host() {
    printf '%s\n' "${HUNMIN_LINUX_HOST:-}"
}

# Remote directory the source is copied into (kept, not wiped on failure).
# Absolute, because a quoted `~` in a remote command does not expand.
hunmin_linux_dir() {
    local home=${1:?hunmin_linux_dir needs the remote home}
    if [ -n "${HUNMIN_LINUX_DIR:-}" ]; then
        printf '%s\n' "$HUNMIN_LINUX_DIR"
    else
        printf '%s\n' "$home/hunmin-build"
    fi
}

# The box's $HOME, asked rather than assumed (the account name decides it).
hunmin_remote_home() {
    hunmin_remote <<'EOF'
printf '%s\n' "$HOME"
EOF
}

hunmin_ssh_opts() {
    # BatchMode first: a prompt inside a build script is a hung build. The key
    # is installed once by the operator (see the pubkey printed on failure).
    printf '%s\n' "-o BatchMode=yes" "-o StrictHostKeyChecking=accept-new" \
                  "-o ServerAliveInterval=30" "-o ConnectTimeout=10"
}

# hunmin_remote <<'EOF' ... EOF   -- run a script on the box, stdin is the script.
hunmin_remote() {
    local host
    host=$(hunmin_linux_host)
    [ -n "$host" ] || { echo "host-env: HUNMIN_LINUX_HOST is not set" >&2; return 2; }
    # shellcheck disable=SC2086  # opts are deliberately word-split
    ssh $(hunmin_ssh_opts) "$host" 'bash -l -s'
}

# hunmin_remote_sync <local-src-dir> <remote-dir>
# rsync is NOT installed on the Windows host (measured), so the transfer is
# tar straight over the ssh connection: the remote command is an argument,
# stdin is the archive. Excludes mirror the WSL script's.
hunmin_remote_sync() {
    local src=$1 dest=$2 host
    host=$(hunmin_linux_host)
    [ -n "$host" ] || { echo "host-env: HUNMIN_LINUX_HOST is not set" >&2; return 2; }
    # shellcheck disable=SC2086
    tar -C "$src" -czf - \
        --exclude=./build --exclude=./build-* --exclude=./.git \
        --exclude=./.cache --exclude=./node_modules \
        --exclude=./frontends/android/.gradle \
        --exclude=./frontends/android/app/build \
        --exclude=./frontends/android/testeditor/build \
        --exclude=./frontends/android/keymaps/build \
        --exclude=./frontends/tsf/build-msvc \
        . \
      | ssh $(hunmin_ssh_opts) "$host" \
            "mkdir -p '$dest' && tar -xzf - -C '$dest'"
}

# Explain a failed connection instead of leaving a bare ssh error. Prints the
# command the operator must run once (on the box) to authorise this checkout.
hunmin_remote_diagnose() {
    local host pub=""
    host=$(hunmin_linux_host)
    [ -f "$HOME/.ssh/id_ed25519.pub" ] && pub=$(cat "$HOME/.ssh/id_ed25519.pub")
    {
        echo "host-env: cannot reach '$host' non-interactively."
        echo "  test:  ssh -o BatchMode=yes $host true"
        if [ -n "$pub" ]; then
            echo "  fix:   install this key on the box (once), then re-run:"
            echo "           $pub"
            echo "         e.g.  ssh-copy-id -i ~/.ssh/id_ed25519.pub ${host#*@}"
        fi
    } >&2
}