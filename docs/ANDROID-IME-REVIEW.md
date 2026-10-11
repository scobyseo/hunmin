# Android IME implementation review — design-guideline and SRP analysis

Provenance: this review was produced on the `task/android-design-review` worktree
(commit `7ce9cb3`), by static reading of all 16 Kotlin files under
`frontends/android/app/src/main/java/org/hunmin/ime/` plus
`keymaps/KeyboardLayoutReceiver.kt` (5,035 lines total) and the Gradle/manifest/
resource tree. The external rubric is the Codepath "Android Design Guidelines"
wiki page, whose effective content for an implementation review is its two
official hubs: **Material Design 3** (theming, color, typography, icons) and
**Design for Android** (system bars, accessibility, adaptive layouts), plus the
common-patterns list (gestures, notifications, navigation). Line references are
to the files as of that commit. Contrast ratios below were computed with the
WCAG formula, not estimated. This is a review, not a refactor: it records
findings; it changes no code.

---

## Part 1 — the Codepath/Android design-guideline analysis

The product is a hardware-keyboard IME with no soft keyboard (0.1 lane;
`docs/MILESTONES.md`), so parts of the common mobile-app rubric do not apply
(app bars, navigation drawers, deep links). What does apply to an IME with a
strip, dialogs and a permission host is assessed below.

### What meets the guidance

- **M3 color roles, light + dark, in the dock.** The dock/strip palette
  (`res/values/colors.xml` and `values-night/colors.xml`) is the M3 baseline
  scheme (light `#F3EDF7/#1D1B20/#6750A4`, dark `#211F26/#E6E0E9/#D0BCFF`) as
  named roles (`hunmin_surface`, `hunmin_on_surface`, `hunmin_primary`, …),
  with a `values-night` variant. Measured contrast (WCAG):
  on-surface/surface 14.85:1 light, 12.57:1 dark; on-primary/primary 6.44:1
  light, 7.71:1 dark — all above the 4.5:1 AA bar.
- **Icons.** Phosphor icons (`ph_microphone`, `ph_gear`, `ph_caret_down`)
  rather than mismatched system glyphs; the Hanja key uses the character 漢
  because that is the Korean-keyboard convention, with a TalkBack
  `contentDescription` (한자 입력) — the M3 "icons carry an accessible name"
  guidance.
- **Accessibility of the strip.** 48 dp touch targets on all keys
  (`minimumWidth/Height = TOUCH_DP/dp(48)`, `LayoutPicker.kt:402`,
  `DockStyle.kt:317`); selection (not color) marks the active 한/A half
  (`DockStyle.kt:253`, commented as the thing a screen reader announces);
  per-state content descriptions on the mic (idle/listening/error); the
  `descendantFocusability = FOCUS_BLOCK_DESCENDANTS` trap is documented as
  measured (`DockStyle.kt:22–32`). These are above what the rubric asks.
- **System bars / insets.** An insets listener pads the strip above the
  navigation bar (`LayoutPicker.kt:490–501`), handling gesture and 3-button
  nav.
- **Adaptive behaviour.** No fixed widths; the elastic child is the layout
  name only; gaps scale with screen width (`gapDp()`, `LayoutPicker.kt:467`);
  the settings dialog sizes to 0.92 of screen width. `dialog` windows use
  sp/dp units throughout. There is no window-size-class handling, but the
  strip's scrollbar fallback and weight-based distribution behave correctly
  across widths, which is the applicable part of adaptive guidance for a strip.
- **Permissions (the pattern the platform pushes).** RECORD_AUDIO requested at
  runtime via a dedicated transparent host, declared and explained in the
  manifest; the IME keeps typing when denied. This matches the Android
  permission-pattern guidance well.
- **Typography.** A deliberate type scale: 14sp/15sp body, 12–13sp keycap
  monospace badges, 18sp 漢 key, 20sp dialog titles. sp units everywhere;
  system families (`sans-serif-medium`, monospace) rather than custom fonts.

### Findings (measurements, not verdicts)

Each row states the finding, the code site, and the gap to the rubric. Per
`AGENTS.md` working rules these are findings for the maintainer to weigh, not
recommendations to change code.

