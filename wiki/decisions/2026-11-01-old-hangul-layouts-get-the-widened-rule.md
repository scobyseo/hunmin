---
title: 옛한글 layouts get the widened validity rule, chosen per layout
created: 2026-11-01
updated: 2026-11-01
type: decision
status: active
contested: false
tags: [decision, modes]
sources:
  - 2026-11-01-b1-retired-the-hangul-judgement-needs-no-source.md
confidence: high
---

# 옛한글 layouts get the widened validity rule, chosen per layout

## The words

The maintainer, 2026-11-01, after `hangul_valid` shipped with the modern-only
shape rule:

> can we add 옛한글 support to hangul_valid?

> it is optional for layouts which support 옛한글

So the structural test does not change its default, and it does not change for
every layout: the widened rule is used exactly where a layout composes 옛한글.

## What was measured before the shape was chosen

Five questions had to be answered from the tree, not assumed:

- **Which layouts can emit an archaic jamo**: six of the fifteen shipped —
  ahnmatmae, dubeolsik-yet, sebeolsik-yet, sin3-p2, sin3-p2-basic, sin3-p2-yet.
  Three routes were checked; the corpus of key values is nested `case`
  expressions, so a scan that reads only the JSON strings under-reports (it found
  five).
- **What those layouts actually render**: never a precomposed syllable —
  `dubeolsik-yet` `\sa k` gives `ᅀᅡ` (U+1140 U+1161), `\sak\sf` gives `ᅀᅡᆶ`,
  `ahnmatmae` `\sg k` gives `ᅌᅵ`, `sin3-p2-basic` `k\sz` gives `ᄀᆞ`. The
  widened rule therefore has to be a grammar over conjoining jamo, not a
  wider code-point range.
- **Whether a vowel-initial syllable is reachable**: yes, and the composer writes
  the CF filler U+115F, which is what makes "a leading cho is required" a
  faithful rule rather than an approximation.
- **What the boundary case answers**: `랴ㅜㄷ` (the maintainer's example) stays
  invalid under the widened rule, because ㅜㄷ has no leading cho.
- **Whether the per-layout answer can be derived**: yes, from the layout's keys
  and combination table; a scratch data copy with one layout's keys scrubbed
  still answers through its table, which is how the table route was proved rather
  than assumed.

## What shipped

`docs/SPEC-MODES.md` §ReqMode8 is the requirement. The surfaces are
`hunmin_hangul_valid_old()`, `hunmin_ctx_layout_autoswitch_capable()` and
`hunmin_ctx_layout_old_hangul()`; the last two are derived at load time, so
`docs/SPEC-SCHEMA.md`'s layout section deliberately has **no** new field. The
tests are tier A `engine-hangul-valid`, and they generate the property over the
extended sets instead of sampling it.

## What it does not decide

- **What the call site does with the answers.** The judgement is #168; until it
  exists nothing consumes either capability, and the "automatically off" half of
  the earlier instruction is only satisfied for a layout that composes no Hangul
  at all (the Latin three). See
  [[2026-11-01-b1-retired-the-hangul-judgement-needs-no-source]] for why the row
  needed no source.
- **Whether a real 옛한글 sentence judges well.** The widened rule accepts the
  shapes; whether the judgement switches correctly on them is a metric, not a
  predicate, and belongs with the metrics harness. The standing instruction that
  a ruling is the maintainer's and a measurement is the agent's is
  [[decisions-belong-to-the-maintainer]].