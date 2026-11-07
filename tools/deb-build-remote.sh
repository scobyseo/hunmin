#!/usr/bin/env bash
# deb-build-remote.sh — the Linux half of `tools/build-deb-linux.sh`.
#
# Runs ON the build host, against a source tree already copied there, and
# does to it what `tools/build-deb-wsl.sh` does to its WSL copy: LF-normalise
# the checkout, normalise file modes, pre-populate the FetchContent source a
# debhelper >= 13.31 insists on, then `dpkg-buildpackage -us -uc -b -nc` and
# report the packages it produced.
#
# Inputs, all through the environment so the driver can stream this over
# stdin without quoting a single argument:
#   HUNMIN_SRC      the source tree (required)
#   HUNMIN_DEB_LOG  log path (default $HOME/deb-build-<version>.log)
#   FCITX_PREFIX    an fcitx5 install prefix, when the distro lacks the
#                   Build-Depends floor (Ubuntu 24.04's 5.1.7 is below it)
#
# Output contract, read by the driver: one `DEB <absolute path>` line per
# produced package, then exit with dpkg-buildpackage's status. Anything else
# on stdout is progress.
set -uo pipefail

SRC=${HUNMIN_SRC:?HUNMIN_SRC is not set}
cd "$SRC" || exit 1

VERSION=$(sed -n 's/^project(hunmin .* VERSION \([0-9.]*\)).*/\1/p' CMakeLists.txt | head -1)
[ -n "$VERSION" ] || { echo "cannot read the version from CMakeLists.txt" >&2; exit 1; }
LOG=${HUNMIN_DEB_LOG:-$HOME/deb-build-$VERSION.log}
GT_TAG=v1.17.0            # must match the GIT_TAG in the top-level CMakeLists

echo "=== host: $(. /etc/os-release 2>/dev/null && echo "$PRETTY_NAME") / $(uname -m)"
echo "=== $VERSION: $SRC (log: $LOG)"

# --- dependencies, named rather than discovered ------------------------------
# The distribution's dirs FIRST, the operator's ~/.local/bin after them. Both
# halves of that order are measured on the box 2026-11-07:
#   * `~/.local/bin/cmake` was a pip wheel that shadows /usr/bin/cmake in a
#     login shell and dies inside dh_auto_configure with
#     `ModuleNotFoundError: No module named 'cmake'` — a package build is the
#     distribution's build, so the system toolchain must win;
#   * but `~/.local/bin` also carries pyright, which docs-lint asserts on
#     inside the package's own test step, so it cannot simply be dropped.
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:$PATH
# A missing tool must say which package provides it: the previous route's
# failures were "command not found" three steps into a build.
missing=""
for c in dpkg-buildpackage dh_installdocs cmake pkg-config git g++ sed find tar pyright; do
    command -v "$c" >/dev/null 2>&1 || missing="$missing $c"
done
# A pip `cmake` shim resolves but cannot run; catch it by shape, not by path.
cmake_bin=$(command -v cmake 2>/dev/null || true)
if [ -n "$cmake_bin" ] && head -1 "$cmake_bin" 2>/dev/null | grep -q 'python'; then
    missing="$missing cmake(pip-shim)"
fi
if [ -n "$missing" ]; then
    echo "=== missing tools:$missing" >&2
    echo "=== install: sudo apt-get install -y build-essential cmake pkg-config git dpkg-dev debhelper fakeroot" >&2
    echo "=== and:     pip install --user pyright   (docs-lint asserts on it inside the suite)" >&2
    echo "=== note: a cmake from pip (~/.local/bin) does not satisfy the cmake requirement" >&2
    exit 3
fi
# Header probes for the Build-Depends that debian/control does NOT declare but
# the tree needs (measured on the box 2026-11-07: the suite's xim-harness
# includes <xkbcommon/xkbcommon.h> unconditionally, while CMakeLists treats
# xkbcommon as optional — so an undeclared package fails the build instead of
# degrading to the warning the CMake code documents). Named here so the
# failure arrives before three minutes of compiling.
hdr_missing=""
printf '#include <xkbcommon/xkbcommon.h>\nint main(void){return 0;}\n' \
    | ${CC:-cc} -x c -fsyntax-only - >/dev/null 2>&1 || hdr_missing="$hdr_missing libxkbcommon-dev"
if [ -n "$hdr_missing" ]; then
    echo "=== missing headers:$hdr_missing" >&2
    echo "=== install: sudo apt-get install -y libxkbcommon-dev libxkbcommon-x11-dev" >&2
    exit 3
fi
if dpkg -s libfcitx5core-dev >/dev/null 2>&1; then
    echo "=== fcitx5 from the distro: $(dpkg-query -W -f='${Version}' libfcitx5core-dev)"
else
    export CMAKE_PREFIX_PATH=${FCITX_PREFIX:?set FCITX_PREFIX to an fcitx5 install prefix, or install libfcitx5core-dev}
    export PKG_CONFIG_PATH="$CMAKE_PREFIX_PATH/lib/pkgconfig"
    echo "=== fcitx5 from the prefix: $CMAKE_PREFIX_PATH"
