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

TSF should therefore follow `frontends/fcitx5/src/engine.cpp` and this buffering rule:

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
3. Never keep a TSF composition that owns the same text you later insert.
   - The adapter inserts only flattened commit text: via a synchronous edit session
     (`RequestEditSession`, `TF_ES_SYNC | TF_ES_READWRITE`) whose edit-doer sets the
     selection range with `ITfRange::SetText` (`FlushSession`, `DeactSession` in
     `frontends/tsf/src/tsf.cpp`). The draft's `ITfInsertAtSelection` wording was
     never implemented; measured 2026-10-17 and corrected here.
   - It never opens an inline composition for preedit: no `StartComposition` call
     exists, and `ITfCompositionSink::OnCompositionTerminated` is a no-op.
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
key (declined engine key: space, Enter, punctuation, backspace, unmapped)
triggers one `hunmin_ic_flush()`, one edit-session insert of the whole word,
and pass-through of the boundary key. No libhunmin change was needed:
`hunmin_ic_commit()` is a destructive read that drains every finalised
syllable so far, and `hunmin_ic_flush()` finalises the in-flight syllable
first. Verified end to end in Notepad (multi-syllable word accumulates in the
overlay, document receives the word once at the boundary). The registration
and key-down chain this rides on is documented in
`docs/WINDOWS-TSF-REGISTRATION.md`.

## Open questions

- Whether UWP and console edit controls need the overlay for readable preedit.
- Capability probing and candidate window behavior for TSF applications.
- Exact caret rect source for overlay movement in different applications.
