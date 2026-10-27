---
title: "The mock TSF harness reaches the surfaces it had only stubbed (issue #107)"
created: 2026-10-26
updated: 2026-10-27
type: session
status: current
tags: [session, windows, tsf, mock-harness, registration, issue-107, issue-145, issue-146]
confidence: high
---

# The mock TSF harness reaches the surfaces it had only stubbed (issue #107)

One line: the tier-F1 harness stopped being a key/buffering harness with
`E_NOTIMPL` everywhere else — it now records and asserts the registration
flow, the conversion compartment, the language-bar manager and both advice
registrations, and doing so turned up two adapter defects and two unruled
mirror questions that had been sitting in code comments.

## What made it possible: one seam, wider than the issue asked for

The issue asked for a `CoCreateInstance` hook so `DllRegisterServer` could run
against doubles. Measurement said that hook cannot be the only one:
registration writes `HKCR\CLSID\…` and `HKLM\…\CTF\TIP\…` (elevation only) and
unregistration **deletes both trees**, so an unseamed harness is either
access-denied or, on an elevated host, destructive to the machine's real TIP
registration. The seam (`frontends/tsf/src/registrationseam.h`) therefore
covers COM activation, the registry calls, the Programs folder and the shell
link — with the **platform call as the default table**, restored by
`install(NULL)`. Not `#ifdef`-ed: a seam compiled out of the production build
runs different code than the one the tests prove, which is the trap
[[the-rules-keep-breaking]] is about in another register.

## What is now observed, and what that exposed

| Surface | Was | Now | Finding |
|---|---|---|---|
| Registration COM flow | device-bound checklist | `tsf-registration-test.cpp`, 6 cases | doc drift: three categories, not two; flow list omits the icon pin and the shortcut |
| Conversion compartment | a board row grepping a symbol | `tsf-session-test.cpp`, writes read per boundary | none — the writes were correct, merely unobserved |
| Language-bar manager | `QI` answered `E_NOINTERFACE` | attach / refusal (the measured `0x80004005`) / detach | `RemoveItem` is **never reached** → #145 |
| Key-sink lifecycle | no advice recorded | advise + unadvise per call, tid and sink | event sink is **never unadvised** → #145 |

The two defects are old news to the wiki in one case: the detach ordering was
already recorded on 2026-10-22 in
[[2026-10-22-tsf-activate-deactivate-extraction]] as "recorded for a ruling,
not fixed". It now has a home with a test — #145 — which is the difference
between a note in a session page and a regression that flips when someone
fixes it.

## Measurements worth keeping

- Tier F1: 49 → 64 cases in the one `tsf-test` binary; the 15 new cases cost
  ~140 ms, so the tier stayed at 4/4 entries and ~3 s.
- Mutation guard holds: dropping one `UnregisterCategory` mirror from
  `DllUnregisterServer` turns the mirror case red; restored, green.
- `docs-lint` was red on this host for a reason that was not the tree: pyright
  absent (the pip wheel cannot install here — its own `dist/dist` nesting), so
  `M5-CLOSED` read "absent". With `npm install -g pyright` and the lint run
  under the interpreter CMake picks, it is 0 errors. A red that names the tool
  rather than the claim is worth one measurement before it is believed.
- A mock defect of my own, kept in the comments so it is not "fixed" back: an
  EMBEDDED double must `AddRef` itself in `QueryInterface`. Adding the
  enclosing object's ref made the adapter's `Release()` free a subobject —
  `STATUS_HEAP_CORRUPTION (0xC0000374)` on the first activated case.

## What was left for a ruling, not decided here

- #145 — both `Deactivate` defects (event sink, lang-bar item), with the
  fix-side expectations named.
- #146 — whether the unregister mirror should delete the Start-menu shortcut,
  and whether `RemoveLanguageProfile` is needed (settled only by the elevated
  cycle in the #84 checklist, still owed).

## Related

- [[2026-10-22-tsf-activate-deactivate-extraction]] — where the detach defect
  was first recorded.
- [[2026-10-19-test-framework-migration]] — the one-binary tier-F1 shape these
  cases ride.
- [[the-rules-keep-breaking]] — a green test that measures nothing.
