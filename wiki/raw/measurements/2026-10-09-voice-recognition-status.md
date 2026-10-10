---
title: Measurement — the Android voice path, measured on the emulator: it runs, and recognition is never tested
created: 2026-10-09
updated: 2026-10-10
type: measurement
status: active
ingested: 2026-10-09
tags: [measurement, android, voice, speech, testing, emulator, correction]
sources:
  - ../../../frontends/android/app/src/main/java/org/hunmin/ime/VoiceInput.kt
  - ../../../frontends/android/app/src/main/java/org/hunmin/ime/HunminInputMethodService.kt
  - ../../../frontends/android/app/src/main/java/org/hunmin/ime/LayoutPicker.kt
  - ../../../frontends/android/app/src/main/AndroidManifest.xml
  - ../../../frontends/android/app/src/test/java/org/hunmin/ime/VoiceSessionTest.kt
  - ../../../tools/android-e2e.sh
  - ../../../tools/android-unit.sh
  - ../../../tools/android-mutations.sh
  - ../../../docs/FRONTEND-CONTRACT.md
  - ~/Android/Sdk/platforms/android-35/android.jar
confidence: high
---

# Measurement — the Android voice path, measured on the emulator

**What this is.** The record answering the question the maintainer asked on
2026-10-07 alongside the space bug — *"그리고 아직 음성 인식 쪽은 동작하지
않나?"* — which two sessions left unanswered. It answers what was asked and
nothing more: what the recogniser is, whether the permission is requested, what
the emulator image can and cannot do, and which of the claims in `AGENTS.md` are
held by a test rather than by a comment.

**It is not a verdict.** Whether voice should ship, what to build next, and
whether a permission the user must grant by hand is acceptable are the
maintainer's calls. Nothing below is a recommendation.

## The answer, in one line

The launch path works and reaches the platform's recogniser; **recognition itself
has never been demonstrated**, because no test has ever given it audio, and on a
normal install the microphone permission is denied until a human grants it — so a
freshly installed APK taps its mic, logs one line, and produces no text.

## (a) What recogniser the voice path uses

One real implementation behind a one-method seam, `Recognizer`
(`VoiceInput.kt:86`), with `SpeechRecognizerRecognizer` (`VoiceInput.kt:109`) as
the production one and a fake for tier E1. It is the **in-process**
`android.speech.SpeechRecognizer`, built with
`createSpeechRecognizer(context, null)` — the null `ComponentName` means the
platform's default `RecognitionService`. The intent is
`RecognizerIntent.ACTION_RECOGNIZE_SPEECH` carrying `EXTRA_LANGUAGE "ko-KR"`,
`EXTRA_PARTIAL_RESULTS false`, `EXTRA_MAX_RESULTS 1`, and **no**
`EXTRA_LANGUAGE_MODEL`.

`VoiceLauncher.pickLaunch` → `VoiceLauncher.decide(recognizerAvailable,
activityAvailable)` (`VoiceInput.kt:264`) is a pure function of two booleans:
recogniser present → `SPEECH_RECOGNIZER` (result by callback); recogniser absent
but an activity present → `FALLBACK_ACTIVITY` (`startActivity`, then `Failed`);
neither → `Unavailable`. `VoiceSession.start()` (`VoiceInput.kt:307`) checks, in
order: null Context → `Unavailable`; `checkSelfPermission(RECORD_AUDIO) !=
GRANTED` → `PermissionDenied`; path null → `Unavailable`; `FALLBACK_ACTIVITY` →
`startFallback()`; otherwise `startInProcess()`.

**Measured on the emulator** (AOSP `hunmin-e2e`, API 35, x86_64), tapping the
mic at `adb shell input tap 943 2300`, recogniser service pid 20788:

```
02:12:33.228 I/RecognitionServiceImpl: RecognitionService#onStartListening
02:12:33.239 I/NetworkSpeechRecognizer: Online recognizer - start listening
02:12:33.241 I/SodaSpeechRecognizer: Offline recognizer - start listening
02:12:33.242 I/SodaSpeechRecognizer: Initialize Soda [locale: ko-KR], [applicationDomain: AMBIENT_ONESHOT]
02:12:33.286 E/SodaSpeechRecognizer: Failed to get language pack of required locale: error 13
02:12:33.293 E/AudioRecord-JNI: Unable to retrieve AudioRecord object
02:12:33.385 W/RecognitionServiceImpl: Speech recognition error type NO_SPEECH_DETECTED with error code -1
02:12:33.387 I/HunminVoice: voice failed: Failed(reason=no-match)
02:12:33.387 W/HunminIme: hunmin: voice failed (no-match)
```

