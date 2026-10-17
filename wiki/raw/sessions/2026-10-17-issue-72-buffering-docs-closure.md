---
title: "Issue #72 doc closure — the buffering rows and the design-doc drift"
created: 2026-10-17
updated: 2026-10-17
type: session
status: active
ingested: 2026-10-17
tags: [session, windows, tsf, docs, issue-72]
sources:
  - ../../../docs/WINDOWS-TSF-BUFFERING.md
  - ../../../docs/REMAINING-WORK.md
confidence: high
---

## What the issue asked and what was already in the tree

Issue #72 ("[tsf] Implement commit-only preedit buffering adapter") names three
deliverables: the design doc, TSF-specific verification, and REMAINING-WORK
updates. Measured at this pass's base (240cb4d): the design doc
`docs/WINDOWS-TSF-BUFFERING.md` exists, the adapter is merged (e88d41e, PR #76)
and reworked by #73 into the self-contained word-buffering form, the 0.2.0
release notes carry the measured Notepad end-to-end run — and the board carried
**zero** rows naming #72. `grep -rn "#72" docs/ wiki/` matched nothing at HEAD.

## The drift was corrected, not recorded silently

`docs/WINDOWS-TSF-BUFFERING.md` (the design draft) said the adapter "inserts
only flattened commit text, via `ITfInsertAtSelection`" and used `EndComposition`
for preedit removal. Neither exists in `frontends/tsf/src/tsf.cpp`: every
document insert is `RequestEditSession` + `sel.range->SetText(ec, 0, t_, -1)`
(`FlushSession` tsf.cpp:860 on a boundary key, `DeactSession` tsf.cpp:329 on
deactivation), and the adapter never opens an inline composition at all — the
`ITfCompositionSink::OnCompositionTerminated` implementation is a no-op
(tsf.cpp:636). The design draft was written before the implementation and named
the mechanism from TSF folklore; the shipped form is equivalent in effect
(flattened commit text lands at the selection, no composition owns that text)
but different in API. The doc now describes the shipped mechanism and says the
draft wording was never implemented [[the-rules-keep-breaking]] — a rule the
docs keep breaking: prose written from intent, checked by nobody, until a later
session reads the code.

## What this pass did

- 4 `TSF-BUFFERING-72` assertion rows added to `docs/REMAINING-WORK.md` §7.5
  (design doc present; destructive `hunmin_ic_commit()` drain into `wordbuf_`;
  insert via the synchronous edit session + `ITfRange::SetText`, with the drift
  note; `StartComposition` absent). `tools/docs-lint.py --build-dir build`:
  0 errors, 8 pre-existing warnings.
- One escaping lesson worth keeping: the assertion grammar must survive a
  round-trip through tools that re-escape backslashes. The first write landed
  4 backslashes where the regex needs 1 after the checker's unescape
  (`\(`), and docs-lint died with "missing ), unterminated subpattern" instead
  of a readable row error.

## What this pass did NOT establish

The ctest suite covers the TSF adapter by nothing: `hunmin-tsf` is a Windows-only
MSVC MODULE outside every tier, and TAT was never measured (still true at this
pass). Those stay open under milestone B (issue #65), per
`docs/release-notes/0.2.0.md` and [[the-outstanding-decisions]]. User-directed
work here: no new issue opened for that; #72 measured, not ruled — closing it
remains the maintainer's call
[[decisions-belong-to-the-maintainer]].
