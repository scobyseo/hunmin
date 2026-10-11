# hunmin — notes for Android UI authors

Hard-won knowledge from building the Android frontend's on-screen UI: the
toolbar dock, the one-line/two-line modes, and the Compact HUD settings
window (`OverlayActivity.kt`). Verified against a Pixel-style emulator
(1080×2400, `emulator-5554`) and a physical Samsung device (2448×1848,
dual-display). Line references are to the files as of the
`task/hud-settings-restyle` work (2026-10-12).

This file is for whoever writes the next screen for this repo. It records
the things that are not in Android's own docs and that cost real time.

---

## 1. The style system

There are two presentation families, and they are deliberately the same
product:

| Family | Where | Palette source |
|---|---|---|
| **Strip** | one-line/two-line toolbar strip | `LayoutPicker.kt` + `@color/hunmin_*` in `res/values/colors.xml` |
| **Compact HUD** | `OverlayActivity.kt` settings/inspector dialogs | `CompactTheme` cache over `@color/hunmin_hud_*` (same two files) |

Since 2026-10-12 (issue #33, ruling "F3 -> implement modes") both families
are day/night pairs over the same two resource files: the HUD's day set is
the M3 light surface family (panel `#F3EDF7`, secondary `#49454F`, accent
`#006494`, confirmed `#106B21`, white keycaps) and the night set is the
dark slate the HUD launched with (`BG_PANEL #1E1E24`, `TEXT_PRIMARY
#F2F2F7`, `KEY_BG #2C2C2E` / `KEY_BORDER #48484A`, `ACCENT #64D2FF`
electric cyan on `ACCENT_BG #152D3D`). The dialog shell
(`res/drawable/dialog_background.xml`) fills with `@color/hunmin_hud_panel`
— see §4 for why that fill is not a free choice.

Row anatomy (settings list): 38dp rows, title left in
`Typeface.DEFAULT_BOLD`, value **badge** right in `Typeface.MONOSPACE,
BOLD` inside an accent-outlined rounded track — the badge is meant to read
like a keycap, so monospace is the style, not a flourish. Titles get
`layout_weight 1` + `ellipsize END`; the badge is `wrap_content`,
`weight 0` + `ellipsize END`. Weighting the badge would compress the
title column to nothing (§3).

The window width is `dialogWidth()` (window size class: 92% of a compact
window capped at 328dp, the 560dp M3 content column on expanded windows —
issue #33, review F10).
A settings list wants the width it asks for; wrap-content lists (§2)
starve it.

Strings stay Korean (`한글 배열`, `창 스타일`, `닫기`, …); they are UI
data, not prose.

## 2. `AlertDialog.setView()` adopts the view's LayoutParams

The container handed to `setView()` must have
`layoutParams = LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT)`
set **on the view before** `setView()`. Otherwise the dialog inflates it
with default `WRAP_CONTENT` width, the window measures to the widest
child, and every long title wraps or ellipsizes no matter what
`setLayout()` asks for. This was the "narrow dialog" bug: ~333px column
on a 1080px screen.

## 3. `android:background` as a *theme item* leaks to every view

Setting `<item name="android:background">…</item>` in a dialog theme
overlay does not background the dialog. `android:background` resolves on
**every view** that inherits the overlay — `parentPanel`, `customPanel`,
`custom`, the container — each drawing the drawable inset by its own
padding: an onion of rounded frames, and the content column starved to a
fraction of the window width however wide `setLayout()` made the window.

**Use `android:windowBackground` for a dialog's panel drawable.** This is
the only correct spelling of "make the dialog look like this card".

## 4. The activity theme must not itself be a Dialog

`OverlayActivity` hosts dialogs, so its own window was themed
`Theme.AppCompat.Dialog.Alert`. The activity window then drew the alert
panel chrome *and* the hosted `AlertDialog` nested a second one inside
it — a two-layer onion that no inner fix could remove. Parent is
`Theme.AppCompat.NoActionBar` (translucent, no dim) now; the visible
panel belongs to the `AlertDialog` alone.

Related and settled earlier: the manifest theme must be **AppCompat-derived**
for `androidx.appcompat.app.AlertDialog` — a platform
`Theme.Translucent.NoTitleBar` surfaces on physical devices as
`Resources$NotFoundException: Resource ID #0x0`. Recorded in the
`themes.xml` header; do not re-litigate it.

Corollary of §3+§4: the shell and the rows must be the *same* dark
`#1E1E24`, or the panel reads as a dark card inside a light shell
(`hunmin_surface` is light `#F3EDF7` and is not the HUD's canvas).

## 5. Stale OverlayActivity windows stack

Rapid gear taps (or a missed `finish()`) leave multiple
`org.hunmin.ime/OverlayActivity` windows stacked — `dumpsys window
windows` shows them, and the screenshot of the top one looks like the
fix "didn't take". Before trusting any UI verification, force-stop the
app and reopen once:

```sh
adb -s <serial> shell am force-stop org.hunmin.ime
adb -s <serial> shell ime set org.hunmin.ime/.HunminInputMethodService
```

Also check `dumpsys input_method` for `mCurMethodId` — it can revert to
the previous IME after an app reinstall, silently sending gear taps
somewhere else.

## 6. On-device visual verification, phone edition

- **Direct `am start` of `OverlayActivity` is not available on
  production builds** — no `adb root` ("cannot run as root in production
  builds"), no `su`. Drive it the way a user does: raise the IME, tap
  the gear in the dock strip.
- **Gear coordinates go stale** when the host app re-lays-out (a
  Settings search page reflows on query change). Locate the strip key
  from `uiautomator dump` bounds every time; do not cache screen
  coordinates.
- **Wireless adb corrupts `exec-out screencap -p`** — the piped PNG
  arrives as `data`, not `PNG image data` (CRLF translation on the TLS
  transport). Use `screencap -p /sdcard/x.png && adb pull`.
- **Samsung dual-display devices**: `screencap` with no `-d` may capture
  the wrong display (it warned and defaulted to a 2448×1848 DeX canvas
  while `uiautomator` saw the portrait one). List displays with
  `dumpsys SurfaceFlinger --display-id` and pass `-d <id>`.
- The emulator remains the reliable tier; `tools/android-e2e.sh` asserts
  the settings window's structure from the UI tree, which is stronger
  evidence than any screenshot.

## 7. E2E assertions track UI *structure*, and structure changes

`tools/android-e2e.sh` matched the settings rows by **combined** strings
(`"한글 배열: 신세벌식 P2 …"`). The Compact HUD restyle split title and
badge into **separate TextViews** — one row, two nodes — so the
substring assertions died while the UI was correct. The script now
asserts the two-node shape with a comment naming §6 of the restyle.
When a restyle changes node structure, update the assertions in the
same commit, or the next agent "fixes" a UI that is not broken.

## 8. Line-count smell

`OverlayActivity.kt` grew past the ~700-line frontend guideline during
the restyle (750 at time of writing). The guideline is a smell, not a
budget — but when a dialog builder is duplicated for the second time
(sections list, key-binding list), the extraction target is the row
builder, not another copy of it.
