---
title: 2026-11-03 the x11 corpus replays every tier-A case, under the policy each one needs
created: 2026-11-03
updated: 2026-11-03
type: session
status: active
tags: [session, conformance, tier-c, corpus, issue-187, fcitx5, word-buffering]
sources:
  - ../../../docs/trace-spec.md
  - ../../../docs/TESTING.md
confidence: high
---

# 2026-11-03 the x11 corpus replays every tier-A case, under the policy each one needs

One line: issue #187's premise did not hold — the 13 cases the x11 corpus was
missing were not stale generation but a deliberate drop — and the fix was to put
the delivery policy into the trace's `meta` rather than to re-emit them.

## What the issue claimed, and what the tree said

The issue said the x11 corpus "was generated before those cases were added to
the goldens, and nothing regenerates it". That premise did not hold, and the
mirror session [[2026-10-31-milestone-f-win32-conformance-corpus]] is where it
came from. Measured: `git log` on
`tests/golden/p2-2-3-chosung.jsonl` and its siblings shows **one** commit since
the initial import, so the goldens never gained cases after the corpus was
generated. The real cause was commit c773fdd (2026-10-25, #74): the corpus went
from 118 files to 87 by dropping 22 per-case traces plus the nine `ref-*`
corpora, because the adapter had started committing whole words at boundaries
and `trace-emit` asserts per-event commit expectations. Regenerating the whole
corpus proved it is deterministic — all 87 committed traces come back
byte-identically — so nothing was stale; the cases really had been removed.

## The measurement that decided the shape of the fix

The 22 dropped cases all share one property: tier A records a commit on a
keypress it also **consumed**. That is the one shape a word-buffered adapter
cannot deliver at the same point, because it holds the text until a declined key
or teardown. Replayed under `HUNMIN_COMMIT_PER_SYLLABLE`
(`trace-emit` gets a `commit_mode` field on the trace's meta) every one of them
matches tier A **verbatim**, on every event — including the teardown flush, which
tier A does not record because its last line is a preedit.

So the corpus did not need weaker expectations; it needed to say which policy it
asserts, in the spirit of the shared-layer rule in
[[2026-11-01-modes-ownership-retires-the-detector]] (the engine judges, the
frontend switches). The alternative — a mode-insensitive projection that compares
«commits so far + preedit» — is what the **win32** runner already does
(`frontends/tsf/test/trace-emit.cpp` measures the document), and it is why the
win32 corpus could carry those cases all along. That asymmetry is the general
answer and is not implemented here: the comparison in `tools/trace-check.py` is
still delivery-point sensitive, so a case that commits mid-stream must name its
policy. Recorded as a finding, not a ruling.

## What shipped

- `tools/gen-conformance.py` derives `commit_mode: syllable` from the golden (a
  commit on a consumed key) and writes it into the trace's meta. It asks the
  golden, not the adapter, so a future golden with a mid-stream commit gets the
  right mode instead of being dropped.
- 22 restored traces: the 13 case instances (11 labels) the win32 corpus
  replayed, plus `p2-2-2-tiers`' eight — a family that had vanished from the x11
  corpus entirely. 87 → 109 traces, 263 events; the eight were then merged into
  one 25-event case (same events, 102 traces) — see "What the eight cases were
  for" below.
- `tools/conformance-coverage.py` + ctest `conformance-coverage`: every tier-A
  case of every family the corpus claims has a trace, every trace belongs to a
  recorded family, and `--regen` reproduces the corpus from goldens byte for
  byte. Verified to fail by deleting one trace and by planting a stray one.

## What it left

## What the eight cases were for

Asked before merging, because the labels looked like they might be load-bearing.
They are not: `p2-2-2-tiers` pins the UserKeyTable (prefix `jk`/`jl`/`j;` × third
key `1`/`!`/`B`, then the exit), and all eight cases already end with an empty
buffer — measured with `hunmin-sim --check` on a merged golden, 25/25 events
identical. So the split bought only failure attribution (a ctest name per
tier/prefix cell) and a grep-able map of which cells are sampled (7 of a 6×N
table). Merged: one case, same 263 events in the corpus, 102 traces, tier-C
126 → 120, and a leaked tier state `T` is now caught at every boundary rather
than only at case starts.

- The `ref-*` corpora are still absent (libhangul parity is tier A only), and
  the `slow` tier measures 94 s against a 60 s budget on this host —
  `engine-d3-edit` alone is 95 s under `-j16`, so the budget line needs a
  measurement on a quiet host before anyone calls it a regression.
- `engine-d1-judgement` is red on this host at the branch's commit, before and
  after this work: the R9 control case (`keys`, judged English) delivers `ㅏ됴`
  instead of `key`. It is a mode-judgement observation, not a corpus one.