fi
DEBHELPER=$(dpkg-query -W -f='${Version}' debhelper 2>/dev/null || echo 0)
echo "=== debhelper: $DEBHELPER"

# --- (1) line endings --------------------------------------------------------
# The tree came from a Windows checkout: CRLF fails the `abi` and
# `krdict-fixture` guards and makes dpkg-source reject
# debian/source/format as "3.0 (native)\r". Unconditional and idempotent —
# a detection step is one more way to be wrong about the input.
before=$(grep -rlI $'\r' --include='*.c' --include='*.h' --include='*.cpp' \
          --include='*.hpp' --include='*.txt' --include='*.json' --include='*.md' \
          --include='*.sh' --include='*.py' --include='*.cmake' --include='*.in' \
          --include='*.yml' --include='*.toml' --include='*.rules' --include='*.kts' \
          . 2>/dev/null | wc -l)
find . -type f \
  \( -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp' -o -name '*.txt' \
     -o -name '*.json' -o -name '*.md' -o -name '*.sh' -o -name '*.py' -o -name '*.cmake' \
     -o -name '*.in' -o -name '*.yml' -o -name '*.toml' -o -name '*.rules' -o -name '*.kts' \) \
  -exec sed -i 's/\r$//' {} +
find debian -type f -exec sed -i 's/\r$//' {} +
echo "=== line endings normalised (files that carried CR: $before)"

# --- (2) modes ---------------------------------------------------------------
# A copy from a filesystem that reports 0777 makes debian/hunmin.docs
# executable; dpkg then treats it as a config script and the install stage
# dies. Data files 644, and only the files that must run 755.
find . -type f -exec chmod 644 {} +
chmod 755 debian/rules debian/*.postinst debian/*.preinst 2>/dev/null
find tools tests -name '*.sh' -exec chmod 755 {} + 2>/dev/null
chmod 755 frontends/android/gradlew 2>/dev/null
echo "=== file modes normalised"

# --- (3) the disconnected FetchContent source --------------------------------
# debhelper 13.31+ passes -DFETCHCONTENT_FULLY_DISCONNECTED=ON
# unconditionally, so googletest must already be at the path the generated
# build dir uses. Older debhelper downloads it and must not be given a
# stale copy, hence the version test rather than an unconditional clone.
if [ "$(printf '%s\n' "$DEBHELPER" "13.31" | sort -V | head -1)" = "13.31" ]; then
    GT_DEPS="$SRC/obj-x86_64-linux-gnu/_deps"
    if [ ! -d "$GT_DEPS/googletest-src" ]; then
        mkdir -p "$GT_DEPS"
        git clone --depth 1 --branch "$GT_TAG" https://github.com/google/googletest.git \
            "$GT_DEPS/googletest-src" || { echo "=== googletest clone failed" >&2; exit 1; }
    fi
    echo "=== googletest pre-populated for the disconnected build"
fi

# --- (4) build, package, suite ----------------------------------------------
# -nc keeps the pre-populated checkout alive: dh_auto_clean would delete the
# build dir and take it with it.
#
# HUNMIN_DEB_SKIP_BUILDDEPS=1 adds -d and skips dpkg-checkbuilddeps, which is
# what the CI deb job does (`dpkg-buildpackage -us -uc -b -d`). It is for a
# host whose tools come from somewhere other than dpkg — measured: cmake
# 4.4.4 installed under ~/.local/bin satisfies the build but not
# `Build-Depends: cmake (>= 3.22)`, so the check aborts a build that works.
# The dependency probe above still runs either way.
DPKG_ARGS=(-us -uc -b -nc)
if [ "${HUNMIN_DEB_SKIP_BUILDDEPS:-0}" = 1 ]; then
    DPKG_ARGS+=(-d)
    echo "=== build-dependency check skipped (-d, as CI does)"
fi
{ dpkg-buildpackage "${DPKG_ARGS[@]}"; } > "$LOG" 2>&1
rc=$?
echo "=== dpkg-buildpackage rc=$rc"
tail -20 "$LOG"

parent=$(dirname "$SRC")
# The package version is the CHANGELOG's, not the CMake project version: a
# prerelease says 0.3.0~rc in debian/changelog while CMake can only spell
# 0.3.0 (numeric VERSION), so dpkg names the files hunmin_0.3.0~rc_*.deb.
# Globbing every .deb in this per-run parent dir is version-agnostic and
# still exact — the dir was created empty for this build, and the -dbgsym
# package is a .ddeb, so it cannot be picked up by mistake.
PKG_VERSION=$(dpkg-parsechangelog -S Version 2>/dev/null || echo "$VERSION")
echo "=== package version: $PKG_VERSION"
for deb in "$parent"/*.deb; do
    [ -f "$deb" ] && printf 'DEB %s\n' "$deb"
done
exit "$rc"