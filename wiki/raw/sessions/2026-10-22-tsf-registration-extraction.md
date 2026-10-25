---
title: "DllRegisterServer/DllUnregisterServer blocks extracted (S5, registration)"
created: 2026-10-22
updated: 2026-10-25
type: session
status: superseded
superseded_by: [[the-tsf-extraction-round-2026-10-22]]
tags: [session, windows, tsf, refactor, registration, issue-127, issue-84]
sources:
  - ../../docs/REFACTOR-TSF-COMPONENTS.md
confidence: high
---

# DllRegisterServer/DllUnregisterServer blocks extracted (S5, registration)

**Superseded 2026-10-25 by [[the-tsf-extraction-round-2026-10-22]]**, which
carries this stage in the round's stage table. The page stays because the
record that the stage ran — and what it measured at the time — is evidence.

What this session established, in one line: registration and unregistration moved to their own TU with file-static helpers called in their original order — the split that let the unregister mirror be written as a mirror. The registry state it produces is measured in `docs/WINDOWS-TSF-REGISTRATION.md`.

## Related

- [[the-tsf-extraction-round-2026-10-22]] — the round this stage belongs to.
- [[2026-10-22-tsf-settings-resolver]] — the neighbouring stage (itself superseded).