Three things in that are evidence rather than narration. `Initialize Soda
[locale: ko-KR]` is the platform echoing the one intent extra the code sets, so
**the ko-KR request provably reaches the recogniser**. `onStartListening`
appearing at all proves the `SPEECH_RECOGNIZER` branch was taken, because
`pickLaunch` prefers it unconditionally. And the error is an answer, not a
timeout: the service came back with `NO_SPEECH_DETECTED`.

## (b) RECORD_AUDIO is never requested at runtime

`VoiceInput.kt:30` says so and names `HunminInputMethodService` as the place to
look. **That pointer is dangling.** Measured:

```
grep -rn -e requestPermissions -e onRequestPermissionsResult \
       -e shouldShowRequestPermissionRationale frontends/ tools/
  (no matches)
```

The only permission API anywhere in the tree is the read,
`context.checkSelfPermission` (`VoiceInput.kt:239`). So RECORD_AUDIO is declared
in `AndroidManifest.xml:31` and never asked for.

**The platform premise behind that is correct**, re-verified here rather than
inherited (`javap` on `android-35/android.jar`): `requestPermissions`,
`onRequestPermissionsResult` and `shouldShowRequestPermissionRationale` are on
`Activity`; `Context` has no `requestPermissions`; `InputMethodService` has
**zero** permission methods, reaching `checkSelfPermission` only through
`Service → ContextWrapper → Context`. The same holds for `onActivityResult`
(Activity only), which is what makes the fallback unable to return a result. So
`docs/FRONTEND-CONTRACT.md` B5 stands on both counts.

**What a denied permission actually does — measured.** `pm revoke`, then a mic
tap:

```
$ adb shell pm revoke org.hunmin.ime android.permission.RECORD_AUDIO
$ adb shell dumpsys package org.hunmin.ime | grep 'RECORD_AUDIO: granted'
        android.permission.RECORD_AUDIO: granted=false, ...
02:17:58.333 I/HunminVoice: voice failed: org.hunmin.ime.VoiceResult$PermissionDenied@13a9a26
02:17:58.333 W/HunminIme: hunmin: RECORD_AUDIO not granted; grant it in Settings
                  > Apps > Default keyboard apps. The keyboard itself is unaffected.
field after typing dks: "ㅎ계"
```

The keyboard typed. The property the manifest comment claims does hold.

**And the grant in CI comes from the harness, not the app.**
`tools/android-e2e.sh:83` installs with `adb install -r -g`; `-g` grants every
runtime permission. So the E2 run has RECORD_AUDIO `granted=true` and has never
exercised the branch above.

## (c) The emulator can run the recogniser; it has no microphone

`SpeechRecognizer.isRecognitionAvailable()` is **true** on this image, and the
tap above proves it behaviourally rather than by declaration —
`adb shell cmd package query-services --brief -a android.speech.RecognitionService`
returns two services, which also **confirms the two-service claim** in
`VoiceInput.kt:106-107` that was previously only asserted.

**Tier E cannot test recognition because of tooling, not because of the
platform.** The recogniser binds, starts, and answers; what it cannot get is
audio:

- `AudioRecord-JNI: Unable to retrieve AudioRecord object` — the AVD was booted
  with `-no-audio` (`tools/android-e2e.sh`, `docs/ENVIRONMENT.md`), so there is
  no capture device to open.
- `SodaSpeechRecognizer: Failed to get language pack of required locale: error 13`
  — no offline ko-KR model is installed on the image.
- The emulator also ships **Google Voice Typing** as a separate IME, visible in
  the input-method chooser.

What is *not* a tooling limit: recognition **quality** needs Google's servers, so
no amount of local work makes it measurable here.

## (d) Asserted by a test, or only by a comment

Of the three properties `AGENTS.md` claims for the voice launcher, two are held
by tests and one is not.

| claim | held by |
|---|---|
| a throwing recogniser is contained | **test** — `VoiceSessionTest:120` + mutation `voice-throw-escapes` |
| whitespace-only audio is silence, not invisible text | **test** — `VoiceSessionTest:91` + mutation `voice-no-trim` |
| exactly one result per attempt | **test** — `VoiceSessionTest:168` + mutation `voice-double-report` |
| **a mic on the input view** | **a comment only** |

The last row is the gap. `tools/android-e2e.sh:391` asserts that
`dumpsys package` mentions `android.permission.RECORD_AUDIO` — that checks a
**manifest line**, not a view and not a click. Nothing in any tier taps the mic.
So "a mic on the input view" had never been executed by a test, and measuring
it turned up two things.

### The strip grows one mic per `bind()`, and `bind()` is called twice

