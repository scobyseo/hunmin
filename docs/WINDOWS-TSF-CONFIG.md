# Windows TSF configuration surface (layout wiring + data loading)

Design record for milestone C (issues #66, #71): how `hunmin-tsf.dll` picks its
layouts and finds its data. What must hold is `docs/SPEC-TSF.md` — TSF6 (the
binding grammar), TSF10 (caps), TSF12 (read order), TSF13 (one data directory),
TSF14/TSF15 (the settings surface) — cited here, not restated.

Provenance: measured on Windows 11 (26200), Windows SDK 10.0.26100, VS
BuildTools 14.44, during the 2026-10-17 milestone C work.

## The store

`frontends/tsf/src/config.{h,cpp}`. One per-user key,
`HKCU\Software\Hunmin\TSF`, `REG_SZ` values `HangulLayout`, `LatinLayout`,
`HangulEngKeys`, `HanjaKey`, `CommitUnit`; an empty string removes the value.
The option names mirror the fcitx5 addon so both frontends read the same
settings; there is no per-frontend store for engine state (SPEC-MODES R13
boundary — engine options go through `hunmin_ic_set_option`).

Env seams (the tier-F1 override, empty means "no override"): 
`HUNMIN_TSF_HANGUL_LAYOUT`, `HUNMIN_TSF_LATIN_LAYOUT`, `HUNMIN_TSF_TOGGLE_KEYS`,
`HUNMIN_TSF_HANJA_KEY`, `HUNMIN_TSF_COMMIT_UNIT`. Defaults are the fcitx5
defaults: `sin3-p2` / `us-qwerty`, and `CommitUnit=word`.

## Slot validation

`resolveLayoutSlot()` accepts a stored id only when the engine can instantiate
it (`hunmin_ctx_layout_supported`) AND the layout's mode matches the slot
(`hunmin_ctx_layout_mode`); otherwise the fcitx5 default returns with a log
line. A stale registry value naming a latin layout in the hangul slot degrades
to a working default, never a silent mispair. (TSF12.)

## The mode-toggle bindings

Grammar `v1:<vk>:<mods>` — decimal, `;`-separated, one `REG_SZ` list; modifier
bits `1 SHIFT 2 CTRL 4 ALT`, with the side carried by the VK itself
(`VK_RMENU` vs `VK_LMENU`). Matching is exact per family, and a malformed entry
parses to nothing rather than swallowing a keystroke. `Activate()` appends the
bare `VK_HANGEUL` whenever the stored list lacks it: the physical 한/영 key is a
platform alternate no rebinding can remove.

Three measurement-backed details the SPEC does not carry:

- **A capture must not store `VK_PROCESSKEY`.** While a Korean IME is active the
  physical 한/한자 keys arrive as `VK_PROCESSKEY` (0xE5 = 229); the first live
  capture stored `v1:229:0` and bound nothing (2026-10-18). The Win32 dialog
  resolves it through IMM32 (`ImmGetVirtualKey`); the WinUI exe has no IMM32, so
  it rejects 229 with an on-screen hint instead.
- **Auto-repeat** is keydown with lParam bit 30 set: one toggle, repeats
  swallowed, so a held key neither double-toggles nor leaks to the application.
- **Caps Lock** is not a toggle; the adapter feeds the VK's base plus the Shift
  bit and sets the engine `caps` option per key event (TSF10). The option feeds
  the yethangul P variable only (`hunmin.h`); tier F1 pins both sides — dubeolsik
  `r` under Caps Lock stays the plain row ㄱ, and sin3-p2-yet's `j`/`k` pair
  resolves the P=1 tier with caps on.

Device-bound: whether Windows routes the physical Han/Eng key to the TIP as
`VK_HANGEUL` is verified per host
(`tests/manual-checklists/tsf-mode-toggle.md`).

## Data loading — one source wins

`hunmin_ctx_from_module` takes the first present: `HUNMIN_DATA_DIR` →
`<module>\data` (the shipped shape; the build stages `libhunmin/data` beside the
DLL and every tier-F1 executable via POST_BUILD `copy_directory`) → the
checkout climb. **The earlier additive chain was a defect, not an
optimisation:** once staging existed the same tree loaded twice — 15 staged
layout files reported `hunmin_ctx_layout_count() == 30` (2026-10-17, the first
red run of `tsf-datadir-test`), because the engine's search path is additive by
design and does not dedupe ids. Overlapping dirs are the caller's to avoid; the
engine contract is untouched. (TSF13.)

## The settings surface

A separate WinUI 3 exe (ruling 2026-10-17, issue #71: "B — separate WinUI 3
settings exe via Windows App SDK; true Fluent; ~50MB+ installer weight"). Tree:
`frontends/tsf/settings-ui/` — net8.0, unpackaged, WindowsAppSDKSelfContained +
SelfContained, win-x64, built by `build-settings-ui.bat`. It reads the layout
JSON (`id`, `name.ko`, `mode`) and writes the store; no engine link, no
candidate logic.

Launch paths, both reaching the same exe: `ITfFnConfigure::Show` through
`settingslaunch.cpp` (ShellExecuteEx, waits on the process handle — the
property-sheet contract now means one Settings host process — then re-applies
the store to the live adapter), and `show-settings.bat` directly. Exe
resolution: `HUNMIN_TSF_SETTINGS_EXE` → `<dll>\settings-ui\publish\…` →
`<dll>\..\settings-ui\publish\…` (dev shape), plus the INSTALLED shape
`<app>\settings-ui\hunmin-tsf-settingsui.exe` required by REL7. Data dir:
`--data` → `HUNMIN_DATA_DIR` → `<exedir>\data` → parent climb.

The key rows are set by CAPTURE, never typed (TSF15): [변경] starts capture, ESC
cancels, auto-repeat is swallowed, modifier-only presses are rejected, and the
result writes one `v1:<vk>:<mods>` entry — the physical Han/Eng alternate
survives regardless. A fourth value, `HanjaKey` (default `v1:25:0`, the bare
Hanja key, not F9), opens the candidate window (issue #67); an unparseable value
falls back to the default in the adapter so the window stays reachable.

Measured sizing on this host (2026-10-17, DPI scale 1.25): the WinUI exe at
545×674 px on screen; `ResizeClient` input scales by the DPI factor, so a
(500,600) request lands 625×750 and the compact request is tuned to the quoted
~480–560 px band. The interim Win32 dialog's template is 216×164 DU, measuring
493×434 px. Publish output is 495 files / 167.4 MiB fully self-contained
(issue #68's packaging weight).

## Tier F1 coverage

`tsf-config-test` (env wins, empty env unset, defaults, save/load round trip
against an isolated `…\TSF-selftest` root deleted on every exit path,
unterminated-id rejection, slot resolution against the real data dir);
`tsf-datadir-test` (staged `<module>\data` alone must load, the staged json count
must equal `hunmin_ctx_layout_count()`); `dubeolsik-seosi.trace.in`/`.gold` (400
events generated from the tier-A `ref-dubeolsik.jsonl` gate, gold reviewed — 400/400
commits equal). The trace runner pins both slots from the trace's own meta, so a
host's real TIP settings cannot move a conformance run.

Registration drift fixed 2026-10-17: the subproject lacked `enable_testing()`
(`add_test` a silent no-op) and the `if(Python3_FOUND)` guard skipped
registration when `-DPython3_EXECUTABLE` was passed. Both are TSF18 now.
