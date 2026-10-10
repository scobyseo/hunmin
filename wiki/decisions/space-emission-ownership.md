---
title: Space emission ownership — the measurement and the decision disagree
created: 2026-10-10
updated: 2026-10-10
type: decision
status: active
contested: true
tags: [decision, space, fcitx5, android, contradiction]
sources:
  - raw/measurements/2026-10-09-where-space-is-emitted.md
confidence: high
contradictions:
  - ../../../docs/REMAINING-WORK.md
---

# Space emission ownership — the measurement and the decision disagree

**This is contested.** The measurement is not in dispute: the engine declines the space and every frontend forwards it. But the log and `docs/REMAINING-WORK.md` B5 record the *decision* as closed with no quoted ruling, and the measurement page says it is still the maintainer's. Named here, resolved by nobody.

## What the measurement established

[[2026-10-09-where-space-is-emitted]] measured both frontends at HEAD with the layout-data change reverted:

- **Android**: `dkssud` + SPACE → `ㅎ곈디<space>` — composition ended **and** space emitted.
- **fcitx5 XIM**: `dkssuds<space>` → `COMMIT:안|COMMIT:녕|COMMIT:ㄴ|FORWARD:<space>` — same shape.

The engine (`hunmin-sim --trace`) reports `consumed: false` for space — it flushes the syllable and emits nothing. The character comes from the frontend forwarding the declined key.

## What the documents say

`docs/REMAINING-WORK.md` B5:

> Space emission is a frontend concern ✅ — measured 2026-10-09. The engine declines space and flushes the syllable; every frontend (fcitx5 XIM, fcitx5 GTK 3/4, Android) forwards the declined space to the client. No engine change is needed or planned.

This reads as a closed decision. But **no ruling is quoted** — no maintainer message saying "this is the design", no `AGENTS.md` entry, no `SPEC-MODES.md` line. The log entry for 2026-10-09 says the same and adds:

> **Superseded by [[space-emission-ownership]]**, which records that the measurement page still calls the design question the maintainer's and that no ruling is quoted anywhere.

The measurement page itself (§What is asserted, and what is not) says:

> **Whether space *should* be the engine's to emit.** This record establishes that it is not, at HEAD, for want of a layout mapping — **not that the design is right**.

## The contradiction

| source | says |
|---|---|
| measurement | engine declines, frontends forward — **not a design verdict** |
| `REMAINING-WORK.md` B5 | "Space emission is a frontend concern ✅" — **reads as settled** |
| log 2026-10-09 | "No engine change is needed or planned" — **reads as a plan** |
| measurement page | "the design question [is] the maintainer's" — **explicitly open** |

The log and `REMAINING-WORK.md` treat a measurement as a decision. The measurement page insists it is not. No document quotes the maintainer closing this.

## What would resolve it

A quoted ruling from the maintainer, recorded in `AGENTS.md` or `SPEC-MODES.md`, saying one of:

- "Space emission stays in the frontends; the engine never emits it."
- "The engine will emit space; add it to the layout key tables."

Until then, this page records the disagreement. **Do not resolve it silently** — see [[decisions-belong-to-the-maintainer]].

## Related

- [[2026-10-09-where-space-is-emitted]] — the measurement this page is about.
- [[2026-10-09-gtk-forward-path]] — the GTK half of the same measurement.
- [[decisions-belong-to-the-maintainer]] — the standing instruction this page follows.