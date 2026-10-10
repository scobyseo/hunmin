---
title: Decisions belong to the maintainer
created: 2026-10-04
updated: 2026-10-10
type: decision
status: active
tags: [decision, status]
sources:
  - ../raw/sessions/2026-10-04-stemming-investigation.md
  - ../concepts/the-rules-keep-breaking.md
confidence: high
---

# Decisions belong to the maintainer

**Standing instruction, 2026-10-04.** Recorded because it was broken in the same
session it was given.

> "네 멋대로 기각하지마" — don't decide on your own.

The sequence: three measurements, then a fourth, and on the strength of them I
wrote **"stemming은 기각합니다"** and moved on as if the question were settled.
The measurements were sound. The conclusion was not mine to draw.

## The distinction

| | who | example |
|---|---|---|
| a **finding** | the agent | "stemming loses at every cap on 624" |
| a **decision** | the maintainer | "so we reject stemming" |

A negative result is a finding. "Therefore reject" is a decision. The agent
produces the first and stops. This holds *especially* when the numbers look
one-sided — that is precisely where a self-appointed verdict does the most
damage.

## And a decision made is not closure either

If the maintainer rules, that authorises implementation and verification, not
the end of measuring what the ruling did not cover. Re-litigating a settled
question because a new measurement arrived is the other failure: it spends the
maintainer's attention on something they already decided.

## Where this binds in the wiki

- `status:` on a page records the **state of a finding**, not a ruling. A page
  whose evidence is contradicted becomes `superseded` — that is bookkeeping. If
  closing the question is at stake, it goes in `decisions/` with the
  maintainer's words as the title.
- `contested: true` means **flagged for the maintainer**, never resolved by the
  agent. `SCHEMA.md`'s update policy says the same: record both positions, flag
  it, do not resolve it.
- `wiki-lint.py`'s output is advisory and its errors are mechanical. A design
  question it raises is a question, not a finding to act on.

## What the agent may conclude alone

That a measurement is wrong, that a tool is broken, that a figure is
inconsistent with its source — anything checkable. The vocabulary in
[[the-three-corpora-are-three-registers]] is an example: "a figure from one
register does not transfer to another" is checkable, and saying it was correct.
"Therefore use 624" is not checkable, and saying it would have been the same
error as the stemming verdict.

## Where it has bound since

Three later instances, kept because they show the rule working in both
directions — the finding reported, and the open question left open rather than
closed:

- **A second mutation that nothing catches.** Measured in a sandbox, tier E1
  stays green, and `docs/ANDROID-MUTATION-SANDBOX.md` §5 records it with three
  options and the note that picking one is the maintainer's. It did not quietly
  become a defect report, and the file was not deleted.
- **A question closed against a later audit.** Both items on
  [[2026-10-05-closed-questions]] are questions only the maintainer can answer,
  recorded with their words and a note on what re-raises them, so that "an
  unanswered question" does not read as "an open one forever".
- **A ruling recorded without a quote.** `docs/REMAINING-WORK.md` §1 carries
  *"Space emission is a frontend concern"* as closed, with a measurement page
  that still calls the design question the maintainer's. Both are recorded; the
  disagreement is named on [[space-emission-ownership]] rather than resolved
  here.