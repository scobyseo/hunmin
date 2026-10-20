# TSF: the adapter's own tray badge - mode indicator + right-click settings (issue #103)

**The ruled surface: an icon WE add ourselves.** The system lanes are measured
closed on Windows 11 25H2 (26200) - the langbar item's `AddItem` is refused
(0x80004005, environmental), the `ITfSystemLangBarItemSink` advice settles but
the shell tray menu never renders it, and the shell tray badge ignores a
third-party TIP's conversion compartment. All of that is record in
`tsf-langbar-badge.md` ("MEASURED RESULT" section). Issue #103's caret-side
overlay design depends on a window issue #98's ruling removed; the maintainer
ruled the tray surface: **tray badge icon - our own `Shell_NotifyIconW`, han/A
icon from resources 2/3, right-click menu "Hunmin 설정 열기" -> the WinUI 3
settings exe.** A left-click mode toggle shipped in the first rounds and was
REMOVED by the maintainer's steer (round 3, 2026-10-20): a tray-click mode
change is not necessary and cannot reach the focused edit without stealing
its focus - measured rounds 1-3 - the badge is the indicator, the mode flips
with the key only.

## What carries what

- `frontends/tsf/src/trayicon.cpp` - included by `src/tsf.cpp` (the
  `keybinding.cpp` pattern, one TU); the owner window, the icon swap, the
  visibility gate, the menu. A console test process must not create a real
  tray icon: the tier-F1 mock mains define `HUNMIN_TSF_TRAY_DISABLED` before
  including `tsf.cpp`, and the module compiles no-op twins under that define.
- `src/tsf.cpp` - `Activate` registers the settings callback
  (`hunmin_tsf_tray_set_callbacks`) before the first icon update;
  `Deactivate` calls `hunmin_tsf_tray_hide`. The mode switch runs ONE core
  (`ToggleModeCore`) from the toggle-key path; the badge only READS the mode
  (`UpdateTrayIcon()` on every mode boundary).

## Visibility rule (one icon per process, foreground only)

A TIP is loaded into every text-input process and nothing deactivates a
background process's TIP, so an ungated `NIM_ADD` would pile up one badge
per host that once activated Hunmin. The gate: `WM_ACTIVATEAPP` on the owner
window plus a foreground-process check at creation - this process
foreground means the badge is added; the foreground moves away means removed;
`Deactivate` means removed unconditionally.

## MEASURED: the overflow, and where the visibility question stopped

The badge self-promotes for its host executable (Win11's
`HKCU\Control Panel\NotifyIconSettings\<key>\IsPromotedDWORD = 1`, matched by
`ExecutablePath`, then one remove+re-add so the taskbar re-reads the state at
creation). Measured in the installer sandbox rounds (2026-10-20):

- round 2: the write LANDS (`tray promote ok=1` in the trace, the entry
  matched by exe path) and the badge stays hidden in the overflow - the
  taskbar does not re-render an icon on an external registry write;
- round 3: the promote+re-add cycle - still hidden. The maintainer stopped
  the iteration here ("still hidden. but let's stop here").

Standing state: the badge's VISIBILITY is the shell's per-executable tray
setting - the user promotes a host once through the taskbar overflow chevron
("항상 표시") or Settings > 개인 설정 > 작업 표시줄 > 기타 시스템 트레이
아이콘, and the choice persists per exe. The self-promotion write stays as a
best effort (it is the same state that page edits); the measured negative is
recorded so nobody re-runs the registry-write hypothesis without a NEW
mechanism (a taskbar-change notification, or a dedicated icon-owning helper
process - an architecture change, its own decision).

## Verification (sandbox GUI rounds, measured 2026-10-20)

1. **Install**: the 0.2 installer silent round - rc=0, `InprocServer32` ->
   `C:\Program Files\Hunmin IME\hunmin-tsf.dll`, the registration log
   (`hunmin-reg-debug.log`) shows Register / AddLanguageProfile /
   EnableLanguageProfile / ActivateProfile all `hr=0x0`.
2. **The settings launch works from the installed shape** (round 2, live):
   right-click the badge -> "Hunmin 설정 열기" -> the WinUI 3 exe. The
   round-1 failure's root cause was the launcher's path list - both
   resolvers tried only `publish\` shapes while the installed tree stages
   the publish content directly under `<app>\settings-ui`; fixed in both
   `hunmin_tsf_launch_settings` (tsf.cpp) and `settingsExeRun` (settings.cpp).
3. **The badge flips with the mode**: 한 (resource 2) on the hangul side,
   A (resource 3) on the latin, within one keystroke of the Han/Eng key,
   without Win+Space reselection.
4. **The foreground gate**: focus another app - the badge vanishes; focused
   back - reappears with the CURRENT mode's icon; background processes show
   nothing (one badge per foreground process, no pile-up).
5. **Left-click does nothing** (the round-3 steer): no mode change, no focus
   change, no menu.
6. **Deactivate leaves nothing stale**: switch the profile to the MS IME
   (Win+Space) - the badge is gone for good until Hunmin is re-selected.
7. `ctest --test-dir frontends/tsf/build-msvc -C Release -L tier-F1` green -
   the mock mains never created an icon; `tools/docs-lint.py --build-dir
   build-msvc` 0 errors.

## Signals in the debug log (HUNMIN_TSF_DEBUG_LOG=1)

- `tray promote ok=1` - the promotion write landed; visibility is still the
  shell's per-exe setting (the measured section above).
- `toggle key vk=... -> hangul|english` / `toggle core -> mode=...` - the mode
  boundaries whose tail ran `UpdateTrayIcon()`.
- `launch settings exe=<path>` - the exe the menu entry resolved (the
  installed shape lands `<install>\settings-ui\hunmin-tsf-settingsui.exe`).

## What this surface deliberately does NOT do

- It does not touch the shell's language list, the compartment write
  (stays: correct TSF, costs nothing), or the langbar item code (dormant,
  re-measured with the checklist `tsf-langbar-badge.md` each milestone -
  the "not lost, just blocked" lane of issue #103's option C).
- It does not toggle the mode from the tray (the round-3 steer above).
