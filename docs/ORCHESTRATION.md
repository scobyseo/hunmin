# ORCHESTRATION — running a second agent on this repository safely

**The one idea.** Orca's orchestration layer already owns placement, authority and
settlement; this repository's job is to hand it a **Task whose acceptance is a
command**, and to hold the line that a worker reports and the maintainer disposes.

Written 2026-10-11 against **Orca 1.4.219**. Everything in "Measured" was measured on
this host that day; everything in the procedure above it is what the version-matched
guide says, and the guide is served by the binary so it cannot drift from the binary:

```sh
orca skills get orchestration                      # semantics, matched to the binary
orca skills get orchestration --references         # the seven action-gate references
orca skills get orchestration --reference references/coordinator-loop.md
```

The host facts — where Orca is installed, how the shim resolves, what the AVD is —
stay in [`ENVIRONMENT.md` §Orca GUI](ENVIRONMENT.md#orca-gui-installed-terminaleditor--worktree-orchestration).
This file is the procedure. `AGENTS.md` points here and holds nothing of this.

---

## 1. The two discovery surfaces, one call each

| question | call | what it answers |
|---|---|---|
| *what am I allowed to do, and what must I never do* | `orca skills get orchestration` | the Run/Task/Dispatch contract, the settlement protocol, the safety floor |
| *what flags does this command actually take* | `orca agent-context --json` | `commands[].usage`, `.flags`, `.notes`, `.examples` for **all** 239 commands |

Both are one call. Guessing flags instead of reading them is the expensive path, and
it was measured: see §5.

```sh
orca agent-context --json | python3 -c '
import json,sys
for c in json.load(sys.stdin)["commands"]:
    if c["command"].startswith("orchestration worker"):
        print(c["usage"]); print("  flags:", " ".join(sorted(c["flags"])))'
```

Note the shape: `flags` entries are **bare names**, not `--names`. A checker that
compares against `--json` verbatim reports drift on every command — that bug was in
the first draft of `tools/orchestrate.sh` and was caught by running it.

### Measured traps (2026-10-11, from the session that motivated this file)

| symptom | the actual form |
|---|---|
| `Unknown flag --run for command: orchestration run-use` | `run-use --id <run_id>` |
| `Unknown flag --run for command: orchestration run-show` | `run-show --id <run_id>` |
| `Unknown command: run-list` | it exists; `run-list [--limit n] [--cursor c]`. The error came from a typo, not a missing command |
| `Unknown command: orchestration task-show` | no such command — `task-list [--status s] [--ready] [--brief]` |
| `Unknown flag --agent for command: orchestration task-create` | `task-create` has no `--agent`; use `--spec`, `--task-title`, `--deps` |
| `New worktrees require --name` | `worker-start --worktree new-child --name task/<id>-<slug>` |
| `consumer_fenced` on `worker-start` | the *coordinator terminal* must be bound to the Run first: `run-use --id <run_id>`. `run-create` binds as a side effect |

`consumer_fenced` is the one that costs the most, because it looks like a permissions
error and it is actually a missing binding.

---

## 2. The sequence

`tools/orchestrate.sh` is steps 1–3, 6 and 8 with this repository's defaults; the raw
`orca` form is given so the doc never becomes the only way to do the work.

```sh
tools/orchestrate.sh doctor          # runtime, bound run, flag-surface drift
tools/orchestrate.sh run-new --objective "…"
tools/orchestrate.sh task --spec "$(cat spec.md)" --title "…" --issue <N>
tools/orchestrate.sh start --task task_… --agent hermes --issue <N> --slug <slug>
tools/orchestrate.sh wait            # blocking; prints the batch, acknowledges nothing
tools/orchestrate.sh board           # tasks, and terminals owing a decision
tools/orchestrate.sh settle --dispatch ctx_… --decision release|retain
```

1. **Bind a Run.** `orca orchestration run-create --objective "…"` creates *and* binds
   it to this terminal. A Run is a namespace and an inbox; it schedules nothing.
   Re-binding later is `run-use --id <run_id>`.
2. **Write the Task spec** — §3. `task-create --spec … --task-title …`, or let
   `worker-start --spec` create and start in one call when there is no dependency to
   record.
3. **Start the worker.** `worker-start --task <id> --agent <id> --worktree new-child
   --name task/<id>-<slug> --base-branch master`. This is the normal path: it composes
   placement, terminal readiness, prompt injection and resource ownership. It exits 0
   only for *ready*; on failure read `failedStage` and `residualResources` from the
   receipt and load `references/recovery-and-cleanup.md` — **do not relaunch it.**
4. **Wait.** `check --wait --types "worker_done,escalation,question" --timeout-ms N`.
   A timeout or an empty result is a checkpoint, not a failure.
5. **Process every message before acknowledging it.** Reply to questions with `reply
   --id <msg_id> --body "…"`, validate each `worker_done` against the expected active
   Dispatch, and decide each settled terminal's next owner. Only then
   `check --ack <delivery_id> --wait …` to take the next batch.
6. **Settle, exactly once per accepted report:** reuse the terminal with a follow-up
   Dispatch, or `worker-retain` it, or `worker-release` it. Release is post-settlement
   cleanup, not cancellation.
7. **Do not end the coordinator turn while `worker-list --terminal-state reclaimable`
   returns a row.** That command is the audit of "what still owes a decision".
8. **Clean up the worktree** under the existing rule, not an automatic one:
   [`ENGINEERING.md` §Worktree cleanup](ENGINEERING.md#worktree-cleanup-when-a-unit-of-work-finishes).

### Worker-side rules the spec must not contradict

The injected preamble is authoritative and already carries them: do only the current
Task, `ask` the coordinator for anything blocking (never a local question TUI), send
heartbeats at the stated cadence, `check` for follow-ups at every checkpoint, and send
`worker_done` **once** with both lifecycle ids and `--outcome succeeded|failed`. The
spec's job is the branch, the issue and the acceptance command — the things the
preamble cannot know.

---

## 3. The task spec

Five fields, and the fifth is what separates a worker that finishes from a worker that
reports. Template in §6.

- **Target** — the files/component in scope, named.
- **Change** — the concrete result to produce.
- **Constraints** — invariants, do-not-touch boundaries, and the host setup the
  worktree needs (a fresh worktree has no gitignored `local.properties`, no exported
  `JAVA_HOME`).
- **Ownership** — what this worker may edit, and what it must not close.
- **Observable acceptance** — a command per claim, pass or fail.

### Naming the acceptance command

| kind of change | acceptance to put in the spec |
|---|---|
| engine / C | `ctest --test-dir build -j$(nproc) -L tier-A`, plus `-LE slow` for the inner loop |
| layout data | `python3 tools/doctor.py libhunmin/data` |
| goldens | `./build/hunmin-sim --data libhunmin/data --check tests/golden/ref-dubeolsik.jsonl` |
| ABI | the `nm --defined-only … > libhunmin/tests/abi/symbols.txt` command in `AGENTS.md`, and `ctest -R abi` |
| Android | `tools/build-android.sh`, `tools/android-unit.sh`, `tools/android-e2e.sh` (the last needs the AVD) |
| docs / wiki | `python3 tools/docs-lint.py --build-dir build`, `python3 tools/wiki-lint.py` |

Select by label, never by test name (the Android corpus test names are generated from
filenames and break silently), and say which tier was run — a filtered run is not a
full run, and `tools/ctest-select.sh` exists precisely because that shape hides.

**One test is red in a fresh worktree regardless**: `data-selfcheck`, which needs the
gitignored 국립국어원 dump. A worker must be told that in advance or it will read the
red as its own.

---

## 4. Issue ↔ Task ↔ Dispatch ↔ worktree ↔ branch

`AGENTS.md`'s issue-tracking section requires the issue to travel with the task.
Measured constraint: **`worker-start` has no `--issue` flag.** `orca worktree create`
does (`--issue <number>`, stored as `linkedIssue`), so there are two routes:

| route | how | what you get | what it costs |
|---|---|---|---|
| spec-borne | put `scobyseo/hunmin#N` in the spec (what `orchestrate.sh task --issue` does) and in the commit body | the link is in the Task, the transcript and the branch history | Orca's own `linkedIssue` stays `null` |
| worktree-borne | `orca worktree create --name … --issue N --base-branch master`, then `worker-start --worktree issue:<N>` | Orca's `linkedIssue` set; `issue:<N>` becomes a selector | an extra call and a terminal the create step opens |

Both are defensible. The repo currently takes the first; `orchestrate.sh` says so on
stderr when you pass `--issue` so the gap is never silent.

---

## 5. Measured on 2026-10-11 (Orca 1.4.219)

The baseline that motivated the file: the Hermes session
`20261011_101505_146066` spent **≈40 turns** discovering flags that one
`orca agent-context --json` call answers, created **two Runs by accident**, and
started **no worker at all**. Its transcript is the evidence; the numbers above are
its own.

The path this document describes, for the same shape of work — create a Run, one Task,
dispatch one worker, read it, settle it — cost **20 `orca` invocations, 0 rejected
flags, 0 retries**, and produced a real commit (`39b6532`, issue #4's measurement,
`docs/REMAINING-WORK.md` §3 and a new manual checklist; nothing outside `docs/` and
`tests/manual-checklists/`). Dispatch half:

| # | invocation | result |
|---|---|---|
| 1 | `orchestrate.sh doctor` | executable, runtime `ready`, bound run `none`, 13/13 flags ok |
| 2 | `orchestrate.sh run-new --objective …` | `run_c3eaa208eaa7`, bound to this terminal |
| 3 | `orchestrate.sh task --spec … --issue 4` | `task_a655ac90b99f [ready]` |
| 4 | `orchestration task-list` (id lookup) | — |
| 5 | `orchestrate.sh start --task … --agent hermes --issue 4 --slug r8-shrinker` | `ctx_09a1621c9a27`, exit 0 |
| 6 | `worker-list --json` | `liveness.verdict: live`, `attention.requiresAction: false` |
| 7 | `worktree list --json` | placement (below) |
| 8 | `worker-read --source auto` | terminal readable while `terminalHandle` is null |
| 9 | `orchestration send --to dispatch:ctx_…` | the branch correction (below) |
| 10 | `check --wait` | polls until `worker_done` (6 min 39 s of real work) |

Settlement half, after validating the report against the expected Dispatch
(`payload.taskId`/`payload.dispatchId` matched, `outcome: succeeded`) and checking the
commit's file list against what the spec's Ownership allowed:

| # | invocation | result |
|---|---|---|
| 11–15 | `check` / `check --terminal` / `inbox` | learned the shape: `check` is empty until the batch exists; `inbox` shows what you sent |
| 16 | `orchestrate.sh settle --decision release` | `state: released`, terminal closed, transcript archived |
| 17 | `check --ack delivery_…` | batch acknowledged, next wait returned empty |
| 18 | `orchestrate.sh board` | `task … completed`, **terminals owing a decision: none** |

Five of those ten are the settlement audit itself, and the last one is the line that
says the loop is finished: a coordinator turn is not over while
`worker-list --terminal-state reclaimable` returns a row.

### What the dispatch measured that the docs had wrong

- **Placement is nested and cannot be changed.** `--worktree new-child --name …`
  produced `/home/scoby/work/hunmin/task-4-r8-shrinker`, *inside* the repository, and
  `git status` in the main checkout then shows `?? task-4-r8-shrinker/`. This confirms
  the 2026-10-10 probe on the current build; the `~/work/<repo>-<task>` convention
  cannot be honoured through the orchestrated path, and no flag moves it.
- **`--name` is not the branch name.** Requesting `task/4-r8-shrinker` produced
  `refs/heads/**scobyseo**/task-4-r8-shrinker`. The AGENTS.md branch convention is
  therefore *not* preserved by this path; the coordinator must read the real branch
  (`git branch --show-current`) and tell the worker, which is what the `send` in step 8
  did. **`ENVIRONMENT.md` §Orca GUI states the opposite** and is corrected here.
- **`--agent hermes` is accepted** even though the `worker-start` help lists
  "claude, codex, cursor, antigravity, muse, zcode, opencode, opencode2" and not hermes.
  The list is illustrative. What a host really runs is visible read-only:
  `orca terminal list --json` → `.agentIdentity`.
- **A dispatched worker's terminal is not in `worker-list`'s projection**
  (`terminalHandle: null`) while the same work is readable with
  `worker-read --source auto`. Use the orchestration verbs, not `orca terminal …`, for
  a worker handle.
- **Bare `orca` on this host is the GNOME screen reader** (`/usr/bin/orca`), and running
  it starts speech. Orca's shim directory comes first on `PATH`, so it resolves to
  Orca — but only because of that ordering. `tools/orchestrate.sh` resolves the
  executable the way the skill says (`$ORCA_CLI_COMMAND`, else the shim inside an Orca
  terminal, else `orca-ide`) and prints which one it chose.

### What this document does not decide

- **Orphaned orchestration state is left alone.** Two Runs and one never-dispatched
  Task from the earlier session still exist (`run_1a8a557d047b`, `run_3f6ccd597fe2`,
  `task_b4d90e9ca958`). Cleaning orchestration state is the maintainer's call, so this
  file records them instead of resetting them.
- **`?? task-*/` in the main checkout's `git status` is a cost, not a bug.** Whether
  that is acceptable, or whether orchestrated tasks should run from a checkout Orca
  does not manage, is a workflow ruling.
- **Whether the orchestrated path should be the default at all.** It is not yet the
  default for anything in this repository; §2 works the same way by hand.

---

## 6. Task spec template

```text
Target: <files/component, named>

Change: <the concrete result>

Constraints:
- Do NOT change <boundary> to make this pass. A failure here IS the measurement.
- Do NOT touch libhunmin/, libhunmin/data/, the ABI symbol list, or any golden.
- Do NOT merge to master. Do NOT close the issue -- the maintainer closes issues.
- Host setup: export JAVA_HOME=…, ANDROID_HOME=…, ANDROID_NDK=…
- `data-selfcheck` is red in a fresh worktree regardless; it needs the gitignored
  국립국어원 dump. Do not read it as your regression.

Ownership: you may edit <paths>. Commit on the branch you were started on
(`git branch --show-current`), with "Refs #<N>" in the body.

Observable acceptance -- each reported pass/fail with the command that produced it:
1. <command> -- exit code and what it printed
2. <command> -- …
If step 1 fails, quote the error verbatim (first 40 lines) and stop; do not attempt
a fix.
```

---

## 7. Where the numbers live

Figures here are measurements with a date and a host, not a status board. The
authoritative statement of what is *open* stays in
[`REMAINING-WORK.md`](REMAINING-WORK.md); the decisions stay in the wiki. If a figure
in this file is ever quoted elsewhere, cite it from here rather than restating it.