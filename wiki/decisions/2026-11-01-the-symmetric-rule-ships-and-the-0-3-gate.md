---
title: The symmetric rule ships and a mixed sentence is the 0.3 gate
created: 2026-11-01
updated: 2026-11-01
type: decision
status: active
tags: [decision, modes, e1, r8, release-gate, 0-3]
sources:
  - ../../../docs/SPEC-MODES.md
  - ../../../docs/OPTIONS-MEASURED.md
  - ../../../docs/MILESTONES.md
confidence: high
---

# The symmetric rule ships and a mixed sentence is the 0.3 gate

**Ruling (2026-11-01, issue #163).** The maintainer's words, verbatim:

> "E1도 당연히 지원해야지. 내 예제가 동작하는 것까지를 0.3 release gate로 보자."

E1 had been open since R8 was written: the formula's second line (English → Hangul)
was implemented on neither side, and the item asked which word list would make it
hold. The ruling answers by building it — both lines ship — and by naming the gate
that proves it.

**The gate.** `English는 좋은 Language입니다` typed as ONE key stream, in Hangul mode,
with `autoswitch` on and the shipped dictionary, comes out in the languages the
person meant: Hangul → English for `English` and `Language`, English → Hangul for
`는`, `좋은`, `입니다`. Pinned by
`WordSetTest.TheZeroThreeGateMixedSentenceWorksInOneStream`; the expectation is the
engine's own rendering of each word on the layout that word was meant in, so the test
states intent without typing Hangul by hand. It fails when the symmetric line is
switched off (verified by rebuilding in that state).

**How it works.** The other half of R8's pair, for a word typed in Latin mode, is the
*Hangul reading* of the same keys — `hangul_reading()` replaying them through the
frontend's Hangul layout, exactly the shadow the other direction already used. The
rule is unchanged, and so are its guards: R9's three key inputs apply in both
directions, and `autoswitch` OFF turns both off.

**What the ruling does not measure, and the risk it takes.** The return direction's
misfire surface is the short-word space: with R9's guard applied it switches on
12.73% of all three-key strings and 6.83% of four-key ones
(`docs/OPTIONS-MEASURED.md` §BND-3). A typo — `helo` — is not an English word and its
Hangul reading is well-formed, so it flips to Hangul. No corpus on this host can
weight that by how often people type it, and the language's own metrics
([[the-outstanding-decisions]] D2-adjacent) still have no pass line. Recorded here as
the cost the gate accepts, not as a defect.

**Where each mode item now stands:** E2 (cap 256 B, half-flush), E3 (three key
inputs) and E6 (`judge_timing`, per-key default) are shipped, E1 is shipped by this
ruling, and E7 — proving it identical on every frontend — is #180 and remains open.
The judgement timing's own page is
[[2026-11-01-the-judgement-timing-is-per-key-by-default]].
