#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# ci-time-budget.sh -- measure the local suite against the CI job's budget,
# and say plainly where local and CI cannot agree.
#
# WHY THIS EXISTS. The linux job's ceiling is 5 minutes
# (.github/workflows/ci.yml), and the inner loop and slow tiers have their own
# budgets (AGENTS.md / docs/TESTING.md). A budget nobody measures locally is a
# budget discovered by a red CI run, and a red CI run costs a CI debug cycle --
# the exact overspend that got CI usage suspended. This script is how a change
# is checked against the budget BEFORE it is pushed.
#
# THE HONEST CAVEAT, which is the point rather than a disclaimer. Local and CI
# do not measure the same suite, for two structural reasons:
#
#   1. tier E1 (engine-android-e1, android-mutations) registers only where
#      ANDROID_HOME is set (CMakeLists.txt:192). On a host without the Android
#      SDK those tests are not skipped -- they are never registered, so local
#      cannot see the 232 s they cost in CI.
#   2. tier D3 (engine-d3-x11/-gtk3/-gtk4/-candidate) SKIPS on the runner
#      image, which ships neither xvfb nor the distro fcitx5 modules
#      (docs/CI-TIMING.md §1). So local pays for a tier CI never runs.
#
# So a green local run is not evidence about the CI budget by itself. This
# script reports BOTH numbers and names the delta, rather than pretending the
# two agree. Run it before pushing anything that adds a test.
#
# usage: tools/ci-time-budget.sh [build-dir]
set -u
BUILD="${1:-build}"
cd "$(dirname "$0")/.." || exit 1

# The budgets, in seconds. One place, so a change is one edit.
# 180 s: .github/workflows/ci.yml, the linux job's `timeout-minutes: 3`.
CI_JOB_BUDGET=180
INNER_LOOP_BUDGET=30   # `ctest -j16 -LE slow`  (AGENTS.md's inner loop)
SLOW_BUDGET=60         # `ctest -j16 -L slow`

# Cores: -j16 is the repo's documented local number (AGENTS.md); the runner
# has 4 and CI uses -j4. Both are reported because the budget is about wall
# clock, and wall clock moves with -j.
CORES="$(nproc)"

fail=0
report() { # $1=label $2=seconds $3=budget
    local over=""
    # Integer comparison on the measured value; awk for the ratio so a 0.4 s
    # run does not read as "0%".
    if [ "$(awk -v a="$2" -v b="$3" 'BEGIN{print (a>b)?1:0}')" = 1 ]; then
        over="  <-- OVER BUDGET"
        fail=1
    fi
    printf '  %-28s %8.1f s   budget %3d s  (%s%%)\n' \
        "$1" "$2" "$3" "$(awk -v a="$2" -v b="$3" 'BEGIN{printf "%.0f", (a/b)*100}')$over"
}

time_run() { # $1=ctest args... ; echoes elapsed seconds
    local start end
    start=$(date +%s.%N)
    ctest --test-dir "$BUILD" "$@" >/dev/null 2>&1
    end=$(date +%s.%N)
    awk -v a="$start" -v b="$end" 'BEGIN{printf "%.2f", b-a}'
}

echo "cores: $CORES   build: $BUILD"
echo "ANDROID_HOME: ${ANDROID_HOME:-(unset -- tier E1 will not register)}"
echo
echo "=== budgets ==="

# The inner loop. This is the one a contributor runs on every edit, so it is
# the one that must never creep.
report "inner loop -LE slow" "$(time_run -j"$CORES" -LE slow)" "$INNER_LOOP_BUDGET"

# The slow tier, which is the D3 set plus docs-lint plus the mutation tiers.
report "slow -L slow" "$(time_run -j"$CORES" -L slow)" "$SLOW_BUDGET"

# The whole suite minus mutation (what the linux job now runs, modulo tier E1
# which this host cannot register). Reported without a pass/fail budget of its
# own, because CI's copy of this number includes tier E1 and this one cannot.
whole=$(time_run -j"$CORES" -LE mutation)
printf '  %-28s %8.1f s   (no local budget -- see caveat)\n' "suite -LE mutation" "$whole"

echo
echo "=== what local cannot see (the local/CI delta) ==="
if [ -z "${ANDROID_HOME:-}" ]; then
    echo "  tier E1 is NOT REGISTERED here (ANDROID_HOME unset)."
    echo "  In CI it costs ~232 s: engine-android-e1 148 s + android-mutations"
    echo "  84 s, cold Gradle (docs/CI-TIMING.md §1). The linux job now excludes"
    echo "  tier E1 (-LE 'tier-E1'), so that cost moved to the android job and"
    echo "  is de-duplicated rather than removed."
else
    echo "  ANDROID_HOME is set; tier E1 registers here."
fi
d3=$(ctest --test-dir "$BUILD" -N -L tier-D3 2>/dev/null | grep -c 'Test #' || true)
echo "  tier D3 registers $d3 test(s) here and SKIPS on the runner image"
echo "  (no xvfb, no distro fcitx5 modules). Local pays for a tier CI never runs."

echo
if [ "$fail" -eq 0 ]; then
    echo "ci-time-budget: within budget"
else
    echo "ci-time-budget: OVER BUDGET -- do not push without trimming"
fi
exit "$fail"
