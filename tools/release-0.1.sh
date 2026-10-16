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
# Idempotency: running --yes twice recomputes the tag (git fails on an existing
# tag, so it reports and stops unless --force-tag is also passed) and re-uploads
# the artifact (gh release upload overwrites by default).
#
set -euo pipefail

REPO="${REPO:-scobyseo/hunmin}"
VERSION="0.1.0"
TAG="0.1.0"
ARTIFACT="hunmin_${VERSION}_amd64.deb"
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
BUILD_RELEASE="${SCRIPT_DIR}/build-release.sh"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --yes)      DRY_RUN=0 ;;
    --dry-run)  DRY_RUN=1 ;;
    --force-tag) FORCE_TAG=1 ;;
    --skip-build)
      shift
      ARTIFACT="${1:-hunmin_${VERSION}_amd64.deb}"
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
    "$BUILD_RELEASE" deb
    if [[ ! -f "$ARTIFACT" ]]; then
      echo "ERROR: ${ARTIFACT} not produced" >&2; exit 1
    fi
  fi
fi

# --- 2. Create the git tag ---------------------------------------------------
if git rev-parse -q --verify "refs/tags/${TAG}" >/dev/null 2>&1; then
  if [[ $FORCE_TAG -eq 1 ]]; then
    echo "==> Step 2: tag ${TAG} already exists; force-updating it"
    git tag -d "$TAG" && git tag "$TAG"
  else
    echo "==> Step 2: tag ${TAG} already exists (use --force-tag to overwrite)" >&2
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
  fi
fi

if [[ $DRY_RUN -eq 1 ]]; then
  echo
  echo "==> Dry run finished. Nothing was built, tagged, pushed, published or uploaded."
  echo "==> To do it for real: tools/release-0.1.sh --yes"
else
  echo "==> Done. GitHub Release: https://github.com/${REPO}/releases/tag/${TAG}"
fi
