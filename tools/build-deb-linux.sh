#!/usr/bin/env bash
# build-deb-linux.sh — build the release .deb on the dedicated Linux box.
#
# The third route to the same artifact:
#
#   Linux host (this box)   tools/build-release.sh deb      -> ../hunmin_*.deb
#   Windows + WSL guest     tools/build-deb-wsl.sh          -> build/release-<ver>/
#   Windows + Linux box     tools/build-deb-linux.sh        -> build/release-<ver>/
#
# Why the box exists at all: the Windows host can build the APK and the TSF
# installer but not a Debian package, and WSL pays drvfs costs on every file
# (the copy, the chmod pass, the suite's own I/O). A native Ubuntu 26.04 box
# over the LAN removes both. The tree still comes FROM the checkout, so the
# artifact is built from the commit under test, not from a second clone.
#
#   HUNMIN_LINUX_HOST=scoby@192.168.219.112 tools/build-deb-linux.sh
#   HUNMIN_LINUX_HOST=... tools/build-deb-linux.sh --keep-src
#
# Exit codes: 0 built, 1 failed, 77 the box is not configured (HUNMIN_LINUX_HOST
# unset) — 77 is the suite's "cannot run here" code, so a caller can fall back
# to the WSL route without treating it as a failure.
#
# Output: every binary package the build produced, staged into
# build/release-<version>/ with bytes and sha256 printed. Measured 2026-11-07:
# the WSL route staged only hunmin_<ver>_amd64.deb and left the
# hunmin-en-us data half behind, so this route stages the set.
set -uo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
# shellcheck source=tools/host-env.sh
. "$HERE/host-env.sh"
hunmin_load_env "$HOME/.hunmin-env" "$ROOT/.env"

KEEP=0
[ "${1:-}" = "--keep-src" ] && KEEP=1

HOST=$(hunmin_linux_host)
if [ -z "$HOST" ]; then
    echo "skip: HUNMIN_LINUX_HOST is not set (e.g. HUNMIN_LINUX_HOST=user@host)" >&2
    exit 77
fi

VERSION=$(sed -n 's/^project(hunmin .* VERSION \([0-9.]*\)).*/\1/p' "$ROOT/CMakeLists.txt" | head -1)
[ -n "$VERSION" ] || { echo "cannot read the version from CMakeLists.txt" >&2; exit 1; }

STAMP=$(date +%Y%m%d-%H%M%S)
REMOTE_HOME=$(hunmin_remote_home 2>/dev/null | tr -d '\r' | tail -1)
if [ -z "$REMOTE_HOME" ]; then
    hunmin_remote_diagnose
    exit 1
fi
REMOTE_DIR="$(hunmin_linux_dir "$REMOTE_HOME")/${VERSION}-${STAMP}"
REMOTE_SRC="$REMOTE_DIR/src"
STAGE="$ROOT/build/release-$VERSION"

echo "=== $VERSION: $ROOT -> $HOST:$REMOTE_SRC"

# --- 0. reachability --------------------------------------------------------------
# BatchMode: a password prompt inside a build is a hung build. Fail with the
# one command the operator has to run, not with "Permission denied".
if ! hunmin_remote </dev/null >/dev/null 2>&1; then
    hunmin_remote_diagnose
    exit 1
fi

# --- 1. ship the tree ------------------------------------------------------------
# tar over ssh, because rsync is not installed on the Windows host (measured).
hunmin_remote_sync "$ROOT" "$REMOTE_SRC" || { hunmin_remote_diagnose; exit 1; }
echo "=== tree copied"

# --- 2. build on the box ---------------------------------------------------------
# The remote half is its own file: it also runs directly on a host that already
# has a checkout, which is how the CI runner will use it.
REMOTE_LOG="\$HOME/deb-build-$VERSION.log"
out=$( {
    printf 'export HUNMIN_SRC=%s\n' "$(printf '%q' "$REMOTE_SRC")"
    printf 'export HUNMIN_DEB_LOG=%s\n' "\"$REMOTE_LOG\""
    printf 'export HUNMIN_DEB_SKIP_BUILDDEPS=%s\n' "${HUNMIN_DEB_SKIP_BUILDDEPS:-0}"
    cat "$HERE/deb-build-remote.sh"
} | hunmin_remote )
rc=$?
printf '%s\n' "$out"
if [ "$rc" != 0 ]; then
    echo "=== remote build failed (rc=$rc)" >&2
    # The log exists only once dpkg-buildpackage has started; a dependency
    # failure exits before that, and tailing a missing file is noise.
    # shellcheck disable=SC2086
    ssh $(hunmin_ssh_opts) "$HOST" \
        "test -f $REMOTE_LOG && tail -40 $REMOTE_LOG || echo '(no build log: the run stopped before dpkg-buildpackage)'" >&2 || true
    [ "$KEEP" = 0 ] && { ssh $(hunmin_ssh_opts) "$HOST" "rm -rf '$REMOTE_DIR'" || true; }
    exit 1
fi

# --- 3. fetch the packages -------------------------------------------------------
mapfile -t DEBS < <(printf '%s\n' "$out" | sed -n 's/^DEB //p')
if [ "${#DEBS[@]}" -eq 0 ]; then
    echo "=== the build reported success but produced no DEB line" >&2
    exit 1
fi
mkdir -p "$STAGE"
for deb in "${DEBS[@]}"; do
    name=$(basename "$deb")
    # shellcheck disable=SC2086
    ssh $(hunmin_ssh_opts) "$HOST" "tar -czf - -C '$REMOTE_DIR' '$name'" \
        | tar -xzf - -C "$STAGE" || { echo "=== fetch failed: $name" >&2; exit 1; }
done

# --- 4. read back what landed ----------------------------------------------------
# The assertion is on the local files, not on the remote build's word.
echo "=== staged in build/release-$VERSION/"
for deb in "${DEBS[@]}"; do
    name=$(basename "$deb")
    [ -f "$STAGE/$name" ] || { echo "=== missing after fetch: $name" >&2; exit 1; }
    printf '  %10d bytes  %s  %s\n' "$(wc -c < "$STAGE/$name")" \
        "$(sha256sum "$STAGE/$name" | cut -d' ' -f1)" "$name"
    # shellcheck disable=SC2086
    ssh $(hunmin_ssh_opts) "$HOST" "dpkg-deb --field '$deb' Package Version Architecture" \
        2>/dev/null | sed 's/^/      /'
done

[ "$KEEP" = 0 ] && { ssh $(hunmin_ssh_opts) "$HOST" "rm -rf '$REMOTE_DIR'" || true; }
echo "=== ok"
exit 0