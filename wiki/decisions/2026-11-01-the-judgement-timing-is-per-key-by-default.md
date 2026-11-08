---
title: The judgement timing is per key by default
created: 2026-11-01
updated: 2026-11-01
type: decision
status: active
tags: [decision, modes, e6, flicker, judge-timing]
sources:
  - ../../../docs/SPEC-MODES.md
  - ../../../docs/OPTIONS-MEASURED.md
confidence: high
---

# The judgement timing is per key by default

**Ruling (2026-11-01, issue #163).** The maintainer's words, verbatim:

> "옵션처리하자. 설정할 수 있게. default는 B로."

Two timings ship, and the flicker is the default:

| `judge_timing` | what the person sees while typing | status |
|---|---|---|
| `1` | the judged language at **every key** — the word flips back and forth (도깨비) | **default** |
| `0` | the current mode's rendering until the delimiter, judged once | available |

`docs/SPEC-MODES.md` §E6 carries the requirement; the shipped seam is
`hunmin_buffered_preedit_set_option(l, "judge_timing", v)` with `judged_display()`
behind `build_preedit()` in `libhunmin/src/buffered_preedit.c`.

**What the ruling does NOT change**, and the reason it is safe to default to the
flicker: the timing is a *display* setting. The word is still delivered once, at
the boundary, in the language `judge_boundary()` decides there, and
`take_judgement` still answers once per word — so [[2026-11-01-the-engine-judges-the-frontend-switches]]
stands unchanged, and ReqMode10 (nothing retroactive) is untouched. A frontend cannot
tell the two timings apart from the text it receives; the tier-A case
`TheJudgementTimingChangesTheDisplayAndNeverTheText` asserts exactly that equality.

**What the default costs, measured with ReqMode9's three-key guard** (the guard keeps the
first two keys of every word unjudged, so it removes chances to flicker):
64.05% of the shipped dictionary's forms change language at least once inside the
word, mean 2.59 changes, `alphabetically` 11. Without the guard the figure is
94.18% / 2.95 — the two are quoted separately in
[[2026-11-01-the-engine-judges-the-frontend-switches]]'s pricing section,
`docs/OPTIONS-MEASURED.md` §BND-4.

**Left open, and it is not this ruling's subject:** the mixed sentence
`English는 좋은 Language입니다` typed in one stream still fails after the first word,
because the judgement only fires in Hangul mode — the return direction is E1, which
has no ruling and no owner. See [[the-outstanding-decisions]] for where each mode
item stands.
