# TSF: the adapter's own tray badge — mode indicator + right-click settings (issue #103)

**The ruled surface: an icon WE add ourselves.** The system lanes are measured
closed on Windows 11 25H2 (26200) — the langbar item's `AddItem` is refused
(0x80004005, environmental), the `ITfSystemLangBarItemSink` advice settles but
the shell tray menu never renders it, and the shell tray badge ignores a
third-party TIP's conversion compartment. All of that is record in
`tsf-langbar-badge.md` ("MEASURED RESULT" section). Issue #103's caret-side
overlay design depends on a window issue #98's ruling removed; the maintainer
ruled the tray surface: **tray badge icon — our own `Shell_NotifyIconW`, 한/A
icon from resources 2/3, right-click menu "Hunmin 설정 열기" → the WinUI 3
settings exe, left-click toggles the mode.**

## What carries what

- `frontends/tsf/src/trayicon.cpp` — included by `src/tsf.cpp` (the
  `keybinding.cpp` pattern, one TU); the owner window, the icon swap, the
  visibility gate, the menu. A console test process must not create a real
  tray icon: the tier-F1 mock mains define `HUNMIN_TSF_TRAY_DISABLED` before
  including `tsf.cpp`, and the module compiles no-op twins under that define.
- `src/tsf.cpp` — `Activate` registers the callbacks
  (`hunmin_tsf_tray_set_callbacks`) before the first icon update and clears
  the pointer in `Deactivate` (`hunmin_tsf_tray_hide`). The mode switch runs
  ONE core (`ToggleModeCore`) for both drivers — the toggle key (focused
  context) and the badge's left-click (no context: a live composition ends
  in its own context, `DetachComposition`; a buffered word is kept for the
  next real boundary) — then `UpdateTrayIcon()`.

## Visibility rule (one icon per process, foreground only)

A TIP is loaded into every text-input process and nothing deactivates a
background process's TIP, so an ungated `NIM_ADD` would pile up one badge
per host that once activated Hunmin. The gate: `WM_ACTIVATEAPP` on the owner
window plus a foreground-process check at creation — this process
foreground ⇒ the badge is added; the foreground moves away ⇒ removed;
`Deactivate` ⇒ removed unconditionally.

## Verification (elevated steps: the maintainer's UAC)

0. **Build and deploy the current Release build** by the file-swap recipe
   (`windows-tsf-ime` skill step 5): rename-and-replace the DLL at the
   registered path, re-register the SAME path, restart
   ctfmon/TextInputHost/ApplicationFrameHost, read back the InprocServer32
   default and the script's marker. Deploy dir must also carry
   `settings-ui/publish/hunmin-tsf-settingsui.exe` for the menu launch.
1. **Background process shows nothing.** After the restart (Hunmin not yet
   selected by any app), the tray must carry NO Hunmin badge at all — the
   foreground gate is the pile-up guard.
2. **Select Hunmin in a text host (WordPad or Notepad), the badge appears**
   beside the clock showing the red 한 (resource 2): hangul mode is the
   activation default. Tooltip = "Hunmin".
3. **The badge flips with the mode.** Press the Han/Eng key: 한 → dark A
   (resource 3), without Win+Space, within one keystroke. Another toggle
   → back to 한.
4. **Left-click toggles the mode** (near the clock, the toggle's own test:
   step 3's behaviour through the mouse instead of the key).
5. **Right-click the badge → "Hunmin 설정 열기" launches the settings
   surface** — the WinUI 3 exe when deployed (layout lists + 한/한자 키
   capture rows), the rundll32 interim dialog otherwise; the trace line
   `launch settings exe=…` in `hunmin-tsf-debug.log` names which.
6. **The foreground gate.** Move focus to another app: the badge vanishes;
   focused back: reappears with the CURRENT mode's icon (it did not reset).
7. **A mode switch by key while the caret holds preedit text** then
   left-click on the badge: the in-flight syllable commits through the same
   boundary path the toggle key uses (composition ends in its own context;
   underline clears). No preedit ghost remains after the click.
8. **Deactivate leaves nothing stale:** switch the profile to the MS IME
   (Win+Space) — the badge is gone for good until Hunmin is re-selected.
9. `ctest --test-dir frontends/tsf/build-msvc -C Release -L tier-F1` green —
   the mock mains never created an icon; `tools/docs-lint.py --build-dir
   build` 0 errors.

## Signals in the debug log (HUNMIN_TSF_DEBUG_LOG=1)

- `ApplyConfigStore pid=…` / `toggle key vk=… -> hangul|english` — the
  boundaries whose tail also ran `UpdateTrayIcon()`.
- No tray lines: the badge module logs nothing by design AND the mock seam
  compiles it out; a MISSING badge in step 2 above is a foreground-gate
  question, not our icon load — check WM_ACTIVATEAPP ran (switch focus away
  and back once before diagnosing).

## What this surface deliberately does NOT do

- It does not touch the shell's language list, the compartment write
  (stays: correct TSF, costs nothing), or the langbar item code (dormant,
  re-measured with the checklist `tsf-langbar-badge.md` each milestone —
  the "not lost, just blocked" lane of issue #103's option C).
- Left-click toggling is adapter-side only; it is NOT the configured
  toggle-binding list read (those live in `ApplyConfigStore`). The badge
  toggles through the same core, not through a button of the store:
  the store's bindings are keyboard semantics, the badge is a mouse
  affordance. Both land on `ToggleModeCore`.
