#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# orchestrate.sh -- drive `orca orchestration` with this repository's defaults.
#
#   tools/orchestrate.sh doctor                        # runtime, binding, flag surface
#   tools/orchestrate.sh run-new --objective "…"        # create + bind a Run
#   tools/orchestrate.sh run-bind run_1a8a557d047b     # bind an existing Run
#   tools/orchestrate.sh task --spec "…" [--title T] [--issue N]
#   tools/orchestrate.sh start --task task_x --agent hermes --issue 4 --slug r8-shrinker
#   tools/orchestrate.sh wait [--timeout-ms 900000]    # blocking; prints, does not ack
#   tools/orchestrate.sh board                         # tasks + who owes a decision
#   tools/orchestrate.sh show --dispatch disp_x        # one dispatch
#   tools/orchestrate.sh settle --dispatch disp_x --decision release|retain
#
# Any command may be run with --dry-run: the exact argv is printed and nothing runs.
#
# WHY THIS EXISTS
# ---------------
# `orca orchestration` has a large, exact flag surface, and none of it is on this
# repository's man page: `docs/ENVIRONMENT.md` §Orca GUI stops at `orca worktree
# create` and the `orca terminal` verbs. The measured cost of that gap is in
# `docs/ORCHESTRATION.md` -- a Hermes session spent ~40 turns discovering flags
# that one `orca agent-context --json` call already answers, and started no worker.
#
# So this wrapper does two things and refuses to do a third:
#
#   1. It supplies what is genuinely this repository's: the `task/<id>-<slug>`
#      branch name, `--base-branch master`, and the issue number that AGENTS.md's
#      "GitHub issue tracking" section requires to travel with the task.
#   2. Before each call it validates every flag it is about to pass against
#      `orca agent-context --json` -- the binary's own machine-readable schema --
#      and aborts with the schema's own wording if the CLI has moved. It never
#      guesses a flag and never falls back to a different executable.
#
# It deliberately does NOT wrap the settlement protocol (check / reply / ack /
# release). That sequence has correctness rules that live in the version-matched
# guide (`orca skills get orchestration`), and a wrapper that "helpfully" acks a
# batch would be a wrapper that could destroy a delivery. `wait` prints and stops.
#
# The screen-reader hazard on Linux is real and this script refuses to walk into
# it: outside an Orca-managed terminal, bare `orca` is the GNOME Orca speech
# tool on this host, and running it starts talking. Resolution follows the
# Orca skill's rule -- $ORCA_CLI_COMMAND, else the shim/`orca-ide`, and bare
# `orca` only when it demonstrably is Orca.
set -euo pipefail

DRY_RUN=0
ORCA=""
CONTEXT=""
TMP_TASKS="$(mktemp)"
TMP_WORKERS="$(mktemp)"

die() { printf 'orchestrate: %s\n' "$*" >&2; exit "${2:-1}"; }

usage() { sed -n '3,16p' "$0" | sed 's/^# \{0,1\}//'; exit "${1:-0}"; }

# ---------------------------------------------------------------- executable --

resolve_orca() {
    local candidate resolved
    if [ -n "${ORCA_CLI_COMMAND:-}" ]; then
        ORCA="$ORCA_CLI_COMMAND"
        return
    fi
    # Inside an Orca terminal the shim is on PATH; outside it, prefer the
    # unambiguous `orca-ide`. `orca` is only accepted when it is Orca's shim.
    if [ -n "${ORCA_WORKTREE_ID:-}" ] || [ "${TERM_PROGRAM:-}" = Orca ]; then
        candidate="$(command -v orca 2>/dev/null || true)"
        if [ -n "$candidate" ] && grep -q 'orca-ide' "$candidate" 2>/dev/null; then
            ORCA="$candidate"
            return
        fi
    fi
    if resolved="$(command -v orca-ide 2>/dev/null)"; then
        ORCA="$resolved"
        return
    fi
    die "no Orca CLI: set ORCA_CLI_COMMAND, or put Orca's shim/orca-ide on PATH" 2
}

# Not named `orca`: a shell function would shadow the real binary and make
# `command -v orca` print the function's own name instead of a path.
orca_cli() { resolve_orca; "$ORCA" "$@"; }

# ------------------------------------------------------------ flag validation --

