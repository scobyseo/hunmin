---
title: Orca worktree probe session — 2026-10-10
created: 2026-10-10
updated: 2026-10-10
type: measurement
status: superseded
ingested: 2026-10-10
tags: [session, orca, worktree]
superseded_by: docs/ENVIRONMENT.md §Orca GUI
---

# Session — Orca worktree orchestration probe, 2026-10-10

> **Superseded.** The maintainer's question — *"does this belong in the LLM
> wiki?"* — was correct: this is a **host toolchain fact**, not engine
> reasoning, and `SCHEMA.md`'s domain rule puts host facts in `docs/`. The
> durable record is **`docs/ENVIRONMENT.md` §Orca GUI**; the procedure is in the
> `engineering/worktree-cleanup-hunmin` skill. The companion measurement page
> [[2026-10-10-orca-worktree-probe]] was marked superseded the same day.
> Both pages are kept, marked, because deleting them destroys the evidence that
> the mistake was made.

Hermes ran inside an Orca terminal in the hunmin repo and probed the Orca GUI
worktree path the maintainer asked for: environment detection, `orca worktree
create --agent hermes --prompt … --activate`, `orca terminal read` on the
launched worker, `orca worktree rm` for cleanup.

**Outcome.** The Orca path works end to end and is now in the skill. Two
`AGENTS.md` rules need explicit overrides under Orca (`--base-branch master`,
`--name task-<id>-<slug>`); the nested default location is unavoidable via the
CLI, and the choice between Orca-managed (nested, visible) and git-native
(`~/work/<repo>-<task>`) is the maintainer's. **No merge, no work kept** — both
probe worktrees were removed and no code changed in the repo.