# Tier E4 — voice check, with a person in the loop

**This is a manual tier, and it is not a gate on recognition accuracy.**

The words come from the platform recogniser (`VoiceInput.kt` selects the
in-process `SpeechRecognizer`, with a fallback), and the input is live audio.
Recorded audio cannot be replayed into `SpeechRecognizer`, so "the transcript was
the words that were said" has no deterministic assertion. A percentage threshold
would be a test that fails on a bad network day and passes on a bad model day;
asserting one honestly would mean swapping the recogniser for a test-only build,
which measures something the APK does not ship.

So the tier is split, and the split is the whole design:

| | who judges | what it covers |
|---|---|---|
| contract | the script, exit code | OUR half: the key is found, a tap reaches the voice path, the mic reports LISTENING, **the mic is painted while it listens**, the session ends and returns to idle, the result reaches the editor, nothing died |
| intelligibility | a person, one y/n per phrase | whether the transcript was what was said |

Run it:

```sh
tools/android-voice-check.sh                    # prompts before each phrase
tools/android-voice-check.sh --only hangul-eojeol
tools/android-voice-check.sh --out ~/e4-samsung.txt
```

`ANDROID_SERIAL` is honoured; `--device SERIAL` overrides it. A person is
prompted before each phrase — the prompt is the cue to speak, and the script is
already waiting when it appears.

## The phrases, and why those five

Chosen for what **our** half does with the text, not for how well the model hears
it. A phrase whose characters would compose into something else if they reached
the engine as keystrokes is worth speaking, because invariant 5 says they must
not.

| id | speaks | what it is for |
|---|---|---|
| `hangul-greeting` | 안녕하세요 | the everyday case |
| `hangul-eojeol` | 서울역에서 만나요 | would compose differently as keystrokes; the invariant-5 case, end to end |
| `hangul-short` | 응 | one syllable; guards whitespace-only from being taken as text |
| `latin-yes` | yes | Latin mode must ask for `en-US`, not `ko-KR` |
| `silence` | (nothing) | no speech: the session must still end and the key return to idle |

The jamo-pipeline invariant itself is already gated deterministically at tier E1
(`HunminEngineTest`: `external text bypasses the jamo pipeline`, commits
in-flight composition first, does not disturb later typing, refuses malformed
text). **E4 does not re-test that**; it tests the path around it.

## Why the paint check is in the contract

"The state changed and the screen did not" is a real defect that shipped in the
dock: `bind()` was repainting an `ImageView` that was never added to the view
tree, so every mic state change was invisible while the log said `LISTENING` and
the field was non-null. Tier E1 cannot see it — the state object is correct.
Tier E2 missed it — it looks for the mic's `content-desc` at a moment chosen by
the harness, and the session is over by then.

It is a **screencap pixel**, not a `uiautomator` dump, and that is deliberate: a
dump of this window takes longer than the ~5 s listening session lasts, so it
reports the state *after* the session has ended. Measured on an SM-S938N, the
whole investigation was wrong for exactly that reason before it was right. The
check tests a pixel inside the key but outside the glyph for **greenness**, not
for an exact colour, so it survives both palettes (Green 600 light, Green 400
dark).

If `ffmpeg` is not installed the paint check prints `paint=SKIPPED`. **SKIPPED is
not PASS**: the contract rows around it still fail the run.

## What a failure means

The script exits non-zero and prints which contract broke. Attach the record it
writes (default `/tmp/hunmin-voice-check-<timestamp>.txt`) to the issue.

- `LISTENING but the mic key is not painted` — the dock's bind path. This one
  shipped before.
- `the mic never reported LISTENING` — the tap did not reach the voice path:
  callbacks captured at build time, or the key not in the tree at all.
- `the session never returned to IDLE` — a recogniser callback that never fires;
  the key stays green and the next tap cancels instead of starting.
- `the recogniser's text never reached the editor` — the commit path, or the
  editor check (invariant I9 is enforced by `RecordingInputConnection` at E1, so
  this is the device side of it).

## Not covered

- Recognition accuracy, by construction (above).
- Bluetooth/headset routing; the E3 checklist covers input paths, not audio paths.
- The Hanja, mode and settings keys: tier E2 owns those.

**Status: the script exists (2026-10-11); a signed-off run per device is a
maintainer session, like E3.** Devices seen so far: SM-S938N (Android 16),
SM-F971N (Android 17).