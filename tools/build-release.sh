#!/usr/bin/env bash
# Build the two 0.1 release artefacts locally, and print exactly where they are.
#
#   tools/build-release.sh              # deb + release APK
#   tools/build-release.sh deb          # .deb only (default)
#   tools/build-release.sh apk          # signed release APK only
#   tools/build-release.sh apk x86_64   # emulator ABI (arm64-v8a is the other)
#
# Why this exists (issue #8): the 0.1 artefacts used to exist only as one
# maintainer's local dpkg-buildpackage run and one local assembleRelease run
# (docs/REMAINING-WORK.md §3, measured 2026-10-11), so they depended on one
# machine. CI now builds both — job `deb` and job `android-release` — and this
# script is the same sequence, runnable by anyone. Whether a release REQUIRES
# the CI artefacts is the ruling issue #8 still asks the maintainer for.
#
# What it does not do:
#
# - It does not produce a SHIP-GRADE signed APK. The keystore path was decided
#   2026-10-13 (issue #1): HUNMIN_RELEASE_STORE, else a gitignored
#   local.properties, else a throwaway key. This script takes the middle and
#   last routes -- it exports HUNMIN_RELEASE_STORE pointing at a generated
#   throwaway key so the choice is visible in the output, and says so on every
#   run. Signing with the debug keystore (the pre-2026-10-12 state of
#   build.gradle.kts:77) is not an option this script takes.
# - It does not bump versionCode (still 1; no tag exists — see the
#   VERSION-CONVENTION assertion in docs/REMAINING-WORK.md §7.5).
#
# Env: ANDROID_HOME (or ~/Android/Sdk), ANDROID_NDK (required by
# build-android.sh), JAVA_HOME (optional; Gradle picks a JDK if exported).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

VERSION="$(sed -n 's/^project(hunmin LANGUAGES C VERSION \([0-9.]*\)).*/\1/p' CMakeLists.txt)"
ABI="${2:-arm64-v8a}"

log() { printf '\n=== %s ===\n' "$*"; }

build_deb() {
    # A Windows shell cannot run dpkg-buildpackage, and with HUNMIN_LINUX_HOST
    # set the operator has asked for the LAN box explicitly. Delegate so that
    # `build-release.sh deb` stays the one name a person types; the route is
    # printed by the callee. CI (native Linux, host unset) is unaffected.
    if [ -n "${HUNMIN_LINUX_HOST:-}" ] || uname -s | grep -qi 'msys\|mingw\|cygwin'; then
        log "deb: delegating to tools/build-deb.sh (this host cannot package directly)"
        exec "$ROOT/tools/build-deb.sh"
    fi
    log "deb: dpkg-buildpackage -us -uc -b (source $VERSION)"
    # -us -uc: unsigned. A key is not required to build the .deb, and
    # requiring one would make the artefact unbuildable on CI and on a fresh
    # machine — the exact property issue #8 is about.
    #
    # -b: binary only. The source build needs the orig tarball the dpkg tools
    # derive from git, and this checkout is a worktree; a binary-only build
    # exercises the same debian/rules path (configure + ctest + package) that
    # the maintainer's 2026-10-11 run did.
    #
    # The suite runs inside the build (debian/rules override_dh_auto_test) and
    # a red test fails the packaging — that is the design, not a side effect:
    # issue #8's verification asks NOT for a packaging job that skips the
    # suite. So the exit code is captured, the failures are printed, and the
    # caller decides.
    set +e
    dpkg-buildpackage -us -uc -b 2>&1 | tee /tmp/hunmin-deb-build.log
    local rc=${PIPESTATUS[0]}
    set -e
    if [ "$rc" -ne 0 ]; then
        echo
        echo "deb: BUILD FAILED (rc=$rc). The suite's verdict is above — read the"
        echo "FAILED list there rather than this hint. Skipped is not red: on a"
        echo "fresh clone data-selfcheck, modes-corpus-selfcheck and"
        echo "engine-android-goldens report Skipped without the gitignored corpora"
        echo "(REMAINING-WORK.md §0), which is by design."
        exit "$rc"
    fi
    local deb
    deb="$(ls -t ../hunmin_*_*.deb 2>/dev/null | head -1 || true)"
    if [ -z "$deb" ]; then
        echo "deb: dpkg-buildpackage exited 0 but produced no .deb" >&2
        exit 1
    fi
    printf '  %s bytes  ->  %s\n' "$(stat -c %s "$deb")" "$deb"
    dpkg-deb --info "$deb" | sed -n '1,4p'
}
build_apk() {
    log "apk: release APK for $ABI, signed with a generated throwaway key"
    # The keystore. Issue #1 (B8) was decided 2026-10-13: build.gradle.kts reads
    # HUNMIN_RELEASE_STORE, else local.properties, else generates a throwaway
    # key. This script takes the first route with a GENERATED throwaway key --
    # it needs SOME storeFile to run assembleRelease at all, and a generated one
    # keeps the build honest: an APK signed with it installs and runs, and it
    # is not a releasable signature. To sign with a real key instead, put it in
    # local.properties (or point HUNMIN_RELEASE_STORE at it before calling); the
    # keystore is then read, not generated, and this block is unused.
    local ks="$ROOT/build/release-ci.keystore"
    mkdir -p "$ROOT/build"
    # keytool and Gradle are NATIVE programs: on an MSYS host they cannot read a
    # /c/... path, and keytool reports it as FileNotFoundException (measured
    # 2026-10-29 on this Windows host). build-android.sh already normalises the
    # same way for cmake/ninja; do it once here and use the native form for both
    # the keytool call and the exported variable.
    local ks_win="$ks"
    command -v cygpath >/dev/null 2>&1 && ks_win="$(cygpath -m "$ks")"
    if [ ! -f "$ks" ]; then
        keytool -genkeypair -keystore "$ks_win" -storepass android -alias hunmin \
            -keypass android -keyalg RSA -keysize 2048 -validity 10000 \
            -dname 'CN=hunmin-ci,O=hunmin,L=CI,C=KR'
    fi
    # Handed to Gradle via the environment, matching the CI android-release job,
    # so the same build.gradle.kts branch runs locally and in CI. build.gradle.kts
    # reads HUNMIN_RELEASE_STORE when set -- see the signingConfigs block there.
    export HUNMIN_RELEASE_STORE="$ks_win"
    ./tools/build-android.sh "$ABI" 24
    cd frontends/android
    ./gradlew --no-daemon :app:assembleRelease
    # The build names its own output hunmin-<versionName>.apk (see the
    # androidComponents block in app/build.gradle.kts), so glob rather than
    # hardcode: a version bump must not need an edit here.
    local apk
    apk=$(ls app/build/outputs/apk/release/hunmin-*.apk 2>/dev/null | head -1)
    [ -n "$apk" ] || { echo "apk: no hunmin-*.apk under app/build/outputs/apk/release (run :app:assembleRelease)" >&2; exit 1; }
    printf '  %s bytes  ->  %s\n' "$(stat -c %s "$apk")" "$ROOT/frontends/android/$apk"
    echo "  signed with: build/release-ci.keystore (THROWAWAY -- not release-grade)"
}

case "${1:-deb}" in
    deb) build_deb ;;
    apk) build_apk ;;
    all) build_deb; build_apk ;;
    *) echo "usage: build-release.sh [deb|apk|all] [abi]" >&2; exit 2 ;;
esac
