# Android — the system IME settings screen (surface B, issue #177)

**A device checklist, written before the emulator round and kept for the physical
one.** The host can prove the screen is registered (`aapt2 dump xmltree --file
res/xml/method.xml` shows `android:settingsActivity`) and that its state source
answers the engine (`AutoSwitchSettingsTest`, tier E1). The steps below were run
on the emulator on 2026-11-03 (see "Run" at the end); what a real device adds is
what an emulator does not have — an OEM's IME switcher, a physical keyboard, and
a battery-restarted process.

Record the result as a measurement: device, Android version, date, and what the
screen showed. A step that fails is a finding; it is not a verdict about the
design (AGENTS.md, "Working rules").

## Setup

```sh
cd frontends/android && ./gradlew :app:assembleDebug   # the APK the steps use
# on the attached device (tools/android-e2e.sh runs the whole tier-E2 flow and
# installs it itself, but it also types into the emulator, so a device session
# installs by hand):
adb install -r -g app/build/outputs/apk/debug/app-debug.apk
```

Settings → System → Languages & input → On-screen keyboard → hunmin → the gear
icon. Every step below starts from that screen.

The *other* surface is the in-keyboard settings window: it is the IME's own input
view gear (`OverlayActivity.startSettings`, `strings.xml`'s
`settings_content_description`), so it is only reachable while a soft keyboard is
on screen. Where a step compares the two, that is the window it means.

## Steps

1. **The screen is reachable through the platform's path.**
   The gear opens a screen titled `hunmin 설정`. If it opens nothing, or opens
   the in-keyboard overlay instead, record which activity the system launched
   (`adb shell dumpsys activity activities | grep -i hunmin`).

2. **The value shown is the engine's.**
   The row reads `자동 한/영 전환` with `꺼짐` (OFF is the default, ruled
   B3 2026-10-11) on a fresh install, and `켜짐` after turning it on in the
   in-keyboard settings window (the input view's gear). If the two surfaces disagree, that is R13-정정's
   invariant broken and the case is a bug.

3. **The key named is the engine's chord.**
   Under the row: `키 조합: Ctrl+한/영`, and the hint names the same chord.
   Compare with the overlay's row — they must read the same string.

4. **The data gate is visible when the data is absent.**
   With `modes/en-US.json` removed from the installed data directory
   (`adb shell run-as org.hunmin.ime ls files/` shows where the tables live),
   the value reads `사용할 수 없음 (자동 전환 사전 없음)`, the switch is
   disabled, and turning it on is impossible. Restore the file afterwards.

5. **A change takes effect in a running editor without a restart.** ← the
   regression this screen exists to avoid
   Open a text field (any editor), make hunmin the active IME with an editor
   still alive, and reach this screen through the platform's path (step 1: the
   gear in Settings, not the IME's own window). Turn the switch on, return to
   the editor, and type an English word on a physical keyboard: it must arrive
   as English (the judgement is ON). Turn it off, type the same word: it must
   arrive as its Hangul reading. No IME restart between the two.

6. **A change takes effect with no editor open.**
   With no text field focused (home screen), open the screen, turn the switch
   on, then open a text field and type. The state must be ON — the stored
   read-back is what the next editor's fresh layer is given.

7. **The switch cannot be driven by its own redraw.**
   Turn the switch on with the data absent (step 4's fixture) and watch it: it
   returns to OFF and stays there. A switch that keeps re-requesting ON is the
   `rendering` guard in `SettingsActivity.write` having failed.

## Run: 2026-11-03, emulator `hunmin-e2e` (API 35), app-debug from this branch

Measured with `adb shell input keyevent` per key (the path a hardware keyboard
takes, and the one `tools/android-e2e.sh` uses), reading the committed text back
out of the platform's own view hierarchy, not from the IME's log.

| step | result |
|---|---|
| 1 reachable through the platform path | Settings → System → Languages & input → On-screen keyboard → hunmin → the row opens `org.hunmin.ime/.SettingsActivity` (`dumpsys activity activities` → topResumedActivity) |
| 2 the value is the engine's | `꺼짐` on a fresh install, `켜짐` after the switch is moved; the stored read-back in `shared_prefs/hunmin.xml` follows it |
| 3 the key is the engine's chord | `키 조합: Ctrl+한/영`, the same string the in-keyboard row shows |
| 4 the gate when data is absent | **NOT REPRODUCIBLE this way.** Deleting `files/hunmin-data/modes/` and forcing the IME to stop makes `HunminDataInstaller.install` re-extract it from the APK (the stamp no longer matches), so the row is enabled again on the next open. The absent-data row was measured instead as the state of the shipped APK *before* the installer fix below; the E1 fixtures (`AutoSwitchSettingsTest`, `AutoSwitchGateTest`) stay the oracle for that state |
| 5 a change reaches a running editor | **PASS.** Editor alive, toggle ON on this screen, return, type `hello ` → `hello`; the same word before the toggle delivered `ㅗ디ㅣㅐ` (dubeolsik, Hangul mode). No IME restart between the two |
| 6 a change with no editor open | **PASS.** With no editor in front: toggle OFF, then open the editor and type `hello ` → `ㅗ디ㅣㅐ` |
| 7 the switch cannot drive itself | not reached: the guard matters only while the row is enabled and the engine refuses, and the refusal state cannot be built on a device (step 4). Covered by the E1 gate fixture instead |

**Defect found by this run, fixed in the same branch.** The APK packaged
`assets/data/modes/en-US.json` (the asset sync copies `libhunmin/data` minus
`krdict`), but `HunminDataInstaller.DATA_SUBDIRS` did not name `modes`, so the
file was never extracted. On the device the row therefore read
`사용할 수 없음 (자동 전환 사전 없음)` with the switch disabled — the B3 gate
working, on data the APK was carrying unused — and no surface could turn the
feature on. `HunminDataInstallerTest` now asserts that the extracted set equals
the shipped set minus `notInTheApk`, and both of its cases were seen red with
`modes` removed from the list before being seen green with it restored.

**What this run also showed about the mechanism.** The screen's live-service
branch is not the path the shipped flow takes: opening this screen ends the
editor's input session, `onFinishInput()` closes the engine, and the write goes
through `AutoSwitchSettings`' own layer. What makes step 5 true is the stored
read-back being re-applied by `HunminInputMethodService.onStartInput` — no IME
restart, but also no live layer.

## What this cannot show

- Whether a **soft keyboard** row should exist — the screen is unrelated to
  P5/0.4 and carries none.
- Whether the platform's Settings app labels the entry per-locale; that is the
  platform's string from `android:label`, and only a Korean-locale device shows
  it as `hunmin 설정`.