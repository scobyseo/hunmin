---
title: R8 mode guessing is adaptor-owned
created: 2026-10-13
updated: 2026-10-13
type: decision
status: active
tags: [decision, modes, question]
sources:
  - ../../../docs/SPEC-MODES.md
  - ../../../docs/MODES-ENGINE-RULES.md
  - ../../../docs/architecture.md
confidence: high
---

# R8 mode guessing is adaptor-owned

**Ruling (2026-10-13).** Settled on the basis of the design argument in
`docs/architecture.md` §3: R8 is adaptor-owned, not engine-owned.

## The ruling

- The fcitx5 implementation of `modeswitch.*` stays in `frontends/fcitx5/src/`.
- The engine (`libhunmin`) keeps `hunmin_ctx` as a single-layout session and
  keeps its ABI stable. No engine change is required for R8.
- The same contract (shadow `hunmin_ctx` + per-boundary replay) is recorded as
  the model for Android, which also passes the judgement forward — it is a
  forwarding contract, not an engine change.

## What the ruling does NOT do

- It does not reopen the requirement. `SPEC-MODES.md` R3's requirement (a single
  rule, one owner, identical in fcitx5/Android/Windows) is unchanged and is now
  requirement-only.
- It does not settle B1 (the hangul judgement source), which is still open and
  gates any implementation of the judgement formula. The adaptor owns the
  mechanism; the source remains a maintainer decision.
- It does not settle E2/E3/E6/E7 or R9's thresholds — those are measured,
  not designed.

## Why adaptor

The engine's job is composition and commit/preedit. R8's judgement probes the
composition buffer (syllable→word, the delimiter boundary on every layout,
`STR_CAP` truncation, preedit length) — a load question with no measurement yet
(`docs/MODES-ENGINE-RULES.md` §4) and a state model not built for dual-layout
shadow sessions. Moving that into the engine would add speculative ABI surface
before the measurement exists. The engine owns *how* text composes; the frontend
owns *which* word is a valid word.

## What would change the ruling

A measured load profile (preedit length in actual editors, `STR_CAP` overflow
profile, composition-boundary spec) showing the adaptor's shadow `hunmin_ctx`
design is unworkable would reopen this. Absent that, the engine stays clean and
the decision is final.
