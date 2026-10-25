---
title: "Activate/Deactivate blocks extracted; the langbar style ruled (S5 first cut)"
created: 2026-10-22
updated: 2026-10-25
type: session
status: superseded
superseded_by: [[the-tsf-extraction-round-2026-10-22]]
tags: [session, windows, tsf, refactor, langbar, issue-127, issue-103]
sources:
  - ../../docs/REFACTOR-TSF-COMPONENTS.md
confidence: high
---

# Activate/Deactivate blocks extracted; the langbar style ruled (S5 first cut)

**Superseded 2026-10-25 by [[the-tsf-extraction-round-2026-10-22]]**, which
carries this stage in the round's stage table. The page stays because the
record that the stage ran — and what it measured at the time — is evidence.

What this session established, in one line: the S5 first cut moved the `Activate`/`Deactivate` bodies out of the shell, and it is where the langbar detach defect was found: `UnadviseSinks()` nulls `threadMgr_` before the detach runs, so the item keeps its sink past deactivation — recorded for a ruling, not fixed.

## Related

- [[the-tsf-extraction-round-2026-10-22]] — the round this stage belongs to.
- [[2026-10-22-tsf-langbar-tearoff]] — the neighbouring stage (itself superseded).
