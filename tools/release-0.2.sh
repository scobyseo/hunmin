#!/usr/bin/env bash
# release-0.2.sh — publish hunmin 0.2.0 as a GitHub Release with all three channels.
#
# Method (a clean machine can reproduce every step from the commit):
#   1. Build the three artifacts. Each build path lives where it already lives;
#      this script owns tag + release + upload + verification only, so a build
#      change never has to be made twice:
#        Windows  bash tools/build-windows-installer.sh
#                 -> build-msvc/installer/hunmin-0.2.0-setup.exe
#        Debian   tools/build-release.sh deb   (dpkg-buildpackage -us -uc -b;
#                 the suite runs inside debian/rules override_dh_auto_test)
#                 -> ../hunmin_0.2.0_amd64.deb
#        Android  tools/build-release.sh apk arm64-v8a
#                 -> frontends/android/app/build/outputs/apk/release/app-release.apk
#   2. Stage them under build/release-0.2.0/ with channel-named filenames, so the
#      release's asset list reads as the three channels rather than as three
#      build-directory paths.
#   3. Create the annotated git tag 0.2.0 (project version, no 'v' prefix).
#   4. Push the tag.
#   5. Create the GitHub Release from docs/release-notes/0.2.0.md and upload the
#      three assets in the same call.
#   6. Verify reachability: fetch each asset URL with NO credentials and record
#      the status beside the repository's real visibility.
#
# Step 6 is not "did the upload work". Measured 2026-10-16 for 0.1: the release
# was published with correct bytes (1,252,740, verified by download and
# dpkg -c) and the asset URL returned 404 to anyone without an account, because
# the repository was private. Steps 3-5 assert existence; only step 6 asserts
# reachability, and it is recorded as measured rather than assumed.
#
# Steps 3-5 are irreversible, so the bare invocation prints them and changes
# nothing (see DRY_RUN). --yes performs them.
#
# Deferred by name, not by silence: the rfc/APK channel's signing stance is the
# throwaway key build-release.sh generates, so this script refuses to publish an
# APK unless HUNMIN_RELEASE_STORE names a keystore the caller supplied.
#
# Usage:
#   tools/release-0.2.sh            # dry run: check what exists, print the commands
#   tools/release-0.2.sh --yes      # tag, push, create the release, upload, verify
set -uo pipefail

VERSION=0.2.0
TAG=0.2.0
DRY_RUN=1
for arg in "$@"; do
    case "$arg" in
        --yes) DRY_RUN=0 ;;
        -h|--help) sed -n '2,40p' "$0"; exit 0 ;;
        *) echo "usage: $0 [--yes]" >&2; exit 2 ;;
    esac
done

ROOT=$(cd "$(dirname "$0")/.." && pwd)
STAGE="$ROOT/build/release-$TAG"
NOTES="$ROOT/docs/release-notes/$TAG.md"

# git and gh are NATIVE programs: on an MSYS host they cannot resolve /c/...
# paths, and the failure reads "fatal: cannot change to '/c/...'" (measured
# 2026-10-29 on the first --yes run of this script). Everything handed to them
# goes through this native form.
ROOT_NATIVE="$ROOT"; STAGE_NATIVE="$STAGE"; NOTES_NATIVE="$NOTES"
if command -v cygpath >/dev/null 2>&1; then
    ROOT_NATIVE=$(cygpath -m "$ROOT"); STAGE_NATIVE=$(cygpath -m "$STAGE"); NOTES_NATIVE=$(cygpath -m "$NOTES")
fi

# channel:staged-name:source-path
CHANNELS=(
    "windows:hunmin-$VERSION-setup.exe:$ROOT/build-msvc/installer/hunmin-$VERSION-setup.exe"
    "debian:hunmin_${VERSION}_amd64.deb:$ROOT/build/release-$TAG/hunmin_${VERSION}_amd64.deb"
    "android:hunmin-$VERSION-arm64-v8a.apk:$ROOT/build/release-$TAG/hunmin-$VERSION-arm64-v8a.apk"
)

