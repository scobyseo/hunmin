#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# ci-env-snapshot.sh -- capture the environment facts that decide whether a CI
# failure is a code defect or an environment fact, BEFORE the failure.
#
# WHY THIS RUNS FIRST, NOT ON FAILURE. A red CI run whose cause is
# environmental (a missing tool, an unset variable, a skip that registered as
# a pass) cannot be reproduced locally without knowing what the runner
# actually had. Collecting that after the failure means the information is
# still there; collecting it as a step that runs regardless means it is there
# even when the failure is a hard abort that skips later steps, and it can be
# diffed against a known-good run. That is the whole design: the snapshot is
# cheap, unconditional, and the diff is the debug.
#
# The alternative this replaces is re-running CI to find out what CI had. Each
# such run is a CI minute, and CI minutes are what got usage suspended --
# tools/ci-time-budget.sh is the budget half of the same concern.
#
# usage: tools/ci-env-snapshot.sh [build-dir]
set -u
BUILD="${1:-build}"
cd "$(dirname "$0")/.." || exit 1

section() { printf '\n===== %s =====\n' "$1"; }

section "runner"
{
    echo "uname: $(uname -a)"
    echo "nproc: $(nproc)"
    echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    # The runner image decides which tiers register at all; CI-TIMING.md §1
    # records the Ubuntu 24.04 image shipping an Android SDK the workflow
    # never installs, which is why tier E1 runs in a job called `linux`.
    if [ -r /etc/os-release ]; then
        echo "os: $(. /etc/os-release && echo "$PRETTY_NAME")"
    fi
} 2>&1

section "toolchain (what decides which tiers register)"
{
    for v in ANDROID_HOME ANDROID_NDK ANDROID_AVD_HOME JAVA_HOME HUNMIN_AVD \
             DISPLAY XDG_SESSION_TYPE; do
        # Print the NAME even when unset: "ANDROID_HOME: (unset)" is the fact
        # that explains a missing tier, and an absent line reads as "not
        # checked" rather than "absent".
        echo "$v: ${!v:-(unset)}"
    done
    for t in java adb cmake ninja pkg-config Xvfb xdotool dbus-run-session \
             pyright fcitx5; do
        if command -v "$t" >/dev/null 2>&1; then
            echo "$t: $(command -v "$t")"
        else
            echo "$t: MISSING"
        fi
    done
} 2>&1

section "fcitx5 (the addon's floor is 5.1.13)"
{
    if command -v pkg-config >/dev/null 2>&1; then
        echo "Fcitx5Core: $(pkg-config --modversion Fcitx5Core 2>/dev/null || echo 'not found via pkg-config')"
    else
        echo "pkg-config: MISSING"
    fi
    # The distro version, which is the one below the floor on Ubuntu 24.04
    # (5.1.7) and the reason the workflow builds 5.1.13 from source.
    if command -v fcitx5 >/dev/null 2>&1; then
        echo "fcitx5 binary: $(fcitx5 --version 2>/dev/null | head -1 || echo present)"
    fi
} 2>&1

section "registered tests by label"
{
    # The count per label is what makes a local/CI comparison possible at all:
    # a tier that does not register locally is not skipped, it is absent, and
    # the two look identical in a pass/fail summary.
    ctest --test-dir "$BUILD" -N 2>/dev/null | tail -1
    for lbl in tier-A tier-B tier-C tier-D1 tier-D2 tier-D3 tier-E1 tier-G \
               slow mutation; do
        n=$(ctest --test-dir "$BUILD" -N -L "$lbl" 2>/dev/null |
            grep -c 'Test #' || true)
        printf '  %-10s %s\n' "$lbl" "$n"
    done
} 2>&1

section "data inputs (a missing one is a skip, not a pass)"
{
    # The krdict dump is gitignored and 368 MB; its absence makes data-selfcheck
    # fail rather than skip in a fresh worktree (REMAINING-WORK.md §0, issue
    # #46). Whether that should be a skip or a fail is a maintainer ruling, so
    # this records the fact and does not interpret it.
    for p in libhunmin/data/hanja/hanja.json \
             libhunmin/data/krdict \
             libhunmin/data/idioms \
             libhunmin/data/layouts/sin3-p2.json; do
        if [ -e "$p" ]; then
            echo "$p: present"
        else
            echo "$p: ABSENT"
        fi
    done
} 2>&1

echo
echo "snapshot complete"
