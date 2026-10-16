---
title: Tier E2 was asserting nothing — dump_ui failed every caller, and now the keys bypass the IME
created: 2026-10-16
updated: 2026-10-16
type: measurement
status: active
tags: [android, e2e, measurement, postmortem]
ingested: 2026-10-16
sources:
  - ../../../tools/android-e2e.sh
  - ../../../docs/TESTING.md
  - ../../../wiki/raw/measurements/2026-10-10-d3-case-coverage.md
confidence: high
---

# Tier E2 was asserting nothing — and now the keys bypass the IME

Two separate findings from one day. The first is closed and fixed; the second is
open and **not root-caused**. Both are recorded here because the first is the
best example in this repository of a tier reporting green while measuring
nothing, and the second is a live blocker for the release gate in
[[2026-10-16-release-gate-all-tests-pass]].

## 1. Closed: `dump_ui` failed every caller (found 2026-10-16)

Two defects in `tools/android-e2e.sh`, both meaning the same thing — the tier
stopped asserting and nobody could tell, because a skip is exit 77 and ctest
counts 77 as not-failed.

**`dump_ui` was defined below its first call.** It is called at line 185 inside
`tap_resource_id`, and defined ~420 lines further down. Bash has no forward
declarations, so every call before the definition ran
`dump_ui: command not found`, the function returned 1, and every tap by
resource id silently did nothing. Introduced by `7468bf6` ("use dump_ui in
tap_resource_id"), which added the call and not the definition.

**`dump_ui`'s validity check could never pass.** It accepted a dump when
`wc -l` was greater than 5. `uiautomator dump` writes the hierarchy as **one
line** — measured 33,540 bytes with `wc -l` returning 0 — so the check was
always false and the function always returned 1, after 5 attempts and 10 s of
sleeping per caller. `${#dump} -gt 200` is the check that works.

Every `tap_resource_id`, every `tap_text` and every `LIST=` dump depends on it.

**Why it stayed invisible.** The tier's failure mode is `skip`, which is exit 77.
So a tier that could not find a widget, could not tap it, and could not read the
screen reported `100% tests passed`. The R8 checklist's own note that tier E2
"asserts against the debug APK by design" is true and was never the issue; the
issue is that it had stopped asserting.

**Proof it was the cause:** with both fixed, `engine-android-e2e` ran to
completion twice on 2026-10-16 at 108 s each, asserting the real thing —
`dkssud` → `ㅎ곈디`, the granted and revoked `RECORD_AUDIO` branches, the
keyboard surviving a denied mic tap, 7 captured trace lines, the settings window
and its four key-binding rows.

## 2. Open: injected keys reach the field as literal ASCII

Once the tier actually ran, this became visible. Signature, measured:

```
field now reads: 'dkssud'          (expected 'ㅎ곈디')
FAIL: literal ASCII reached the field
```

Simultaneously, `dumpsys input_method` reports all of: `mCurMethodId` =
hunmin, `mHaveConnection=true`, `mBoundToMethod=true`, `mVisibleBound=true`,
`mInputShown=true`, `mWindowVisible=true`, with a non-null `mServedView` and
`mCurrentFocus` on Settings' `SearchActivity`. **The IME is current, connected,
bound, shown and visible, and the key still bypasses it.** The IME process is
running; `logcat` shows `ActivityManager` scheduling restarts of
`org.hunmin.ime/.HunminInputMethodService` at one point during the day, so the
service was crashing at least once, but no crash stack is available now and it
does not obviously account for the steady-state failure.

### Hypotheses tested and refuted

Recorded because the refutations are the useful part — each one is a measurement
that cost time and would otherwise be re-tried.

| Hypothesis | Test | Result |
|---|---|---|
| Settings' search widget is missing on this image | dump the hierarchy after a cold `pm clear` + launch | **refuted**: `com.android.settings:id/search_bar` present, within 3 s warm and after 20 s cold |
| The AVD has drifted into a bad state | `adb emu kill`, reboot the AVD from scratch, re-run | **refuted**: identical failure on a freshly booted emulator |
| The first render is slow, so one dump samples too early | measured the node appearing at 3/6/9/12/18 s | **refuted**: present at 3 s, and an 18 s bounded poll did not help |
| `field_text` reads an EditText from the wrong window (`--windows`) | re-ran with the focused-window dump | **refuted**: same result either way |
| The platform hides the IME because it sees a hardware keyboard | `settings get secure show_ime_with_hard_keyboard` | **refuted**: `1` |

It also alternates between `Failed` and `Skipped` across *identical
consecutive runs*, which is why a single green run is not evidence that it is
fixed — the same lesson as [[2026-10-10-d3-case-coverage]].

**Pass rate, measured.** It is not reliably broken and it is not reliably green;
it is intermittent, and the rate is the useful number:

| When | Result |
|---|---|
| twice, right after the `dump_ui` fixes | **Passed**, 108.8 s and 108.9 s, full assertions |
| `tools/release-gate.sh -j4`, whole suite | **Passed** (the one gate run that was green) |
| 3 consecutive runs after that | **Failed, Failed, Skipped** — 0 of 3 |

So a green `engine-android-e2e` in one run is not evidence about the next. Any
count of "the suite is green" that does not say how many times E2 ran is
meaningless here — which is the argument for the gate re-running a failing test
rather than trusting a single sample.

### What would settle it

The gap is between "the IME window is up" and "the IME receives the key". The
decisive measurement is not a dumpsys summary but the platform's own key
routing: whether `dispatchKeyEventPreIme` reaches the IME for these injected
events, or whether the EditText's own handling consumes them first. Concretely:
- capture `adb logcat` **around** the probe rather than after the run, and
  diff it between a passing and a failing run;
- check whether the AVD's hardware-keyboard profile changed (a `config.ini` /
  `hw.keyboard` difference between the host where it passed and this one);
- try `input keyevent --longpress` / `input text` as a control, to establish
  whether *any* injected key reaches the IME on this device right now.

## What is NOT concluded

That the IME is broken, and that the tier is unfixable. The engine asserted
correctly twice the same day; the failure is in key delivery, not composition.
No cause is recorded because none was found, and a guess here would be worse
than the gap.