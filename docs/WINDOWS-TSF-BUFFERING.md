# Windows TSF buffering model for Hunmin

**Status: design record.** What must hold is `docs/SPEC-TSF.md` (TSF1, TSF4) and
`docs/SPEC-FRONTEND.md` (FE4, FE5); why the buffer lives in the library rather
than in this adapter is the comment above `hunmin_buffered_preedit_new`
(`libhunmin/src/buffered_preedit.c`). This page keeps the design that got the
frontend there, and the two corrections measured along the way.

## Purpose

Avoid leftover and duplicated preedit text. The frontend treats TSF as a
commit-only surface: finalized syllables belong to the document, and what is
still being composed stays the engine's until a boundary. The overlay that the
first design used for preedit is retired (`docs/WINDOWS-TSF-API-NOTES.md` records
why inline composition replaced it).

## The rule that survived, and the two that did not

- **Insert exactly once, at a boundary.** A key the engine consumes inserts
  nothing; a declined key, Enter, an edit key, a toggle or a chord drains the
  buffered word once. This is now `SPEC-FRONTEND` FE4 and is asserted by the
  win32 corpus.
- **Rule 3, "never keep a TSF composition that owns the same text you later
  insert", was superseded 2026-10-20 (issue #98).** The adapter now opens an
  inline composition for preedit; the rule's intent survives — the same text is
  never delivered twice — because `EndComposition` fuses the range into the
  document at the first boundary.
- **The insert path is flattened commit text through a synchronous edit session**
  (`RequestEditSession`, `TF_ES_SYNC | TF_ES_READWRITE`, `ITfRange::SetText`).
  The draft's `ITfInsertAtSelection` wording was never implemented; measured
  2026-10-17 and corrected here.

## Measured dates

- 2026-10-17 — the adapter state confirmed end to end in Notepad.
- 2026-10-20 — inline composition for preedit ruled in (issue #98); the overlay
  kept only the candidate popup.

## Measured state, and what moved since

Measured 2026-10-17 (issue #73): finalised syllables accumulated in the ADAPTER
(`wordbuf_`), the overlay showed the word plus the in-flight preedit, and the
first boundary key produced exactly one insert of the whole word. Issue #80 gave
that contract an automated check (mock-TSF, tier F1: one insert per boundary).

**The adapter-side buffer is gone** (issue #128, 2026-10-25): the word lives in
`hunmin_buffered_preedit`, and the adapter hands every key to the layer. What the
2026-10-17 measurement asserted is now `SPEC-FRONTEND` FE4 with the same
assertion channel — the win32 corpus — and the reason the buffer moved is the
comment above `hunmin_buffered_preedit_new`.

## Open questions

- Whether UWP and console edit controls need an overlay for readable preedit.
- The caret-rect source for any overlay movement on such hosts.

## Key routing notes (field test 2026-10-18, issue #67)

The chord and edit-key rules measured here are requirements now: a CTRL/ALT chord
flushes and passes (`docs/SPEC-TSF.md#§TSF7`), and the edit/navigation cluster is
a key of passage (`docs/SPEC-TSF.md#§TSF8`). The verbatim user note those came
from is quoted in `docs/SPEC-TSF.md`.
