#!/bin/sh
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# The context an agent needs BEFORE it touches an issue (protocol §3).
#
#   tools/agent-brief.sh <issue-number> [--full]
#
# Why this exists: the maintainer's example of a system fix worth having —
# "read the issue I am to handle, check git for what changed since it was filed,
# and look at the issues still pending, so the context is loaded before the work
# starts". All three were done by hand (and skipped) in the session that produced
# docs/AGENT-WORKING-PROTOCOL.md; three units of work went the wrong way because
# the issue's assumptions had moved and nobody compared them.
#
# Read-only. It never writes, never fetches, never touches a daemon. Everything it
# prints is a fact with a command behind it:
#
#   1  the issue: state, labels, age, premises/out-of-scope if the body states them
#   2  the paths the issue names, and whether they still exist
#   3  what changed on origin/master since the issue was filed (approximate: the
#      creation timestamp, because a commit id at filing time is not recorded)
#   4  whether this branch already carries work for the issue
#   5  the pending queue (open issues), so the work is placed in its context
#   6  the local state an agent must not get wrong (branch, dirty tree, builds)
#
# The issue number is an argument, never a default.
set -u

num=${1:-}
[ -n "$num" ] || { echo "usage: $0 <issue-number> [--full]" >&2; exit 2; }
full=0
[ "${2:-}" = "--full" ] && full=1
repo=$(git -C "$(dirname "$0")" remote get-url origin 2>/dev/null | sed 's|.*github.com[:/]||; s|\.git$||')
[ -n "$repo" ] || { echo "no origin remote — run this inside the checkout" >&2; exit 2; }

json=$(gh issue view "$num" --repo "$repo" --json number,title,state,labels,createdAt,updatedAt,body,comments 2>&1) \
    || { echo "cannot read issue #$num from $repo:"; echo "$json" | head -3; exit 1; }

say() { printf '%s\n' "$*"; }
py() { python3 -c "$@"; }

say "=== issue #$num ($repo)"
printf '%s' "$json" | python3 -c '
import json,sys
d=json.load(sys.stdin); b=d.get("body") or ""; c=d.get("comments") or []
print(f'"'"'  {d["title"]}  [{d["state"]}]'"'"')
print(f'"'"'  filed {d["createdAt"]}  updated {d["updatedAt"]}  comments {len(c)}'"'"')
print("  labels: " + ", ".join(l["name"] for l in d.get("labels") or []) if d.get("labels") else "  labels: none")
if c:
    last=c[-1]
    print(f'"'"'  last comment {last.get("createdAt","?")} by {last.get("author",{}).get("login","?")}'"'"')
import re
for head in ("Premises","Out of scope"):
    m=re.search(r"^#+\s*"+head+r"\s*$(.*?)(?=^#+\s|\Z)", b, re.M|re.S|re.I)
    if m:
        lines=[l.strip() for l in m.group(1).strip().splitlines() if l.strip()][:5]
        print(f"  {head}: " + " | ".join(lines))
    else:
        print(f"  {head}: NOT STATED — the context this issue assumed is not written down")
if sys.argv[1:]==["1"]:
    print("  --- body ---"); print(b)
' "$full"

say ""
say "=== paths the issue names"
printf '%s' "$json" | python3 -c '
import json,sys,re,os,subprocess
b=json.load(sys.stdin).get("body") or ""
paths=[]
for t in re.findall(r"`([^`\n]+)`", b):
    if ("/" in t or re.search(r"\.(md|c|h|cpp|kt|py|sh|json|jsonl|txt|cmake)$", t)) and not t.startswith("-"):
        if t not in paths: paths.append(t)
for p in paths[:10]:
    ok = os.path.exists(p) or subprocess.run(["git","ls-files","--error-unmatch",p],
                                             capture_output=True).returncode==0
    print(("  ok      " if ok else "  MISSING ") + p)
if not paths: print("  (none)")
'

say ""
say "=== what changed on origin/master since the issue was filed (creation timestamp)"
filed=$(printf '%s' "$json" | python3 -c 'import json,sys; print(json.load(sys.stdin)["createdAt"])')
git log --oneline --no-merges --since="$filed" origin/master 2>/dev/null | head -12 \
    | sed 's/^/  /' || say "  (git cannot compare; is origin/master fetched?)"
n=$(git log --oneline --no-merges --since="$filed" origin/master 2>/dev/null | wc -l)
say "  ($n commit(s) since $filed; the filing commit id is not recorded, so this is an upper bound)"

say ""
say "=== this branch already carries work for the issue?"
git log --oneline "origin/master..HEAD" --grep="#$num" 2>/dev/null | head -8 | sed 's/^/  /' \
    || true
[ -z "$(git log --oneline "origin/master..HEAD" --grep="#$num" 2>/dev/null)" ] && say "  (none with #$num in the message)"

say ""
say "=== pending queue (open issues, newest first)"
gh issue list --repo "$repo" --state open --limit 20 --json number,title,labels,createdAt 2>/dev/null \
    | python3 -c '
import json,sys
rows=json.load(sys.stdin)
for r in rows:
    tags=",".join(l["name"] for l in r.get("labels") or [])
    print("  #%-5s %s  %-70s %s" % (r["number"], r["createdAt"][0:10], r["title"][:70],
                                     ("["+tags+"]") if tags else ""))
if not rows: print("  (no open issues)")
' || say "  (cannot list issues)"

say ""
say "=== local state"
say "  branch: $(git rev-parse --abbrev-ref HEAD)  head: $(git log --oneline -1 2>/dev/null)"
dirty=$(git status --porcelain 2>/dev/null | wc -l)
say "  dirty files: $dirty"
for d in build build-trace; do
    if [ -d "$d" ]; then say "  $d: present"
    elif [ "$d" = build ]; then say "  build: absent — cmake -B build && cmake --build build"
    else say "  build-trace: absent — cmake -B build-trace -DHUNMIN_TRACE=ON"
    fi
done
say "  baseline before touching anything: ctest --test-dir build -j16 -LE slow"