---
title: "Legacy interim input path for classic hosts (issue #119)"
created: 2026-10-20
updated: 2026-10-20
type: session
status: active
ingested: 2026-10-20
tags: [session, windows, tsf, legacy, interim, issue-103, issue-119]
sources:
  - ../../../docs/TSF-INTERIM-INPUT-SPIKE.md
  - ../../../tests/manual-checklists/tsf-installer-fresh-vm.md
confidence: high
---

## What the issue asked

Issue #119: the 0.2 TIP's native composition preedit ([[2026-10-19-tsf-native-composition-preedit]])
measured healthy on modern TSF hosts and broken on default-sink hosts
(classic `EDIT`, msftedit `RICHEDIT50W` — the composition dies per key,
the engine resets, the word never syllabifies). The maintainer ruled
"file issue to implement legacy support"; this session implemented the
ruled support on the interim-input route the spike
([[2026-10-18-milestone-d-candidate-edit-mode-ux]] →
`docs/TSF-INTERIM-INPUT-SPIKE.md`) measured first.

## Shipped (three commits, tier-F1 4/4 + docs-lint 0 errors)

- **Runtime detection, live per service**: the FIRST host-driven
  `OnCompositionTerminated` that fires MID-WORD (compText non-empty over
  empty buffers, not our own `endingSelf_` flip) flips the service to the
  interim path for its life; `HUNMIN_TSF_INTERIM=1` remains the spike
  hatch. No composition exists on the path, so nothing can terminate.
- **A Delivery seam instead of per-mode branches**: `NativeDelivery` rides
  the #98 composition (unchanged, mock-verified); `InterimDelivery` owns
  the span state (`preStart_/preLen_`), the posted-write deferral (thread
  timer + the next key event's entry flush) and the EM protocol. The
  common key handling never asks the mode.
- **Measurements that reshaped the key rules** (all live, both hosts):
  msftedit never calls `OnKeyDown` for an uneaten printable key — the
  boundary drain runs at the TEST phase; the space lands
  (`dkssud ` → `안녕 `); fast typing loses nothing (the first inline
  write's `OnSetFocus` storm ate keystrokes — writes are now posted);
  the drain never moves the application's caret (measured: parked inside
  the word ahead of the space, scrambling typing and backspace) and never
  inserts at an anchor whose span is closed (the duplicate-delivery
  shape); the backspace at a closed word passes to the app.
- **The one-char Hanja ruling** (maintainer, 2026-10-20: "in interim
  mode, only one char conversion should be supported"): anchor = the
  in-flight preedit (`src=녕 count=35` live), one-char candidates only
  (`dkssud~1` → `안寧`), multi-char refused whole.
- Rebasing onto master (> #122) layered the tray-badge work (#103) rather
  than picking a side: the dispatcher's `ApplyModeToggle` calls the
  tray's shared `ToggleModeCore`, so the badge's left-click and the key
  path toggle through the same core. The dead overlay remnants
  (member/init/dtor/no-op hide) were stripped; the candidate popup keeps
  the mechanic per the ruling.

## What was overturned on the way

- The spike's EXP-A space row (`안녕`, 2 chars) recorded the space
  VANISHING unexamined; the implementation rounds attributed it to the
  eaten-space artifact and the shipped predicate changed. The spike's
  dated §3-D holds the correction — never rewrite a measured row.
- The tier-F1 mock stayed green while classic hosts failed (the mock
  never terminates compositions AND its `Key()` always calls OnKeyDown);
  the fixture's 24px capture EDIT rejected `EM_REPLACESEL` growth
  entirely (doclen frozen at 1) — the historic mock failures were the
  fixture's shape, and the capture control now carries ES_AUTOHSCROLL +
  width.

## Pending

- The modern-host no-double-delivery row of the #119 checklist is not
  measured on this machine (a real Notepad/Edge round is device-bound;
  the native path's code shape is unchanged and its mock corpus is
  green) — tracked in its own issue.
- The fresh-vm checklist's sandbox rounds are adjusted for the shipped
  shape when next run.