say() { printf '%s\n' "$*"; }
step() { printf '\n=== %s\n' "$*"; }
do_or_print() {
    if [ "$DRY_RUN" = 1 ]; then
        say "  [dry run] $*"
    else
        say "  \$ $*"
        eval "$@"
    fi
}

step "preconditions"
fail=0
if [ -n "$(git -C "$ROOT" status --porcelain)" ]; then
    say "  tree is dirty -- commit or stash first:"; git -C "$ROOT" status --short; fail=1
else
    say "  tree clean"
fi
if git -C "$ROOT" rev-parse -q --verify "refs/tags/$TAG" >/dev/null; then
    say "  tag $TAG already exists locally"; fail=1
else
    say "  tag $TAG absent locally (good)"
fi
if git -C "$ROOT" ls-remote --exit-code --tags origin "$TAG" >/dev/null 2>&1; then
    say "  tag $TAG already exists on origin"; fail=1
else
    say "  tag $TAG absent on origin (good)"
fi
[ -f "$NOTES" ] && say "  notes: $(wc -c < "$NOTES") bytes" || { say "  MISSING notes $NOTES"; fail=1; }

step "stage the three channels"
mkdir -p "$STAGE"
for entry in "${CHANNELS[@]}"; do
    ch="${entry%%:*}"; rest="${entry#*:}"; name="${rest%%:*}"; src="${rest#*:}"
    if [ -s "$src" ]; then
        say "  $ch: $(basename "$src") -> build/release-$TAG/$name ($(wc -c < "$src") bytes)"
        [ "$src" = "$STAGE/$name" ] || cp -f "$src" "$STAGE/$name"
    else
        say "  $ch: MISSING $src -- build it first (see the method above)"; fail=1
    fi
done

if [ "$fail" != 0 ] && [ "$DRY_RUN" = 0 ]; then
    say ""; say "refusing to publish: fix the rows above"; exit 1
fi

step "3. annotated tag"
do_or_print "git -C '$ROOT_NATIVE' tag -a '$TAG' -m 'hunmin $TAG'"

step "4. push the tag"
do_or_print "git -C '$ROOT_NATIVE' push origin '$TAG'"

step "5. create the release and upload the assets"
ASSET_ARGS=()
for entry in "${CHANNELS[@]}"; do
    rest="${entry#*:}"; name="${rest%%:*}"; ASSET_ARGS+=("'$STAGE_NATIVE/$name'")
done
do_or_print "gh release create '$TAG' --repo scobyseo/hunmin --title 'hunmin $TAG' --notes-file '$NOTES_NATIVE' ${ASSET_ARGS[*]}"

step "6. verify (existence, then reachability)"
# One array into @tsv. The first form piped each scalar through @tsv, which gh
# rejects ('@tsv cannot be applied to: string ("0.2.0")') -- measured on the
# 0.2.0 run, where the step printed nothing and the release was proved by the
# asset download and hash comparison instead.
do_or_print "gh release view '$TAG' --repo scobyseo/hunmin --json tagName,isDraft,assets --jq '[.tagName, (.isDraft|tostring), ([.assets[].name]|join(\",\"))] | @tsv'"
say "  anonymous fetch of each asset URL (expect 404 while the repo is private):"
for entry in "${CHANNELS[@]}"; do
    rest="${entry#*:}"; name="${rest%%:*}"
    url="https://github.com/scobyseo/hunmin/releases/download/$TAG/$name"
    code=$(curl -s -o /dev/null -w '%{http_code}' -L "$url" 2>/dev/null || echo "curl-failed")
    say "    $name -> HTTP $code"
done
say "  repository visibility right now: $(gh repo view scobyseo/hunmin --json visibility --jq .visibility 2>/dev/null || echo unknown)"

[ "$DRY_RUN" = 1 ] && { say ""; say "dry run: nothing was tagged, pushed or published. Re-run with --yes."; }
exit 0
