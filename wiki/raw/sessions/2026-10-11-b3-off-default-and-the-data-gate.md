---
title: B3 decided — auto-switch OFF by default, and the data gate that replaces the timeline
created: 2026-10-11
updated: 2026-10-11
type: session
status: active
ingested: 2026-10-11
tags: [session, modes, decision, release-plan]
sources:
  - ../../../docs/SPEC-MODES.md
  - ../../decisions/2026-10-11-auto-switch-off-by-default.md
confidence: high
---

# B3 decided — auto-switch OFF by default, and the data gate that replaces the timeline

Session record. The ruling itself is
[[2026-10-11-auto-switch-off-by-default]]; this page keeps the quotation and the
reasoning that lived only in the conversation.

## What the maintainer said (2026-10-11)

> B3 는 0.3 전까지 off를 기본으로 하자. 자동변환에 필요한 사전이든 뭐든 추가
> 데이터는 별도 배포를 가정하고 그 데이터가 있을때만 기능을 켤 수 있게 하면
> transition이 용이할 거 같아.

Two statements, with different force, and the difference is kept:

1. **A ruling.** OFF is the default until 0.3. This closes B3's undecided line in
   `docs/SPEC-MODES.md` (R13 and E5 both carried it as "not settled by guesswork").
2. **A design direction, phrased as one** ("~용이할 거 같아"): the additional data
   the automatic switch needs — a dictionary or whatever it ends up being — ships
   separately, and the feature can only be enabled where that data is present.
   Recorded as the direction 0.3 is built against, not as a settled mechanism:
   which data, what format, and how "present" is detected are still to be designed.

## Why the second statement matters as much as the first

The release plan gated the feature on a **date** (0.3). The data gate replaces the
date with a **condition**: the switch appears where the data exists and is absent
where it doesn't. Three consequences the session named:

- **D1 loses its hostage-taker.** The outstanding decision that was blocking B3 —
  "ON or OFF by default" — was hard partly because defaulting ON with an unchosen
  judgement source means shipping a feature keyed to an undefined dictionary.
  With OFF-by-default *and* an optional data package, D1 (the judgement source) can
  be answered on the data's schedule rather than the release's.
- **B1's scope boundary stops biting the binary.** krdict (CC-BY-SA) sat outside
  the 2026-10-05 scope boundary partly because of what bundling it into the main
  package would do to the licence surface. A separately distributed data package
  keeps `debian/copyright` and the licence isolation story simple: the engine and
  frontends stay as they are; the optional package declares its own terms next to
  its own files.
- **Transition risk drops.** A user on 0.1/0.2 never sees a half-defined automatic
  switch. Installing the data package is the opt-in; the feature flag and the data
  package are the same event from the user's side.

## What was checked against the tree before writing this down

- `docs/SPEC-MODES.md` carries the default as undecided in **two** places (the R13
  bullet list and the E5 section), with the same "not settled by guesswork" note.
  Both needed the same update, and updating one would have recreated the two-homes
  failure `docs/REMAINING-WORK.md` §7 describes.
- The engine already has an option channel precedent: R13's own text records that
  `hunmin_ic_set_option()` is used by the preprocessing-expression override
  (`ic.c:610`). The on/off state mechanism therefore has a documented predecessor;
  the *data-presence gate* does not — that is new design work in 0.3.
- Nothing in the tree yet distributes data separately: `libhunmin/data/` is one
  directory shipped in one package. "별도 배포" is a packaging future, not a
  present fact, and `MILESTONES.md` says so.

See [[decisions-belong-to-the-maintainer]] for why the ruling is quoted rather
than paraphrased, and [[the-outstanding-decisions]] for D1, which stays open —
closing the default-value question does not answer it.