| # | Finding | Site | Gap |
|---|---|---|---|
| F1 | Two unshared palettes: the dock uses M3 tokens, but `OverlayActivity`'s `CompactTheme` hardcodes a second palette in Kotlin (`#1E1E24` slate, `#64D2FF` cyan) | `OverlayActivity.kt:64–73` | M3 "one theme, one source of truth"; a second palette is a second thing to keep in sync with the night variant |
| F2 | Five inline `Color.parseColor` literals in the capture dialog are outside both palettes: `#79747E` (tvSub, 3.64:1), `#6750A4` (preview, 2.57:1), `#2E7D32` (confirmed, 3.23:1), `#9E9E9E` (hint, 6.19:1) — computed against the `#1E1E24` panel | `OverlayActivity.kt:489–506`, `:574–578` | M3 color tokens; WCAG AA 4.5:1 for normal-size text. The preview and confirmed colours are the live feedback of the capture UI |
| F3 | `values-night` exists but the Compact HUD / settings dialogs have no dark-mode variants — `CompactTheme` is one fixed dark palette regardless of system theme | `OverlayActivity.kt:64`, `themes.xml` | Design-for-Android dark-theme guidance. Deliberate HUD choice (`docs/ANDROID-UI-NOTES.md` §1), so recorded, not scored |
| F4 | Flat one-line/two-line strip (styles B/C) renders hard black-on-white pills (`0xFF000000`/`0xFFFFFFFF`) with no dark-mode variant and no Material container shape; only the dock path has the M3 theme | `LayoutPicker.kt:359–406` | M3 color + dark mode; the flat styles predate the dock and were never re-themed |
| F4b | The flat strip's mic listening tint `0xFF43A047` over a white pill measures 3.30:1 — the color-only state signal in that style | `LayoutPicker.kt:452–456` | WCAG AA; contrast-as-state |
| F5 | The dock's `buildDock()` is dead code — nothing calls it; `rebuild()` routes to `oneLine()`/`twoRows()`, so the M3-themed dock (DockStyle.kt, 374 lines) is unreachable from production. The shipped strip is the flat style | `LayoutPicker.kt:248–262` vs `:241–242` | Effectively F4 is the *only* shipped strip; the M3-styled one is not reachable |
| F6 | `modePill`/flat buttons have `isFocusable = false`, which is correct for the editor caret, but no `clickable`-gated focusable states for keyboard users; TalkBack can still reach them (measured, DockStyle header) | `LayoutPicker.kt:365–406` | Design-for-Android accessibility: switch/keyboard navigation |
| F7 | No predictive-back opt-in (`android:enableOnBackInvokedCallback` absent from the manifest) | `AndroidManifest.xml` | Codepath common-patterns "Predictive back". minSdk 24 to targetSdk 35: the flag is not required, but targetSdk 35 users on gesture nav get the legacy behavior |
| F8 | Notifications are never used — all transient user feedback goes through Toasts launched via a full `OverlayActivity` start | `HunminInputMethodService.kt:376–384` | Codepath common-patterns "Notifications"; a mode-toggle toast does not need a notification, so this is a "does not apply" entry recorded for completeness |
| F9 | `onEvaluateFullscreenMode() = false` and `onCreateExtractTextView() = null` are correct landscape choices for a hardware-keyboard IME | `HunminInputMethodService.kt:667–671` | No gap; listed as confirmed-correct |
| F10 | `dialog.window.setLayout(widthPixels * 0.92, …)` sizes against the physical metrics rather than window-size classes | `OverlayActivity.kt:288–291`, `:470–473` | Adaptive guidance: fine for phones, untested on foldables/tablets |
| F11 | `KeyboardLayoutReceiver` logs at `.d` with full descriptor lists on every `ACTION_QUERY_KEYBOARD_LAYOUTS` broadcast | `keymaps/KeyboardLayoutReceiver.kt:30–37` | Not a design-guideline item; noted because it is the module's only unstructured logging |
| F12 | The `keymaps` module (`R.xml.keyboard_layouts`) provides physical-keyboard layout descriptors to InputManagerService — a specialised Android mechanism, not part of the rubric | `keymaps/…` | No gap; documented as intended |

