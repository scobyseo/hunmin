---
title: "Native composition machinery extracted (C)"
created: 2026-10-22
updated: 2026-10-22
type: session
status: active
ingested: 2026-10-22
tags: [session, windows, tsf, refactor, composition, issue-127, issue-98]
sources:
  - ../../../docs/REFACTOR-TSF-COMPONENTS.md
  - ../../../frontends/tsf/src/composition.cpp
  - ../../../frontends/tsf/src/tsf.h
confidence: high
---

## What was directed

The approved component plan's composition stage: the native preedit
machinery, its edit sessions and the boundary/caret helpers are one
responsibility and were the bulk of what remained inline in the shell.

## Shipped

`composition.cpp` (457) now holds, unchanged:

- `OnCompositionTerminated` — the self-end cleanup, the host-driven
  termination detection, the dead-copy capture and the one-way flip to the
  interim delivery (issue #119).
- `CompositionSession` — one edit session that starts the
  `ITfContextComposition` on the caret's empty range, rewrites it, applies
  the display attribute and collapses the caret (issue #98).
- `CompositionWrite` / `CompositionEnd` / `DetachComposition` — the shell's
  scheduling half, including the detach path that flushes the engine into
  the word buffer first.
- `FlushSession`, `InsertBufferedWord`, `FlushEngineIntoWordbuf`,
  `BoundaryFlush` — the plain-insert fallback and the engine/boundary drain.
- `RectSession` / `CaretRect` — the candidate popup's anchor read.

The three session classes stay **nested members of `CTextService`** —
declared in `tsf.h`, defined in this TU — so they keep the enclosing class's
access to the composition state without a new public seam or a
`CompositionHost` abstraction. The delivery seam and `DisplayAttr` are used
exactly as before. `tsf.cpp` 1,384 -> 945.

## Verification

Clean build, no warnings; tier-F1 4/4 — the three win32 traces and the
candidate group already drive composition start, rewrite, attribute writes
and end. Board rows `TSF-BUFFERING-72`, `TSF-NATIVECOMP-98` and
`TSF-INTERIM-FLIP-119` were re-pointed at `composition.cpp`, and their
line references refreshed.

## Next

The key-dispatch TU (test/down, passage, feed, mode toggle) is next; it needs
a decision about how the mock's key seam reaches the moved code. After that
the settings-exe resolver unification (needs a maintainer ruling on candidate
order) and the table-driven QI. See [[2026-10-22-tsf-datadir-extraction]] for
the preceding stage.
