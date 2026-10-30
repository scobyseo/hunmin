#!/usr/bin/env bash
# build-deb-wsl.sh — build the release .deb on a Windows host, inside WSL.
#
# Why this exists: tools/build-release.sh deb is the Linux route (it is what CI
# runs). On this host the distribution is a WSL guest, and three host facts make
# a naive `wsl.exe bash -c 'dpkg-buildpackage'` fail in ways that look like code
# breakage. Each is handled below and each was measured on 2026-10-29:
#
#  1. The worktree is a CRLF checkout. The package build runs ctest, where the
#     `abi` and `krdict-fixture` guards are red on line endings alone, and
#     dpkg-source rejects debian/source/format as "3.0 (native)\r" (exit 25).
#     The copy is normalised to LF, which is what a Linux checkout looks like.
#  2. debhelper 13.31 (Ubuntu 25.10) passes -DFETCHCONTENT_FULLY_DISCONNECTED=ON
#     unconditionally, so FetchContent refuses to download googletest and
#     requires obj-x86_64-linux-gnu/_deps/googletest-src to exist already.
#     CI's older debhelper does not, which is why the workflow has no such step.
#     The checkout is pre-populated at that exact path.
#  3. dpkg-buildpackage -b runs `debian/rules clean` first, and dh_auto_clean
#     deletes the build dir, taking the checkout from (2) with it. -nc skips
#     that clean. The cost is that the caller must not rely on a stale tree:
#     the working copy is recreated from scratch on every run.
#
# Usage:
#   tools/build-deb-wsl.sh                 # from the Windows side: wsl.exe bash tools/build-deb-wsl.sh
#   tools/build-deb-wsl.sh --keep-src      # keep the copy under $HOME for inspection
#
# Output: the .deb is copied to <repo>/build/release-<version>/hunmin_<version>_amd64.deb,
# which is where tools/release-0.2.sh stages it from.
set -uo pipefail

KEEP=0
[ "${1:-}" = "--keep-src" ] && KEEP=1

ROOT=$(cd "$(dirname "$0")/.." && pwd)
VERSION=$(sed -n 's/^project(hunmin .* VERSION \([0-9.]*\)).*/\1/p' "$ROOT/CMakeLists.txt" | head -1)
[ -n "$VERSION" ] || { echo "cannot read the version from CMakeLists.txt" >&2; exit 1; }

DEST=${HUNMIN_DEB_SRC:-$HOME/hunmin-deb-$VERSION}
LOG=$HOME/deb-build-$VERSION.log
GT_TAG=v1.17.0            # must match the GIT_TAG in the top-level CMakeLists
STAGE=$ROOT/build/release-$VERSION

echo "=== $VERSION: $ROOT -> $DEST (log: $LOG)"
rm -rf "$DEST"
mkdir -p "$DEST"
rsync -a \
  --exclude '/build' --exclude '/build-*' --exclude '/.git' \
  --exclude '/build-android' --exclude '/build-msvc' --exclude '/build-tsf-devhost' \
  "$ROOT"/ "$DEST"/

# (1) LF-normalise the text files; binary assets keep their bytes.
find "$DEST" -type f \
  \( -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp' -o -name '*.txt' \
     -o -name '*.json' -o -name '*.md' -o -name '*.sh' -o -name '*.py' -o -name '*.cmake' \
     -o -name '*.in' -o -name '*.yml' -o -name '*.toml' -o -name '*.rules' -o -name '*.kts' \) \
  -exec sed -i 's/\r$//' {} +
find "$DEST/debian" -type f -exec sed -i 's/\r$//' {} +

# The /mnt/c mount reports every file as 0777, so a plain copy makes
# debian/hunmin.docs executable -- dpkg then treats it as a config script and
# the install stage dies with 'debian/hunmin.docs (executable config) returned
# exit code 2' (measured 2026-10-29, after the suite had already passed).
# Normalise: data files 644, and only the files that must run 755.
find "$DEST" -type f -exec chmod 644 {} +
chmod 755 "$DEST"/debian/rules "$DEST"/debian/*.postinst "$DEST"/debian/*.preinst 2>/dev/null
find "$DEST/tools" "$DEST/tests" -name '*.sh' -exec chmod 755 {} + 2>/dev/null
chmod 755 "$DEST/frontends/android/gradlew" 2>/dev/null
echo "=== file modes normalised"

cd "$DEST" || exit 1

# fcitx5 comes from a source prefix only when the distro cannot provide the
# Build-Depends floor; Ubuntu 25.10 has 5.1.19, so the build is hermetic here.
if dpkg -s libfcitx5core-dev >/dev/null 2>&1; then
    echo "=== fcitx5 from the distro: $(dpkg-query -W -f='${Version}' libfcitx5core-dev)"
else
    export CMAKE_PREFIX_PATH=${FCITX_PREFIX:?set FCITX_PREFIX to an fcitx5 install prefix}
    export PKG_CONFIG_PATH="$CMAKE_PREFIX_PATH/lib/pkgconfig"
    echo "=== fcitx5 from the prefix: $CMAKE_PREFIX_PATH"
fi

# (2) pre-populate the FetchContent source the disconnected build requires.
GT_DEPS="$DEST/obj-x86_64-linux-gnu/_deps"
if [ ! -d "$GT_DEPS/googletest-src" ]; then
    mkdir -p "$GT_DEPS"
    git clone --depth 1 --branch "$GT_TAG" https://github.com/google/googletest.git \
        "$GT_DEPS/googletest-src" || { echo "=== googletest clone failed" >&2; exit 1; }
fi

# (3) -nc keeps that checkout alive.
{ dpkg-buildpackage -us -uc -b -nc; } > "$LOG" 2>&1
rc=$?
echo "=== dpkg-buildpackage rc=$rc"
tail -20 "$LOG"

deb=$(ls "$HOME/hunmin_${VERSION}_"*.deb 2>/dev/null | head -1)
if [ "$rc" = 0 ] && [ -n "$deb" ]; then
    mkdir -p "$STAGE"
    cp -f "$deb" "$STAGE/"
    echo "=== staged: $STAGE/$(basename "$deb") ($(wc -c < "$deb") bytes)"
else
    echo "=== no .deb produced; full log at $LOG" >&2
    [ "$KEEP" = 0 ] && rm -rf "$DEST"
    exit 1
fi

[ "$KEEP" = 0 ] && rm -rf "$DEST"
exit 0