**Gap fill record (issue #33, branch `task/android-design-review`, measured 2026-10-12).**
F1, F2, F4, F4b and F7 are filled; each line cites what changed and the
verification that covers it:

- **F1 — filled.** The HUD palette is now `@color/hunmin_hud_*` tokens in
  `values/colors.xml` and `values-night/colors.xml` (same dark values in
  both: the HUD stays always-dark by design, ANDROID-UI-NOTES §4).
  `CompactTheme` is a cache object reading the tokens (`CompactTheme.of()`
  returning an immutable `Colors`), not a second palette;
  `dialog_background.xml` reads `@color/hunmin_hud_panel` instead of a
  literal. Verified: `assembleDebug` green; `CompactTheme` has zero
  `Color.parseColor` left.
- **F2 — filled.** All five inline literals replaced with tokens: preview
  `#6750A4` (2.57:1) → `hunmin_hud_accent` (9.64:1), confirmed `#2E7D32`
  (3.23:1) → `hunmin_hud_confirmed` (7.01:1, still green), sub `#79747E`
  (3.64:1) → `hunmin_hud_secondary` (5.09:1), hint `#9E9E9E` → secondary.
  Every replacement states its measured before/after ratio in a comment.
  Verified: `grep Color.parseColor OverlayActivity.kt` returns nothing;
  contrast computed with the WCAG formula.
- **F4 — filled.** The flat strip now renders M3 roles: the one-line row
  carries the dock's own `toolbar_dock` pill (surface + outline + elevation),
  the mode pill is the filled `toolbar_segment_active` / track pair with
  on-primary/on-surface text (the same selected-semantics as the dock's
  segment, so the e2e `selected="true"` assertion keeps holding), and the
  Hanja/mic/settings keys use the `toolbar_chip` background with
  on-surface text. Night mode flows through the existing tokens — this also
  fixes a real night bug: at night the old hard pair drew black 漢 on the
  dark dock. Verified: `assembleDebug` green.
- **F4b — filled.** The flat mic tint reads `@color/hunmin_mic_active`, so
  the night palette's `#66BB6A` (7.01:1 on the dark dock) replaces the
  single literal `0xFF43A047` (3.30:1 on light, and unchanged at night).
- **F7 — filled.** `android:enableOnBackInvokedCallback="true"` on
  `<application>`. Verified: manifest compiles (`assembleDebug` green).
- **F5 — NOT filled, maintainer-gated.** `DockToolbar` removal is a
  decision (374 lines of unreachable view code), not a gap fill; recorded
  in issue #33.
- **Verification for the whole fill**: `tools/android-unit.sh` → **PASS:
  97/97 tier E1 tests ran and passed** (zero skips, the wrapper asserts
  them); `assembleDebug` green for both ABIs; the emulator e2e
  (`tools/android-e2e.sh`) is environment-bound and recorded as unrun in
  issue #33.

Additional guideline observations without row numbers:

- **Gestures:** no touch gestures exist beyond button taps (hardware-keyboard
  product). The HorizontalScrollView fallback for narrow screens
  (`LayoutPicker.kt:270–295`) is the applicable piece.
- **Wireframing/benchmarking sections of the Codepath page** are process
  guidance for app design, not implementation-checkable; not assessed.

### Summary of Part 1

The dock's visual layer meets M3 and accessibility guidance unusually well for
a self-built IME strip — named roles, night variant, 48 dp targets,
selection-as-state, per-state spoken labels, measured inset handling. The
gaps concentrate in (a) the settings/HUD surfaces, which carry an unshared
second palette with sub-AA inline colours in the capture dialog, and (b) the
flat strip styles, which never received the dock's theme and — per F5 — are
the only strip the code actually reaches. The two presentation families
already documented in `docs/ANDROID-UI-NOTES.md` §1 are therefore three
families in code: M3 dock (dead), flat strip (shipped), Compact HUD (shipped).

## Part 2 — SRP / dependency analysis

### Per-class single-responsibility and dependency table

| Class (file) | LOC | Responsibility (as built) | Depends on (Kotlin types) | SRP verdict |
|---|---|---|---|---|
| `HunminInputMethodService` | 849 | IME lifecycle + key routing + layout selection + voice orchestration + hanja trigger + prefs + settings-bus install + receivers | HunminData, HunminNative, HunminEngine, LayoutCatalog, LayoutPicker, OverlayActivity, KeyBindings/KeyCaptureSession, VoiceSession/PlatformRecognizer/AndroidPermissionGate, ImeSettings, HunminTrace | **Wide.** The documented reason (frontends stay thin; AGENTS.md ~700-line smell) is being exceeded: 849 lines and 11 collaborator types |
| `OverlayActivity` | 750 | Transparent overlay host for: hanja dialog, settings dialog, settings sub-sections, style picker, keybinding list, key capture, toast | ImeSettings, LayoutCatalog, KeyBindings, KeyCaptureSession, CompactTheme (in-file) | **Wide.** Six dialog responsibilities in one activity; one-man-band class |
| `LayoutPicker` | 519 | The IME strip view: two style variants + state rendering + insets + rebuild | ToolbarState, LayoutEntry, DockToolbar (dead), ToolbarStyle, VoiceState | Mixed: view + style-switching + partial dead code |
| `DockToolbar` (DockStyle.kt) | 374 | Dock view builder/binder | ToolbarState, VoiceState, resources | Clean, but **unreachable** (F5) |
| `KeyBinding.kt` (4 types) | 426 | Key binding data + storage + matching + capture session singleton | KeyEvent constants only | Clean; pure logic, JVM-testable, documented |
| `VoiceInput.kt` (7 types) | 460 | Voice path: enum, results, launcher (pure decisions), recogniser, permission gate, session | PermissionRequestActivity (canRaise/raise), SpeechRecognizer | Clean layers with seams for testing; `VoiceLauncher`→`PermissionRequestActivity` is a UI-to-UI dependency justified by the B5 constraint (no channel from a service) |
| `HunminEngine` | 283 | Engine adapter: key→char→process→commit/preedit | HunminKeys, HunminTrace, NativeOps (injected) | Clean; `NativeOps` seam keeps it JVM-testable |
| `HunminKeys` | 220 | KEYCODE→engine-char table + chord mask | none (data only) | Clean; the doc's own "tables are data" idea |
| `HunminNative` | 111 | JNI declarations | none | Clean; pass-through by design |
| `HunminData` | 151 | Asset extraction/stamping | none | Clean |
| `HunminTrace` | 138 | Trace sink | none | Clean |
| `LayoutCatalog`/`LayoutEntry`/`ToolbarState`/`LayoutMode`/`ToolbarStyle`/`ImeSettings` (LayoutCatalog.kt) | 174 | Layout list logic + mode enum + strip-state value + settings bus | none | Fine, but the file holds five responsibilities — a "kitchen-drawer" file |
| `PermissionRequestActivity` | 145 | Permission host | VoiceLauncher.hasPermission | Clean; single-purpose |
| `KeyboardLayoutReceiver` | 101 | Keyboard-layout XML broadcast | none | Clean |
| `TestActivity`/espresso | — | test-only | — | — |

### Dependency graph (mechanical, non-test, non-JNI-receiver)

```
HunminNative ← HunminEngine ← HunminInputMethodService
                                   │ (installed bus: ImeSettings callbacks)
        ┌──────────────────────────┼───────────────────────────┐
   LayoutPicker              OverlayActivity                 VoiceInput family
        │                        │                              (VoiceSession,
   DockToolbar (dead)     KeyBindings, KeyCaptureSession        PlatformRecognizer,
        │                  LayoutCatalog, ImeSettings           AndroidPermissionGate→
   ToolbarState          CompactTheme (in-file)                 PermissionRequestActivity)
HunminKeys ← HunminEngine
HunminTrace ← HunminEngine
HunminData → (filesDir) HunminInputMethodService
```

### Findings

- **S1 — HunminInputMethodService is the widest class (849 lines).** It is
  149 lines over the AGENTS.md frontend smell bar, and its onCreate alone
  wires data extraction, JNI ctx, prefs migration, layout resolution, and the
  settings bus. The key-routing logic in `onKeyDown` (binding loop, capture
  feed, space handling, modifier pass-through, mode routing) is ~80 lines of
  branching that is engine-adjacent logic in a lifecycle class. However: the
  count includes ~280 lines of KDoc-style comments; the logic mass is smaller
  than the file suggests, and every branch has a tier E test. A split into
  e.g. `KeyRouter`/`VoiceOrchestrator` would trade one wide class for one
  more cross-object seam in a process that already carries two singletons
  (`ImeSettings`, `KeyCaptureSession`) for exactly that reason.
- **S2 — OverlayActivity carries six dialog responsibilities** (settings,
  layout section, style section, keybinding list, key capture, hanja, toast).
  The capture logic alone (`showKeyCaptureDialog`, `handleCaptureKey`,
  `storeChord`, `updatePreviewUI`) is a controller living inside an activity.
  Same trade as S1: each extraction adds a seam to reach state that only
  lives in the activity (dialog reference, capturing flags).
- **S3 — two process-wide singletons as the activity↔service bridge**
  (`ImeSettings`, `KeyCaptureSession`). Dependency count is minimal per class,
  but the *global mutable state* is the hidden dependency: every consumer
  depends on "the service installed the callbacks before I read them", an
  ordering property nothing enforces. `KeyCaptureSession`'s own header
  documents why (keys reach the IME window, not the activity). Recorded as
  the measured cost of the IME constraint, not a flaw to fix silently.
- **S4 — dead code**: `buildDock()`/`DockToolbar` unreachable (F5);
  `OverlayActivity.startHanja`/`showToast` helpers, `RESULT_HANJA_SELECTED`,
  `LayoutCatalog.choose` (test-only), `setHanjaConverting` (no caller),
  `HunminEngine.suggestKind` and `IME_FLAG_NO_PERSONALIZED_LEARNING` (no
  caller), `HunminTrace`'s `caseLabel`/`source` set externally only via the
  trace receiver. 374 lines of unreachable view code plus ~9 unused members.
  AGENTS.md forbids drive-by refactors, so these are recorded, not removed.
- **S5 — file cohesion issue, not class issue:** `LayoutCatalog.kt` holds
  `LayoutCatalog`, `LayoutMode`, `LayoutEntry` (adjacent), `ToolbarStyle`,
  `ImeSettings` — five types, three concerns (layout logic, strip style,
  service-bus). The classes themselves are cohesive; the file is not.
- **S6 — the pure-layer discipline is the strong result.** `KeyBinding`,
  `HunminKeys`, `LayoutCatalog`, `VoiceLauncher.decide/decidePermission`,
  `ToolbarState` are framework-free and JVM-testable, which is what makes the
  tier E1 suite possible without Robolectric. `HunminEngine` reaches the
  framework only through two injected seams (`NativeOps`, `InputConnection`
  recording fake). Minimal-dependency question asked of each class: **yes for
  13 of 16 production types; the two wide classes (service, overlay) and the
  two singletons carry the known trade.**

### Minimal-dependency answer (the direct question)

- All leaf/pure classes (`HunminKeys`, `HunminData`, `HunminTrace`,
  `HunminNative`, `KeyBinding`'s pure half, `LayoutCatalog`, `ToolbarState`,
  `LayoutEntry`, `VoiceLauncher` decisions) have **zero** framework or
  cross-class dependencies beyond Android constants — minimal.
- The engine adapter (`HunminEngine`) has two injected dependencies
  (`NativeOps`, `InputConnection`) — minimal, and the injection is what the
  tests use.
- The two activity/service classes are the honest exceptions: they are
  lifecycle hosts that also own routing/dialog logic, and they depend on
  nearly everything. Their width is measured above (S1, S2); whether to split
  is a maintainer decision (an extra singleton seam is the cost).
- The hidden dependencies are the two singletons' install-order (S3), and the
  dead `DockToolbar` (S4/F5) which currently pads the dependency graph
  without being reachable.

## Verification

- Static review only; no code changed, so the applicable gate is
  `python3 tools/docs-lint.py` (new doc introduced) — see the commit on this
  branch. `tools/doctor.py`/ctest unaffected (no data change).
