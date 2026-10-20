# TSF: the langbar item as the mode indicator + right-click config menu (issue #103)

**The ruled surface (2026-10-20 steer): OUR OWN language-bar item — not the
`ITfSystemLangBarItemSink` system-menu extension, not the own
`Shell_NotifyIconW` tray badge.** Both removed lanes are recorded in board
row TSF-ITEM-SINK-67 (the extension advice rendered nothing in the shell
tray menu; the tray badge lost the overflow-visibility iteration, PR #124
rounds 1-3) and the item's hosting recipe (GUID_LBI_INPUTMODE +
BTN_BUTTON|SHOWNINTRAY + an implemented ITfSource) is the shape the sibling
jamotong TIP is hosted live with on this host.

## What carries what

- `frontends/tsf/src/tsf.cpp` — the item lives ON `CTextService` (implements
  `ITfLangBarItemButton` + `ITfSource`): `GetInfo` carries
  `guidItem = GUID_LBI_INPUTMODE` and `BTN_BUTTON | SHOWNINTRAY`;
  `GetIcon`/`GetText` are the 한/A mode indicator (resources 2/3);
  `AdviseSink` accepts the kernel-provided `ITfLangBarItemSink`;
  `UpdateLangBarItem()` notifies `OnUpdate(TF_LBI_ICON|TF_LBI_TEXT)` at
  every mode boundary; `OnClick(CLK_RIGHT)` shows the one-entry config menu
  ("Hunmin 설정 열기") → `hunmin_tsf_launch_settings`, `InitMenu`/
  `OnMenuSelect` stay wired for a floating-bar host that routes the
  menu-button shape.
- `Activate` adds the item (`AddItem`), `Deactivate` removes it
  (`RemoveItem`) and releases the sink. The conversion-compartment write
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
   "Hunmin 설정 열기" → the WinUI 3 settings exe opens; on an installed
   shape the exe path logged is
   `<app>\settings-ui\hunmin-tsf-settingsui.exe` (the resolver carries the
   installed shape beside the dev `publish\` shapes).
6. Left-click opens the settings surface directly (the BTN_BUTTON command
   action) — the mode is never flipped by a click (the round-3 steer
   stands; the mode flips with the key only).
7. `ctest --test-dir frontends/tsf/build-msvc -C Release -L tier-F1` green;
   `tools/docs-lint.py --build-dir <build>` 0 errors.

## Signals in the debug log (HUNMIN_TSF_DEBUG_LOG=1)

- `langbar AddItem hr=0x…` — the item lane on this host.
- `toggle core -> mode=…` — the mode boundaries whose tail ran
  `UpdateLangBarItem()` (`OnUpdate` only fires when the kernel advised a
  sink).
- `launch settings exe=<path>` — the menu entry's resolved exe.
