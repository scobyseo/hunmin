# AGENT-WORKING-PROTOCOL — how an agent works in this repository

Status: **adopted 2026-11-28** on the maintainer's instruction ("일하는 방법을 바꿀 수는
있어"). It governs the agent's *behaviour*, not the project's requirements: no REQ
row, and nothing here overrides `AGENTS.md` or a SPEC.

## Why this exists (the cost it is paying for)

One session's measured losses, all recorded in
`wiki/raw/sessions/2026-11-27-t2-direction-and-the-three-proposals.md` and its §10:

- Three units of work went in a direction the maintainer did not ask for: T2 turned
  into gate measurement, a metric replaced per-case detection, and an idea nobody
  requested (a user dictionary) appeared in a decision list.
- One system change broke the maintainer's input method and took three recovery
  round-trips (a wrapper installed without the data path its engine needed).

Each had a cheap prevention. They are the sections below.

## 1. Task contract — five lines before any work

In the reply, before executing:

1. the request in one sentence (the maintainer's words where the wording matters);
2. **the kind of work**: design / implementation / measurement / documentation /
   decision-support — if it is design or measurement on a track, cite that track's
   recorded purpose (`docs/MILESTONES.md` §0.4 Track 2 is the model);
3. the deliverable (files, commands);
4. **what will not be done** (the out-of-scope line, which is what the three
   wrong turns lacked);
5. **the one question whose answer would change the plan** — or "none".

Cheap, reversible work then proceeds in the same reply. Expensive or hard-to-reverse
work waits for one word.

## 2. Density — decision first, one screen, one question

- The answer in the first two lines; evidence after.
- At most one screen of prose per reply, options in a table.
- **Every number carries its evidence grade** —
  `docs/MODES-MEASUREMENT.md` §5 rule 7: `engine-measured` / `readable-prior-art` /
  `reported` / `unverifiable-here`.
- Procedures live in `tools/` or `docs/`; the chat carries the pointer and the
  decision, never the procedure restated.
- Never restate a document; cite its section. A second home is drift.
- Reply in the language the maintainer used; repository prose stays English
  (`AGENTS.md`).

## 3. Premise check — at the start, and after any fetch or rebase

1. Re-read the issue; write down its premises (what it assumed about the tree).
2. `git log --oneline origin/master -20` and `git diff --stat` since the issue was
   filed.
3. If a premise changed, say so **before** executing, and say which line of the
   issue it invalidates.

This is the mechanism the maintainer named as missing: an issue written with enough
context, then overtaken by later commits, silently measures the wrong thing.

## 4. Checkpoints — one line, then wait

Ask before:

- writing code that changes behaviour;
- writing docs or wiki pages (a user-visible record);
- **touching the system**: `/usr` files, daemons, package manager, `--force` push,
  rewriting an issue body;
- anything not trivially reversible.

Everything else: do it and say what was done, in the same reply.

## 5. Revert first, diagnose second

When something on the input, authentication or session path breaks: restore the
last known-good state with one command, *then* investigate. The 2026-11-28 input
failure is the instance — the maintainer kept logging enabled on a dead input
method while the agent diagnosed, and both of them paid for it.

## 6. Self-audit — three lines at the end of a task

- was the contract stated, and did the deliverable match it?
- did any premise change mid-task, and was it reported before the work moved?
- how many units went the wrong way, and which section above would have caught it?

Recorded in the reply, not in a new document.

## 7. What the maintainer does not have to change

A one-word or numbered answer is enough ("1 yes, 2 no"). The agent asks at most one
question per reply, and never asks the maintainer to restate an intent that was
already written down — it re-reads instead.