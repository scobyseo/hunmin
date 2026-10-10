---
title: The three corpora are three registers, and conclusions do not transfer
created: 2026-10-04
updated: 2026-10-10
type: concept
status: active
tags: [modes, measurement, data]
sources:
  - raw/sessions/2026-10-04-stemming-investigation.md
confidence: high
---

# The three corpora are three registers, and conclusions do not transfer

**Every stemming measurement in this project reversed when the corpus changed.**
That is not noise — it is the shape of the problem, and it is why the word
lists are kept separately rather than merged.

| | wikipedia | 624 | 119 |
|---|---|---|---|
| source | ko.wikipedia dump 2026-09-09 | AIHub 030 (웹데이터) | AIHub 119 (국가기록물) |
| register | encyclopedia prose | web text | official documents |
| forms | 7,495,250 | 12,588,072 | 7,592,702 |
| tokens | 191,498,857 | 1,016,338,031 | 238,721,031 |
| head of list | 년 분류 월 일 제 수 | 이름 일 있다 고 수 등 | 및 제 수 등 년 대한 |
| what the head is | wiki metalanguage | ordinary typed vocabulary | document connectives |
| shippable | yes (CC BY-SA 4.0) | no (AIHub) | no (AIHub) |
| typed english | no | **yes** | no |

## The measurements that established it

The question was whether a stem + paradigm (어간 + 어미) list can ship a smaller
dictionary than a form list at equal coverage, with the endings mined from the
corpora rather than taken from the 2003 survey. Eight measurements, in order:

| # | measurement | result |
|---|---|---|
| 1 | stemming, krdict-derived stems, wikipedia | 30.50% token mass |
| 2 | **the same, dictionary-free** | **38.19%** — #1 was measuring the dictionary |
| 3 | wikipedia, equal-entry comparison | stem wins at 50k, form list wins at 200k+ |
| 4 | 624 (webdata), same comparison | **form list wins at every cap** |
| 5 | 119 (official documents), same | **not measured** |
| 6 | surface ambiguity | **41.25%** of token mass has a stem that is also a standalone form |
| 7 | unreachable by any stem list | 2.75%, headed by proper nouns (머니투데이뉴스 1.13M) |
| 8 | survey endings | 722 distinct; **121 (16.8%) written with a leading 받침** |

Measurement 3 says stemming is viable. Measurement 4, on the corpus closest
to the population the feature acts on, says it loses at every cap. **The
question "is stemming better" has no answer without naming the corpus.** #2 is
the sharper version of the same shape: the answer changed when one input was
removed. A verdict that flips when the input changes is not a fact about the
design; it is a fact about the measurement — and drawing a conclusion from it is
the maintainer's call, not the agent's. See
[[decisions-belong-to-the-maintainer]].

**#5 is still open**, and with it whether the paradigm needs the 받침-이동 rule at
all: the 2003 survey ships both spellings for the inflections that matter
(`습니다` as a complete form), and the 받침-led spellings occur **zero** times as
corpus text — 0 occurrences of any form ending `ㄴ다 ㄴ가 ㄴ데 ㄹ까 ㅂ니다 ㅂ시다
ㅂ니까 ㄴ지 ㄹ게` over 400,000 wikipedia forms. So arm A (concatenate as written)
and arm B (relocate the consonant) may be equivalent, which would make the rule
unnecessary. This is a measurement still to take, not a conclusion.

## Why this matters more than the numbers

`docs/MODES-DATA.md` §4-7 established that the english side **saturates** at a
cap of 50,000 while the hangul side **does not** — 200,000 forms reach 2.90% of
hangul forms. The explanation offered there is that wikipedia is
"restricted-vocabulary text" while the hangul corpus "generates long tangled
forms". Both halves of that are about **which corpus**, and they do not
generalise to the other two.

So: a figure measured on one of these is a fact about that register. It is not
a fact about the feature. See [[the-outstanding-decisions]] for which register
the feature actually acts on — the answer is 624, and that is a judgement the
maintainer makes, not a measurement.

## Provenance

Each corpus carries a `meta.json` written by its own producer, recording the
source, the method, what was filtered out with measured shares, and what the
numbers do **not** represent. `119-clean` is verified byte-identical on
rebuild; `624` is not reproducible because its original producer was never
recorded, and its `meta.json` says so rather than implying otherwise. The
producers are `tools/build-modes-corpus.py`, `tools/build-modes-624.py` and
`tools/build-modes-119.py`; `tools/fetch-aihub.sh` is the route that works (see
its header). **`--pages 0` matters** — the default sample of 4,000 pages passes
the selfcheck while describing one hundredth of the corpus.

## The one contamination worth knowing about

119's latin list was dominated by AIHub's own entity markers: `<|start|>` and
`<|end|>` tokenise to the english words `start` and `end`, **5.90% of every
latin token**. Removing them (and the `#@이름4#` labels) by substituting a
**space** is what makes the corpus readable — substituting nothing glues the
neighbouring eojeol together and manufactures non-words.

None of the three is a corpus. All three are projections of one, and the
directory name `wikicorpus` is misleading on that point. The mistakes made
while building them are in [[the-rules-keep-breaking]].