# Flags are checked against the binary that will run them, not against a copy of
# its help text: an Orca upgrade changes the surface, and the check is one call.
load_context() {
    [ -n "$CONTEXT" ] && return 0
    CONTEXT="$(mktemp)"
    trap 'rm -f "$CONTEXT" "$TMP_TASKS" "$TMP_WORKERS"' EXIT
    orca_cli agent-context --json >"$CONTEXT" 2>/dev/null \
        || die "orca agent-context --json failed; cannot validate flags" 3
}

# require_flags <schema-command> <flag>...
require_flags() {
    local cmd="$1"; shift
    load_context
    python3 - "$CONTEXT" "$cmd" "$@" <<'PY'
import json, sys
path, cmd, wanted = sys.argv[1], sys.argv[2], sys.argv[3:]
data = json.load(open(path))
entry = next((c for c in data.get("commands", []) if c.get("command") == cmd), None)
if entry is None:
    sys.exit(f"orchestrate: this orca has no command '{cmd}'")
known = {f.lstrip("-") for f in entry.get("flags", [])}
missing = [f for f in wanted if f.lstrip("-") not in known]
if missing:
    sys.exit(
        "orchestrate: refusing to guess. '{cmd}' does not report {missing}; "
        "it reports {have}. Re-read `orca orchestration {leaf} --help` and update "
        "this script.".format(
            cmd=cmd,
            missing=", ".join("--" + m.lstrip("-") for m in missing),
            have=", ".join("--" + h for h in sorted(known)),
            leaf=cmd.split()[-1],
        )
    )
PY
}

# ------------------------------------------------------------------- runner ----

run() {  # run <flag>... -- <argv...>; the schema name is argv[0] argv[1]
    local flags=() argv=()
    while [ "$1" != "--" ]; do flags+=("$1"); shift; done
    shift
    argv=("$@")
    require_flags "${argv[0]} ${argv[1]}" "${flags[@]+"${flags[@]}"}"
    if [ "$DRY_RUN" -eq 1 ]; then
        printf 'would run: %s' "$ORCA"
        printf ' %q' "${argv[@]}"
        printf '\n'
        return 0
    fi
    "$ORCA" "${argv[@]}"
}

# ------------------------------------------------------------------ commands --

cmd_doctor() {
    resolve_orca
    printf '== orca ==\n'
    printf 'executable   %s\n' "$ORCA"
    printf 'terminal     %s\n' "${ORCA_WORKTREE_ID:-<not an Orca terminal>}"
    printf 'app version  %s\n' "${ORCA_APP_VERSION:-<unknown>}"
    printf 'runtime      %s\n' "$(orca_cli status --json | python3 -c 'import json,sys; print(json.load(sys.stdin)["result"]["runtime"]["state"])')"

    printf '\n== bound run ==\n'
    orca_cli orchestration run-current --json | python3 -c '
import json, sys
run = json.load(sys.stdin).get("result", {}).get("run")
print("none" if not run else "{id}  gen {g}  {o}".format(
    id=run["id"], g=run.get("consumer_generation"), o=run["objective"]))'

    printf '\n== flag surface this script depends on ==\n'
    load_context
    python3 - "$CONTEXT" <<'PY'
import json, sys
data = json.load(open(sys.argv[1]))
want = {
    "orchestration run-create": ["--objective", "--json"],
    "orchestration run-use": ["--id", "--json"],
    "orchestration run-current": ["--json"],
    "orchestration task-create": ["--spec", "--task-title", "--json"],
    "orchestration task-list": ["--brief", "--json"],
    "orchestration worker-start": ["--task", "--agent", "--worktree", "--name",
                                   "--base-branch", "--comment", "--json"],
    "orchestration worker-show": ["--dispatch", "--json"],
    "orchestration worker-read": ["--dispatch", "--source", "--limit", "--json"],
    "orchestration worker-list": ["--terminal-state", "--json"],
    "orchestration worker-release": ["--dispatch", "--json"],
    "orchestration worker-retain": ["--dispatch", "--json"],
    "orchestration check": ["--wait", "--types", "--timeout-ms", "--json"],
    "orchestration reply": ["--id", "--body", "--json"],
}
bad = 0
for cmd, flags in want.items():
    entry = next((c for c in data.get("commands", []) if c.get("command") == cmd), None)
    if entry is None:
        print(f"  MISSING COMMAND  {cmd}")
        bad = 1
        continue
    known = {f.lstrip("-") for f in entry.get("flags", [])}
    gone = [f for f in flags if f.lstrip("-") not in known]
    print(f"  {'ok  ' if not gone else 'DRIFT'} {cmd}" + (f"   missing: {' '.join(gone)}" if gone else ""))
    bad = bad or bool(gone)
sys.exit(bad)
PY
}

