# Windows TSF buffering model for Hunmin

## Purpose

Avoid leftover and duplicated preedit text by treating TSF as a commit-only frontend,
following the Jamotong model: finalized syllables go into the document, the pending
syllable stays in the engine buffer and is rendered by an overlay instead of inline
TSF composition.

## Design

The engine already keeps the data:

- `hunmin_ic_preedit(ic)` is the buffering state.
- `hunmin_ic_commit(ic)` and `hunmin_ic_flush(ic)` finalize pending syllables.

TSF should therefore follow `frontends/fcitx5/src/hunminadapter.cpp` and this buffering rule:

1. On every key, map exactly as fcitx5 does.
   - Modifier-only combinations: leave composition and pass through, or flush when a
     new actual key arrives.
   - Unmapped printable or punctuation key: flush the pending engine data, then
     deliver the original key.
   - Mapped Hangul keys: run the engine and use only its result.
2. Render pending composition without inserting it into the document.
   - If the target application supports client preedit, use that presentation path.
   - Otherwise show the pending preedit through a small overlay anchored at the text
     caret, mirroring Jamotong.
3. ~~Never keep a TSF composition that owns the same text you later insert.~~
   **Superseded 2026-10-20 (issue #98):** the adapter now opens an inline
   composition for preedit. The rule's intent — the same text must never be
   delivered twice — is preserved: the composition range holds the buffered word
   plus in-flight preedit, and `EndComposition` fuses it into the document as
   ordinary text at the first boundary. The old sub-bullets (no `StartComposition`,
   `OnCompositionTerminated` a no-op) are history.
   - The adapter inserts only flattened commit text: via a synchronous edit session
     (`RequestEditSession`, `TF_ES_SYNC | TF_ES_READWRITE`) whose edit-doer sets the
     selection range with `ITfRange::SetText` (`FlushSession`, `DeactSession` in
     `frontends/tsf/src/tsf.cpp`). The draft's `ITfInsertAtSelection` wording was
     never implemented; measured 2026-10-17 and corrected here.
4. It must insert exactly once.
   - Commit text comes from `hunmin_ic_commit(...)`.
   - If the engine declines a keystroke, use `hunmin_ic_flush(...)` only once, then
     clear presentation.
   - Every path that passes a key to the application also updates or clears the
     pending preedit before returning.
5. TSF control code should be thin.
   - All key mapping logic and composition state live in `libhunmin`.
   - TSF code handles COM registration, edit sessions (insert only), TSF manager
     events, overlay placement, and presentation, with behavior equivalent to fcitx5.

## Expected benefits

- No partial syllables get re-inserted or left in the edit document.
- The preedit remains visible while typing.
- Space, Enter, punctuation, and arbitrary keys behave as pass-through after the
  engine buffer has been flattened.

## Adapter state (measured 2026-10-17, issue #73)

The TSF adapter implements the buffering rule adapter-side: finalised syllables
from `hunmin_ic_commit()` accumulate in the adapter (`wordbuf_`), the overlay
shows the accumulated word plus the in-flight preedit, and the first boundary
key (declined engine key: space, Enter, punctuation, backspace, unmapped; and
the edit/navigation keys — ←/→/↑/↓/Home/End/PgUp/PgDn/Insert/Delete, measured
2026-10-18 (issue #95) after the printable-VK fallback converted them into
layout characters and ate them: a cursor move is a composition boundary, the
boundary drain commits the buffered word once and the key passes through)
triggers one `hunmin_ic_flush()`, one edit-session insert of the whole word,
and pass-through of the boundary key. No libhunmin change was needed:
`hunmin_ic_commit()` is a destructive read that drains every finalised
syllable so far, and `hunmin_ic_flush()` finalises the in-flight syllable
first. Verified end to end in Notepad (multi-syllable word accumulates in the
overlay, document receives the word once at the boundary). The registration
and key-down chain this rides on is documented in
`docs/WINDOWS-TSF-REGISTRATION.md`.

Since issue #80 the contract also has an automated check: the mock-TSF harness
(`frontends/tsf/test/`, ctest label `tier-F1`, docs/TESTING.md "Tier F") drives
the same adapter over in-memory COM doubles and asserts, per boundary event,
exactly one edit-session insert carrying the whole buffered word — regressed
if someone drops the `wordbuf_` append or inserts per syllable. The overlay
window itself is out of its scope (`ITfContextView::GetTextExt` returns
failure, so the adapter takes the no-metrics hide path).

## Open questions

- Whether UWP and console edit controls need the overlay for readable preedit.
- ~~Capability probing and candidate window behavior for TSF applications.~~
  The candidate window is implemented (issue #67): the F9 popup over the whole
  buffered word, its contract and what stays manual/device-bound live in
  `docs/WINDOWS-TSF-CANDIDATES.md`. Host-dependent caret-rect probing itself
  remains open here.
- Exact caret rect source for overlay movement in different applications.

## Key routing notes (issue #67 field test, 2026-10-18)

- **CTRL/ALT chords in hangul mode** (user note, verbatim): "when ctrl or alt
  is pressed in korean mode, it should be treated as english mode - may need
  buffer flush - and produce output accordingly." Shipped as: a CTRL/ALT
  chord does not compose — the pending preedit commits (the boundary-flush
  answer), the candidate window closes, and the chord passes to the app
  uneaten (Ctrl+A selects all, not types). The shipped form keeps the MODE
  itself on the hangul side after the chord — **ruled 2026-10-18, issue #101**:
  "will not switch mode; transient processing is enough." The note's stronger
  reading (the chord SWITCHES the mode persistently to english) is closed, the
  record is `wiki/decisions/2026-10-18-tsf-chord-transient-not-persistent.md`,
  and no code change was needed: the ruling matches what shipped. Measured
  defect that preceded this: the letter inside the chord fed the composition.
  Not pinned behaviourally yet: the tier-F1 replay carries no ctrl/alt chord
  case because `docs/trace-spec.md` defines only the Shift mods bit (0x01)
  today — the source greps behind board rows TSF-CHORD-67 and
  TSF-MODEPERSIST-67 are the current guard.
- **Hanja fallback to the last syllable** (user note, verbatim): "when hanja
  is pressed, and you did not found the matching idiom, you should produce
  candidate items with last syllable." Shipped 2026-10-18 (issue #100): when
  the Hanja key's whole-word lookup yields 0 while the buffered word has
  more than one syllable, the adapter retries the lookup with the LAST
  syllable (tsf.cpp `LastSyllableOf`, a UTF-8 character walk) and marks the
  list as a syllable-fallback list (`candFallback_`,
  `candSource_` = the syllable); the commit replaces ONLY that syllable —
  the rest of the word stays Hangul — and the buffer drains once exactly
  like the ordinary commit. One-syllable and empty buffers keep the
  zero-candidate pass-through. Both key phases mirror the decision
  (tsf.cpp `HanjaListCount`). Measured anchors against the shipped tables:
  가가가 0 whole-word → 가 65; 가디 and 디 0 in both lookups; tier-F1
  tsf-candidate-test cases 8/16/17/18 carry the contract.
