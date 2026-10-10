---
title: Session 2026-10-04 — the stemming investigation, and what it cost
created: 2026-10-04
updated: 2026-10-10
type: measurement
status: superseded
ingested: 2026-10-04
tags: [modes, measurement, postmortem, provenance]
superseded_by: concepts/the-three-corpora-are-three-registers.md
confidence: high
---

# Session 2026-10-04 — the stemming investigation

> **Superseded.** Folded, not deleted. The eight measurements and the
> overturned verdict are in `wiki/concepts/the-three-corpora-are-three-registers.md`,
> which now owns them alongside the corpus table — two homes for the same table
> is how they drift, which is the failure this wiki exists for. The six
> mistakes are split by what they are in
> `wiki/concepts/the-rules-keep-breaking.md`; the ones about state-tracking are
> also listed there by name. This page is kept, marked, because the sequence is
> the finding and deleting it destroys the evidence that the reversal happened.

The session that produced three corpora, one overturned verdict, and this wiki.

## What moved where

| in this record | now in |
|---|---|
| measurements 1–8, and why the verdict reversed | `wiki/concepts/the-three-corpora-are-three-registers.md` |
| the three corpora: forms, tokens, register | `wiki/concepts/the-three-corpora-are-three-registers.md` |
| mistakes 1, 2, 3, 6 | `wiki/concepts/the-rules-keep-breaking.md` |
| mistake 4 (`NIKE` → `NIKL` → back), mistake 5 (어미 656 / 조사 184) | `wiki/concepts/the-rules-keep-breaking.md`, then `AGENTS.md` and `docs/GLOSSARY.md` |
| *"네 멋대로 기각하지마"* | `wiki/decisions/decisions-belong-to-the-maintainer.md` |
| open #5 (119 under stemming); the 받침-이동 question | `wiki/concepts/the-three-corpora-are-three-registers.md` |
| the outstanding **decisions** (hangul judgement method, E1 symmetry) | `wiki/decisions/the-outstanding-decisions.md` |

## The one answer that does not travel with the corpus

> "상용 소프트웨어는 그렇게 하고 있거든?" — commercial software does it this way.

Checked against three implementations on this host and the vendored sources:

```
kime          src/engine/dict/ EXISTS -- hanja, math symbols, emoji.
              No hangul/latin automatic switching. grep over src/: 0 hits.
ibus-hangul   src/*.c (6 files) grep dict|morph|josa: 0
fcitx5-hangul automatic switching logic: 0
```

**The premise does not hold for the feature being built.** It does hold for
완성형 한글 입력기, which is a different product: an eojeol-level candidate
engine, not a mode-switch decision. (`docs/MODES-POSTMORTEM.md` §2 carries the
fcitx5-hangul half; the ibus-hangul row is here only.)

This is the check behind the standing instruction in
`wiki/raw/sessions/2026-10-03-requirement-shaped.md` — a stated premise gets
verified too, and **absent prior art is a capability gap, not a verdict on
whether the feature should exist.**