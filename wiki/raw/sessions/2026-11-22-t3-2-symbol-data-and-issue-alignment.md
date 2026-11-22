---
title: "T3-2: the symbol screen became data, and the T3 issues were brought back in line"
created: 2026-11-22
updated: 2026-11-22
ingested: 2026-11-22
type: session
status: active
tags: [android, layouts, decision, tests]
sources:
  - ../../../docs/SPEC-ANDROID-KEYPAD.md
  - ../../../docs/DESIGN-ANDROID-SYMBOLS.md
  - ../../../docs/REQ-MAP.md
  - https://github.com/scobyseo/hunmin/issues/297
  - https://github.com/scobyseo/hunmin/issues/298
confidence: high
---

# T3-2: the symbol screen became data, and the T3 issues were brought back in line

Issue #297 was written before `#298` ruled the symbol screen's data home. It said
"extend `dubeolsik.json` and `us-qwerty.json` with the decided third layer —
data only, no engine change", and the ruling said the opposite: no third per-key
row, an app-read data file. Nothing was implemented while that stood; the two
readings were put to the maintainer rather than one being picked.

The maintainer's instruction (2026-11-22, verbatim):

> "T3-1의 결정과 나머지 T3 issue들의 내용이 일치하지 않으면 지금 고치자. T3-1의
> 결정으로 대체할 거야."

## What the contradiction actually was

`#285` (Track 3) presented a third per-key symbol row as "still data, **no engine
change**". `docs/DESIGN-ANDROID-SYMBOLS.md` §1.2 had already measured that premise
to be false for both spellings: `doctor.py` rejects a `symbol` modifier, and
`alt` is accepted by the linter but never loaded by the engine — so A was an
engine change wearing a data change's label. See
[[2026-11-22-symbol-screen-app-read-data-file]] for the ruling itself.

## The correction (issues, then docs)

Issues carry no requirement text; they point at the SPEC. So the fix was to
delete the superseded prose and point at KP11:

- **#285** — the "three layers per key / no engine change" design question became
  a recorded decision, with option A marked rejected and the measured reason.
- **#297** — retitled to the app-read symbol data; its verification changed from
  "golden per new layer" (which assumed a third row existed) to a data-contract
  check plus an app-path golden.
- **#312** — its "data home" open question was closed by KP11: the variant
  mapping uses the same app-read file.

Three doc sentences still said the data home was open, which is the same
contradiction in another place: `docs/SPEC-ANDROID-KEYPAD.md#§KP10`, the mockup
doc's closing paragraph, and `docs/REQ-MAP.md`'s KP10 row. They were corrected to
point at KP11.

## The implementation (#297)

- `frontends/android/app/src/main/assets/keypad/symbols.json` — the app-read
  file. Committed, deliberately outside `assets/data/` (that tree is synced from
  `libhunmin/data` and is the engine's). The KP6-ruled three symbol rows, the
  ticketing rows as function-key **ids**, and `x-notes` naming what is absent and
  why (Tier 2 slot → #312; frequent-symbol keys → still open).
- `tools/symbol-data-check.py` — the content contract, ctest
  `keypad-symbol-data` (tier-B guard). It asserts the ruled arrangement, the
  closed function-id vocabulary, KP9's no-space/enter, the KP6 navigation order,
  and that no engine-layout field appears in the file.
- `SymbolScreen.kt` + `SymbolScreenTest.kt` — the parse and the commit path.
  `commitSymbol` commits the character directly (the engine is not involved), and
  the test iterates every symbol in the *shipped* file through
  `RecordingInputConnection`: exactly that character, nothing composing, no I9
  violation. These tests need no JNI shim, so they never skip.
- No engine, ABI or schema change: `libhunmin/data` and
  `libhunmin/tests/abi/symbols.txt` are untouched.

The unit-test JVM needed a real `org.json` on the test classpath. Without one,
`isReturnDefaultValues = true` makes android.jar's stub answer nulls and the
parse silently succeeds on anything — a green run measuring nothing. That is the
same failure shape `tools/android-unit.sh` was written to make loud.

## Two pre-existing reds this uncovered

Neither was caused by this work; both were invisible while tier E1 was already
red, and both are fixed here because a commit has to leave `ctest` green.

1. `HunminAdapterTest.every layout key has a keycode` scanned **every** `.json`
   under `data/layouts/`, including `meta.json` (provenance, added in #292),
   which has no `keys` object — so it threw. `tools/keymap-fixture.py` got the
   same exclusion in the commit that added the file; this parallel scan did not.
   Baseline measured before the fix: 159 tests, 1 failed.
2. `android-mutations`' `always-consume` plant anchored on the trace call's old
   single-line shape and no longer applied: "NOT APPLIED (pattern moved?)". A
   mutation that does not apply reports "the suite stayed green" for a mutation
   that never happened, which is the one result `apply-mutation.py` refuses to
   produce silently. The anchor was updated to the current call; the plant is
   now caught again (10 caught, 1 recorded known gap).

Only the first was reachable before the second appeared: the mutation driver
stops at its baseline gate, and the baseline gate was that failing test.
## Measurements (this host, 2026-11-22)

`tools/ci-time-budget.sh build`, configured with `ANDROID_HOME` and
`ANDROID_NDK` exported (so tier E1/E2 register), emulator `hunmin-e2e` up:

| run | before this work | after |
|---|---|---|
| inner loop `-LE slow` | 133.35 s, 1 failed | 133.4 s, 0 failed |
| slow tier `-L slow` | 49.74 s, 2 failed (mutation aborted at its baseline gate) | 111.8 s, 0 failed |
| suite `-LE mutation` | not taken | 176.9 s, 0 failed |

Both budgets are over on this configuration, before and after: the inner loop is
dominated by `engine-android-e2e` (88.9 s, `tier-E2`, not labelled `slow`), and
the slow tier by `android-mutations` (102–110 s here; `docs/CI-TIMING.md` §1
records 84 s in CI and 138 s in the android job). `docs/CI-TIMING.md` §4 records
`tier-E1: 0` for the host those budgets were set against — a configure without
`ANDROID_HOME` never registers these. So this is the documented local/CI delta,
not a cost this work added: the work's own delta is the five JVM cases inside
`engine-android-e1` and a 0.02 s tier-B checker.

The slow tier's +62 s after the change is the mutation tier running to
completion for the first time: on the untouched tree it aborted after 17 s
because tier E1 was red. No budget line was moved — that is the maintainer's.

Verified green after the work: `ctest -LE slow` (110 tests), `-L slow` (11,
mutation 10 caught / 1 known gap), `engine-android-e2e` (88.9 s, real APK on the
emulator), `engine-android-e1` (164 tests, 0 skipped, 0 failed),
`keypad-symbol-data`, `schema-contract`, `req-map`, `docs-lint`, `wiki-lint`.
