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

## 2b. Decision requests — six slots, one screen, one decision

Never ask a decision in prose. The maintainer's recurring cost was the opposite of
too little information: the options carried *what the agent would build* and dropped
*what the person would feel and give up*, which is the axis a decision is made on.

Every request has these six slots, in this order:

1. **What is being decided · why now** — two lines, and what it blocks.
2. **Options, two to four.** For each: a one-line definition; **what the person
   feels** (the screen, the hand — not what will be measured); the cost (what must
   be built / what they must do / whether it is reversible); what it forecloses.
3. **What happens if nothing is decided** — the status quo, and what stays broken.
4. **A recommendation and the single deciding fact**, plus the evidence that would
   flip it.
5. **The answer needed** — a number or one word.
6. **Confidence** — which figures are measured (`engine-measured`) and which are the
   agent's estimates.

A request is incomplete if an option has no "what you would feel" line, if the
recommendation has no flip condition, or if there is no "do nothing" row. **If the
options cannot be filled in, the missing thing is a measurement**: ask which
measurement first, not which design.

One decision per message. A long analysis may not end with a question — the question
goes at the top, in this shape, or not at all.

## 3. Premise check — at the start, and after any fetch or rebase

1. Run `tools/agent-brief.sh <issue>`: it prints the issue and (if stated) its
   premises and out-of-scope lines, the paths it names and whether they exist, the
   commits on `origin/master` since it was filed, whether this branch already
   carries work for it, the pending queue, and the local state. Read it before
   anything else.
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
## 8. Proposals — about the method, applied or asked

The maintainer's standing permission (2026-11-28), in their own framing: *"일하는
방식에 대해, 일하는 과정에서 뭔가 문제가 생겼을 때, 이런 식으로 수정해보면 어떨까요?
하고 제안할 수 있게"* — the working method is not fixed, and friction is the signal
that changes it.

Two kinds, and only two:

- **A change to the agent's own behaviour** (how it writes, what it checks, which
  tool it runs, when it asks) is adopted **immediately** and written into this file
  with a changelog line. The maintainer reads the diff and can revert it. No
  question, because the cost of the change lands on the agent.
- **A change to the maintainer's habits, the repository's conventions, an issue
  template, a gate or a requirement** is **one question**, in §2b's shape.

A proposal is a line in the changelog, or (when it needs more than a line) a file
`docs/PROPOSAL-<topic>.md` whose first line is `Status: **proposal, not ruled.**`
and which carries no decision request of its own — the request, if the maintainer
wants one, is a separate §2b message. A rejected proposal stays marked rejected:
the evidence that it was considered is what stops it being re-proposed.


## 9. Changelog — friction, change, revert

One line per change to how the agent works: `date / friction / change / how to
revert`. The friction is what someone would have to rediscover if the line were
missing.

- **2026-11-28 / friction:** a permission to propose *method* changes was read as a
  permission to file *project* proposals, and it produced one more decision request.
  **Change:** §8 limits proposals to the working method and splits "agent's own
  behaviour" (immediate) from "the maintainer's or the repository's" (one question).
  **Revert:** delete §8 and this section.
- **2026-11-28 / friction:** an issue's premises had moved before the work started
  (121 commits on `origin/master` since #295 was filed, and the issue states no
  premises at all) — the pattern that produced three wrong-direction units this
  session. **Change:** §3 starts with `tools/agent-brief.sh <issue>`, which prints
  exactly that comparison. **Revert:** delete the tool's line from §3; the tool is
  read-only and harmless if left.
