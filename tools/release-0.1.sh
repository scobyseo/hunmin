#!/usr/bin/env bash
# release-0.1.sh — publish hunmin 0.1.0 as a GitHub Release with the .deb artifact.
#
# Method (a clean machine can reproduce this from the commit):
#   1. Build the .deb:     tools/build-release.sh deb   (unsigned binary only;
#                             this is the same script docs/REMAINING-WORK.md §7.5
#                             asserts, and it runs dpkg-buildpackage -us -uc -b
#                             with the suite inside debian/rules override_dh_auto_test)
#   2. Create git tag:     0.1.0   (project version, no 'v' prefix — versionname
#                             ruling in docs/MILESTONES.md §0.1 names the release
#                             "0.1.0" bare, per issue #9)
#   3. Push the tag:      git push origin 0.1.0
#   4. Create GitHub Release:  gh release create 0.1.0 --title "hunmin 0.1.0" \
#                              --notes-file docs/release-notes/0.1.0.md
#   5. Upload artifact:    gh release upload 0.1.0 hunmin_0.1.0_amd64.deb
#   6. Verify reachability: curl the asset URL with NO credentials and compare
#                             the status against the repository's real visibility
#
# Step 6 is the one that answers issue #7's third verification item, and the
# reason it is not "did the upload work" is that the upload working is not the
# question. Measured 2026-10-16: the 0.1 release was published with the correct
# bytes (1,252,740, verified by download and dpkg -c) and the asset URL returned
# 404 to anyone without an account, because the repository is private until after
# 0.2. Steps 1-5 all assert existence; only step 6 asserts reachability.
#
# Steps 3-5 are the irreversible ones, which is why the bare invocation prints
# them instead of running them (see DRY_RUN below).
#
# Why step 1 delegates rather than repeating the command: the build path lives in
# one place. build-release.sh carries the worktree/krdict failure explanation that
# `data-selfcheck` needs in a fresh checkout, and a second dpkg-buildpackage call
# here would drift from it. This script owns tag + release + upload only.
# The .deb is produced the same way that produced the local artefact measured on
# 2026-10-11: `debian/` already carries changelog (hunmin (0.1.0) unstable),
# control, rules, postinst and copyright. No key signing is needed for a
# -us -uc unsigned build. The build gate is the tier-D ctest suite (see
# PLAN.md:156 "debs -> PPA").
#
# Host requirements (listed in docs/ENVIRONMENT.md) — what this script additionally
# needs: `gh` (configured for scobyseo), `git`, a build machine with the
# Build-Depends from debian/control installed.
#
# Usage:
#   tools/release-0.1.sh            # dry run: prints every command, changes nothing
#   tools/release-0.1.sh --yes      # build .deb, tag 0.1.0, create + upload release
#   tools/release-0.1.sh --skip-build ARTIFACT_PATH
#                                   # assume the .deb is already at ARTIFACT_PATH
#
# Idempotency, corrected 2026-10-16 after a release that needed it. Running
# --yes twice: the tag is kept when it already points at HEAD (see step 2), and
# `gh release upload` does NOT overwrite -- it fails with "asset under the same
# name already exists". The header used to claim it overwrote; measured, it does
# not, and the asset has to be deleted first:
#
#   gh release delete-asset <tag> <asset> -y && gh release upload <tag> <file>
#
# Re-uploading an asset for an already-cut release is also NOT something this
# script does from a later HEAD: step 2 refuses when HEAD is ahead of the tag,
# which is correct -- the replacement has to be built FROM the tag, not from a
# newer commit.
#
set -euo pipefail

REPO="${REPO:-scobyseo/hunmin}"
VERSION="0.1.0"
TAG="0.1.0"
ARTIFACT_NAME="hunmin_${VERSION}_amd64.deb"
# Default is the SAFE one. This was 0 on 2026-10-16, which contradicted the
# usage line directly above ("tools/release-0.1.sh  # dry run: prints every
# command, changes nothing"): the bare command took the real path end to end —
# dpkg-buildpackage, git tag, git push, gh release create, gh release upload.
# Measured: running it bare started a real dpkg-buildpackage; the run was cut
# short by a closed pipe, and no tag and no release existed afterwards, so
# nothing was published — but nothing in the script prevented it. A script that
# publishes must default to printing, because the published thing is the
# irreversible step (a tag push and a public release), not the build.
DRY_RUN=1
FORCE_TAG=0
# Derived from this script's own location, never a hardcoded path (AGENTS.md).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_RELEASE="${SCRIPT_DIR}/build-release.sh"
# dpkg-buildpackage writes the .deb ONE LEVEL ABOVE the source tree -- the fact
# build-release.sh honours with `ls -t ../hunmin_*_*.deb`. This script used a bare
# relative name, i.e. the CURRENT directory, and on 2026-10-16 that published the
# wrong bytes: a .deb from an earlier build sat in the repo root, satisfied the
# existence check, and was uploaded while the build that had just run wrote its
# output to the parent. Measured: published 1,253,210 bytes from 09:00; the build
# that ran was 1,252,740 bytes at 11:36.
ARTIFACT="${ROOT}/../${ARTIFACT_NAME}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --yes)      DRY_RUN=0 ;;
    --dry-run)  DRY_RUN=1 ;;
    --force-tag) FORCE_TAG=1 ;;
    --skip-build)
      shift
      ARTIFACT="${1:-${ARTIFACT_NAME}}"
      SKIP_BUILD=1
      ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
  shift
