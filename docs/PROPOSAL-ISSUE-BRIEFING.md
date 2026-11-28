# PROPOSAL — every issue states its premises and what is out of scope

Status: **proposal, not ruled.** Written 2026-11-28 under the standing permission
in `docs/AGENT-WORKING-PROTOCOL.md` §8. It changes how issues are *written*; it does
not change a requirement, a SPEC or a gate.

## The problem, with its instance

Track 2's issue (#284) was written with enough context for its author and not enough
for its reader. Its scope says "improve auto-switch to fire more correctly"; its
Decisions list is eleven measurement decisions; the purpose the maintainer actually
had — *find the design, by proposing and testing alternatives* — was in the
conversation and reached the issue only when the agent's work was already going the
other way. Three units of work then went in a direction nobody asked for, and one
later commit changed a premise the issue still assumed. The record is
`wiki/raw/sessions/2026-11-27-t2-direction-and-the-three-proposals.md` §2.

The pattern is not specific to Track 2: an issue is a *brief*, and the two things an
agent most needs from a brief are the ones least often written — what the author
assumed about the tree, and what must not be done.

## The change

Two fields in the issue template, and one convention:

```markdown
## Premises
<!-- what I assumed about the tree when I wrote this; if one of these is
     no longer true, say so before doing the work -->
- the design is already settled; this issue only measures it
- the corpus is the one pinned in docs/MODES-GATES.md

## Out of scope
- changing the judgement rule itself
- adding a frontend setting row
```

and the convention: when a premise changes, the author edits the issue body with a
dated line (`**Premise changed 2026-11-28:** …`) *or* the agent comments
`premise changed: <which>` before starting. Either way it is visible on the issue,
not only in a commit message.

## Cost, and what it does not fix

- Author: two to four lines per issue. Agent: a re-read at the start of every task,
  already required by the protocol's premise check (§3).
- It does not fix issues filed before the template; for those the agent's `git log`
  comparison is the only check and it cannot recover an intent that was never
  written.
- It does not replace the verification checklist; premises are about the world the
  issue assumes, not about how it will be proved.

## Alternatives

| | what it changes | cost | where it fails |
|---|---|---|---|
| **A. template fields** (this proposal) | every new issue | 2–4 lines per issue | needs a habit; no mechanical guard |
| **B. a per-track premise register in `docs/`** | one file per track, issues cite it | one file to keep; drifts unless checked | the register becomes a second home for what the issue already says |
| **C. agent premise check only** (already in the protocol §3) | nothing in the issue | none | catches a changed premise, never an unstated one — which is exactly Track 2's failure |

## Decision request

1. **What** — whether new issues carry `## Premises` and `## Out of scope`, and
   whether a changed premise is recorded on the issue. **Why now** — the next issues
   (the T2 design comparison, and whatever the collected log produces) are the ones
   this session's failures came from.
2. **Options**: **A** the two template fields + the dated-line convention; **B** a
   per-track premise register; **C** leave issues as they are and rely on the
   agent's premise check.
   - **What you would feel**: A — two extra lines when you file, nothing afterwards.
     B — one more document to keep true. C — nothing to write, and the same kind of
     drift remains possible.
   - **Cost**: A — a template edit plus a habit. B — a file per track and a check to
     keep it honest. C — none.
   - **What it forecloses**: A — nothing. B — making a premise statement in two
     places (the rule this repository spends most of its rules on). C — the ability
     to tell a changed premise from an unstated one.
3. **If nothing is decided** — issues keep their current shape; the agent's §3
   premise check still catches a premise a commit changed, and still cannot catch one
   that was never written.
4. **Recommendation: A.** The deciding fact: Track 2's failure was an *unstated*
   premise, which only A prevents at the source; C is already in place and did not
   help. **It would flip** if you would rather not carry a filing habit — then C stays
   and this proposal is recorded as rejected.
5. **The answer needed**: `A` / `B` / `C`.
6. **Confidence**: the instance and its cost are measured (the session record); "2–4
   lines per issue" is the agent's estimate from the issues written this session.
