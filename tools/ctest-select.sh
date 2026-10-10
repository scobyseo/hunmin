#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# ctest-select.sh -- run part of the ctest suite by LABEL, and print what it left out.
#
#   tools/ctest-select.sh -L tier-A            # list the selection, then run it
#   tools/ctest-select.sh -n -L tier-D         # print the selection only
#   tools/ctest-select.sh -LE slow             # everything under the slow cut
#   tools/ctest-select.sh -L 'tier-A|tier-B'   # regexes work; so do -R, -E
#   HUNMIN_BUILD_DIR=build-trace tools/ctest-select.sh -L tier-C
#
# Every other argument is handed to ctest untouched, so this is a wrapper, not
# a subset of ctest.
#
# WHY THIS EXISTS
# ---------------
# `ctest -L <label>` prints the tests it selected and nothing about the ones it
# did not. A selection that passes therefore reads exactly like a full run that
# passed, and this repo has been bitten by that shape before: engine-d3-x11
# reported "Passed" in 5.01 s having run `dubeolsik/dkssuds` and none of the
# other seven cases (wiki/raw/measurements/2026-10-10-d3-case-coverage.md).
# Nothing errored; the loop simply had no more input.
#
# So the omission is printed, by name, before anything runs. A green run of
# `tools/ctest-select.sh -L tier-C` is followed by the list of what it did not
# check, which is the thing that makes it readable as a partial result.
#
# WHY IT ASKS CTEST TWICE
# -----------------------
# The selected set is whatever `ctest -N <args>` says it is, not a re-reading
# of the label vocabulary. Re-implementing -L/-LE/-R here would let the wrapper
# and ctest disagree, and a wrapper that selects something other than what it
# printed is worse than no wrapper.
#
# This is a LOCAL convenience and deliberately not a gate: .github/workflows/
# ci.yml runs the plain unfiltered `ctest`, because a filtered run cannot prove
# what it skipped.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${HUNMIN_BUILD_DIR:-$ROOT/build}"
DRY_RUN=0
ARGS=()
JOBS_SET=0

usage() {
    sed -n '3,31p' "$0" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

while [ $# -gt 0 ]; do
    case "$1" in
        -n|--dry-run) DRY_RUN=1 ;;
        -h|--help)    usage 0 ;;
        -j*)          JOBS_SET=1 ;;
        *)            ARGS+=("$1") ;;
    esac
    shift
done

if [ ! -f "$BUILD/CTestTestfile.cmake" ]; then
    echo "ctest-select: no configured build tree at $BUILD" >&2
    echo "  cmake -B \${HUNMIN_BUILD_DIR:-$BUILD} && cmake --build \$HUNMIN_BUILD_DIR" >&2
    exit 2
fi

names() {  # ctest args... -> test names, one per line, in ctest's own order
    ctest --test-dir "$BUILD" -N "$@" 2>/dev/null \
        | sed -n 's/^[[:space:]]*Test[[:space:]]*#[0-9]*:[[:space:]]*//p'
}

ALL=()
mapfile -t ALL < <(names)
SEL=()
mapfile -t SEL < <(names "${ARGS[@]+"${ARGS[@]}"}")

# Set difference, order-independent, over the names ctest just printed.
omit() {
    local keep=" $* "
    local n
    for n in "${ALL[@]}"; do
        case "$keep" in *" $n "*) ;; *) printf '%s\n' "$n" ;; esac
    done
}

printf '== ctest selection ==\n'
printf 'build      %s\n' "$BUILD"
printf 'args       ctest %s\n' "${ARGS[*]-<none>}"
printf 'selected   %d of %d registered tests\n' "${#SEL[@]}" "${#ALL[@]}"
if [ "${#SEL[@]}" -gt 0 ]; then
    printf '  run:      %s\n' "${SEL[@]/#/                 }"
fi

mapfile -t SKIPPED < <(omit "${SEL[@]+"${SEL[@]}"}")
if [ "${#SKIPPED[@]}" -gt 0 ]; then
    printf 'NOT RUN    %d tests this invocation makes no claim about:\n' "${#SKIPPED[@]}"
    printf '  skip:     %s\n' "${SKIPPED[@]/#/                 }"
else
    printf 'NOT RUN    none -- this is the whole suite\n'
fi

if [ "$DRY_RUN" -eq 1 ]; then
    exit 0
fi

printf '\n== running ==\n'
if [ "$JOBS_SET" -eq 0 ]; then
    # ctest's own default is serial, which is never what anyone wants here.
    set -- -j"$(nproc 2>/dev/null || echo 4)"
else
    set --
fi
exec ctest --test-dir "$BUILD" "$@" ${ARGS[@]+"${ARGS[@]}"}