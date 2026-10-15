---
title: "engine-d3-candidate: the F9 candidate window gets a ctest tier, and the #45 abort regresses through it"
created: 2026-10-15
updated: 2026-10-15
type: measurement
status: active
tags: [measurement, fcitx5, e2e, testing, hanja, ci]
ingested: 2026-10-15
confidence: high
---

# engine-d3-candidate — the F9 candidate window gets a ctest tier

Issue [#53](https://github.com/scobyseo/hunmin/issues/53). The ruling asked for
is *which disposition* — a tier (A), a CI job wiring the repro script (B), or a
stated gap (C) — and the disposition taken here is **A**: a ctest tier that
draws the window. This page records what was measured while building it and
what the tier proved, so the choice stays comparable to the alternatives it
beat. The abort itself is measured in
[[2026-10-13-f9-candidate-window-abort]]; that page's last line is this one's
first: the abort had no ctest coverage.

## The gap, restated as a tree fact

Two tiers covered the path and neither could reach the crash:

| tier | has | lacks |
|---|---|---|
| `engine-d2-ui` | opens the F9 window, asserts `pageSize <= 10` | loads **no UI addon**, so nothing calls `label()` |
| `engine-d3-x11` / `-gtk3` / `-gtk4` | the whole real stack | `DisabledAddons=classicui`, so the window is never drawn |

The abort fires in the code between them: classicui walks every candidate on
the page and calls `label()` on each. `tools/repro-f9-candidates.sh` (issue
#45) was the missing middle, and nothing in CI ran it.

## What was built

`engine-d3-candidate` — `tests/e2e/run-d3.sh` with `HUNMIN_D3_CLIENT=candidate`.
One difference from the ordinary D3 XIM run: `CANDIDATE_CLASSICUI` is empty in
that mode, so the `DisabledAddons=classicui` line is omitted and classicui
loads. Everything else is the same staged stack, the same `reserve_display`
reservation, the same rc-77 skip behaviour. Cases live in
`tests/e2e/cases-candidate.tsv` (간 at 67 candidates, 간+Return, 간+Down+Return,
낙 at 11).

Three assertions per case, and the order is the design:

1. **fcitx5 survives** (`kill -0` after injection). This is the #45 abort
   oracle, and it is checked *before* the window, because a dead fcitx5 has
   torn down its X connection and `xwininfo` would report "no window" for a
   death that should read as a death.
2. **The window is drawn** (`xwininfo -root -children` finds
   `Fcitx5 Input Window`). Measured geometry: a one-character preedit window is
   ~27px tall; a candidate column is 293–706px. "Is there a window at all" is
   answerable from the height, so "fcitx5 survived" means "the window opened"
   rather than "nothing crashed because nothing was drawn".
3. **The expected COMMIT text**, where the case carries one.

## The red half — measured, which is the whole point

With `kMaxCandidatePageSize` changed from 10 to 1000 and the addon rebuilt,
the deep case fails:

```
run-d3.sh: line 914: 68196 Aborted (core dumped) fcitx5
FAIL [candidate/sin3-p2/kfs{F9}]: fcitx5 DIED during the F9 path
  F2026-10-15 ... CommonCandidateList: invalid label idx
```

That is the #45 abort reproduced by a ctest tier for the first time. Every
other tier goes green with the cap reverted — that was the gap, stated in the
issue and confirmed by building the tier that closes it.

## Two injector facts, both measured the hard way

- **Named keys must be braced.** `kfs{F9}Return` was read by the injector as
  the literal characters `R e t u r n` (six Latin keys), so the run committed
  `간|ᅟᅥᆸ|ㅋ|더` instead of `刊`. The token is `{Return}`, and
  `tests/e2e/xtest-type.py` gained the `{NAME}` keysym table (`{F9}`, `{Return}`,
  `{Down}`, `{Escape}`, `{Page_Down}`, `{Page_Up}`). XK_F9 = 0xFFC6, not 0xFFC2
  (F5): an earlier repro run sent F5, the engine declined it as an unmapped
  keysym, and the transcript read "F9 committed the syllable".
- **F9 opens the window asynchronously.** The injector settles 0.8s after F9.
  A `Return` at the default 40ms arrives before `candidateActive()` is true, so
  it is not handled as a candidate key: it flushes the source syllable to the
  client and the rest of the sequence lands as Latin garbage. The repro script
  settles 1.5s after F9 for the same reason.

## What the expectations are, and why they are hand-written

The sim cannot type `{F9}`, so a sim-derived expectation for a candidate
sequence would be a fiction. The third column of `cases-candidate.tsv` is
therefore a hand-written expected commit, verified against
`tools/repro-f9-candidates.sh` on the same build: `kfs{F9}{Return}` → `刊`,
`kfs{F9}{Down},{Return}` → `肝`, and the no-commit cases assert geometry +
survival only.

## A coincidence that first read as a drift, and why it was not

The first draft of this page claimed a doc-vs-code drift: the #45 wiki page
says "Page_Down reaches page 2 and page 3", the engine.cpp comment says
"arrows move the cursor rather than the page", and a shallow measurement
showed `Down,Return` and `Page_Down,Return` both committing `肝`. That reading
was wrong, and the way it was wrong is worth recording.

간 resolves to **67** candidates, and the first three are krdict word senses
(`kind=word`), not hanja:

```
[0] 刊  word    [1] 肝  word    [2] 間  word
[3..66] hanja ... [10] 肝  hanja ...
```

肝 appears at **index 1** (a word sense) *and* at **index 10** (a hanja).
So `Down` moves the cursor to index 1 → 肝, and `Page_Down` moves to page 2's
first candidate, index 10 → 肝. Both commit the same character because the
two indices hold the same character, not because the handlers do the same
thing. `Down,Down,Return` → 間 (index 2) and `Page_Down,Page_Down,Return` →
竿 (index 20) confirm the distinction. The code and both documents were
correct all along; the "drift" was a coincidence of the candidate list, and
the mistake was trusting a commit character without dumping the indices.

**This is the second time on this repo that a single measurement without the
underlying data was read as a contradiction** — the first is in
[[2026-10-13-f9-candidate-window-abort]]'s own history. When two claims appear
to disagree, dump the indices before recording the disagreement.

## Disposition cost, for comparability

The two alternatives this beat, and what each would have cost: **B** (wire the
repro script into CI) is one workflow job but leaves the F9 path outside the
`ctest` surface — a separate runner, a separate skip convention, and the tree
fact still unasserted by the suite a contributor runs locally. **C** (state
the gap) is cheapest and leaves the abort unregressable through `ctest`. **A**
costs ~30 lines of runner, an injector token table and a case file, and makes
the abort fail `ctest --test-dir build -LE slow`, which is the inner loop.

See [[decisions-belong-to-the-maintainer]] — this page records the
measurement and the build; the disposition itself is the maintainer's to
confirm or overturn.
