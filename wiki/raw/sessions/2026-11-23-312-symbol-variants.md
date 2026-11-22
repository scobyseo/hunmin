---
title: "312: Tier 2 became gesture variants — provisional data, comparable designs, no verdict"
created: 2026-11-23
updated: 2026-11-23
ingested: 2026-11-23
type: session
status: active
tags: [android, layouts, tests, measurement]
sources:
  - ../../../docs/DESIGN-ANDROID-SYMBOL-VARIANTS.md
  - ../../../docs/SPEC-ANDROID-KEYPAD.md
  - ../../../docs/ANDROID-SYMBOL-INTERACTION.md
  - https://github.com/scobyseo/hunmin/issues/312
confidence: high
---

# 312: Tier 2 became gesture variants — provisional data, comparable designs, no verdict

Issue #312 extends the symbol screen ([[2026-11-22-symbol-screen-app-read-data-file]],
KP11) so the Korean special symbols are reached by swipe/long-press on Tier 1
keys instead of a Tier 2 page (KP10). The data home was already settled; this
session owed the variant mapping, the gesture set bounded by KP8, and a
checkable data contract — with the choice left to the maintainer.

## What was measured

- The kowiki frequency reading behind the Tier 2 candidate set was **re-run**
  (`tools/symbol-frequency.py`, same dump `de25cba4…`, 3000 pages) and
  reproduced the recorded counts exactly. Two findings the mapping uses:
  《》 outranks 〈〉 ~7.6× (the one real shape-vs-frequency fork), and ※/₩ are
  *present but tiny* in encyclopedia prose — their placement is justified by
  the use case, not the register, and the caveat is stated in the design doc.
- Baseline check before touching anything: the inner loop had one red,
  `engine-d2-ibus-ui`, which passed on rerun — a parallel-load flake, not a
  tree fact. Recorded so the next session does not chase it.
- **Cost of the added tests** (`tools/ci-time-budget.sh`, 2026-11-23): the
  checker with its selfcheck runs in 26 ms; tier E1 went 10.2 s → 11.8 s
  (+3 unit tests). The budget script reports OVER locally on **both** trees
  (this worktree inner 90.2 s / slow 156.8 s; the master checkout inner
  69.4 s / slow 141.4 s, same environment) — the local/CI delta the script
  prints (ANDROID_HOME registers the android lanes, D3 runs locally and skips
  on the runner), plus device/gradle variance between runs. #312's own spend
  is the ~1.6 s E1 delta; the overage is pre-existing, not this change's.
- **`engine-android-e2e` is environment-bound red here, on master too.**
  With a physical phone *and* the emulator attached, the script's unscoped
  adb needs `ANDROID_SERIAL=emulator-5554`; even then it fails at varying
  late points (settings-window focus, a dead test-editor process), and the
  master checkout fails the same lane the same way. The typing, mic and
  settings rounds before the failure all pass with the variants APK, so the
  lane's red is not evidence against #312 — but it is not a green either.
  Recorded, not fixed: the emulator was not this session's to restart.

## What was built (provisional, not ruled)

- `docs/DESIGN-ANDROID-SYMBOL-VARIANTS.md` — mapping rules (shape family vs
  frequency-across-keys vs physical position, the last degenerate for Tier 2),
  the within-family ordering fork, four gesture sets compared against KP8,
  and a recommendation with what-would-change-it. No verdict recorded.
- The app-read `symbols.json` gained a `variants` section: seven hosts, the
  issue's twelve characters, swipe-up = the family's most frequent member,
  long-press popup = the family in the same order. Marked provisional in the
  file's own `source`/`x-notes`.
- `tools/symbol-data-check.py` validates **structure and scope, not the
  assignment** — pinning the unruled assignment would write a verdict into the
  contract. A built-in negative selfcheck (12 fixtures) runs on every
  invocation, so no new test registration was spent.
- `SymbolScreen.kt` parses and exposes the variants (the checker-accepts /
  app-ignores divergence is the recorded failure mode from
  [[2026-11-22-t3-2-symbol-data-and-issue-alignment]]'s lane); three new E1
  tests pin parse, non-committability of variant characters, and loud failure
  on corruption. Gesture UI wiring is deliberately absent — that is the open
  part, and KP5's defences are requirements on it.

## What remains open

The maintainer's ruling on the mapping, the ordering fork (《 vs 〈 on the
up slot), the gesture set (G2 recommended) and the popup cap; the gesture UI;
a chat-register measurement for ※/₩; the inventory beyond the twelve.