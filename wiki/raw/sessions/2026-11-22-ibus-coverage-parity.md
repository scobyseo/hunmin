---
title: "IBus evaluation coverage parity: the XIM probe, two shared-harness defects, and the lanes that closed the gap"
created: 2026-11-22
updated: 2026-11-22
type: session
status: active
tags: [session, ibus, fcitx5, testing, d3, conformance, ci]
sources:
  - ../../../docs/TESTING.md
  - ../../../docs/REQ-MAP.md
  - ../../../frontends/ibus/test/test_state.cpp
  - ../../../frontends/ibus/test/test_engine_ui.cpp
  - ../../../tests/e2e/run-d3.sh
  - ../../../tests/e2e/xtest-type.py
  - https://github.com/scobyseo/hunmin/issues/316
  - https://github.com/scobyseo/hunmin/issues/283
confidence: high
ingested: 2026-11-22
---

# IBus evaluation coverage parity (issue #316)

The T1 follow-up to #283: IBus had D1 state, D1 judgement corpus, D2 UI
signals and one D3 lane, while fcitx5 had accumulated a much larger evaluation
history. This session inventoried the gap, added the missing equivalent
lanes, and recorded what the probes measured. The coverage table itself lives
in `docs/TESTING.md` (§IBus ↔ fcitx5 evaluation coverage) — one home, this
page is how we got there.

## What was added

- **D1** (`engine-d1-ibus`): mode persistence across the focus flush/reset,
  the autoswitch value surviving a layer rebuild, the sin3-p2 symbol tier at
  the word boundary, punctuation as a word end.
- **D2** (`engine-d2-ibus-ui`): focus_out/reset through the real vfuncs,
  candidate_clicked (the mouse half), page_down/page_up, release/bare-Shift/
  Ctrl-chord declines, the mode property, and CapsLock as composition
  (sin3-p2-yet, the `p2-2-4-3-caps.jsonl` authority).
- **Tier C** (`conformance-ibus-*`): a new `ibus-trace-emit` replays the SAME
  x11 `.trace.in`/`.gold` corpus through `hunmin_ibus_translate_key` +
  `hunmin_state`, observing commits on a recording sink — no expectation
  queue to echo, so a wrong commit cannot pass silently.
- **D3** (`engine-d3-ibus-edit`, `engine-d3-ibus-candidate`): the edit and
  candidate case families through the real daemon, GTK client, same case
  files and sim oracle as fcitx5. The candidate lane asserts daemon+engine
  survival and delivery; the host lookup table has no own window to draw.
- **CI**: the jammy job's selection covers the new lanes and a JUnit gate
  fails on a skip or an absent lane; the noble gate counts the tier-C IBus
  lanes by prefix.

## The XIM probe, and why the lanes use GTK

The XIM client was probed first (fcitx5's edit lane uses it). Three host facts
made its stream a different shape from the shared oracle — all recorded in
`docs/TESTING.md` beside the coverage table:

1. IBus's XIM server ignores UTF-8 negotiation: compound text with three
   designations (ISO-2022-KR, GB2312, UTF-8). glibc iconv's ISO-2022-KR module
   passes the 7-bit form through UNCHANGED (measured with a 10-line probe);
   Xlib's `Xutf8TextPropertyToTextList` decodes all three.
2. A stale preedit snapshot is committed beside the real commit
   (`dkssud^` → `COMMIT:안녀ㅕ | COMMIT:안녕`). The D-Bus-level D2 stream is
   correct; this is ibus-x11's preedit termination.
3. A teardown flush routes into the NEXT case's client (the engine process
   persists across cases).

The GTK immodule is the path issue #292 measured (the 22.04 release ships
IBus only — [[2026-11-09-the-22-04-release-ships-ibus-only]]); the new lanes
reuse it. The probe is recorded so it is not repeated.

## Two shared-harness defects the new lanes exposed

- `xtest-type.py` sent no Shift for shifted SYMBOLS: `^` typed `6`. The
  fcitx5 edit lane's `dkssud^` case was comparing the engine's answer to `^`
  against a `6` that was actually typed — a defect in the shared injector,
  not in either frontend. Fixed for both.
- `run-d3.sh`'s edit loop did not tear down the reused GTK harness on a
  layout change: two harnesses wrote one case log (sparse, NUL-filled) and
  the old window could hold the X focus. The ordinary loop's teardown now
  runs in the edit loop too.

## The mask: why the injector defect stayed hidden

A SECOND harness defect cancelled the first. `xim-harness.c`'s `on_forward`
passed the keycode as `mods_depressed` to `xkb_state_update_mask`; keycode 15
(the 6/^ key) is the bit pattern Shift|Lock|Control|Mod1, so a `6` typed
without Shift resolved to `^`. The fcitx5 edit lane passed for the wrong
reason — the [[the-rules-keep-breaking]] shape exactly. The IBus GTK lane
resolves keys correctly and exposed the injector; fixing the injector alone
would then have turned the fcitx5 XIM lane red. Falsification pair, measured:
fixed harness + old injector → `FAIL ... FORWARD:6`; fixed harness + fixed
injector → green, whole XIM D3 set re-run green (one documented start-gate
flake on the way, passed on re-run). The mask is now `ev->state`.

## What was wrong in this session's own first cut

- The D2 chord test asserted a Ctrl chord leaves the word in flight alone.
  The shared `classify()` rule says a chord is a BOUNDARY: the word is
  delivered on the way out. The test was inventing an expectation; the
  engine was right. Fixed to assert the shared rule.
- The first D3 attempt used the XIM client and an iconv decode; both were
  dropped after the probe above, not papered over.

## Budget

The baseline was ALREADY over both local budgets before any test was added
(inner loop and slow tier — see the issue comment for the measured numbers
and the delta this change adds). The pre-existing overrun is a finding for
the maintainer, not something this issue rules on; the new D1/D2/C cases are
cheap, the two new D3 lanes land in the slow tier, and the numbers are
reported rather than smoothed over. See [[the-rules-keep-breaking]] for the
green-by-omission shape the CI gates exist to prevent.
