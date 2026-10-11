---
title: The dock, and the style that measured zero pixels wide
created: 2026-10-11
updated: 2026-10-11
type: measurement
status: active
ingested: 2026-10-11
tags: [android, ui, phosphor, measurements]
sources:
  - ../../../frontends/android/app/src/main/java/org/hunmin/ime/DockStyle.kt
  - ../../../frontends/android/app/src/main/java/org/hunmin/ime/LayoutPicker.kt
  - ../../../frontends/android/licences/phosphor-icons/README.md
  - ../measurements/2026-10-11-key-settings-window.md
confidence: high
---

# The dock, and the style that measured zero pixels wide

Two records. The first is a design the maintainer specified in full; the second is a
measurement that has nothing to do with it and would not have been found without it.

## The dock

The maintainer specified the toolbar as a floating pill: 48 dp high, rounded 16–24 dp,
floating above the bottom edge, with the status group (layout name, then 한/영) on the
left, the action group (Hanja, voice, settings) on the right, and a spacer between.

Two implementation choices are measurements rather than preferences.

**Views, not Compose.** The repository has no Compose dependency and adding one is not
a rendering decision — it is a build, a startup cost and an APK size, on a frontend that
AGENTS.md caps at roughly 700 lines of event mapping and presentation. The shapes the
specification asks for are six shape drawables and three vector glyphs; the rest is
`LinearLayout`, which is what the other two styles already use.

**Phosphor for the glyphs.** The maintainer's choice, on the grounds that its six
weights are tuned together and that Duotone is a cheap way to show a state inside an
icon. The mic is the one place that pays off: `microphone-duotone.svg` is the line art
plus the capsule silhouette at 0.2 opacity, so the listening state is a colour change
and nothing else. Gear and caret-down are Regular. Three files, converted mechanically
from the 2.1.1 SVGs — the conversion and its three substitutions are recorded in
`frontends/android/licences/phosphor-icons/README.md`, and the MIT terms in
`debian/copyright`. Nothing in the strip's logic depends on a particular glyph.

## Style C measured nothing

While checking the new dock on the phone (SM-F971N, Android 17) the IME window came up
with no content in it: `uiautomator dump` showed the window frame and its two rows and
none of their children, and a raw pixel sample of the IME band was `#FCFCFC` — the
window background, not a toolbar.

The phone had style C saved. `twoRows()` adds its two rows to a **vertical**
`LinearLayout` with `LinearLayout.LayoutParams(0, WRAP, 0f)` — and width 0 with weight 0
is not "take the leftover", it is zero pixels. The helper was written for horizontal rows
(where `0` plus weight 1 is the elastic child) and reused where the axis had turned.

Styles A and B never showed it, which is why it survived the rebuild and the crash fix
that preceded this one: they are horizontal, so the same call is correct there. The
measurement that would have caught it earlier is the one tier E2 already performs —
looking for a known `content-desc` in the IME window — because an empty strip and a
strip whose buttons are off-screen produce the same missing node.

Fix: `MATCH_PARENT` on the two rows, `WRAP` on the mode pill inside them.

## What this does not settle

Whether style C earns its place now that the dock exists is a question for the
maintainer; the measurement here is only that it rendered nothing and now renders two
rows. See [[2026-10-11-key-settings-window]] for the strip this replaces, and
[[decisions-belong-to-the-maintainer]] for why none of this is written down as a ruling.