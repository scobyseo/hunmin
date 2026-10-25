---
title: "TSF langbar item as a tear-off (stage 4 of the clean-code pass)"
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

# TSF langbar item as a tear-off (stage 4 of the clean-code pass)

**Superseded 2026-10-25 by [[the-tsf-extraction-round-2026-10-22]]**, which
carries this stage in the round's stage table. The page stays because the
record that the stage ran — and what it measured at the time — is evidence.

What this session established, in one line: the language-bar item became a tear-off object with its own TU and reference count. The page records what it does NOT establish: on Win11 25H2 the tray never calls the item's menu or click handlers.

## Related

- [[the-tsf-extraction-round-2026-10-22]] — the round this stage belongs to.
- [[2026-10-22-tsf-activate-deactivate-extraction]] — the neighbouring stage (itself superseded).
