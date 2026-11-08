---
title: ReqMode8 mode guessing is adaptor-owned
created: 2026-10-13
updated: 2026-11-01
type: decision
status: active
tags: [decision, modes, question]
sources:
  - ../../../docs/SPEC-MODES.md
  - ../../../docs/DESIGN-MODES.md
  - ../../../docs/architecture.md
confidence: high
---

# ReqMode8 mode guessing is adaptor-owned

> **Superseded 2026-11-01 (#164).** The maintainer ruled the judgement
> engine-owned and the switch frontend-owned; this page is kept as the record of
> what was decided on 2026-10-13 and why it did not survive the data moving into
> the core. See [[2026-11-01-the-engine-judges-the-frontend-switches]].

**Ruling (2026-10-13).** Settled on the basis of the design argument in
`docs/architecture.md` §3: ReqMode8 is adaptor-owned, not engine-owned.

## The ruling

- The fcitx5 implementation of `modeswitch.*` stays in `frontends/fcitx5/src/`.
- The engine (`libhunmin`) keeps `hunmin_ctx` as a single-layout session and
  keeps its ABI stable. No engine change is required for ReqMode8.
- The same contract (shadow `hunmin_ctx` + per-boundary replay) is recorded as
  the model for Android, which also passes the judgement forward — it is a
  forwarding contract, not an engine change.

## What the ruling does NOT do

- It does not reopen the requirement. `SPEC-MODES.md` ReqMode3's requirement (a single
  rule, one owner, identical in fcitx5/Android/Windows) is unchanged and is now
  requirement-only.
- It does not settle B1 (the hangul judgement source), which is still open and
  gates any implementation of the judgement formula. The adaptor owns the
  mechanism; the source remains a maintainer decision.
- It does not settle E2/E3/E6/E7 or ReqMode9's thresholds — those are measured,
  not designed.

## Why adaptor

The engine's job is composition and commit/preedit. ReqMode8's judgement probes the
composition buffer (syllable→word, the delimiter boundary on every layout,
`STR_CAP` truncation, preedit length) — a load question with no measurement yet
(`docs/DESIGN-MODES.md` §4) and a state model not built for dual-layout
shadow sessions. Moving that into the engine would add speculative ABI surface
before the measurement exists. The engine owns *how* text composes; the frontend
owns *which* word is a valid word.

## What would change the ruling

A measured load profile (preedit length in actual editors, `STR_CAP` overflow
profile, composition-boundary spec) showing the adaptor's shadow `hunmin_ctx`
design is unworkable would reopen this. Absent that, the engine stays clean and
the decision is final.
