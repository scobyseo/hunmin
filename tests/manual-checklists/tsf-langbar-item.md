# TSF: the langbar item as the mode indicator + right-click config menu (issue #103)

**The ruled surface (2026-10-20 steer): OUR OWN language-bar item — not the
`ITfSystemLangBarItemSink` system-menu extension, not the own
`Shell_NotifyIconW` tray badge.** Both removed lanes are recorded in board
row TSF-ITEM-SINK-67 (the extension advice rendered nothing in the shell
tray menu; the tray badge lost the overflow-visibility iteration, PR #124
rounds 1-3) and the sibling jamotong TIP is hosted live on this host with
GUID_LBI_INPUTMODE + BTN_BUTTON|BTN_MENU|SHOWNINTRAY + an implemented
ITfSource; the maintainer ruled 2026-10-22 that OUR item carries
BTN_MENU|SHOWNINTRAY (no BTN_BUTTON).

## What carries what

- `frontends/tsf/src/langbar.cpp` — the item is a tear-off
  (`LangBarItem`, S6): `GetInfo` carries `guidItem = GUID_LBI_INPUTMODE`
  and `BTN_MENU | SHOWNINTRAY` (ruled 2026-10-22, no BTN_BUTTON);
  `GetIcon`/`GetText` are the 한/A mode indicator (resources 2/3);
  `AdviseSink` accepts the kernel-provided `ITfLangBarItemSink`;
  `UpdateLangBarItem()` notifies `OnUpdate(TF_LBI_ICON|TF_LBI_TEXT)` at
  every mode boundary; `InitMenu` adds the one-entry config menu
  ("Hunmin 설정 열기") and `OnMenuSelect` launches it through
  `hunmin_tsf_launch_settings()` (no caller anchor: the launcher anchors on
  its own address). `OnClick` is implemented but not advertised under
  `BTN_MENU`.
