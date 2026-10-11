---
title: Auto Hangul/English switch — OFF by default, enabled only when its data is installed
created: 2026-10-11
updated: 2026-10-11
type: decision
status: active
tags: [decision, modes, release-plan, data]
sources:
  - raw/sessions/2026-10-11-b3-off-default-and-the-data-gate.md
  - ../../../docs/SPEC-MODES.md
confidence: high
---

# Auto Hangul/English switch — OFF by default, enabled only when its data is installed

Closes **B3** ("ON or OFF by default") from `docs/REMAINING-WORK.md` §1.

**Maintainer, 2026-10-11** (verbatim, in the session record
[[2026-10-11-b3-off-default-and-the-data-gate]]):

> B3 는 0.3 전까지 off를 기본으로 하자. 자동변환에 필요한 사전이든 뭐든 추가
> 데이터는 별도 배포를 가정하고 그 데이터가 있을때만 기능을 켤 수 있게 하면
> transition이 용이할 거 같아.

## The two parts, kept distinct

1. **Ruling — the default.** Automatic Hangul/English switch is **OFF by default**
   until release 0.3. `docs/SPEC-MODES.md` R13/E5 recorded this as "not settled by
   guesswork"; it is now settled by the maintainer, and both places in that file
   say so. Release lanes are in `docs/MILESTONES.md`: this removes B3 from 0.1's
   blocking set.
2. **Design direction — the gate.** The data the feature needs (dictionary or
   otherwise — **which data is still D1, still open**) ships as a **separate
   distribution**, and the feature can be enabled **only when that data is
   present**. The maintainer offered this as the easy-transition rationale; 0.3 is
   built against it. The mechanism (data format, presence detection, package
   name) is design work, not ruled.

## What this does NOT close

- **D1 / B1 — the hangul judgement source.** The ruling removes the *pressure*
  (no release has to answer it to ship a default), not the question. See
  [[the-outstanding-decisions]].
- **B4 / E5 — the key assignment.** Still open; only the default *value* item of
  E5 is closed.
- **B2 — pass lines.** A feature defaulted off and data-gated still needs the
  maintainer's numbers before 0.3 exits.
- The **user-facing on/off control** itself (R13's surfaces) is unaffected — it
  continues to exist; what changed is its initial state.

## Why the record matters later

The gate turns a **date** (0.3) into a **condition** (data installed). That is the
part worth remembering if the plan is re-cut: the feature's risk was never the
switch key or the UI — it was shipping a judgement whose source was undefined. If
D1 is answered early, nothing here prevents the data package and the feature from
landing ahead of 0.3; the ruling constrains the *default*, not the calendar.
Equally, if the data gate turns out hard to build, "OFF by default" alone still
holds — it was the ruling; the gate was the offered mechanism for making the
transition easy.

See [[decisions-belong-to-the-maintainer]] — this page records a ruling the
maintainer made, which is the only kind of ruling that belongs here.
