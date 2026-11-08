---
title: Romaja registration and the coding corpus — both closed, 2026-10-05
created: 2026-10-05
updated: 2026-10-10
type: decision
status: active
tags: [decision, modes, scope]
sources:
  - ../raw/measurements/2026-10-05-drift-audit.md
confidence: high
---

# Romaja registration and the coding corpus — both closed, 2026-10-05

A drift audit on 2026-10-05 compared the maintainer's own transcript against the
documents and produced six items. Two of them are **closed here**, because both
were questions only the maintainer can answer and an unanswered question reads
as an open one forever. The audit's own method and limits are in
[[2026-10-05-drift-audit]].

## 1. `hunmin-romaja.conf.in` — the document is right. Do not "fix" it.

The audit reported that `frontends/fcitx5/src/hunmin-romaja.conf.in:9-11`
asserts romaja is *"the 'English input' mode"*, that this is the sentence the
maintainer rejected once (`docs/MODES-POSTMORTEM.md` mistake #8), and recommended
removing or correcting it.

**Maintainer, 2026-10-05: "1.2는 지금 문서가 맞아. drop 해."**
— *the document is right as it stands; drop it.*

**So: no change, and the reasoning matters.** ReqMode2 puts English mode *inside*
hunmin as a mode over a Latin layout. The conf file's comment says romaja — a
transliteration layout that outputs Hangul — is the English input mode. Those
read as contradictory, and the audit was right that they do.

The resolution is that **the conf describes a second configuration surface, not a
second definition of "English mode".** Its own first paragraph says so: two
registrations for one addon is how fcitx5 offers two configurations in the IM
list, and the per-item `Layout=` in the profile selects between them. That is a
packaging statement. It does not claim romaja *is* English mode in ReqMode2's sense, and
ReqMode2's "no separate `us` registration" is not violated by a second configuration of
the same addon.

**The trap for the next session:** an audit that reads only the one-line comment
will re-report this every time, because the comment *does* use the phrase the
maintainer rejected. The rejection was of a **claim in PLAN.md about what romaja
is for**, made as though the user had said it — not of the file's existence, and
not of the wording in a file whose subject is fcitx5 registration mechanics.

## 2. The GitHub coding corpus — curiosity, out of scope

The audit reported a fourth corpus that exists nowhere: repos with Korean in
comments or string literals, gathered to estimate how developers switch
languages while coding (`[17139]`). It flagged this as the register none of the
three documented corpora covers, and as **still wanted** — the transcript
narrows the search at `[17177]`/`[17242]` but never cancels it.

**Maintainer, 2026-10-05: "5. 이건 그냥 궁금해서 한거니, 제외."**
— *that was just curiosity, exclude it.*

No corpus will be built, and no document needs to record a register gap for it.
The narrowing in the transcript was the end of the line, not a pause in it.

## What remains open, and is not closed by this page

- **§4-7c's hangul judgement criterion** — still open. The 2026-10-05 krdict
  boundary removed the dictionary option (b) named; no replacement source has
  been chosen. See `docs/MODES-DATA.md` §5-1 and [[the-outstanding-decisions]].
- **③-bis (wrong-direction switch)** — defined 2026-10-05, no number, and it
  cannot get one until the judgement criterion above is settled.
  See `docs/MODES-METRICS.md` §1.4.
- **Both layouts** — the mode-switch feature must be evaluated against 두벌식 and
  세벌식 alike (`PLAN.md` P8).

Closing the coding corpus does not close the register it represented: a fourth
register — how developers switch languages while coding — is simply not among
the three in [[the-three-corpora-are-three-registers]], and no document records
that as a gap, because the maintainer closed it as curiosity.