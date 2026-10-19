---
title: "TSF native composition preedit (issue #98)"
created: 2026-10-19
updated: 2026-10-19
type: session
status: active
ingested: 2026-10-19
tags: [session, windows, tsf, composition, preedit, display-attribute, issue-98, issue-106]
sources:
  - ../../../docs/WINDOWS-TSF-BUFFERING.md
  - ../../../tests/manual-checklists/tsf-native-composition-preedit.md
confidence: high
---

## What the issue asked

Issue #98: the overlay window's preedit role is replaced by a NATIVE TSF
composition — inline text at the caret, underlined through a display
attribute — closing the milestone-D thread that
[[2026-10-18-milestone-d-candidate-edit-mode-ux]] left with the overlay
drawing the composing text itself.

## Shipped (one commit, tier-F1 7/7 + docs-lint 0 errors)

- The adapter opens an `ITfContextComposition` on the caret's empty selection
  range at the first preedit key, rewrites the range per engine transition,
  and ends it at the first boundary (space, Enter, punctuation, backspace,
  unmapped, modifier chord, mode/layout switch). The composed text fuses
  into the document as ordinary text at the boundary; the overlay window is
  retired for preedit and keeps only the candidate popup; the non-TSF-app
  fallback is dropped. Engine untouched.
- Underline via `GUID_PROP_ATTRIBUTE` + a registered
  `GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER` whose single attribute is the
  solid-underline, `TF_ATTR_INPUT` shape.
- Rebasing onto master (which had gained #106's `hunmin_ic_process_key`
  migration in the same file) verified BOTH change-sets coexist in the key
  path; the rebase was textually clean and semantically verified by re-run
  tests.

## What the re-verification corrected

- The fallback-commit case (16) still asserted the pre-#98 delivery channel
  (a plain edit-session insert); the candidate commit now fuses through the
  composition END, which the mock records on the composition channel.
  Re-expressed as `AssertCompositionEnded` + `AssertNoInsert` — the same
  one-insert contract, right channel. The pre-rebase "7/7 measured" claim
  did not hold; the commit message says so.
- The display attribute was re-encoded mid-round to the maintainer's ruled
  shape ("ms ime has transparent bg with underline only"): `crBk`
  `TF_CT_COLORREF`/`CLR_NONE`, text and underline `TF_CT_SYSCOLOR`/
  `COLOR_WINDOWTEXT`.

## What is left (the minor issue, noted on the issue)

Notepad still paints its light-blue composition highlight over the preedit
under BOTH encodings measured. Suspected, not yet measured: the highlight
comes from the app's own composition state, not the resolved attribute.
The ruling belongs to the maintainer on the revisit; the measurement belongs
here first — per [[decisions-belong-to-the-maintainer]], and this is the
second instance this week of a green suite describing nothing about desktop
rendering ([[the-rules-keep-breaking]]).
