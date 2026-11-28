---
title: Tier A asserts the projection, and tier C still owns the per-key split
created: 2026-11-27
updated: 2026-11-27
type: session
status: active
tags: [session, tests, goldens, tier-a, tier-c, projection, fe18, reqmode4, pipeline]
sources:
  - ../../../docs/DESIGN-FRONTEND.md
  - ../../../docs/SPEC-SCHEMA.md
  - ../../../tools/_projection.py
  - ../../../tools/gen-conformance.py
confidence: high
---

# The projection is the quantity tier A asserts

One line: tier A's designed corpora stopped asserting the commit/preedit split and
start now assert `visible` (the text a person ends up with, per key), while tier C
keeps the per-key split, `consumed` and the delivery point — and the trace pipeline
was proved unchanged by regenerating four families **byte-identically**.

## Why

`docs/DESIGN-FRONTEND.md` §5.2 had recorded the question since 2026-10-25: "the same
preedit string after each key" and "the commit boundary is a frontend choice" cannot
both hold literally. FE18 (#332) made the cost concrete: one engine change forced a
golden re-accept, a trace re-emit and a change to the OFF-identity model, all because
the expectations encoded *where* text was delivered rather than *what text* the person
gets.

The quantity that survives the difference is the projection:

```
document += this key's commit                       (the engine delivered it)
document -= its last code point   if a BackSpace is DECLINED   (the client deletes)
document += the key's character   if a printable key is DECLINED (the client inserts)
visible   = document + preedit
```

Two implementations of the same rule keep each other honest: the checker in
`tools/sim.c` and `tools/_projection.py` (used by the generator, the merger and the
trace generator). A generator and a checker that share one implementation would agree
on a wrong rule.

## The measurement that decided the split

`rkrk\b\b\b\b`, per key, through three drivers — `hunmin_ic`, the boundary layer and
the syllable layer — gives the same `visible` sequence:

```
ㄱ → 가 → 각 → 가가 → 가ㄱ → 가 → "" → ""
```

while `consumed` differs on the third BackSpace (the boundary layer still holds the
word; the other two have delivered it). So the projection is the mode-invariant
assertion and `consumed` is the protocol, owned by tier C.

**The projection alone does not catch FE18's defect.** Mutation-measured: disabling
`backspace_deletes_word` leaves the projection comparison green (a commit followed by
the client's delete is text-equivalent) and turns red the assertion that a BackSpace
**commits nothing** in either mode. Both assertions therefore live in
`engine-buffered-preedit`; a text-only corpus would have been green while the defect
shipped. That is the second measured instance of the class
`docs/DESIGN-FRONTEND.md` §6 and `TRACE-X11-BOUNDARY-220` already record.

## What changed

| artefact | change |
|---|---|
| `tests/golden/case-{dubeolsik,sebeolsik-final,sin3-p2,sin3-p2-basic,sin3-p2-yet}.jsonl` | `commit`/`preedit` → `visible`, `meta.assert = "projection"` |
| `tools/sim.c --check` | asserts `visible` (cumulative, client edits modelled); refuses a line carrying both fields, and an event without `visible` under the projection meta |
| `tools/gen-case-golden.py` | writes `visible` |
| `tools/merge-golden-cases.py` | recomputes `visible` for the merged stream from the engine |
| `tools/gen-conformance.py` | a projection corpus's per-key split comes from `hunmin-sim --trace`, cross-checked against `visible` |
| `ref-*`, `patim-*` (`mode: "total"`), `judgement-*` | unchanged — external oracles keep their own schema |

`total` is **not** `visible`: it is patim's per-step composing-else-commit, not the
cumulative user text. The two names are kept apart in `docs/SPEC-SCHEMA.md`.

## The proof the pipeline did not change meaning

For each converted family the trace was regenerated from the engine and compared with
the committed file:

```
case-dubeolsik  (107 events)  byte-identical
case-sebeolsik-final (75)     byte-identical
case-sin3-p2    (100 events)  byte-identical
digitrow-sin3-p2-basic (12)   byte-identical
digitrow-sin3-p2-yet   (12)   byte-identical
```

Only `tests/golden/*.jsonl` changed shape; `.in` and `.gold` are untouched. That is
the diff a reviewer checks instead of trusting the migration.

## What is still open

- `ReqMode4`'s "consumed/declined for each key" and FE18 answer differently for one
  BackSpace; the measurement is in `docs/SPEC-MODES.md#§ReqMode4` and
  `docs/SPEC-FRONTEND.md#§FE18`, the ruling is the maintainer's.
- The other corpora (`coverage-*`, `cultural-*`, `words-*`) could take the projection
  next; they were left alone so the diff stayed one change with one proof.
- A post-fix usability session is the only evidence for the user-level outcome
  (DEL runs that no longer leave text behind); the trace statistic is named in FE18.

## Related

- [[2026-11-27-fe18-backspace-edits-the-word]] — the fix whose churn motivated this.
- [[2026-11-26-first-usability-session]] — the measurement that produced FE18.