---
title: Orca GUI worktree orchestration — environment probe, 2026-10-10
created: 2026-10-10
updated: 2026-10-10
type: measurement
status: superseded
tags: [measurement, tooling, worktree, orca]
superseded_by: docs/ENVIRONMENT.md §Orca GUI
ingested: 2026-10-10
confidence: high
---

# Orca GUI worktree orchestration — environment probe

> **Superseded.** The maintainer pointed out this is a **host toolchain fact,
> not a wiki topic**: the wiki's domain is the hunmin engine (architecture
> decisions, measurements, postmortems), and `docs/` stays authoritative. This
> is a fact about the installed Orca GUI, the same class of item as the Android
> toolchain in `docs/ENVIRONMENT.md`. The content now lives there, under
> **§Orca GUI (installed terminal/editor — worktree orchestration)**; the
> procedure lives in the `engineering/worktree-cleanup-hunmin` skill. This page
> is kept, marked, because deleting it destroys the evidence the mistake was
> made (`SCHEMA.md`: a retracted page stays).

## What moved where

The nine-row findings table — environment detection, `--agent hermes`,
`--prompt`, `--activate`, the default worktree path, `git status` pollution,
`worktree rm` cleanup, `defaultBaseRef`, `worktree set --comment` — is
**`docs/ENVIRONMENT.md` §Orca GUI**, together with the two `AGENTS.md` worktree
rules it conflicts with and the two flags that override them
(`--base-branch master`, `--name task-<id>-<slug>`). What is not in `docs/`, and
kept here, is the reasoning behind the retraction itself.

## The lesson, in the maintainer's words

> "does this belong in the LLM wiki?"

No. Three tests decided it, and they generalise:

1. **Would a reader of this repository care?** The engine's behaviour is
   unchanged by anything on this page.
2. **Is it about this machine or about this project?** `TERM_PROGRAM=Orca` and
   the absence of a `--path` flag are properties of the host. `SCHEMA.md`'s
   domain rule puts host facts in `docs/ENVIRONMENT.md`.
3. **Does the wiki become a second home for it?** The wiki page was written
   first and the durable copy written afterwards — the two-homes shape
   `wiki/concepts/the-rules-keep-breaking.md` warns about.

So the measurement page was marked `superseded` and the session record
[[2026-10-10-orca-probe]] reframed as the evidence trail, the same day. The
skill now cites `docs/ENVIRONMENT.md`.

**Not covered, and not carried over** because `docs/` does not need it:
`--activate` was never isolated from the create call; no `orchestration` DAG flow
was exercised; `--setup`/`--run-hooks` were left at `inherit`; and whether a
nested worktree breaks a hunmin tool that walks the tree was not measured.