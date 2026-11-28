---
title: "The consumed/declined answer is not a contract; the text is"
created: 2026-11-27
updated: 2026-11-27
type: decision
status: active
tags: [decision, tests, fe18, reqmode14, consumed, projection, commit-unit]
sources:
  - ../../../docs/SPEC-MODES.md
  - ../../../docs/SPEC-FRONTEND.md
  - ../../../docs/DESIGN-FRONTEND.md
  - ../../../tools/sim.c
  - https://github.com/scobyseo/hunmin/issues/332
confidence: high
---

# The consumed/declined answer is not a contract

## The ruling (maintainer, 2026-11-27)

> "consumed는 계약으로 보지 않는 게 맞다고 생각해."

Recorded in `docs/SPEC-MODES.md#§ReqMode14` and `docs/SPEC-FRONTEND.md#§FE18`.

## Why it needed a ruling

ReqMode14 (then written as one sentence with the text) constrained both "the text
handed to the client" and "the consumed/declined decision for each key". FE18
(#332) measured the contradiction: a BackSpace that crosses a finished syllable is
consumed by the commit unit that still holds the word and declined by the one
whose syllables are already in the client. Same user-visible text, different flag.
Two documents then disagreed, and a test edit is not where that is settled.

## What changed with the ruling

- **Off path** (`hunmin-sim --off-check`, ctest `engine-off-identity`): compares the
  text only. The client-delete model on a declined BackSpace stays — the client is
  not in that loop — but the per-key `consumed` comparison is gone.
- **Tier A differential** (`engine-buffered-preedit`): the projection is equal per
  key across the engine and both commit units; the consumed answer is no longer
  compared. FE18 keeps its own assertion: a BackSpace commits nothing in either
  mode, because the projection alone cannot catch it (a commit followed by the
  client's delete is text-equivalent — mutation-measured the same day).
- **Tier C** still records `consumed` per event: it is the expectation for what an
  adapter RELAYS, generated from the engine, not a mode-independent rule.

## What this does not settle

Which commit unit a frontend should use for an editor whose state makes the word
buffer risky (password, no-composing, self-committing) — that is an editor-policy
question, tracked by the input-hint work (#333), not by this ruling.

## Related

- [[2026-11-27-fe18-backspace-edits-the-word]] — the measurement that forced it.
- [[2026-11-27-projection-goldens-and-the-trace-pipeline]] — the quantity that
  replaced the flag in tier A.
