#!/usr/bin/env bash
# release-0.1.sh — publish hunmin 0.1.0 as a GitHub Release with the .deb artifact.
#
# Method (a clean machine can reproduce this from the commit):
#   1. Build the .deb:     dpkg-buildpackage -us -uc -b   (unsigned binary only)
#   2. Create git tag:     0.1.0   (project version, no 'v' prefix — versionname
#                             ruling in docs/MILESTONES.md §0.1 names the release
#                             "0.1.0" bare, per issue #9)
#   3. Create GitHub Release:  gh release create 0.1.0 --title "hunmin 0.1.0" \
#                              --notes-file docs/release-notes/0.1.md
#   4. Upload artifact:    gh release upload 0.1.0 hunmin_0.1.0_amd64.deb
#
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
DRY_RUN=0
FORCE_TAG=0

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
  echo "==> Step 1: building ${ARTIFACT} (dpkg-buildpackage -us -uc -b)"
  if [[ $DRY_RUN -eq 1 ]]; then
    echo "  dpkg-buildpackage -us -uc -b"
  else
    dpkg-buildpackage -us -uc -b
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

echo "==> Done. GitHub Release: https://github.com/${REPO}/releases/tag/${TAG}"