`LayoutPicker.bind()` calls `addView(ImageButton(...))` unconditionally
(`LayoutPicker.kt:130`), and `bind()` is reached from two places:
`onCreateInputView()` (`HunminInputMethodService.kt:345`) and `onStartInput()`
(`HunminInputMethodService.kt:140`). Measured on the emulator by counting dark
34-px clusters in the IME window's frame:

| state | mic glyphs in the strip |
|---|---|
| IME process freshly started, then one field focused | 1 |
| IME process alive across the Settings search flow | **3**, reproducible 3/3 rounds |

Three rounds, same count, same process pid 21789 — not a render artefact. All
of them carry the same `contentDescription` (`음성 입력`, `strings.xml`) and the
same handler, so the duplication is cosmetic here, but the count is a function of
how many times `bind()` ran on the live view, not a constant.

### The mic's lower half is under the navigation bar

```
IME window          frame=[0,2266][1080,2400]   (h=134)
navigation bar      frame=[0,2337][1080,2400]
mic glyph rows      2305..2373
```

The glyph straddles the nav bar. Measured: `input tap 943 2339` — the visual
centre — reaches **nothing**; the screen dims and the system's *Choose input
method* dialog appears, four times out of four. `input tap 943 2300` fires the
handler every time. So on this image only the top ~40% of the mic is reliably
tappable. Whether that holds with 3-button navigation is **not measured**.

## Where a document and the artefact disagree

`VoiceInput.kt:169-173`, `docs/FRONTEND-CONTRACT.md` B5 and `PLAN.md`'s risk entry
all state that `RecognizerIntent.LANGUAGE_MODEL_FREE_FORM` does not exist. On the
same `android.jar` they were checked against, it does:

```
$ javap -constants -cp android-35/android.jar android.speech.RecognizerIntent | grep -e FREE_FORM -e VOICE_INPUT
  public static final java.lang.String LANGUAGE_MODEL_FREE_FORM = "free_form";
$ # ACTION_VOICE_INPUT -> NoSuchFieldException   (this one really is absent)
```

The half that holds is `ACTION_VOICE_INPUT`. The behaviour is unaffected — the
code omits `EXTRA_LANGUAGE_MODEL`, free-form is the default when it is unset,
and that is the mode the recogniser ran in above — but the stated reason for not
naming the constant is not what the platform says. Recorded, not corrected:
which document changes is the maintainer's.

## What was wrong on the way to these numbers

All of it was the measurement being wrong before the code was.

- **Five taps produced no log and I nearly recorded "the mic does nothing".**
  The touch was landing on the navigation bar, not the button, and a
  `uiautomator dump` of the strip had already shown the reason — the dump only
  ever contained the *Settings* hierarchy, because the IME window is a separate
  window that `uiautomator dump` does not cross. I had been reading "no IME
  window in the hierarchy" as "nothing to tap".
- **An IME-switcher dialog was open for several taps** and swallowed them. It
  came from my own earlier spinner tap. Two taps at the same coordinates gave
  opposite answers, which is the tell.
- **`input tap 943 2339` reported `mInputShown=true` and still did nothing.** The
  flag says the window exists, not that the point is reachable.
- **Counted "3 mics" once, then could not reproduce it at all** by restarting
  the IME between rounds. It only reproduces with the process left alive — which
  is the condition under which it matters, and which I had been destroying.
- **`pm revoke` measured nothing** for the same reason: the field had lost focus,
  so the strip was mid-recreation and the tap went nowhere. The denied-permission
  result above is the second run, after `focus.sh` settled the state.

The reusable part: *every one of these looked like "voice does not work" from
the outside.* A tap that reaches nothing and a tap that reaches a button which
then fails are indistinguishable until you check whether the touch arrived.

## Not established here

- **Recognition quality**, and any end-to-end audio → text result. It needs a
  microphone and Google's servers. Unanswerable in this environment.
- **The `FALLBACK_ACTIVITY` and no-path branches.** The AVD has an in-process
  recogniser, so both are read from code and from `VoiceLauncher.decide`'s unit
  tests, never run.
- **Any OEM device**, including the One UI phone the risk entry is about. The
  Samsung `SpeechRecognizer`-availability question is untouched.
- **The duplicated-mic count under other focus flows.** 3 was measured and
  reproduced; "N `bind()` calls → N mics" is read from the code, not measured
  for N beyond 3.
- **Mic tappability with 3-button navigation**, where the nav bar is 48 dp of
  solid buttons rather than a 63 px gesture strip.
- **Release behaviour.** The APK measured here is the debug build; the signing
  and R8 work `PLAN.md` lists as missing is untouched by this record.