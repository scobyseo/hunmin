---
title: Measurement — the drift audit of 2026-10-05, and what it verified
created: 2026-10-05
updated: 2026-10-10
type: measurement
status: active
ingested: 2026-10-05
tags: [audit, drift, modes, verification]
sources:
  - ../../../../_drift-10-02-10-03.md
  - ../../../../_maintainer-directives.txt
---

# Measurement — the drift audit of 2026-10-05

**What this is.** The record behind [[2026-10-05-closed-questions]] and the
documentation corrections of 2026-10-05. It exists so that "the audit said so" is
retrievable, and so the audit's own claims can be re-checked rather than
trusted.

## Inputs

| input | what it is | where |
|---|---|---|
| transcript | 342 maintainer turns across 24 sessions | `~/work/_maintainer-directives.txt` — **outside the repo on purpose**; session records do not belong in git |
| report | `~/work/_drift-10-02-10-03.md`, 313 lines | outside the repo, same reason |
| documents | as committed at the time | in the repo |

**Not represented here.** The report is an audit of documents against one
transcript; it says nothing about whether the *code* does what any document
claims. Three of its six findings were documentation defects, and fixing a
document does not move any behaviour.

## What the audit's claims were worth

The report was itself verified before being acted on — 46 cited message ids and
60 `file:line` references all resolve. That is a check on its citations, not on
its conclusions, so the six findings were re-checked directly against the tree:

| audit claim | re-check | result |
|---|---|---|
| PLAN.md does not know the hangul/latin feature exists | `grep -c '한영\|모드 전환\|hangul/latin\|SPEC-MODES' PLAN.md` | **0 — confirmed** |
| `Status: approved plan (2026-09-26)` predates the requirement | read PLAN.md:3 | confirmed, and the date is the *cause* of the omission |
| `docs/` listed as 5 files, 17 exist | `ls docs/` | confirmed |
| romaja conf asserts the rejected "English input mode" | read `hunmin-romaja.conf.in:9-11` | confirmed as text — **closed by the maintainer, see the decision page** |
| ENGINEERING.md says D3 fails on purpose | read `ENGINEERING.md:112` vs `TESTING.md:165`, `D3-ROOT-CAUSE.md:3` | **confirmed stale** |
| maintainer's 3rd metric has no definition | `grep -rniE 'false switch\|wrong mode\|오탐' docs/*.md` | **0 hits — confirmed absent** |
| GitHub coding corpus in no document | `grep -rniE 'github.*코딩' wiki/ docs/ tools/` | **0 — confirmed absent** |
| `sin3-p2` in no measurement document | `grep -c sin3-p2 docs/MODES-{METRICS,DATA}.md` | **0 in both — confirmed** |

**The audit's two open questions the maintainer then closed** are on
[[2026-10-05-closed-questions]], with their words.

## The report's own limit

It is a **document-versus-transcript** comparison. Three items in its §5 it
could not determine, and the maintainer answered two of them. What it cannot do
at all is say whether the engine implements anything the documents claim — and
on the one question that matters for P8, `grep -rn 'hangul_valid\|english_valid'
libhunmin/ frontends/` returns nothing. The data work is extensive; the engine
side does not exist. **A document can be perfectly accurate about a feature that
has no code**, which is why PLAN.md's status line mattered more than its content.

## Method note, and a defect in this page's own first draft

This page's `sources:` originally pointed at
`raw/measurements/2026-10-04-stemming-three-corpora.md`, a file that does not
exist — `wiki/raw/measurements/` was not even a directory. A wiki page required
to carry measurement provenance pointed at a missing measurement.

That defect was not detectable by `tools/wiki-lint.py` as it then stood, which
checked double-bracket wikilinks and the index, **not** frontmatter `sources:`.
The measurement it cited was never made; the *page* was made, from a session
record. **The lint check was added to both `tools/wiki-lint.py` and
`tools/docs-lint.py` afterwards**, so a page may no longer cite a measurement
that was never made. Both tools still carry the comment recording the debt —
worth deleting, because a check described as owed is a check nobody runs.