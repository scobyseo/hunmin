# SPEC-TSF — the Windows TSF frontend's own requirements

What binds TSF **only**. The behaviour every frontend shares — the key rule, the
word buffer, one delivery per boundary, the commit unit, caps, the display
source, the corpus, the thin-adapter budget — is `docs/SPEC-FRONTEND.md` and is
cited here, never restated. If a requirement below looks common, it belongs
there instead.

Mechanisms live in the design documents listed at the end. **Ruling** entries
quote the decision (verbatim where the board note carries the words);
**Derived** entries are extracted from a measured board note and are marked so.

# Requirements

### TSF1. Preedit rides a live composition

Preedit is an `ITfComposition` on the caret's range, rewritten per engine
transition and ended at the first boundary, where the text fuses into the
document as ordinary text. *Ruling:* issue #98, 2026-10-20. *Implementation:*
`frontends/tsf/src/composition.cpp#StartComposition`. *Refuted by:* `tsf-test`.

### TSF2. Preedit is transparent with an underline

The composition range is underlined through the TIP's display-attribute provider
and carries no background of its own. *Ruling:* 2026-10-20, ruled live against
the MS IME. *Implementation:* `frontends/tsf/src/displayattr.cpp#DisplayAttr`.
*Refuted by:* `none` — **partial**: Notepad still paints its own highlight, so
the ruled shape is not reached by the attribute alone; desktop rendering is not
ctest-measurable.

### TSF3. The interim path is detected, not configured

