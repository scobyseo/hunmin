---
title: "IBus candidate gloss: option A (auxiliary text) ruled, implemented, evaluated"
created: 2026-11-22
updated: 2026-11-22
type: decision
status: active
tags: [ibus, candidates, presentation, decision, fe16]
sources:
  - ../../../docs/SPEC-FRONTEND.md
  - ../../../frontends/ibus/src/hunmin_engine.c
  - ../../../frontends/ibus/test/test_engine_ui.cpp
  - ../../../tools/ibus-gloss-probe.sh
  - https://github.com/scobyseo/hunmin/issues/319
confidence: high
ingested: 2026-11-22
---

# IBus candidate gloss: option A ruled

## The ruling (maintainer, 2026-11-22)

> "A로 구현해서 평가해 보자. 못 쓰겠으면 B로 바꾸자."

Option A = the highlighted candidate's gloss as IBus auxiliary text, tracking
the cursor. Option B (gloss embedded in the candidate text) is the named
fallback if A proves unusable. The requirement itself is FE16
(`docs/SPEC-FRONTEND.md`): "ibus로 전환하기 위해서는 그 기능을 지원할 방법을
찾아야 해". The usability verdict stays with the maintainer; this page
records what was implemented and measured.

## What was implemented

`frontends/ibus/src/hunmin_engine.c#sink_candidates`: on open and on every
cursor change (the sink already fires per `candidates_notify`), publish
`hunmin_state_candidate_detail(state, cursor)` via
`ibus_engine_update_auxiliary_text`; hide it when the detail is empty or the
list closes. The candidate TEXT stays bare Hanja — the shared expectations
do not move. No ABI change (frontend-only).

## What was measured (issue #319)

- **D2** (`engine-d2-ibus-ui`,
  `CandidateGlossRidesTheAuxiliaryTextAndTracksTheCursor`): the
  UpdateAuxiliaryText signal equals the state's own detail accessor at
  cursor 0, tracks to cursor 1 on Down, clears on Escape and on commit; the
  candidate text is asserted unchanged. Green.
- **Real daemon bus** (`tools/ibus-gloss-probe.sh`, panel ENABLED, unlike the
  D3 lanes): `UpdateAuxiliaryText` flows engine → daemon →
  `org.freedesktop.IBus.Panel`, carrying the actual gloss strings —
  candidate 0: "회의 등에서 어떤 안건에 대해 표결을 할 때 찬성한다는 표시.",
  candidate 1: "옳을 가 | may, can, -able; possibly". The GTK3 panel process
  runs and its window is drawn (xwininfo geometry). Down+Return commits
  candidate 1 correctly (both first candidates of 가 are 可 — the krdict
  sense and the hanja — distinguished by their glosses).
- **D3 regression**: `engine-d3-ibus` green (45.5 s).
- Screenshot artefacts: `build/gloss-probe*.png` in the task worktree — the
  maintainer's visual judgment is the usability verdict; the agent cannot
  view images in this session, so the drawing claim rests on the signal
  chain + panel window geometry, not on pixel reading.

## Probe findings recorded so they are not re-chased

- The daemon's DEFAULT panel/config lookup fails on this host ("Can not
  execute default panel program"); name `-p`/`-c` explicitly.
- The memconf panel ABORTS in `panel_switch_engine` on an empty engine list;
  seed `engines-order`/`preload-engines` via gsettings under a staged HOME
  (dconf backend), keeping the real user config untouched.
- IBus runs a PRIVATE dbus server; session-bus monitoring sees nothing.
  Monitor the address from `$XDG_CONFIG_HOME/ibus/bus/*`.
- Bare Xvfb + model-less setxkbmap gave an evdev-style keymap (r=19); the
  1.5.34 immodule's declined-key re-forward then landed on `]`. The probe
  uses `-model pc105`; a stray `]` after the commit can still appear in the
  probe transcript (host immodule/keymap interaction, after the engine's
  correct commit) — the D3 gate lanes are clean, so this is a probe
  artefact, not an engine defect. Real-desktop judgment:
  `tools/ibus-real-use.sh`.

See also [[2026-11-22-ibus-coverage-parity]] (the coverage context) and
[[the-rules-keep-breaking]] (why the earlier "IBus cannot" claim was
corrected rather than defended).
