---
title: "Deactivate stops leaking the event-sink advice and the lang-bar item (issue #145)"
created: 2026-10-27
updated: 2026-10-28
type: session
status: current
tags: [session, windows, tsf, deactivate, advice, langbar, issue-145, issue-127]
confidence: high
---

# Deactivate stops leaking the event-sink advice and the lang-bar item (issue #145)

One line: the two `Deactivate` defects the #107 mock harness measured as
*today's behaviour* are fixed — `UnadviseSinks()` now withdraws both advice
registrations, and the language-bar detach runs while the thread manager is
still alive — so the two tier-F1 cases that encoded the defects flipped from
"assert the leak" to "assert the pairing".

## The two defects, side by side

| Defect | Cause | Fix | Regression |
|---|---|---|---|
| event-sink advice outlives the service | `UnadviseSinks()` only QI'd `ITfKeystrokeMgr`; `sinkCookie_` never reached `ITfSource::UnadviseSink` | the cookie is unadvised (only when non-zero), then reset | `TsfSessionTest.DeactivateUnadvisesTheThreadManagerEventSink` |
| `RemoveItem` never runs | `Deactivate` called `langbar_->Detach(threadMgr_)` *after* `UnadviseSinks()` had nulled `threadMgr_`, and `Detach` returns early on a null manager | the advice withdrawal and the reference drop are now separate steps (`UnadviseSinks()` / `ReleaseThreadMgr()`), so the detach sits between them | `TsfSessionTest.DeactivateRemovesTheLangBarItem` |

## What the shape change buys, and what it costs

The gate was one call doing two jobs: it withdrew the advice *and* dropped the
manager reference. Splitting it is what makes the teardown order expressible at
all — advice off first (no new event reaches a session that is going away),
manager released last (the lang-bar item still has to be removed through it).
Two refusal paths are pinned so the new withdrawal cannot over-reach:
`DeactivateWithARefusedEventSinkAdviceUnadvisesNothing` (a host whose
`AdviseSink` failed left no cookie, so `UnadviseSink(0)` — `E_INVALIDARG`
against the real manager — is never issued) and the pre-existing
`DeactivateWithARefusedAddCallsNoRemoveItem` (nothing added, nothing removed).

## Measurements

- Tier F1: 4/4 entries, ~3 s, `tsf-test` 1.7 s. The `Deactivate*` filter runs
  five cases, all green.
- Mutation check, measured rather than argued: restoring the pre-fix
  `src/tsf.cpp`/`tsf.h` (keeping the new tests) turns both new cases red —
  `eventSinkUnadvises` empty, `langBarMgr.removed` empty — and green again on
  restore. A flipped expectation that passes against the old code would have
  proven nothing.

## Live round on a real image (2026-10-28)

The doubles are the tier; a real kernel is the field. Run through the repo's
own live lane — `tools/tsf-sandbox.ps1`, a disposable Windows Sandbox, the TIP
registered in-guest by `regsvr32`, `frontends/tsf/test/ime-probe.c` as the host
app, `HUNMIN_TSF_LOG_DIR` mapped back — twice, same case, only the sources
different:

| Line | pre-fix (`f7e66d0`) | fixed |
|---|---|---|
| event-sink advice | *(no unadvise at all)* | `Unadvise threadmgr event sink cookie=67108866 hr=0x0` |
| detach | `langbar detach attached=1 tm=0000000000000000` | `langbar detach attached=1 tm=000002622D3C60E0` |
| `RemoveItem` | never reached | `langbar RemoveItem hr=0x0` |
| committed text | `text=안녕 한ㄱ이 pass=1` | identical |

Two premises this corrected: `AddItem` answers `0x0` on a clean image (the
`0x80004005` is this dev host's registration, not the item's shape), and the
host accepts the unadvise — a leaked advice is silent, not fatal. The
user-visible hypothesis in #145 (a duplicate tray item after a reload) stays
unmeasured, and so does cookie accumulation across repeated cycles inside one
process; each round is one process.

Getting there cost a host repair worth recording: the sandbox refused to start
for a day — the guest booted and then reset itself (`Hyper-V-Worker` 18601 then
18515, compute teardown `0xC0370110`) through both the `.wsb` shell and the
`wsb` CLI, while WSL2 booted fine on the same HCS stack. `dism /restorehealth`,
`sfc`, the feature state and the container services were all healthy; a plain
reboot did not help; `dism /disable-feature` + reboot (which re-staged the
component) did. The persisted guest delta carried no hunmin traces, so an
unclean in-guest uninstall was excluded as the cause.


## Related

- [[2026-10-26-tsf-mock-unmodelled-surfaces]] — where both defects were
  measured and the tier-F1 double that records them was added.
- [[2026-10-22-tsf-activate-deactivate-extraction]] — where the detach
  ordering was first recorded, for a ruling rather than fixed.
- [[the-rules-keep-breaking]] — the class of thing a case that asserts the
  defect is: an expectation that must be flipped deliberately, not a guard.