cmd_run_new() {
    local objective=""
    while [ $# -gt 0 ]; do
        case "$1" in
            --objective) objective="$2"; shift 2 ;;
            *) die "run-new: unknown argument $1" ;;
        esac
    done
    [ -n "$objective" ] || die "run-new: --objective is required"
    run --objective --json -- \
        orchestration run-create --objective "$objective" --json
}

cmd_run_bind() {
    [ $# -eq 1 ] || die "run-bind: exactly one run id"
    run --id --json -- orchestration run-use --id "$1" --json
}

cmd_task() {
    local spec="" title="" issue=""
    while [ $# -gt 0 ]; do
        case "$1" in
            --spec) spec="$2"; shift 2 ;;
            --title) title="$2"; shift 2 ;;
            --issue) issue="$2"; shift 2 ;;
            *) die "task: unknown argument $1" ;;
        esac
    done
    [ -n "$spec" ] || die "task: --spec is required"
    # worker-start has no --issue flag, so the GitHub link has to ride inside the
    # spec. Appending it here keeps issue <-> Task linkage a property of the task
    # rather than of whoever remembers to mention it in a prompt.
    if [ -n "$issue" ] && [ "${spec#*\#"$issue"}" = "$spec" ]; then
        spec="$spec

GitHub issue: scobyseo/hunmin#$issue -- the issue carries the Verification checklist."
    fi
    local argv=(orchestration task-create --spec "$spec" --json)
    [ -n "$title" ] && argv+=(--task-title "$title")
    run --spec --task-title --json -- "${argv[@]}"
}

cmd_start() {
    local task="" agent="" issue="" slug="" mode="new-child" base="master" comment=""
    while [ $# -gt 0 ]; do
        case "$1" in
            --task) task="$2"; shift 2 ;;
            --agent) agent="$2"; shift 2 ;;
            --issue) issue="$2"; shift 2 ;;
            --slug) slug="$2"; shift 2 ;;
            --worktree) mode="$2"; shift 2 ;;
            --base-branch) base="$2"; shift 2 ;;
            --comment) comment="$2"; shift 2 ;;
            *) die "start: unknown argument $1" ;;
        esac
    done
    [ -n "$task" ] || die "start: --task is required"
    [ -n "$agent" ] || die "start: --agent is required (the id this host runs; \
see \`orca terminal list --json\` -> agentIdentity)"
    local argv=(orchestration worker-start --task "$task" --agent "$agent" --json)
    [ -n "$mode" ] && argv+=(--worktree "$mode")
    # AGENTS.md: one task, one worktree off master, branch task/<id>-<slug>.
    if [ -n "$issue" ] || [ -n "$slug" ]; then
        [ -n "$issue" ] && [ -n "$slug" ] || die "start: --issue and --slug go together"
        argv+=(--name "task/${issue}-${slug}" --base-branch "$base")
    fi
    if [ -n "$comment" ]; then argv+=(--comment "$comment"); fi
    run --task --agent --worktree --name --base-branch \
        --comment --json -- "${argv[@]}"
    if [ "$DRY_RUN" -eq 0 ] && [ -n "$issue" ]; then
        printf 'note: worker-start has no --issue; the GitHub link rides in the task\n'
        printf '      spec and this worktree comment. Check the Orca-side link with\n'
        printf '      orca worktree list --json  (linkedIssue).\n'
    fi
}

cmd_wait() {
    local types="worker_done,escalation,question" timeout=900000
    while [ $# -gt 0 ]; do
        case "$1" in
            --types) types="$2"; shift 2 ;;
            --timeout-ms) timeout="$2"; shift 2 ;;
            *) die "wait: unknown argument $1" ;;
        esac
    done
    run --wait --types --timeout-ms --json -- \
        orchestration check --wait --types "$types" --timeout-ms "$timeout" --json
    # shellcheck disable=SC2016  # literal command names, not expansions
    printf 'note: nothing was acknowledged. Process the batch (reply / validate each\n'
    # shellcheck disable=SC2016
    printf '      worker_done against the expected dispatch), then re-run with --ack\n'
    # shellcheck disable=SC2016
    printf '      <delivery_id> exactly as `orca skills get orchestration` describes.\n'
}