- `Activate` calls `LangBarItem::Attach` (`AddItem`), `Deactivate` calls
  `Detach` (`RemoveItem`) and the detach releases the sink. The ordering was
  fixed 2026-10-27 (#145): the manager reference is dropped after the detach,
  so `RemoveItem` is reached (board row TSF-DEACTIVATE-LANGBAR-145). The conversion-compartment write
  (`UpdateConversionCompartment`) is unchanged.

## Why the item should host at all (the recipe's basis)

The 2026-10-18 AddItem refusals (guidItem F2C70003, BTN_BUTTON alone →
`0x80004005`) measured OUR former shape. The sibling jamotong TIP on THIS
host ships its item hosted in the tray input indicator with the recipe
above; the "environmental" reading of 2026-10-18 never excluded the
item-code hypotheses (missing `ITfSource` support, wrong item GUID, missing
tray-style bit) because its reference probe carried none of them. This
checklist measures the re-shaped item; a red row records which hypothesis
survived, it does not re-assert 2026-10-18.

## Verification

1. `reg query "HKLM\SOFTWARE\Microsoft\CTF\TIP\{F2C70001-0000-0000-0000-000000000001}"`
   shows the profile enabled; the installed/dev DLL is the current build.
2. Open a text host (WordPad measured all session) with
   `HUNMIN_TSF_DEBUG_LOG=1`: `langbar QI hr=0x0`, then
   `langbar AddItem hr=0x…`:
   - `0x0` = the item lane is OPEN on this host — the item shows.
   - an `0x80004005`-class = the refusal measured with the NEW shape; record on
     issue #103 and go on to rows 4-6 (they measure the item's own
     interfaces, not the host's grace).
3. The input indicator hosts the Hunmin entry: the Win11 taskbar input
   indicator shows the Hunmin icon (한 red badge / A badge) beside the MS
   IME entry when Hunmin is the active profile, WITHOUT any per-exe promote
   step (this indicator slot is language-profile-driven, not the
   `NotifyIconSettings` store the removed tray badge lived in).
4. The indicator flips with the mode: toggle with the Han/Eng key —
   한 ↔ A within one keystroke, no Win+Space reselection. A red result
   with AddItem landed means the sink (`ITfSource::AdviseSink`) or the
   `OnUpdate` channel did not install: the debug log names which
   (`langbar AddItem hr`, followed by the toggle's `OnUpdate` reach).
5. Right-click the Hunmin indicator entry → the one-entry menu
   "Hunmin 설정 열기" → the WinUI 3 settings exe opens; the debug log shows
   `langbar InitMenu`, then `langbar OnMenuSelect id=1`, then
   `launch settings exe=<path>`. On an installed shape the exe path logged is
   `<app>\settings-ui\hunmin-tsf-settingsui.exe` (the resolver carries the
   installed shape beside the dev `publish\` shapes). A menu that opens but
   does nothing is the former null-anchor defect: `OnMenuSelect` passed null,
   `GetModuleHandleExW` failed, `E_FAIL` (fixed 2026-10-23, board row
   TSF-LANGBAR-MENU-103). **MEASURED 2026-10-23 on this Win11 host: RED.**
   `AddItem hr=0x0`, but the tray indicator never calls the item: left click
   opens the shell's language list, right click dismisses it, and the log
   shows neither `InitMenu` nor `OnClick`. The null-anchor fix is real but
   unreachable on this host.
6. The menu-button surface is `InitMenu`/`OnMenuSelect` (the ruled style is
   BTN_MENU | SHOWNINTRAY, no BTN_BUTTON); `OnClick` stays implemented but
   is not advertised. Adding `BTN_BUTTON` was measured 2026-10-23 and did
   not make the tray indicator route the click either, so it was reverted.
   The mode is never flipped by a click (the round-3 steer stands; the mode
   flips with the key only).
7. `ctest --test-dir frontends/tsf/build-msvc -C Release -L tier-F1` green;
   `tools/docs-lint.py --build-dir <build>` 0 errors.

## MEASURED (2026-10-20/21, dev host + sandbox rounds 4-5, this branch)

The lane MEASURES OPEN now (premise of the 2026-10-18 "environmental"
verdict falsified — it read our former shape):

- **Dev host (23:56, real WordPad load of the re-shaped build)**:
  `langbar AddItem hr=0x0` on every activation — never seen before any
  shape.
- **Sandbox fresh-VM round 4/5** (installed shape): install silent
  `rc=0`; `DllRegisterServer` chain all `hr=0x0` (Register /
  AddLanguageProfile / EnableLanguageProfile / ActivateProfile attempt:
  `E_80070057` in the scripted probe — the known unmeasurable; the human
  Win+Space selection is the activation that counts); probe+host traces:
  `langbar AddItem hr=0x0`, mode toggles through the Han/Eng key
  (`toggle core -> mode=…`).
- **The hosted-indicator row, now measured CLOSED on BOTH hosts (rounds
  5-6 sandbox + the dev host's deep scan, 2026-10-21 01:14 — after
  Hunmin selected in the focused Notepad):** the desktop-wide UIA tree
  (deep-uia.ps1: the whole desktop walk, not just `Shell_TrayWnd`) holds
  **no CTF-hosted language-bar window at all** (zero CTF/CUIHosted/
  FloatAsset/CUAS class hits); the indicator is ONE
  `SystemTray.NormalButton` whose name re-labels to
  `트레이 입력 표시기 한국어 / Hunmin` — the shell's per-profile
  identity. Every legal item shape is in place and accepted
  (`guidItem = GUID_LBI_INPUTMODE`, `BTN_MENU|SHOWNINTRAY` (re-ruled
  2026-10-22 without BTN_BUTTON),
  `ITfSource`, `AddItem hr=0x0` on every fresh host) and the embedded
  bar STILL renders no per-TIP slot: the jamotong comment's hosting rule
  described the Win8/10-era path. The single unmeasured comparator left
  is jamotong itself re-registered on THIS build (no DLL in tree,
  unregister measured today) — if the maintainer revives it, measure it;
  nothing in our item code changes before then.
- **Round-5 bug report (sin3-p2 chosung)**: `/` `;` `'` reached the app as
  raw symbols — the test phase answer for every feedable key OUTSIDE the
  letters was eat=FALSE, and msftedit never calls an uninterested key's
  down phase. Fixed with the general interested branch (hangul mode,
  key of passage or modifier excluded, `VkToBase != 0`); the engine's
  consume/decline at the down phase decides compose-or-pass. Regression
  case `TsfConfigTest.Sin3P2ChosungOnTheOemRow` (`;->ㅂ` preedit assert).
  The caret-before-symbol half rides the same unfed class (the app
  inserted the symbol itself); re-measure at the next live round.

- **Menu-launch defect (2026-10-23, source + tier-F1, live re-measured
  RED)**: the right-click menu's entry launched nothing because
  `OnMenuSelect` passed a null anchor; `GetModuleHandleExW` failed and the
  item returned `E_FAIL` before `ShellExecuteExW`. The launcher now takes no
  anchor and uses its own address. Tier-F1
  `TsfLangbarItem.OnMenuSelectOneLaunchesTheSettingsSurface` drives id 1
  through the real launch path to a windowless stand-in
  (`tsf-settings-noop.exe`, selected with `HUNMIN_TSF_SETTINGS_EXE`), and
  `TsfLangbarItem.InitMenuAddsTheOneSettingsEntry` locks the menu content.
  Live row 5 is red for a different reason: this host never calls
  `InitMenu`/`OnClick` (see row 5), so the fixed handler is not reached.
- **Stale tray entries removed (2026-10-23)**: the removed PR #124
  `Shell_NotifyIcon` build had left four `HKCU\Control Panel\
  NotifyIconSettings` entries with `InitialTooltip=Hunmin` (hosts
  `conhost.exe`, `WindowsSandbox.exe`, `Code.exe`, `Notepad.exe`); they were
  exported to `%TEMP%\opencode\hunmin-notify-20261023-213959\` and deleted,
  and explorer was restarted. `TrayNotify\IconStreams` and
  `PastIconsStream` now contain no Hunmin string. No tray-icon code remains
  in the tree.

## Signals in the debug log (HUNMIN_TSF_DEBUG_LOG=1)

- `langbar AddItem hr=0x…` — the item lane on this host.
- `toggle core -> mode=…` — the mode boundaries whose tail ran
  `UpdateLangBarItem()` (`OnUpdate` only fires when the kernel advised a
  sink).
- `langbar InitMenu` / `langbar OnMenuSelect id=<n>` — the menu-button
  route: which handler the host called and with which id.
- `launch settings exe=<path>` — the menu entry's resolved exe.
