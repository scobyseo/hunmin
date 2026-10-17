---
title: "Milestone C — layout wiring + data loading (issue #66)"
created: 2026-10-17
updated: 2026-10-17
type: session
status: active
ingested: 2026-10-17
tags: [session, windows, tsf, layouts, data-loading, issue-66, issue-71]
sources:
  - ../../../docs/WINDOWS-TSF-CONFIG.md
  - ../../../docs/REMAINING-WORK.md
confidence: high
---

## What the issue asked

[[2026-10-17-tsf-scaffold-milestone-a-verification]] closed milestone A. Issue
#66 is draft milestone C, "[windows] Milestone C - Layout wiring + data
loading", part of the 0.2 lane; the per-milestone draft lives in the
maintainer's board. The maintainer attached #71 — the TSF minimal
configuration dialog — as its subtask, and directed that the dialog hook into
the IME configuration surface of the Windows Settings app (guidance assembled
from the Jamotong field manual §9.3 and the `ITfFnConfigure` reference;
measured on this host during the dialog work; rulings stay with
[[decisions-belong-to-the-maintainer]]).

## What the tree could not answer before this work

- The adapter hard-coded `sin3-p2` in `Activate()` and both mode-toggle
  branches — no layout came from anything configurable, and the activate log
  even mislabelled the layout as dubeolsik.
- `<module>\data`, the shipped-shape data branch, never held data: the build
  did not stage any, so the adapter always lived on the repo-climb fallback.
- Every win32 conformance trace replayed the hard-coded sin3-p2; a
  layout-wiring regression could not be seen by any test.

## Measured failures that produced the design

- **The additive data chain double-loaded one tree.** After staging
  `libhunmin/data` beside the harness executables, `hunmin_ctx_layout_count()`
  reported 30 for 15 staged layout files: `hunmin_ctx_new`'s search path is
  additive and does not dedupe ids across dirs, and the adapter passed both
  the staged copy and the identical repo tree. Fixed adapter-side by collapsing
  to first-present priority (env → module dir → repo climb); the engine
  contract stays untouched.
- **Tier F1 had been measuring nothing in the documented flow.** The
  standalone `frontends/tsf` configure registered ZERO tests — `add_test` is a
  silent no-op without `enable_testing()`, and a `-DPython3_EXECUTABLE` value
  passed explicitly skips `find_package`, leaving the `if(Python3_FOUND)`
  guard false. ctest printed "No tests were found" while the harness built
  fine. Both fixed; the first datadir-test red run (files=15, count=30) is the
  double-load measurement above.
- **docs-lint check 6 cannot see a loop-variable `add_test`.** The tier-F1
  registrations are written out literally so the assertion rows can be
  verified against the sources mechanically.

## The shape that resulted

Config store (`frontends/tsf/src/config.cpp`): `HKCU\Software\Hunmin\TSF`,
values `HangulLayout`/`LatinLayout`, mirroring the fcitx5 addon config's option
names and defaults (sin3-p2 / us-qwerty) so both frontends read the same two
settings; env overrides are the tier-F1 seam. Slot resolution validates
supported + mode match before accepting any stored id. Data loads from one
tree, first present. Tier F1 gained `tsf-config-test` and `tsf-datadir-test`,
plus a dubeolsik win32 trace (generated from the tier-A
`tests/golden/ref-dubeolsik.jsonl` gate; gold reviewed 400/400 commits equal, 153 UTF-8
bytes both sides — never blind-accepted).

## Open

The #71 dialog itself (ITfFunctionProvider + ITfFnConfigure, the Windows
Settings-app IME configuration button linkage, its own reachability
measurement) lands in its own worktree on top of this branch.

The mode-toggle key moved the same session, on the maintainer's instruction:
the **physical Han/Eng key (VK_HANGEUL) is the default toggle**, additional
keys (Right Alt, Shift+Space, ...) configure through the store, and the shape
is Android's KeyBinding.kt ported to Windows vocabulary (`v1:<vk>:<mods>`,
exact per-family matching, the physical key an unkillable platform alternate).
CapsLock stopped being the toggle and is ignored in Korean input mode — the
maintainer flagged the engine's case-encoding input convention ("it would be
better to receive ascii code and to transmit strings, but some env does not
support it" — checked: `hunmin_ic_process(uint32_t)` is the whole input ABI
and the tier-A goldens/trace corpus carry the ch convention, so a
string/ascii-based input is a core change needing the docs/SPEC-P2.md
justification and a golden regen, NOT a frontend tweak; and the tier-F1 mock
has no keyboard layout for ToUnicode-style synthesis, which is exactly the
env-dependent fragility). Device-bound and still open: does Windows route the
physical Han/Eng key to the TIP as VK_HANGEUL (`tests/manual-checklists/
tsf-mode-toggle.md`), and Han/Eng-as-pseudo-modifier for future chorded
actions.