done

if [[ $DRY_RUN -eq 1 ]]; then
  echo "[dry-run] would run the following; nothing was executed."
fi

usage_exit() { echo "usage: $0 [--yes|--dry-run|--force-tag] [--skip-build PATH]" >&2; exit 2; }

command -v gh >/dev/null 2>&1 || { echo "gh not found (needed for GitHub Release)" >&2; exit 1; }
command -v git >/dev/null 2>&1 || { echo "git not found" >&2; exit 1; }
command -v dpkg-buildpackage >/dev/null 2>&1 || { echo "dpkg-buildpackage not found (Debian build)" >&2; exit 1; }
[[ -x "$BUILD_RELEASE" ]] || { echo "not executable: $BUILD_RELEASE" >&2; exit 1; }

# --- 0. Sanity: current branch is where 0.1.0 should land ---------------------
BRANCH="$(git branch --show-current 2>/dev/null || true)"
if [[ -z "$BRANCH" ]]; then
  echo "ERROR: not inside a git worktree" >&2; exit 1
fi
# Keep the script from accidentally landing on an unrelated branch.
case "$BRANCH" in
  master|main|task/*|scobyseo/*) ;;
  *) echo "NOTE: currently on branch '$BRANCH' (not master/main). Tag + push will use this branch." >&2 ;;
esac

# --- 1. Build the .deb -------------------------------------------------------
if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
  echo "==> Step 1: building ${ARTIFACT} (via tools/build-release.sh deb)"
  if [[ $DRY_RUN -eq 1 ]]; then
    echo "  tools/build-release.sh deb"
  else
    # Fingerprint before and after. dpkg overwrites the path, so "the file is
    # there" is not evidence that THIS build produced it -- which is exactly how
    # the 2026-10-16 release published an hour-old .deb. If the bytes are
    # unchanged the build wrote no artefact and the upload must not happen.
    PRE_SHA="none"
    [[ -f "$ARTIFACT" ]] && PRE_SHA="$(sha256sum "$ARTIFACT" | cut -d' ' -f1)"
    "$BUILD_RELEASE" deb
    if [[ ! -f "$ARTIFACT" ]]; then
      echo "ERROR: ${ARTIFACT} not produced -- looked one level above the source" >&2
      echo "       tree, which is where dpkg-buildpackage writes" >&2
      exit 1
    fi
    POST_SHA="$(sha256sum "$ARTIFACT" | cut -d' ' -f1)"
    if [[ "$PRE_SHA" == "$POST_SHA" ]]; then
      echo "ERROR: the build left ${ARTIFACT} byte-identical (sha256 ${POST_SHA})." >&2
      echo "       Refusing to upload an artefact this run did not produce." >&2
      exit 1
    fi
    echo "  built ${ARTIFACT}"
    echo "  sha256 ${POST_SHA}"
  fi
fi

# --- 2. Create the git tag ---------------------------------------------------
if git rev-parse -q --verify "refs/tags/${TAG}" >/dev/null 2>&1; then
  if [[ $FORCE_TAG -eq 1 ]]; then
    echo "==> Step 2: tag ${TAG} already exists; force-updating it"
    git tag -d "$TAG" && git tag "$TAG"
  elif [[ "$(git rev-list -n1 "$TAG")" == "$(git rev-parse HEAD)" ]]; then
    # Already tagged at HEAD: a re-run to fix or re-upload the artefact must not
    # be forced to move the tag. Measured 2026-10-16 -- the first release had to be
    # re-run to publish the right bytes with the tag already correct, and
    # aborting here would have meant --force-tag, moving a published tag.
    echo "==> Step 2: tag ${TAG} already exists and points at HEAD; keeping it"
  else
    echo "==> Step 2: tag ${TAG} already exists at a DIFFERENT commit." >&2
    echo "        Refusing to move a published tag without --force-tag." >&2
    exit 1
  fi
else
  echo "==> Step 2: creating git tag ${TAG}"
  if [[ $DRY_RUN -eq 1 ]]; then
    echo "  git tag ${TAG}"
  else
    git tag "$TAG"
  fi
fi

# --- 3. Push the tag ---------------------------------------------------------
echo "==> Step 3: pushing tag ${TAG}"
if [[ $DRY_RUN -eq 1 ]]; then
  echo "  git push origin ${TAG}"
else
  git push origin "$TAG"
fi

# --- 4. Create the GitHub Release --------------------------------------------
echo "==> Step 4: creating GitHub Release ${TAG}"
if [[ $DRY_RUN -eq 1 ]]; then
  echo "  gh release create ${TAG} --title \"hunmin ${VERSION}\" --notes-file docs/release-notes/${TAG}.md"
else
  gh release create "$TAG" --title "hunmin ${VERSION}" --notes-file "docs/release-notes/${TAG}.md"
fi

# --- 5. Upload the .deb ------------------------------------------------------
if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
  echo "==> Step 5: uploading ${ARTIFACT}"
  if [[ $DRY_RUN -eq 1 ]]; then
    echo "  gh release upload ${TAG} ${ARTIFACT}"
  else
    gh release upload "$TAG" "$ARTIFACT"
    echo "  uploaded ${ARTIFACT}"
    echo "  sha256 $(sha256sum "$ARTIFACT" | cut -d' ' -f1)"
  fi
fi

# --- 6. Reachability: can the audience this documents name actually fetch it? --
# The third verification issue #7 asks. Steps 1-5 all assert EXISTENCE -- the tag
# exists, the release exists, the asset is there, and f904d6a's fingerprinting
# made the bytes right. None of that is the question: a private repository serves
# a correct .deb to nobody. Measured 2026-10-16, when the 0.1 release was
# published with the right bytes behind a 404.
#
# So this fetches the asset URL with NO credentials -- not gh, which is
# authenticated as whoever runs it, and not a cookie -- and compares the status
# against the repository's real visibility. A private repo returning 404 is a
# recorded state (wiki/decisions/2026-10-16-the-repository-is-private-until-after-0.2.md);
# documents advertising an anonymous download while the repo is private is a
# contradiction, and that is what this refuses to finish on.
verify_reachable() {
  local url="https://github.com/${REPO}/releases/download/${TAG}/${ARTIFACT_NAME}"
  local notes="${ROOT}/docs/release-notes/${TAG}.md"
  local visibility status

  if [[ $DRY_RUN -eq 1 ]]; then
    echo "  curl -sS -o /dev/null -w '%{http_code}' ${url}   # no credentials"
    echo "  # then compare against \$(gh repo view ${REPO} --json visibility -q .visibility)"
    return 0
  fi

  command -v curl >/dev/null 2>&1 || { echo "curl not found (reachability check)" >&2; exit 1; }

  # No -L on purpose for the private case: following the redirect to a login page
  # turns "hidden" into "200", which is the exact false green this check exists to
  # prevent. A public asset serves 200 directly.
  status="$(curl -sS -o /dev/null -w '%{http_code}' "$url" 2>/dev/null || echo 000)"
  visibility="$(gh repo view "$REPO" --json visibility -q .visibility 2>/dev/null || echo UNKNOWN)"

  echo "==> Step 6: reachability"
  echo "  ${url}"
  echo "  anonymous GET -> ${status}; repository visibility -> ${visibility}"

  case "$visibility" in
    PUBLIC)
      if [[ "$status" != "200" ]]; then
        echo "ERROR: repository is PUBLIC but the asset URL returns ${status}, not 200." >&2
        echo "       A public release that a clean machine cannot fetch is the" >&2
        echo "       failure issue #7's third item names." >&2
        exit 1
      fi
      echo "  OK: public repository, anonymous download works."
      ;;
    PRIVATE)
      if [[ "$status" != "404" ]]; then
        echo "NOTE: repository is PRIVATE but the asset returned ${status} unauthenticated."
        echo "      Re-measure and update the private-until-0.2 ruling if this is now public."
      else
        echo "  OK: private repository, 404 unauthenticated -- the recorded state."
      fi
      # The documents must agree with the measurement. This is the part that
      # would have caught the published .deb whose README said no release existed.
      if [[ -f "$notes" ]] && ! grep -qiE 'private|account with access|no anonymous download' "$notes"; then
        echo "ERROR: the repository is private, but ${notes} does not say so." >&2
        echo "       The release body is what a user reads first; an access claim" >&2
        echo "       that the measurement contradicts is the defect this checks." >&2
        exit 1
      fi
      if ! grep -qiE 'private|account with access' "${ROOT}/README.md"; then
        echo "ERROR: the repository is private, but README.md does not say so." >&2
        exit 1
      fi
      echo "  OK: the release notes and README both state the access boundary."
      ;;
    *)
      echo "ERROR: could not read visibility for ${REPO} (got '${visibility}')." >&2
      echo "       Refusing to reach a verdict here is the point: an unknown" >&2
      echo "       visibility must not read as a pass." >&2
      exit 1
      ;;
  esac
}

verify_reachable

if [[ $DRY_RUN -eq 1 ]]; then
  echo
  echo "==> Dry run finished. Nothing was built, tagged, pushed, published or uploaded."
  echo "==> To do it for real: tools/release-0.1.sh --yes"
else
  echo "==> Done. GitHub Release: https://github.com/${REPO}/releases/tag/${TAG}"
fi