cmd_board() {
    orca_cli orchestration task-list --brief --json >"$TMP_TASKS" 2>/dev/null \
        || orca_cli orchestration task-list >"$TMP_TASKS"
    orca_cli orchestration worker-list --terminal-state reclaimable --json >"$TMP_WORKERS"
    python3 - "$TMP_TASKS" "$TMP_WORKERS" <<'PY'
import json, sys


def load(path):
    raw = open(path).read().strip()
    try:
        return json.loads(raw).get("result", {})
    except json.JSONDecodeError:
        return {"_text": raw}


tasks = load(sys.argv[1])
workers = load(sys.argv[2])
if "_text" in tasks:
    print(tasks["_text"])
else:
    print("== tasks ==")
    for t in tasks.get("tasks", []):
        print(f"  {t.get('id')}  {t.get('status','')}  {t.get('task_title','')}")

rows = workers.get("workers", [])
print("\n== terminals owing a decision ==")
if not rows:
    print("  none -- every settled dispatch has been reused, retained or released")
else:
    for w in rows:
        p = w.get("projection", w)
        print(f"  {p.get('dispatchId')}  {p.get('attention')}  {p.get('nextAction')}")
counts = workers.get("counts") or {}
if counts:
    print("  counts: " + ", ".join(f"{k}={v}" for k, v in sorted(counts.items())))
PY
    rm -f "$TMP_TASKS" "$TMP_WORKERS"
}

cmd_show() {
    local dispatch=""
    while [ $# -gt 0 ]; do
        case "$1" in --dispatch) dispatch="$2"; shift 2 ;; *) die "show: unknown argument $1" ;; esac
    done
    [ -n "$dispatch" ] || die "show: --dispatch is required"
    run --dispatch --json -- \
        orchestration worker-show --dispatch "$dispatch" --json
}

cmd_settle() {
    local dispatch="" decision=""
    while [ $# -gt 0 ]; do
        case "$1" in
            --dispatch) dispatch="$2"; shift 2 ;;
            --decision) decision="$2"; shift 2 ;;
            *) die "settle: unknown argument $1" ;;
        esac
    done
    [ -n "$dispatch" ] && [ -n "$decision" ] || die "settle: --dispatch and --decision are required"
    case "$decision" in
        release) run --dispatch --json -- \
                     orchestration worker-release --dispatch "$dispatch" --json ;;
        retain)  run --dispatch --json -- \
                     orchestration worker-retain --dispatch "$dispatch" --json ;;
        *) die "settle: --decision is release or retain, nothing else" ;;
    esac
}

# --------------------------------------------------------------------- main ----

[ $# -gt 0 ] || { usage 1; }
CMD="$1"; shift
case "$CMD" in
    -h|--help|help) usage 0 ;;
esac
ARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --dry-run) DRY_RUN=1 ;;
        *) ARGS+=("$1") ;;
    esac
    shift
done
[ "$CMD" = doctor ] && DRY_RUN=0

resolve_orca
case "$CMD" in
    doctor)    cmd_doctor "${ARGS[@]+"${ARGS[@]}"}" ;;
    run-new)   cmd_run_new "${ARGS[@]+"${ARGS[@]}"}" ;;
    run-bind)  cmd_run_bind "${ARGS[@]+"${ARGS[@]}"}" ;;
    task)      cmd_task "${ARGS[@]+"${ARGS[@]}"}" ;;
    start)     cmd_start "${ARGS[@]+"${ARGS[@]}"}" ;;
    wait)      cmd_wait "${ARGS[@]+"${ARGS[@]}"}" ;;
    board)     cmd_board "${ARGS[@]+"${ARGS[@]}"}" ;;
    show)      cmd_show "${ARGS[@]+"${ARGS[@]}"}" ;;
    settle)    cmd_settle "${ARGS[@]+"${ARGS[@]}"}" ;;
    *)         usage 1 ;;
esac