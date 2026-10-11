---
title: The Compact HUD restyle, and the three theme traps — 2026-10-12
created: 2026-10-12
updated: 2026-10-12
type: session
status: active
ingested: 2026-10-12
tags: [session, android, ui, theme, verification]
sources:
  - ../../../docs/ANDROID-UI-NOTES.md
  - ../../../frontends/android/app/src/main/res/values/themes.xml
  - ../../../frontends/android/app/src/main/java/org/hunmin/ime/OverlayActivity.kt
confidence: high
---

# The Compact HUD restyle, and the three theme traps — 2026-10-12

Session record. The durable artefact is **`docs/ANDROID-UI-NOTES.md`**; this
page keeps what the conversation held and the tree does not: the order the
defects were found in, and why each wrong fix was plausible.

The task was small — restate the settings dialog as a compact HUD (38dp
rows, dark-slate `CompactTheme`, monospace keycap badges) — and the diff
grew to four theme/layout traps, each invisible to the fix before it:

1. **Weight the badge, starve the title.** First attempt gave the badge
   `layout_weight 1`; every title ellipsized to two glyphs. The badge is
   the thing that may truncate (it has `ellipsize END` and monospace
   metrics); the title column is the one that must breathe.
2. **`setView()` and the missing LayoutParams.** The dialog stayed ~333px
   wide however hard `setLayout()` pushed. `AlertDialog.setView()` adopts
   the custom view's LayoutParams, and a view built in code has none
   until you set them — `WRAP_CONTENT` by default, so the *window*
   measured to the widest row and `setLayout` fought the content.
3. **`android:background` as a theme item.** The "onion" of rounded
   frames looked like a drawable problem. It was not: as a *theme* item
   `android:background` resolves on every inheriting view — parentPanel,
   customPanel, custom, container — each drawing the stroke inset by its
   own padding. One line: `windowBackground`. Trap 2 and trap 3 had the
   same symptom (a starved column) from different mechanisms, which is
   why the first fix alone did not clear it.
4. **The activity theme was itself a Dialog.** With
   `Theme.HunminOverlay` parented to `Dialog.Alert`, the host window drew
   alert chrome *and* nested the `AlertDialog`'s own — a two-layer onion
   no inner fix could reach. Recorded in `docs/ANDROID-UI-NOTES.md` §4
   beside the older settled fact that the manifest theme must be
   AppCompat-derived.

The verification story costs as much as the code. A stale stacked
`OverlayActivity` window made one correct fix look wrong (see
`docs/ANDROID-UI-NOTES.md` §5); wireless adb corrupted `exec-out
screencap` PNGs; the Samsung device's `screencap` default display was not
the one `uiautomator` saw; and `tools/android-e2e.sh` failed on a *correct*
UI because its assertions matched the old combined-string row shape — the
restyle split title and badge into two TextViews. Structure assertions
must change in the same commit as the structure they assert
(`docs/ANDROID-UI-NOTES.md` §7), the same lesson
[[2026-10-11-orca-orchestration-path]] drew for acceptance commands:
the check that survives a refactor is the one naming the structure, and
the one that fails honestly is worth more than the one that passes by
accident. See also [[the-rules-keep-breaking]] — the E2E assertion going
red on a good UI is a sixth instance of state-that-stopped-being-true
with nothing recording the transition, caught here only because the
script prints what it looked for.
