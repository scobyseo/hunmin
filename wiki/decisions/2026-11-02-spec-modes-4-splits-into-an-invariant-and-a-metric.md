---
title: SPEC-MODES §4 splits into an invariant and a metric, and the invariant gets ReqMode14
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

# SPEC-MODES §4 splits into an invariant and a metric, and the invariant gets ReqMode14

**Ruling (2026-11-02, issue #179).** The maintainer's words, verbatim:

> "§4 는 뭐니? 요구사항이 누락된 이유는 뭘까?"

> "해당 절에 적혀있는 내용이 모호해 보이는데, 어떤 의도를 가진 것인지 판단해
> 보고, 문장을 명확하게 바꿔볼까? 그러고 나서 요구사항 아이디를 붙이든지 하자."

## What it settles

- **§4's OFF half is a requirement with an id: ReqMode14, a child of A2.** It is the
  decidable half — *with the switch off the IM delivers what it delivered
  before the feature existed* — and its oracle is the recorded corpus
  (`tests/golden/`), not a recollection. `docs/SPEC-MODES.md` §ReqMode14 holds the
  text; ctest `engine-off-identity` is what verifies it.
- **The scope of "100% of the previous behaviour" is delivery**: the text handed
  to the client and the consumed/declined decision per key, at whichever commit
  policy a frontend is set to. A frontend's presentation is an allowed
  difference (`docs/FRONTEND-EQUIVALENCE.md` §3) and is named as uncovered in
  ReqMode14's own text rather than left to the reader.
- **§4's ON half is not a functional requirement at all** (refined the same day,
  below): the statement is a *quality* requirement with no pass line. Its
  measurand is ③ *unrepresentability* (`docs/MODES-METRICS.md` §1.3), whose
  pass line would be E4's and does not exist. So the only obligation standing
  today is that ③ be measured.
- **§4's "(E4)" pointer was half right and is now split.** E4 owns the *pass
  line*; ③ owns the *definition*. The old bullet named only the first, which is
  why "untypable input" read as an undefined term.

## Why the requirement was absent from the map

`tools/req-map-check.py` collects requirement ids from headings **inside a
level-1 section whose title contains "requirement"** — `# 1. Settled
requirements (DECIDED)` only. `# 4. What this feature must not produce [D]` does
not match, and its contents were bullets rather than numbered headings, so no
id ever entered the collection and no row was ever owed. The nearest numbered
item, ReqMode5, only *cited* §4 ("preserves current behaviour 100% (§4)"), which is
why the pointer chain looked closed from the map and was not.

That is a structural blind spot, not a one-off: any future section that states a
requirement outside a "requirements" heading is invisible to the map the same
way. What fixes it for this section is that the number now lives inside §1; the
general case is unruled.

## What it does NOT settle

- **Whether the SPEC is the right home for a statement that is not evaluable.**
  Keeping it in §4 costs a section that cannot have a test; moving the prose to
  a design document and leaving §4 as a pointer costs the reader one hop.
- **The marker vocabulary.** §4 now carries no `[D]`/`[E]` (it is neither), and
  the document defines only those two. A third marker (`[Q]`) is a format
  decision, not taken here.
- **Whether ③'s measurement corpus exists.** It does not: ③ is NOT MEASURED,
  and no tool in the tree computes it (the 2026-11-02 search found the word
  "untypable" only inside `docs/SPEC-MODES.md`).
- **Whether ReqMode5's map row is still true.** Its status cell reads *"none: no OFF
  state exists to be per-character in"* (written 2026-10-25). The layer now
  carries both commit policies and the autoswitch option, which the identity
  check drives; what ReqMode5 also needs is the per-character English path in a
  frontend, and that part was not measured here.
- **tier-C's delivered-text gap.** The x11 traces carry no boundary key, so the
  frontend lane sees composing text and not committed text (issue #179's
  comment carries the measurement).

## Why the record matters later

The next session that reads §4 will find one bullet pointing at ReqMode14 and one at
③, instead of two bullets that both looked like requirements and neither of
which was indexed. It will also find, in ReqMode14, the sentence that says what the
oracle is — so "100% of the previous behaviour" cannot be re-derived as "the
suite is green". The decision itself stays where it belongs: see
[[decisions-belong-to-the-maintainer]].

The state ReqMode14 constrains on its OFF side is
[[2026-10-12-auto-toggle-disabled-by-default-in-0-1]]'s default: OFF is the
identity path, and the check fails when the option stops being the only guard.

## Refined the same day: the statement is a quality requirement, not an evaluable one

The maintainer, after reading the rewrite:

> "+> 자동변환 기능을 껐을 때는 입력가능하겠지만, 입력 불가가 0%인 것이
> > 이상적이다. 아직은 기능 결함으로 판단하지 않을 생각이며, 입력 불가
> > 단어는 실제 사용될 확률이 매우 작아야 함.
>
> 이 문장을 다시 써줄게.
>
> 자동변환 기능을 끄면, 한글 모드의 입력은 모두 한글로, 영문 모드의 입력은 모두
> 영문자로 입력되기 때문에 입력할 수 없는 문자열이 없다. 자동변환 기능을 켜면,
> 한글 모드라도 영어로 판단되는 문자열을 입력할 수 없을 수 있으며, 영문 모드에서
> 일부 영어 표현을 입력할 수 없을 수 있다.
> 이상적으로는 어느 모드에서든 사용자가 의도적으로 해당 모드의 문자열을 모두
> 입력할 수 있어야 하지만, 그럴 수 없는 경우에는 자동변환 기능을 꺼서라도
> 입력할 수 있어야 한다.
>
> 이게 내가 의도한 것인데, 다시 써준 이 표현도 모호한 부분이 있을 수 있어.
> 이 문장 자체는 기능 요구사항이 아니야. 품질 요구사항에 가까운데, 평가 가능한
> 요구사항도 아니야. 이 상태로 요구사항으로 만들어 봤자 용도가 없어."

Three consequences, all applied:

1. **The wording is replaced, not annotated.** §4 carries his 2026-11-02 statement
   verbatim; the 2026-10-04 sentence it replaces stays only in this record. The
   duplicated copy of the old quote in §ReqMode14 is gone — ReqMode14 points at §4 for the
   basis instead, so a third reword cannot drift in two places.
2. **§4 is a quality statement and says so in its own first line**: no `[D]`/`[E]`
   marker, no pass line, no acceptance test. That is a genre correction to this
   page's earlier reading, which had invented a prohibition ("no word may become
   impossible to type") and given it ③'s rate — the new statement says the
   intended *reading* can be stolen and that this is accepted, with the switch as
   the remedy. The prohibition was stronger than the words.
3. **ReqMode14 stays the only requirement**, and it now carries the finite-sample
   sentence: the corpus proves identity for the sequences the shipped layouts
   carry, which is what makes OFF a real fallback for them, and for nothing else.

Two readings the new wording still leaves open, recorded in §4 itself rather than
resolved: "입력" is the letters and not every key (a Hangul layout's digit and
symbol keys resolve to literals), and "입력할 수 없는 문자열이 없다" is a claim
about (switch off + layout) — a layout's own gaps predate this feature, which ③'s
own note says.

## Refined again: the explanation becomes a requirement (ReqMode15), and §4 disappears

Asked where a statement that cannot be tested belongs, he answered that the
question was mine to have created:

> "1. 자리: 테스트를 가질 수 없는 문장은 SPEC에 두는 게 맞나 —> 네가 이해할 수
> 있는 설명을 추가로 전달한 건데, 그걸 요구사항으로 정제하기 보다는 그냥 원문
> 그대로 두었기 때문에 그런 결과가 된 것임. 지금 내가 요청한 것처럼 이해하고
> 정제하는 과정을 거쳐야 함."

> "내가 쓴 표현을 요구사항으로 쓸 수 있게 변환하자. 그 문장을 그대로 쓸 필요는
> 없어. 내가 요구사항으로 변환된 문장이 적절하다고 생각하면 그걸로 대체하자."

A statement nothing can test is a statement nobody has refined yet. What the
refinement produced:

- **ReqMode15. The switch costs no word.** Two obligations, both checkable: where the
  judgement changes a word's reading, the off path delivers the engine's text for
  the same keys (§ReqMode14's identity, but over the set the switch actually touches —
  the judgement corpus is where those words are listed, with the off-path answer
  computed from the engine rather than typed); and flipping the switch while a
  word composes delivers nothing and changes no delivered byte, so the fallback
  cannot cost the word it exists to save.
- **§4 is gone.** Nothing was left in it: the off-side claim is ReqMode14, the on-side
  fallback is ReqMode15, the ideal it named is ③'s target with no pass line, and the
  digit/symbol caveat is a fact about layouts. `# 5. Other files` became `# 4.`,
  and the two outside citations (`MODES-METRICS` §1.3 and its §8 requirement
  line) now name ReqMode15.
- **What was deliberately not converted: the ideal sentence.** ReqMode15 quotes it and
  says it has no pass line; setting one is E4's, and ③ would need a corpus before
  a number could mean anything.

Falsified before claiming: with the autoswitch guard removed, the pair reddens on
the judgement corpus (`ㅗ디ㅣㅐ` → `hello`), so the new clause is a gate and not a
restatement of ReqMode14.

Sequence of the record, so a later session does not re-derive it: §4 was a
quality statement with no id (first message), then an invariant plus ③ (second),
then his own wording as a quality statement (third), and now two requirements
with cases and no §4 at all (fourth). The first three are history; the numbers
ReqMode14 and ReqMode15 are the current answer.

