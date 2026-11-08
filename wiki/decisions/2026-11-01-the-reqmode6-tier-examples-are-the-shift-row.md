---
title: ReqMode6's tier examples are the shift row, and the tier is single-shot
created: 2026-11-01
updated: 2026-11-08
type: decision
status: active
tags: [decision, modes, layouts, measurement]
sources:
  - ../../../docs/SPEC-MODES.md
  - ../../../tools/gen-case-golden.py
confidence: high
---

# ReqMode6's tier examples are the shift row, and the tier is single-shot

**Instruction (2026-11-01, the day issue #171 was measured).** The maintainer's
words, verbatim:

> "R6문구는 적절히 변경하자"

## What was wrong with the wording

§ReqMode6 illustrated the terminal tiers with `1` → `Ⅰ` at 101, `₁` at 102,
`¹` at 103. Measured against the engine, those three are **shift+1**; plain `1`
at those tiers is `µ`, `①`, `⑪`. The tier is also **single-shot** — only the
first key of a digit run resolves at it — so an example that runs three digits
through one tier described behaviour the layout does not have.

## What the sentence says now

The requirement states the shift row as the shift row and the tier as
single-shot; the measured facts live in `docs/SPEC-MODES.md` §ReqMode6 itself
(plain vs shifted at 101/102/103, `sebeolsik-final`'s digit row being jamo), and
the replay that pins them is tier-A `engine-buffered-preedit` plus the designed
case `r6-literal-digit-row-held-to-the-boundary`.

## Why the record matters later

The wording changed because a measurement contradicted it, not because someone
preferred a phrasing. A later rewrite that quotes the original three-digit
example as fact would re-import the error; the correction is the layout's, so it
belongs in the requirement's text rather than in a measurement paper — which is
also why nothing was lost when the pricing paper around it was deleted
([[2026-11-08-the-retired-corpus-measurement-is-deleted]]). See
[[2026-11-01-syllable-commit-turns-autoswitch-off]] for the setting interaction
the same measurement established, and
[[2026-11-01-the-buffer-cap-is-256-bytes-configurable]] for the cap that bounds
the run.