---
title: "312: the variant-mapping drift — built on an issue paraphrase, corrected by three rulings"
created: 2026-11-23
updated: 2026-11-23
ingested: 2026-11-23
type: session
status: superseded
tags: [android, layouts, decision, tests, measurement]
sources:
  - ../../../docs/SPEC-ANDROID-KEYPAD.md
  - ../../../docs/DESIGN-ANDROID-SYMBOL-GESTURES.md
  - ../../../docs/ANDROID-KEYPAD-MOCKUP.md
  - https://github.com/scobyseo/hunmin/issues/312
confidence: high
---

# 312: the variant-mapping drift — built on an issue paraphrase, corrected by three rulings

**Superseded**: this page first recorded a Tier 2 variant-mapping implementation
for #312 (provisional data, contract, E1 parse lane). That work answered a
question the maintainer never asked. It was removed on 2026-11-23; the page
stays, marked, because deleting it would destroy the evidence of the drift
([[the-rules-keep-breaking]]). The corrected design is
`docs/DESIGN-ANDROID-SYMBOL-GESTURES.md`.

## The drift chain

1. T3-1 (2026-11-22): the maintainer said, verbatim: "심볼 키에 대한 swipe 동작이나
   longpress로 어떤 기호를 입력할지 선택하는 게 tier2 버튼을 두는 것보다 나아
   보인다. 일단 이렇게만 정리해 두고 이 기능을 확장하는 건 별도 이슈로 만들자."
   The quotation was recorded accurately (KP10).
2. The agent-written issue #312 body narrowed "심볼 키" to the symbol-character
   keys of the Tier 1 screen and invented a twelve-symbol variant inventory,
   mapping questions and a verification checklist around that reading. The
   mockup's "Update (KP10)" paragraph and the SPEC's "deprioritises the Tier 2
   page" sentence recorded the same narrowing.
3. This session built on the issue body: a shape-family/frequency mapping
   analysis (M1–M3, G1–G4), a provisional `variants` section in `symbols.json`,
   a structure-only checker contract with a negative selfcheck, and a Kotlin
   parse lane with three E1 tests. Three commits, all local, nothing pushed.
4. The maintainer: "T3-1에서 이야기한 것과 전혀 상관없는 것들인데?"

## What the maintainer actually meant (2026-11-23, verbatim)

> "어느 키에서든 swipe up을 하면 shift 를 누른 상태로 입력하는 것과 동일한 동작으로
> 구현하자는 거였고. 그 방식으로 숫자키로 기호 입력을 할 수 있게 한다는 거였어."

> "기호 모드 키를 swipe up 하면 기존에 tier 2라고 설계했던 layout을 포함해서 다양한
> 심볼 입력을 할 수 있게 하자고 한 거였어."

> "기호 모드 키를 swipe up 했을 때는 일반적으로 많이 제공하는 이모지, 확장 기호,
> 수식 기호 페이지를 고를 수 있게 하고, 그걸 고르면 실제 기호 선택 창이 뜨는 방식으로
> 구현하고 싶은 거야."

So: **swipe-up = shift+key on any key** (KP7 clarified), and **swipe-up on the
기호 mode key opens a page picker** (emoji / extended symbols / math symbols)
whose pick opens the symbol page (KP10 corrected). The designed Tier 2 panel is
content reached by the gesture, not a deprecated alternative; the gesture
replaces the Tier 2 *button*. Both rules open things that already exist — no
per-key variant table was ever requested.

## How the correction was verified, and what was removed

The original T3-1 conversation was recovered from the pi session log
(`~/.pi/agent/sessions/--home-scoby-work-hunmin-0.4-t3-1-symbol-layer-decision-schema--/`),
not from the issue body — the quotation was accurate, the *reading* was not.
Removed: the `variants` section, the checker's variant contract + selfcheck,
the Kotlin parse lane and its tests, `DESIGN-ANDROID-SYMBOL-VARIANTS.md`, and
the REQ-MAP/SPEC implementation cells. Corrected in place: SPEC KP7/KP10
(verbatim clarifications added, drift recorded), the mockup's KP10 paragraph,
the `symbols.json` tier2-slot note, the #312 issue body. The removed files
remain in the branch history (commits `db1d41d`, `42a76bd`, `3dc35b6`).

## What survives

- The kowiki frequency re-run (2026-11-23, dump `de25cba4…`, 3000 pages)
  reproduced the recorded Tier 2 counts exactly; it grounds the extended-
  symbols page content regardless of the entry mechanism. Artefact +
  `meta.json` live outside the repository (`~/work/hunmin-measurements/`).
- The environment findings: `engine-d2-ibus-ui` parallel-load flake;
  `engine-android-e2e` red on master too with two devices attached
  (`ANDROID_SERIAL` needed); local budget overage pre-existing on both trees.

## The lesson, recorded

The quotation was checked; the **paraphrase around it** was not. An issue body
written by an agent is not the maintainer's words, and a design built on it
inherits its drift — the failure mode [[where-the-practices-came-from]] names
("the maintainer's words are the requirement, not your paraphrase"). The
cheap defence that was skipped: re-read the originating session before
building, not after being challenged. A second gap of the same family,
2026-11-23: the KP14 decision record quoted only the short confirmation and
paraphrased the reason, while the maintainer's full explanation (2026-11-22,
"많은 글자를 지우기 위해 반복해서 누르는 게 쉬운 일이 아니거든?") sat in the #312
comment and the T3-3 wiki page — "이것도 왜 그렇게 결정했는지 설명을 한참 했는데?"
Corrected by recording the origin verbatim in SPEC KP14 and checking the
prior-art premise it carries (survey §5: open-source IMEs repeat characters,
not words). A decision record must carry the reasoning the maintainer gave,
found in the tracked request — not a paraphrase of it. See also
[[2026-11-22-t3-2-symbol-data-and-issue-alignment]], the previous round where
issue bodies had drifted from a ruling and were corrected.