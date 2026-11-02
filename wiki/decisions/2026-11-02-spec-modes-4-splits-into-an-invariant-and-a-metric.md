---
title: SPEC-MODES §4 splits into an invariant and a metric, and the invariant gets R14
created: 2026-11-02
updated: 2026-11-02
type: decision
status: active
tags: [decision, modes, requirements, req-map, spec, 0-3]
sources:
  - ../../../docs/SPEC-MODES.md
  - ../../../docs/MODES-METRICS.md
  - ../../../docs/REQ-MAP.md
confidence: high
---

# SPEC-MODES §4 splits into an invariant and a metric, and the invariant gets R14

**Ruling (2026-11-02, issue #179).** The maintainer's words, verbatim:

> "§4 는 뭐니? 요구사항이 누락된 이유는 뭘까?"

> "해당 절에 적혀있는 내용이 모호해 보이는데, 어떤 의도를 가진 것인지 판단해
> 보고, 문장을 명확하게 바꿔볼까? 그러고 나서 요구사항 아이디를 붙이든지 하자."

## What it settles

- **§4's OFF half is a requirement with an id: R14, a child of A2.** It is the
  decidable half — *with the switch off the IM delivers what it delivered
  before the feature existed* — and its oracle is the recorded corpus
  (`tests/golden/`), not a recollection. `docs/SPEC-MODES.md` §R14 holds the
  text; ctest `engine-off-identity` is what verifies it.
- **The scope of "100% of the previous behaviour" is delivery**: the text handed
  to the client and the consumed/declined decision per key, at whichever commit
  policy a frontend is set to. A frontend's presentation is an allowed
  difference (`docs/FRONTEND-EQUIVALENCE.md` §3) and is named as uncovered in
  R14's own text rather than left to the reader.
- **§4's ON half is not a second requirement; it is ③ *unrepresentability*.**
  The metric, its definition and its two strengths ("must be low", ideal 0%)
  live in `docs/MODES-METRICS.md` §1.3; its pass line is E4's and does not
  exist. So the ON half obliges exactly one thing today — that ③ be measured —
  and numbering it again would be a third home for one fact.
- **§4's "(E4)" pointer was half right and is now split.** E4 owns the *pass
  line*; ③ owns the *definition*. The old bullet named only the first, which is
  why "untypable input" read as an undefined term.

## Why the requirement was absent from the map

`tools/req-map-check.py` collects requirement ids from headings **inside a
level-1 section whose title contains "requirement"** — `# 1. Settled
requirements (DECIDED)` only. `# 4. What this feature must not produce [D]` does
not match, and its contents were bullets rather than numbered headings, so no
id ever entered the collection and no row was ever owed. The nearest numbered
item, R5, only *cited* §4 ("preserves current behaviour 100% (§4)"), which is
why the pointer chain looked closed from the map and was not.

That is a structural blind spot, not a one-off: any future section that states a
requirement outside a "requirements" heading is invisible to the map the same
way. What fixes it for this section is that the number now lives inside §1; the
general case is unruled.

## What it does NOT settle

- **Whether the ON half should also be numbered** (an R15 beside R14). Cost of
  doing it: a third home for ③, beside `docs/MODES-METRICS.md` §1.3 and E4's
  pass line.
- **Whether ③'s measurement corpus exists.** It does not: ③ is NOT MEASURED,
  and no tool in the tree computes it (the 2026-11-02 search found the word
  "untypable" only inside `docs/SPEC-MODES.md`).
- **Whether R5's map row is still true.** Its status cell reads *"none: no OFF
  state exists to be per-character in"* (written 2026-10-25). The layer now
  carries both commit policies and the autoswitch option, which the identity
  check drives; what R5 also needs is the per-character English path in a
  frontend, and that part was not measured here.
- **tier-C's delivered-text gap.** The x11 traces carry no boundary key, so the
  frontend lane sees composing text and not committed text (issue #179's
  comment carries the measurement).

## Why the record matters later

The next session that reads §4 will find one bullet pointing at R14 and one at
③, instead of two bullets that both looked like requirements and neither of
which was indexed. It will also find, in R14, the sentence that says what the
oracle is — so "100% of the previous behaviour" cannot be re-derived as "the
suite is green". The decision itself stays where it belongs: see
[[decisions-belong-to-the-maintainer]].

The state R14 constrains on its OFF side is
[[2026-10-12-auto-toggle-disabled-by-default-in-0-1]]'s default: OFF is the
identity path, and the check fails when the option stops being the only guard.