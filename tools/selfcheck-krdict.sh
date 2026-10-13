# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Determinism of tools/krdict2data.py against the real 국립국어원 dump
# (368 MB, gitignored, NOT committed — see tools/fetch-krdict.sh).
#
# NOT a ctest test. data-selfcheck (tier-B) must be self-contained in a fresh
# worktree, where data-orig/krdict/ does not exist: a dump-dependent run cannot
# live on the ctest surface. The self-contained guarantee for the converter's
# logic is ctest test krdict-fixture (tier-B), which diffs a committed fixture
# against a committed expected JSON. This script is the maintainer-only
# full-dump companion, run when the dump has been refetched or the converter
# has changed:
#
#   tools/fetch-krdict.sh            # once, 368 MB into data-orig/krdict/
#   tools/selfcheck-krdict.sh
#
# Compares the committed table libhunmin/data/krdict/krdict.json (what ships)
# against what the converter produces from the local dump today. Any diff is
# converter drift or a stale committed table; both are findings, not verdicts.
set -eu
root="$(cd "$(dirname "$0")/.." && pwd)"
DMP=${1:-"$root/data-orig/krdict"}
DATA="$root/libhunmin/data"

if [ ! -d "$DMP" ] || [ -z "$(ls "$DMP"/*.xml 2>/dev/null)" ]; then
  echo "selfcheck-krdict: no *.xml under $DMP" >&2
  echo "selfcheck-krdict: fetch the dump first: tools/fetch-krdict.sh" >&2
  echo "selfcheck-krdict: or rely on the self-contained ctest test krdict-fixture" >&2
  exit 2
fi

td=$(mktemp -d)
trap 'rm -rf "$td"' EXIT
python3 "$root/tools/krdict2data.py" --orig "$DMP" --out "$td"
diff -u "$DATA/krdict/krdict.json" "$td/krdict/krdict.json" \
  && echo "selfcheck-krdict: committed table matches the dump-derived table" \
  || echo "selfcheck-krdict: DIFFERS — committed table and converter output disagree; regenerate with:" \
          "  python3 tools/krdict2data.py --orig data-orig/krdict --out libhunmin/data"
