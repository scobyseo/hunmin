# Android word-boundary buffering model for Hunmin

> **SUPERSEDED (2026-10-25).** The buffer this page designs no longer exists in
> the adapter: issue #128 moved it into `libhunmin` (`hunmin_buffered_preedit`,
> `libhunmin/src/buffered_preedit.c:6`), and the Android adapter now calls that
> layer (`frontends/android/app/src/main/java/org/hunmin/ime/HunminAdapter.kt` holds no `wordbuf`), with the
> commit unit a user setting. What remains true and is NOT restated here: the
> layer's contract lives in `docs/FRONTEND-EQUIVALENCE.md` §5 and §8.1, and the
> measured reason the three buffers were one buffer is the comment at
> `buffered_preedit.c:6-25`. This page stays as the dated record of the design
> that preceded it — do not read its `[wordbuf]` prose as the current shape.

## Purpose

Avoid the fragmenting-commit presentation: before issue #75 the Android adapter
committed each finalised syllable the moment the engine produced it, so a word
landed one commitText per syllable. The TSF frontend (issues #72/#73) measured
the same problem and fixed it adapter-side; this is the same design for the
Android adapter — buffer the pending preedit until a whole word is composed,
render through the client preedit path, and insert once at the boundary.

## Design

The engine already keeps the state (no libhunmin change, same property measured
for TSF in `docs/WINDOWS-TSF-BUFFERING.md` §Adapter state):

- `hunmin_ic_commit(ic)` is a DESTRUCTIVE read that drains every finalised
  syllable so far into the adapter's word buffer ([`wordbuf`]).
- `hunmin_ic_flush(ic)` finalises the in-flight syllable first (its saved
  commit buffer plus the flight remainder), which is what the boundary emits.

The adapter ([`HunminAdapter`], formerly HunminAdapter) applies five rules:

1. On a consumed key, the engine's commit is appended to [wordbuf] and never
   reaches the client directly — the client sees the accumulated word rendered
   as composing text (`updatePreedit`: `wordbuf` + `hunmin_ic_preedit`).
2. On the FIRST boundary key — the engine declining a key (space, Enter,
   punctuation, backspace, unmapped), a modifier chord, or a mode/layout
   switch — `commitWord` joins [wordbuf] with `hunmin_ic_flush`'s remainder
   and inserts the WHOLE word in ONE commitText, then clears the composing
   region. Short words therefore still behave exactly as before.
3. The service's Space handler (the Hangul-mode interceptor) passes through:
   `commitWord` first, then the literal space — the same order the TSF rule 1
   uses. On the decline path the engine has already moved the in-flight
   syllable into its commit buffer (`ic_push` → `save_commit` before
   returning false), so `commit` and `flush` read back the same text there.
4. External text (voice, invariant 5) delivers [wordbuf] first, then the
   spoken text — "typed, then spoken" ordering is preserved even though the
   engine only knows about the in-flight part.
5. `commitHanja` and `reset` drop [wordbuf] WITH the composition: a Hanja
   replaces the word one-for-one (never appends after it), and a focus change
   must not leave a word the engine has forgotten.

Naming: the method is `commitWord` rather than an inherited `flush` because
what it does is not the ABI's flush — it joins the buffer with the ABI's flush
output and delivers once. `HunminAdapter` carries the adapter's role in its
name (design-pattern naming was applied across the Android frontend in the
same step: `HunminNativeBridge`, `HunminKeyRegistry`, `HunminDataInstaller`,
`HunminTraceRecorder`, `HangulModeRepository`, `KeyBindingRepository`,
`LayoutRegistry`).

## Adapter state (measured 2026-10-17, issue #75)

Tier E1 (`tools/android-unit.sh`, engine-android-e1): **116/116** with the
buffering contract asserted — a two-syllable word composes as ONE client
region ("안녕"), commits nothing per keystroke, and the boundary delivers
"안녕" once; the tier-A golden corpus (`manual-sin3-p2.jsonl`) is replayed
end-of-word: expected total = per-keystroke commits joined + the LAST event's
preedit, compared against the adapter's total after one boundary
`commitWord`. Tier C (`engine-android-conformance-*`) is shim-level and
stays 16/16 — the buffering lives adapter-side, above the traced ABI.

## Open questions

- Tier E2/E3 (device): the Espresso suite exercises the service-level Space
  path on the AVD and must be re-measured here once the Windows host has an
  emulator hypervisor driver (currently absent, admin needed). The E2
  assertions that a space commits the syllable are unchanged by the buffer;
  what a device must re-measure is the presentation, not the text.
- Whether user-visible replacement of the composing region on commitText
  behaves identically in every client implementation (the standard contract
  says it does; RecordingInputConnection models it and tier E2 would catch a
  client that does not).
