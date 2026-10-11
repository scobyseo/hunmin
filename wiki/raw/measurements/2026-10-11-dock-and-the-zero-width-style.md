---
title: The dock, and the style that measured zero pixels wide
created: 2026-10-11
updated: 2026-10-12
type: measurement
status: superseded
ingested: 2026-10-11
tags: [android, ui, phosphor, measurements]
sources:
  # DockStyle.kt was removed with the whole unreachable DockToolbar
  # (F5, maintainer ruling 2026-10-12, issue #33); citing a deleted file
  # would fail lint, so the citation lives here as a comment and the
  # body below is the record of the measurement as it was made.
  - ../../../frontends/android/app/src/main/java/org/hunmin/ime/LayoutPicker.kt
  - ../../../frontends/android/licences/phosphor-icons/README.md
  - ../measurements/2026-10-11-key-settings-window.md
confidence: high
---

# The dock, and the style that measured zero pixels wide

> **Superseded 2026-10-12 — the dock is removed.** The maintainer ruled
> "F5 -> remove!" (issue #33) and `DockStyle.kt`/`DockToolbar` — the
> unreachable style A — is deleted. The measurement below stands as the
> evidence of how the dock was built and measured while it lived, and of
> the zero-width finding that still governs the flat strip's
> `MATCH_PARENT` fix. Nothing here is a plan to bring it back.

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

## The mic that listened and showed nothing

The maintainer reported it as *"마이크 버튼을 눌러도 변화가 보이지 않네?"* — the
button changes nothing when pressed. It did change: `startVoiceInput()` ran, the
platform recogniser opened the audio stream (`PCM_RECORD` in logcat), and the
state went LISTENING. The screen did not.

`bind()` repainted an `ImageView` that was never added to the view tree. The
builder method `micKey()` returned a fresh view while the same-named **field** was
a separate `ImageView` created at construction, so every state write landed on an
orphan. The chip and the 한/영 segment never had this shape — their fields ARE the
views that get added — which is exactly why 한/영 reacted to a tap and the mic did
not. Fixed by assigning what the builder adds (`.also { micKey = it }`).

### The measurement that was wrong twice

The session lasts about five seconds. `uiautomator dump` of the IME window takes
longer than that, so every dump landed *after* the session ended and reported
`IDLE` — a correct reading of a state that had already ended. A `screencap` in the
same window returns in well under a second and shows the capsule.

So for about a day this defect read as "not a defect": the state log said
LISTENING, the a11y dump said nothing had changed, and the honest conclusion from
those two together is contradictory rather than diagnostic. What settled it was
sampling inside the window (screencap) and then confirming the view identity with
an attachment check.

The pixel gate that came out of it, validated against both states of the same
screen: **(67, 160, 71) = `#43A047`** (Green 600) while listening, **(243, 237, 247)
= `#F3EDF7`** (the dock's own surface) while idle. Tested for *greenness*, not for
an exact colour, so the dark palette's `#66BB6A` passes the same test.

## Tier E4, and why accuracy is not in it

The maintainer approved splitting the voice tier: the script gates our contract,
a person judges intelligibility. The constraint that forces the split is the
platform, not taste — `SpeechRecognizer` is the recogniser, recorded audio cannot
be replayed into it, and a percentage threshold would be a test that fails on a
bad network day. The invariant-5 half (spoken Hangul must not be composed a second
time) was found to be **already gated at E1** in `HunminEngineTest`, so E4 tests
the path around it rather than repeating it.

What E4 does not have yet: a completed end-to-end run. The script needs a focused
editor and asks a person for one by design (the Settings search surface is an AVD
fixture; OEMs ship different widgets), so a non-interactive run refuses rather
than guessing. Its four state and pixel checks have each been measured by hand on
an SM-S938N; the loop as a whole has not been run end to end.

## What this does not settle

Whether style C earns its place now that the dock exists is a question for the
maintainer; the measurement here is only that it rendered nothing and now renders two
rows. See 2026-10-11-key-settings-window for the strip this replaces, and
decisions-belong-to-the-maintainer for why none of this is written down as a ruling.