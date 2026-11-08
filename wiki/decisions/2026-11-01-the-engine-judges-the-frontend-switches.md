---
title: The engine judges, the frontend switches
created: 2026-11-01
updated: 2026-11-01
type: decision
status: active
tags: [decision, modes, modes-ownership, r8, r9]
sources:
  - ../../../docs/SPEC-MODES.md
  - ../../../docs/OPTIONS-MEASURED.md
  - ../../../docs/FRONTEND-EQUIVALENCE.md
confidence: high
---

# The engine judges, the frontend switches

**Ruling (2026-11-01, issue #164).** The maintainer's words, verbatim:

> "fcitx5의 제약이 있으면, engine은 판정만 하고, 전환은 frontend가 할 수 밖에
> 없지 않니? 3자 이상이면 판단하자."

The same message set ReqMode9's threshold at three, and **the unit was clarified the same
day: key inputs.**

> "3. 키입력 세개로 명확히 하자."

The ruling's own wording was `3자` (three characters) and this page first repeated
it; asked which unit the requirement should name, the maintainer named key inputs,
which is what `JUDGE_MIN_KEYS` had already implemented and what
`docs/SPEC-MODES.md` §ReqMode9 now says. The distinction is not cosmetic: `안녕` is 6 keys
and 2 characters, so a key rule judges it and a character rule never would. The
message also asked for the fcitx5 off/on surface to be built if the decision left no
problem (it is #176).

A principle stated in the same exchange, and the reason the answer is this shape:

> "모든 frontend가 동일한 기능을 구현해야 한다면, 그 기능은 반드시 engine에서 통합
> 구현한다. 만약 한 frontend라도 같은 구현을 사용할 수 없다면 frontend에 구현하고
> 가변성으로 관리한다. 세 frontend는 구현할 수 없는 기능을 제외하고 모두 동일한
> 기능을 제공한다."

The judgement is the one thing all three frontends *can* share, so it is the
engine's. The switch is the one thing they *cannot* share — fcitx5's trigger belongs
to its host ([[2026-10-13-r8-mode-guessing-is-adaptor-owned]]'s FE13 constraint) —
so it is the frontend's, as variability.

## What it settles

- **The judgement is engine-owned.** ReqMode8's formula runs once, at the boundary, over
  the finished word (`libhunmin/src/buffered_preedit.c#judge_boundary`), and the
  answer is read through `hunmin_buffered_preedit_take_judgement`.
- **The triggering word is delivered in the judged language** by the engine, since
  ReqMode10 forbids reassembling text already in the client and the boundary is the last
  moment before it lands.
- **Each frontend performs the mode change** its host allows: TSF (conversion
  compartment + langbar), Android (which adapter receives the next key), fcitx5
  (the layer's switch, its one IM's two modes).
- **The adaptor-owned shape is retired**: `frontends/fcitx5/src/language_detect.{h,cpp}`
  and its `engine-d1-language-detect` case are gone, and the six `docs/REQ-MAP.md`
  rows that named them point at the engine's formula and `engine-wordset`.

## What it does not settle

- **ReqMode8's symmetric half** ("hangul_valid AND NOT english_valid") — the requirement
  itself leaves it open, and only the Hangul-to-English direction switches anything.
- **The ON/OFF state's persistence** — no frontend stores it yet; it flips through
  the ruled chord and lives with the layer, so a rebuilt layer starts from the
  engine default.
- **fcitx5's settings row and any host-level mode** — #176.

## Where the measurement lives

`docs/OPTIONS-MEASURED.md` §MRO prices the three options (layer owns both, adapter
owns both, layer judges and frontend acts), with the premise checks that changed the
question: the replay had already moved into the core, fcitx5 was building a
one-language layer, and #136's "no caller" claim was stale by two frontends.

Also relevant: [[the-outstanding-decisions]] for the rulings that are still open,
and [[decisions-belong-to-the-maintainer]] for why this page records the ruling
rather than making one.
