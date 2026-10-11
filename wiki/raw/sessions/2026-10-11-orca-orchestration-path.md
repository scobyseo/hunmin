---
title: Orca orchestration, measured end to end — 2026-10-11
created: 2026-10-11
updated: 2026-10-11
type: session
status: active
ingested: 2026-10-11
tags: [session, orca, orchestration, worktree]
sources:
  - ../../../docs/ORCHESTRATION.md
  - ../../../docs/ENVIRONMENT.md
  - ../../../tools/orchestrate.sh
confidence: high
---

# Orca orchestration, measured end to end — 2026-10-11

Session record. The durable artefact is **`docs/ORCHESTRATION.md`** plus
`tools/orchestrate.sh`; this page keeps what the conversation held and the tree does
not: the shape of the failure that motivated the work, and the four places the
documents were wrong about what Orca actually does.

The predecessor of this page is [[2026-10-10-orca-probe]] and its companion
[[2026-10-10-orca-worktree-probe]], both retracted because a host toolchain fact
belongs in `docs/`, not here. That retraction was correct and is not reversed by this
page — but it left one thing unwritten, which is the thing this session measured: the
**orchestration layer** below `worktree create`. The probe had stopped one level too
high.

## The shape of the cost

The maintainer's question was *"orca의 orchestration 스킬을 사용하기 적합하게 구성해
볼까?"* — configure the orchestration skill so it can be used efficiently. The Hermes
session `20261011_101505_146066` tried to answer it by using it, and the transcript is
the measurement: **≈40 turns**, **two Runs created by accident**, **zero workers
started**, and a repeated restatement of intent between each guess.

Nothing there was a hard problem. Every wrong flag was already answered by one call —
`orca agent-context --json` reports `usage`, `flags`, `notes` and `examples` for all 239
commands. The session grepped that payload for command *names* and then guessed.

This is [[the-rules-keep-breaking]]'s shape once more: an agent that reported a result
while measuring something else. `Unknown command: run-list` was recorded as *the
command does not exist*; `run-list` exists, the caller had a typo. A negative result
that was never a measurement is still a finding — but it is a finding about the
measurement, and it has to say so.

## What the dispatch measured, and the documents that were wrong

Issue #6, one dispatched Task (GitHub issue #4, the R8 shrinker), one `hermes` worker,
Orca 1.4.219:

- **Placement is nested and immovable.** `--worktree new-child --name …` produced
  `/home/scoby/work/hunmin/task-4-r8-shrinker`, inside the checkout, and the main
  tree's `git status` then shows `?? task-4-r8-shrinker/`. The `~/work/<repo>-<task>`
  convention cannot be honoured through this path.
- **`--name` is not the branch name.** `task/4-r8-shrinker` became
  `refs/heads/scobyseo/task-4-r8-shrinker`. `docs/ENVIRONMENT.md` §Orca GUI asserted the
  opposite and is corrected. The branch convention in `AGENTS.md` survives only because
  the coordinator read the real branch back and messaged the worker; that message is
  part of the procedure now, not an improvisation.
- **`worker-start` has no `--issue`.** `orca worktree create` does. So the Orca-side
  `linkedIssue` stays `null` when the link rides in the Task spec. Two routes, both
  documented; neither is a bug.
- **`--agent hermes` works although the help's list omits it.** The list is
  illustrative; `orca terminal list --json` → `.agentIdentity` is what the host runs.
- **A worker's terminal is not in `worker-list`'s projection** (`terminalHandle: null`),
  while `worker-read` serves the same output. The orchestration verbs, not
  `orca terminal`, are the way to read a worker.

## What was left, and who decides

The same session left an orphaned Run (`run_1a8a557d047b`), a second one
(`run_3f6ccd597fe2`), and a `ready` Task that was never dispatched. The maintainer was
asked and answered: **report, do not touch**. That is
[[decisions-belong-to-the-maintainer]] applied to state rather than to a verdict —
orchestration state is not recoverable by an agent that was not asked to change it.

Two further questions are recorded without an answer, because they are workflow
rulings: whether `?? task-*/` in the main checkout is an acceptable cost, and whether
the orchestrated path should become the default for this repository at all. Neither is
a measurement, and neither is the agent's to settle.