The first host-driven mid-word `OnCompositionTerminated` that is not our own
flips the service to the interim delivery for its life — provided the focused
window is an EDIT-family control (`EDIT`, `RichEdit20W`, `RICHEDIT50W`), the
only kind the interim path's `EM_REPLACESEL` writes reach. Any other host keeps
the native path, and the composition text that host fused into its document is
re-adopted by the next composition, which starts one range earlier so its first
write replaces the copy. *Ruling:* issue #119 — "file issue to implement legacy
support". The class condition is measurement from issue #144 (2026-10-28,
`docs/TSF-INTERIM-INPUT-SPIKE.md#§3-F`; *ruled* 2026-10-29 (maintainer
instruction, issue #144: "merge and close").
*Implementation:* `frontends/tsf/src/composition.cpp#IsClassicEditHost`.
*Refuted by:* `tsf-test`.

### TSF4. Interim text is document text

On the interim path the finished text is the document's and only the in-flight
syllable is rewritten, through one write per settle, posted rather than inline,
with the application's caret saved and restored. *Ruling:* issue #119.
*Implementation:* `frontends/tsf/src/delivery.cpp#InterimDelivery`.
*Refuted by:* `tsf-test`.

### TSF5. Enter is a boundary at the test phase on the interim path

A default-sink host inserts the paragraph break itself and never calls the down
phase, so the boundary must happen in the test phase. *Ruling:* issue #119.
*Implementation:* `frontends/tsf/src/keydispatch.cpp#FeedEngineKey`.
*Refuted by:* `tsf-test`.

### TSF6. The toggle is a configurable binding list

`v1:<vk>:<mods>`, decimal, `;`-separated, matched with exact per-family
modifiers; a malformed entry never swallows a key; the bare physical Han/Eng key
is a platform alternate no rebinding can remove; a toggle is a composition
boundary. *Ruling:* maintainer instruction 2026-10-17, the Android
`KeyBinding.kt` port. *Implementation:*
`frontends/tsf/src/keybinding.h#HUNMIN_TSF_VIRTUAL_HANGEUL`. *Refuted by:*
`tsf-test`.

### TSF7. A chord flushes and passes

A CTRL/ALT chord in hangul mode flushes the pending word, closes the candidate
window, passes to the application uneaten, and leaves the mode unchanged.
*Ruling:* maintainer note 2026-10-18, issue #101 — "transient is enough".
*Implementation:* `frontends/tsf/src/keydispatch.cpp#FeedEngineKey`.
*Refuted by:* `tsf-test`, `engine-key-rule`.

### TSF8. Edit keys are keys of passage

Arrows, Home/End, PageUp/PageDown, Insert and Delete drain the word once and pass
uneaten; they are never translated into layout characters. *Ruling:* issue #95,
measured live 2026-10-18. *Implementation:*
`frontends/tsf/src/keydispatch.cpp#DrainAndPass`. *Refuted by:* `tsf-test`.

### TSF9. The candidate window and its key

The window opens on a configured key (default the bare Hanja key, `v1:25:0`) for
the whole buffered word; while open its keys belong to it; an unparseable
binding falls back to the default so the window can never become unreachable.
*Ruling:* issue #67. *Implementation:*
`frontends/tsf/src/candidates.cpp#hunmin_tsf_hanja_candidates_new`.
*Refuted by:* `tsf-test`, `tsf-mock-tests_conformance_win32_words.trace.in`.

### TSF10. Whole-word miss falls back to the last syllable

When a whole-word lookup finds nothing over a multi-syllable buffer, the lookup
retries with the last syllable, the commit replaces only that syllable, and the
rest stays Hangul. *Ruling:* maintainer note (issue #67), shipped by issue #100.
*Implementation:* `frontends/tsf/src/candidates.cpp#LastSyllableOf`.
*Refuted by:* `tsf-test`.

### TSF11. Interim candidate: in-flight anchor, one character only

On the interim path the anchor is the in-flight syllable alone and a multi-
character candidate is refused whole, leaving the word intact. *Ruling:* issue
#119. *Implementation:* `frontends/tsf/src/delivery.cpp#CandidateCommit`.
*Refuted by:* `tsf-test`.

### TSF12. One settings read order, and unresolved ids degrade

Environment override (empty means "no override") → `HKCU\Software\Hunmin\TSF` →
the cross-frontend default; an id that does not resolve against the loaded data
returns the default with a log line rather than being used. *Ruling:* issues
#66/#71. *Implementation:*
`frontends/tsf/src/config.cpp#hunmin_tsf_config_load`. *Refuted by:* `tsf-test`.

### TSF13. One data directory wins

Environment → `<module>\data` → checkout climb, first present, never an additive
chain; the shipped shape stages `libhunmin/data` beside the DLL. *Ruling:* issue
#66 — the additive chain loaded the same tree twice and reported 30 layouts for
15 files. *Implementation:* `frontends/tsf/src/datadir.cpp`,
`frontends/tsf/CMakeLists.txt#copy_directory`. *Refuted by:* `tsf-test`.

### TSF14. The settings surface is out of process

`ITfFnConfigure::Show` launches a separate executable, waits on the process and
re-applies the store to the live adapter; a missing executable is logged, never
silently replaced by the in-DLL dialog. *Ruling:* issue #71, 2026-10-17.
*Implementation:* `frontends/tsf/src/settingslaunch.cpp#hunmin_tsf_settings_exe_run`.
*Refuted by:* `tsf-test`.

### TSF15. A setting appears on both surfaces, and keys are captured

A setting the product exposes must exist on the in-DLL dialog and the settings
application alike, and a key binding is set by pressing it, never typed as a
store string. *Ruling:* issue #71 — "키 입력 방식"; extended to the commit unit by
#128. *Implementation:* `frontends/tsf/src/settingsdialog.cpp#SetWindowsHookExW`,
`frontends/tsf/settings-ui/MainWindow.xaml.cs#Root_KeyDown`. *Refuted by:*
`none` — judged live (`tests/manual-checklists/tsf-settings-window.md`).

### TSF16. The language bar item is the TIP's own

The indicator is an `ITfLangBarItemButton` whose `guidItem` is
`GUID_LBI_INPUTMODE`, and its menu's settings entry launches the same surface
`ITfFnConfigure` does. *Ruling:* issue #103, 2026-10-20 — "use langbar, not
systemlangbar, for menu and indicator". *Implementation:*
`frontends/tsf/src/langbar.cpp#HunminLbiInputModeGuid`. *Refuted by:*
`tsf-test`. **Open:** on Win11 25H2 the tray never calls `InitMenu`/`OnClick`.

### TSF17. Unregistration mirrors registration

Per-profile entry first, categories through `ITfCategoryMgr`, the
`HKLM\SOFTWARE\Microsoft\CTF\TIP` tree deleted after the COM calls; the HKCU
SortOrder/Assemblies store is never touched because registration never writes it.
*Ruling:* issue #84. *Implementation:*
`frontends/tsf/src/registration.cpp#DllUnregisterServer`. *Refuted by:* `none` —
device-bound (`tests/manual-checklists/tsf-unregister-cleanup.md`).

### TSF18. The subproject registers its own tests

`enable_testing()` in the subproject and a literal `add_test(NAME …)` per entry:
a loop variable and a `Python3_FOUND` guard each register nothing silently.
*Ruling:* measured 2026-10-17, tier F1 was measuring zero tests.
*Implementation:* `frontends/tsf/CMakeLists.txt`. *Refuted by:* `tsf-test`.

## Design documents

- `docs/WINDOWS-TSF-BUFFERING.md` — the insert path and the boundary rules.
- `docs/WINDOWS-TSF-CONFIG.md` — the store, the slot rules and the data search.
- `docs/WINDOWS-TSF-CANDIDATES.md` — the candidate window.
- `docs/WINDOWS-TSF-REGISTRATION.md` — registration and the language bar.
- `docs/WINDOWS-TSF-API-NOTES.md` — the MS Learn surface digest.
- `docs/TSF-INTERIM-INPUT-SPIKE.md` — the dated spike that led to the interim path.
