---
title: Session 2026-10-03 — the hangul/latin requirement in the maintainer's words
created: 2026-10-05
updated: 2026-10-10
type: measurement
status: active
ingested: 2026-10-05
tags: [modes, decision, question]
confidence: high
---

# 2026-10-03 — the requirement, in the maintainer's words

`docs/SPEC-MODES.md` is the requirement and it is authoritative. This page holds
**the quotations SPEC-MODES paraphrases or omits**, because a paraphrase drifts
and the drift is what gets built. Session `20261003_225238_08d054`, 826
messages, 139 of them the maintainer's. The instruction at the time:

> "적어야 하는 내용 중에는 요구사항 부분이 가장 중요해. 내가 왜 이 기능을 구현하는지."

## Why the feature exists

> "내 요구사항 중 하나가, 한글 모드로 두고 영어를 치면 영어 모드로 변경되는 게 있었잖아?"

> "반대로 실수로 영어 모드로 두고 한글을 타이핑 했을 때, hunmin으로 전환되는 기능을
> fcitx가 제공하지는 못할 거잖아? 2번 두벌식/세벌식 분리는 꼭 필요한 건 아니니, IM은
> 하나로 하자. 그래서 IM 변경 없이 동작 모드로 한글<->영어 변경이 되는 게 굉장히
> 편리해."

The convenience argument is the driver: **one IM, no configuration change, the
mode follows what is being typed.** Splitting dubeolsik from sebeolsik is a
separate axis; splitting hangul from latin is not.

## What was ruled out, and why

> "us layout을 쓸 필요는 없어. us layout을 삭제하고 hunmin만 쓴다고 생각하자구."

> "그래서 사전을 사용하는 게 좋을 것 같다고 하는 거야. 명백히 영어 단어이면 영어로
> 바꾸고, 명백히 한글 단어이면 한글로 바꾸는 거지. 모호하면 현재 모드로 출력."

> "사전은 … 명백히 한글로 쳤는데, 그게 영어로 바뀌어 버리는 경우" — the failure that
> motivates the whole feature.

That last sentence is what R8's *"when the judgement diverges, depend on the
current mode"* is, and `docs/MODES-POSTMORTEM.md` #19 records what happened when
it was generalised into a one-directional rule instead.

## Where the data comes from

> "위키 백과나 나무 위키가 좋은 점은 거기에 쓰여진 내용들에서 한국인들이 한글과 영어를
> 섞어 쓰는 패턴을 찾아볼 수 있기 때문이야. 그래서, 미국인들이 사용하는 영어의 빈도와
> 한국인이 사용하는 영어의 빈도는 또 달라."

The origin of the register argument in
[[the-three-corpora-are-three-registers]]: **the corpus must be Korean-written
text, not English text about Korea.** Every figure measured since has been a
fact about which register it came from, which is why that page exists.

## Licence — settled early, and restated wrongly later

> "나무위키의 내용을 그대로 쓰는 게 아니라, 거기에서 통계를 낸 정보를 쓰는 건데도
> 안되는 건가?"

> "상업적으로 쓸 생각은 없고, 데이터 자체를 공유하는 건 아냐. 데이터를 처리한
> 결과를 활용하는 거고."

> "이 데이터를 기반으로 한글 타이핑을 원하는 건지, 영어 타이핑을 원하는 것인지
> 학습해서 모델을 만드는 건 어때? 이건 라이선스 위반 아니잖아?"

**The answer is no**, and the distinction is *use vs redistribution* — see
`docs/MODES-DATA.md` §1. A later session restated this as "AIHub data cannot be
distributed" and used it to exclude those corpora from **use** as well, which is
a different and wrong claim. The distinction is recorded here because the wrong
version was written into a document and had to be taken back out.

## Commercial prior art, and the standing instruction about it

> "유사한 기능을 상업용 소프트웨어인 한컴오피스와 MS오피스에서는 제공해. … 이
> 기술이 필요없다는 주장은 하지마. 어떤 방식이 더 효율적인지 판단해."

**Absent prior art is a capability gap, not a verdict on wanting** — now
`AGENTS.md` §Working rules. This session confirmed by grep that kime,
ibus-hangul and fcitx5-hangul have no hangul/latin switching. **That premise
turned out to be false for the feature being built**: it holds for 완성형 한글
입력기, an eojeol-level candidate engine, which is a different product. The
check is recorded in `docs/MODES-POSTMORTEM.md` §2 and in the stemming session
record, `raw/sessions/2026-10-04-stemming-investigation.md`, and it is the first
instance of *check the maintainer's claims as carefully as your own*.

## The success metric

> "가장 의미있게 2/3번 지표를 측정하는 방법은, 주어진 문장을 한영 전환없이 타이핑
> 했을 때 동일한 문장이 나오는가? 이게 100% 성공할 필요는 없어. … 키 입력이 이
> 기능이 없을 때보다 많이 줄일 수 있으면 충분해."

> "어떤 단어나 어떤 표현을 아예 입력할 수 없다는 건 불량" — expressiveness is a
> defect; the switch rate is not, as long as the user can correct it.

Both are now in `docs/SPEC-MODES.md` (R-metrics) and in
[[the-outstanding-decisions]] §Open measurements, where the pass lines are
recorded as *not yet set* — a number only the maintainer can supply
(`docs/REMAINING-WORK.md` B2).

See also [[decisions-belong-to-the-maintainer]] for the instruction that shaped
how this work was reported, and [[where-the-practices-came-from]] for the rules
the same project's sessions run on.