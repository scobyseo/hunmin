# Tier E3 — physical BT-keyboard checklist

**This is a manual tier and cannot be automated.** No amount of emulator work
replaces it: what E3 exists to find is the set of things a real keyboard, a real
OEM's IME switcher and a real user's hands do that an emulator never will.

**Status: NOT STARTED. No device has been signed off.** The tooling it needs —
a trace sink in the shipped APK and `tools/e3-trace.sh` to collect it — landed
2026-10-04; the sessions themselves have not happened. PLAN.md's P5 gate
("tier-E suite + BT-keyboard manual matrix") is therefore still open.

**Partial verification on device R5KL808WBPN (Galaxy S21+, Android 13):**
- `hunmin-sim --layout sin3-p2 --keys dkssud` produces expected output: commits `ㅎ곈`, ends on preedit `디`
- Trace mechanism works: `tools/e3-trace.sh start/stop/pull` captures key events with modifiers and IME response
- Trace files written to `tests/conformance/` with meta + event JSON lines
- Additional layouts (dubeolsik, kfcQ) also produce traceable output
- Some compositions may differ slightly from sim due to `input text` vs hardware key mapping

Sessions must still be run on physical devices with BT keyboards for full sign-off.

## Why a session must record a trace

The point of E3 is not the pass/fail column. It is that **every session leaves
behind a tier-C trace**, so a device quirk becomes regression data instead of a
story someone remembers. `tests/conformance/android-<device>.trace.gold` is the
artefact; a finding with no trace is a finding that gets rediscovered.

So the order matters: **start the trace first**, then type. A trace enabled
after the interesting keypress records nothing about it.

```sh
tools/e3-trace.sh start                 # on the device, via adb
# ... do the checklist ...
tools/e3-trace.sh pull                  # writes tests/conformance/<name>.trace
```

## Target devices

At least one One UI phone and one tablet, per TESTING.md tier E3. One UI is not
optional: Samsung's IME switcher and its handling of a third-party IME is a
different code path from AOSP's, and the plan already assumes it (see the voice
risk entry — Samsung's `SpeechRecognizer` availability varies).

| # | Device | OS | AVD/serial | Result | Trace |
|---|---|---|---|---|---|
| 1 | One UI phone (Galaxy S/A series) | Android 13+ | R5KL808WBPN | ☐ | — |
| 2 | One UI tablet (Tab S series) | Android 13+ | — | ☐ | — |
| 3 | AOSP phone (Pixel) | Android 14+ | — | ☐ | — |

Add rows as devices are signed off; do not delete a row that failed. A failure
with a trace attached is worth more than a blank tick.

## Steps

Expectations come from `hunmin-sim`, which is the single source of truth — the
same rule `tests/e2e/cases.tsv` follows. Run the command to get the expected
value; do not type what you remember.

### 1. Typing smoke

| key sequence | layout | expected | command |
|---|---|---|---|
| `dkssud` | sin3-p2 | commits `ㅎ곈`, ends on preedit `디` | `hunmin-sim --layout sin3-p2 --keys dkssud` |
| `dkssud` | dubeolsik | commits `안`, ends on preedit `녕` | `hunmin-sim --layout dubeolsik --keys dkssud` |
| `kfcQ` | sin3-p2 | commits `각`, ends on preedit `ㅒ` | `hunmin-sim --layout sin3-p2 --keys kfcQ` |

Note the "ends on preedit" half: the trailing consonant has no 받침 partner yet,
so it cannot become a syllable and stays in the preedit. That is correct
behaviour, and a tester who expects `안녕` in one shot will file it as a bug.

Type each into a plain text field (Settings' search box is fine). ☐

**What to look for beyond the text:** a key the physical keyboard sends that the
emulator never did — an OEM key, a different `mods` value for the same physical
key, or a key that produces nothing. Those are exactly what the trace captures.

### 2. Shift is data, not a modifier

| keys | expected | command |
|---|---|---|
| `k`, `K`, `k` under sin3-p2 | `ㄱ`, `"`, `ㄱ` | `hunmin-sim --layout sin3-p2 --keys kKk` |

Hold Shift through all three. If Shift held for letter 2 leaks into letter 3, the
adapter has a modifier-state bug the emulator cannot show. ☐

**Note.** Under sin3-p2 the shifted `K` is the **double-quote symbol**, not a
consonant tier — the engine is right, that is just what the layout says.

This is also where `hunmin-sim --trace` was found emitting **invalid JSON**:
a commit of `ㄱ"` produced `"commit":"ㄱ""`, which no parser can read, and
`tools/gen-conformance.py` builds the tier C corpus from exactly this output.
Fixed in the same commit (`json_escape` in `tools/sim.c`), so the expectation
above is reproducible and machine-readable.

### 3. CapsLock quirks

The engine takes the **unshifted** letter plus a `caps` option, because AOSP
delivers the shifted form under CapsLock. On a real keyboard:

| test | expected |
|---|---|
| CapsLock on, type `k` | same as `k` — no shift row |
| CapsLock on, type `K` (Shift+k) | the shift row |

`sin3-p2-yet` is the layout that reads `caps`; on `sin3-p2` this step is
inert and only proves nothing crashes. ☐

**This is the step most likely to fail on Samsung.** Record the exact symptom.

### 4. Voice button

**On device (this tier):** Tap the mic on the IME strip, speak a Korean sentence,
confirm it lands **verbatim** and is not re-composed. If RECORD_AUDIO has not
been granted the IME says so and keeps typing — that is the correct outcome,
not a failure. An IME has no permission-request hook (FRONTEND-CONTRACT B5),
so the grant happens in system settings. ☐

**On emulator (tier E2):** `tools/android-e2e.sh` taps the mic button in both
granted and revoked states, verifies the platform reports the permission state
correctly, and asserts the keyboard still types after a denied tap. The voice
result itself is unobservable on the emulator (no SpeechRecognizer service, and
`pm revoke` puts the permission in AppOps `ignore` mode which is a silent
deny), so the *effect* of the tap is verified at tier E1 via
`VoiceLauncher.decidePermission` unit tests. ☐

### 5. One-tap IME switch, and back

1. Switch to the OEM keyboard (Samsung's gesture or the system picker).
2. Type `dkssud` — the OEM keyboard must produce its own behaviour, unhindered.
3. Switch back to hunmin in one action.
4. Type `dkssud` again → the field shows `ㅎ곈디`: `ㅎ곈` committed and `디`
   still composing (same split as step 1).

Step 2 is the one that matters: **hunmin must not swallow keys when it is not
the active IME.** ☐

### 6. Focus changes mid-composition

1. Type `dk` (a composing syllable), then tap another text field.
2. The composition must be dropped, not committed into the new field.

Covered by `onStartInput` closing the engine. If text appears in the wrong
field, capture it — this is a data-corruption bug. ☐

## Recording a result

For each step, per device:

- ☐ pass / ☐ fail / ☐ blocked, with **what** happened, not "broken"
- the pulled trace, committed to `tests/conformance/`
- for a fail: the tier A command you compared against, and its output

A finding without those three is not actionable and will be closed as
unreproducible.

## What this tier cannot tell you

- Anything about the engine. E0/E1/E2 cover that, on a device, automatically.
- Whether a *specific* OEM build behaves like the ones tested here. One signed
  off is a sample, not a guarantee, and the tier says so rather than implying
  coverage it does not have.
- Voice recognition quality. Recognition is an external service (FRONTEND-CONTRACT
  B6); step 4 checks the *path*, not the speech.
