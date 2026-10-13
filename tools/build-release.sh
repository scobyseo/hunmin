#!/usr/bin/env bash
# Build the two 0.1 release artefacts locally, and print exactly where they are.
#
#   tools/build-release.sh              # deb + release APK
#   tools/build-release.sh deb          # .deb only (default)
#   tools/build-release.sh apk          # signed release APK only
#   tools/build-release.sh apk x86_64   # emulator ABI (arm64-v8a is the other)
#
# Why this exists (issue #8): CI builds neither 0.1 artefact. The .deb exists
# only as one maintainer's local dpkg-buildpackage run, and the signed release
# APK only as one local assembleRelease run (docs/REMAINING-WORK.md §3, measured
# 2026-10-11). This script is the same sequence, runnable by anyone, so the
# artefacts stop depending on one machine. It does NOT make a release a CI
# product — that is the ruling issue #8 asks the maintainer for.
#
# What it does not do:
#
# - It does not produce a SHIP-GRADE signed APK. A real release key lives in a
#   gitignored local.properties (issue #1's direction); this script generates a
#   throwaway key for the CI-style release build and documents that choice in
#   the APK name. Signing with the debug keystore (the pre-2026-10-12 state of
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
    # data-selfcheck is red in a worktree without the gitignored krdict dump
    # (REMAINING-WORK.md §0) — so the exit code is captured, the failures are
    # printed, and the caller decides. A packaging job that skips the suite is
    # what issue #8's verification asks NOT to do.
    set +e
    dpkg-buildpackage -us -uc -b 2>&1 | tee /tmp/hunmin-deb-build.log
    local rc=${PIPESTATUS[0]}
    set -e
    if [ "$rc" -ne 0 ]; then
        echo
        echo "deb: BUILD FAILED (rc=$rc). The suite's verdict is above;"
        echo "REMAINING-WORK.md §0 names the one test red in a fresh worktree"
        echo "(data-selfcheck: krdict2data finds no *.xml under data-orig/krdict)."
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
    # The keystore. Issue #1 (B8) is unresolved: build.gradle.kts:77 hardcodes
    # /home/scoby/.android/debug.keystore under a comment saying the keystore
    # lives in gitignored local.properties. This script does not pick a side --
    # it needs SOME storeFile to run assembleRelease at all, and a generated
    # throwaway key keeps the build honest: an APK signed with it installs and
    # runs, and it is not a releasable signature. When B8 is ruled, delete the
    # generation below and read the key from wherever the ruling points.
    local ks="$ROOT/build/release-ci.keystore"
    mkdir -p "$ROOT/build"
    if [ ! -f "$ks" ]; then
        keytool -genkeypair -keystore "$ks" -storepass android -alias hunmin \
            -keypass android -keyalg RSA -keysize 2048 -validity 10000 \
            -dname 'CN=hunmin-ci,O=hunmin,L=CI,C=KR'
    fi
    # Handed to Gradle via the environment, matching the CI android-release job,
    # so the same build.gradle.kts branch runs locally and in CI. build.gradle.kts
    # reads HUNMIN_RELEASE_STORE when set -- see the signingConfigs block there.
    export HUNMIN_RELEASE_STORE="$ks"
    ./tools/build-android.sh "$ABI" 24
    cd frontends/android
    ./gradlew --no-daemon assembleRelease
    local apk="app/build/outputs/apk/release/app-release.apk"
    [ -f "$apk" ] || { echo "apk: no artefact at $apk" >&2; exit 1; }
    printf '  %s bytes  ->  %s\n' "$(stat -c %s "$apk")" "$ROOT/frontends/android/$apk"
    echo "  signed with: build/release-ci.keystore (THROWAWAY -- not release-grade)"
}

case "${1:-deb}" in
    deb) build_deb ;;
    apk) build_apk ;;
    all) build_deb; build_apk ;;
    *) echo "usage: build-release.sh [deb|apk|all] [abi]" >&2; exit 2 ;;
esac
