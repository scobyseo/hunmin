# Windows TSF configuration surface (layout wiring + data loading)

## Purpose

Describe how `hunmin-tsf.dll` picks its layouts and finds its engine data — the
milestone C deliverables (issues #66 and its subtask #71). This is the Windows
companion to the fcitx5 addon config (`frontends/fcitx5/src/config.h`):
mirrored option names so both frontends read the same two settings, one store
per platform. The settings dialog that writes the store is #71 and is
documented by its own work.

Provenance: every rule below was measured on this host — Windows 11 (26200),
Windows SDK 10.0.26100, VS BuildTools 14.44 — during the 2026-10-17 milestone C
work; failures that produced each rule are named inline.

## The configuration store

- File: `frontends/tsf/src/config.cpp` (+ `config.h`).
- Registry: `HKCU\Software\Hunmin\TSF`, values `HangulLayout` / `LatinLayout`
  (`REG_SZ`), written by the settings dialog (#71), read at every `Activate()`
  and by the mode toggle.
- Read order — first source with a non-empty value wins:
  1. environment `HUNMIN_TSF_HANGUL_LAYOUT` / `HUNMIN_TSF_LATIN_LAYOUT` (the
     tier-F1 seam: tests inject layouts without touching a real user's
     registry, and empty values mean "no override", the same rule
     `hunmin_ctx_from_module` applies to `HUNMIN_DATA_DIR`);
  2. the registry values above;
  3. defaults `sin3-p2` / `us-qwerty` — the fcitx5 defaults, unchanged.
- SPEC-MODES R13 boundary holds: no per-frontend settings store for engine
  state. The only fields here are layout ids; feature state (the autoswitch
  row, disabled until 0.3 per the recorded 2026-10-11 ruling) stays with the
  engine via `hunmin_ic_set_option`.

## Slot validation

`resolveLayoutSlot()` (`frontends/tsf/src/tsf.cpp`) accepts a stored id only
when the engine can instantiate it (`hunmin_ctx_layout_supported`) AND the
layout's mode matches the slot (`hunmin_ctx_layout_mode`, #71's dialog lists
them split by the same field). Anything else falls back to the fcitx5 default
with a log line — a stale registry value naming a latin layout in the hangul
slot degrades to a working default, never a silent mispair.

## The mode-toggle bindings

Maintainer instruction (issue #66 / #71, 2026-10-17): **the Han/Eng key is the
default toggle, and additional keys are configurable** — Right Alt, Shift+Space,
and so on. The shape is Android's `KeyBinding.kt` ported to Windows vocabulary
(`frontends/tsf/src/keybinding.h/.cpp`):

- Grammar: `v1:<vk>:<mods>`, DECIMAL numbers, entries joined with `;` — the
  same format Android stores (`v1:<keyCode>:<modifiers>`); the numbers are the
  engine's own, and comparing them is the only contract. One REG_SZ list,
  `HangulEngKeys`; env override `HUNMIN_TSF_TOGGLE_KEYS` (the tier-F1 seam).
- Modifier bits `0x1 SHIFT 0x2 CTRL 0x4 ALT`; side selection is carried by the
  VK itself (VK_RMENU vs VK_LMENU — the mirror of Android's side-coded
  `META_ALT_RIGHT`).
- Matching is exact per family (Android's `modifiersMatch`): a bare binding
  does not fire under a modifier and a chord does not fire bare, so Shift+Space
  and a shifted layout row never fight over the same event. A malformed entry
  parses to nothing — a binding nobody set must not swallow a keystroke.
- **Capture must not store VK_PROCESSKEY.** While a Korean IME is active, the
  physical 한/Hanja keys arrive at a capture surface as VK_PROCESSKEY (0xE5 =
  229) — the first live capture stored `v1:229:0` and bound nothing (measured
  2026-10-18). The Win32 dialog resolves it through IMM32
  (`ImmGetVirtualKey`) before recording; the WinUI exe has no IMM32 access and
  instead rejects 229 with an on-screen "이 키는 IME가 가로챕니다" hint. The
  tier-F1 harness sets `HUNMIN_TSF_HANJA_KEY`/`HUNMIN_TSF_TOGGLE_KEYS` env
  defaults so a store a live session mutated cannot flip test expectations.
- The **physical Han/Eng key is a platform alternate that survives every
  rebinding** (the Android rule: "a user rebinding 한/영 must not end up with a
  keyboard whose physical 한/영 key does nothing"): `Activate()` appends the
  bare `VK_HANGEUL` whenever the stored list does not carry it.
- A toggle is a composition boundary (Jamotong manual §9.5; the Android adapter
  commits at the mode switch): the pending syllable is flushed and the buffered
  word inserted exactly once before the engine is rebuilt with the other slot's
  layout.
- Auto-repeat is keydown with lParam bit 30 (preceding-down) set: the press
  toggles once and repeats are swallowed, delivering neither a wrong toggle nor
  the repeat to the application.
- CapsLock is not a toggle and never selects a row in Korean input mode: the
  adapter hands the engine the VK's unshifted base plus the Shift bit
  (`hunmin_ic_process_key`, issue #106), and the Shift bit is the only row
  transport — CapsLock alone composes the plain row, on every platform.
  The engine `caps` option **is wired from the key path** (issue #112,
  ruling 2026-10-19: "wire and make it common to all platform"): one
  `hunmin_tsf_capslock_from_state()` read beside the modifier snapshot
  (`frontends/tsf/src/keybinding.cpp`, the toggle LOW bit of
  `GetKeyState(VK_CAPITAL)`), and one `hunmin_ic_set_option(ic, "caps",
  1|0)` per key event at the snapshot in tsf.cpp — the same per-event shape
  fcitx5 has had since hunminadapter.cpp:327 and Android since #105 (aligned to
  1/0 per event by #112; the set-1-only shape left the P variable sticky-on
  after the first CapsLock use). The option feeds the yethangul P variable
  only (`hunmin.h:93`); tier F1 pins both sides — dubeolsik r under
  CapsLock stays the plain row ㄱ, and sin3-p2-yet's j/k pair resolves the
  P=1 tier with caps on and the plain tier with caps off (the
  `p2-2-4-3-caps.jsonl` golden pair, replayed through the adapter).

Device-bound open question: whether Windows routes the physical Han/Eng key to
the TIP as `VK_HANGEUL` (vs. the input hotkey system consuming it) is verified
per host — `tests/manual-checklists/tsf-mode-toggle.md` records the steps; the
key-state mechanics themselves (`OnKeyDown`/`OnTestKeyDown` with
`GetKeyState`-pinned modifiers; chords like `v1:21:1` = the Han/Eng key as the
target with Shift held, Android's voice action "Shift+KANA" shape) are covered
by `tsf-config-test`.

## Data loading — one source wins

The adapter resolves one data tree per load, in priority order
(`hunmin_ctx_from_module`):

1. `HUNMIN_DATA_DIR` (explicit override);
2. `<module>\data` — the shipped shape: the build stages `libhunmin/data`
   beside `hunmin-tsf.dll` and every tier-F1 executable (POST_BUILD
   `cmake -E copy_directory`, frontends/tsf/CMakeLists.txt);
3. the repo checkout climb (`<module>/../../../libhunmin/data`), the dev
   convenience, reached only when neither of the above holds.

The earlier additive chain (env + module dir + repo climb together) is not an
optimisation but a defect once staging exists: `hunmin_ctx_new`'s search path
is additive by design and does not dedupe layout ids across dirs, so the same
tree loaded twice — 15 staged layout files reported
`hunmin_ctx_layout_count() == 30` (measured 2026-10-17, red first run of
`tsf-datadir-test`). Overlapping dirs are the caller's to avoid; the engine's
contract is left untouched (no core change).


## The settings dialog (WinUI 3 exe)

The configuration UI is a **separate WinUI 3 settings exe** — the maintainer's
ruling 2026-10-17 on issue #71 ("B — separate WinUI 3 settings exe via Windows
App SDK; true Fluent; ~50MB+ installer weight; milestone E packaging
implications"). It replaces the plain-Win32 dialog on the TSF surface; the
interim dialog stays reachable only through `show-settings.bat`'s rundll32
fallback until the Win32 TU is removed.

Tree: `frontends/tsf/settings-ui/` — `HunminTsfSettingsUI.csproj` (net8.0,
unpackaged, `WindowsAppSDKSelfContained=true`, `SelfContained=true`,
win-x64), `App.xaml(.cs)`, `MainWindow.xaml(.cs)`,
`build-settings-ui.bat`. No engine link, no candidate logic: the exe reads
the layout JSON entries (`id`, `name.ko`, `mode` — docs/SPEC-SCHEMA.md) and
writes the store, nothing else.

- Launch paths (both reach the exe):
  1. TSF-native — `ITfFnConfigure::Show` (`frontends/tsf/src/settingslaunch.cpp`)
     launches the exe via `ShellExecuteEx` (edge case logged, never a
     fallback to the interim dialog) and **waits on the process handle**: the
     property-sheet contract ("do not return while the dialog is open") now
     means one Settings host process. On exit it re-applies the store to the
     live adapter (`hunmin_tsf_apply_config_store`, the #71 apply bridge).
  2. Direct — `frontends/tsf/show-settings.bat` launches the same exe
     (resolved beside the DLL, InprocServer32 fallback), rundll32 interim
     dialog as fallback.
- Exe resolution (first hit wins): `HUNMIN_TSF_SETTINGS_EXE` env >
  `<dll dir>\settings-ui\publish\hunmin-tsf-settingsui.exe` >
  `<dll dir>\..\settings-ui\publish\hunmin-tsf-settingsui.exe` (the
  dev-checkout shape).
- Data dir (same precedence the adapter uses): `--data` arg >
  `HUNMIN_DATA_DIR` env > `<exedir>\data` > the parent-dir climb
  (`<exedir>\..\data`, the deployed shape — the exe always sits under
  `<dll dir>\settings-ui\`).
- The mode-toggle key row: **one configurable binding set by key capture**
  (ruling quoted: "한영 전환 키는 하나만 선택해도 되고, 복잡하게 입력하는 게
  아니라 키 입력 방식을 사용하자") — [변경] starts capture ("키를
  누르세요…", ESC cancels), the dialog's own KeyDown catches the next press,
  auto-repeat is swallowed by the repeat bit, and modifier-only presses are
  rejected (Android's isModifierKey mirror). The result writes ONE
  `v1:<vk>:<mods>` entry to `HangulEngKeys`: the store's ';' list grammar is
  unchanged, and the adapter still appends the implicit bare Han/Eng
  platform alternate — the physical key can never be captured away.
- Store contract untouched (config.{h,cpp} untouched): `HKCU\Software\Hunmin\TSF`,
  `REG_SZ` values `HangulLayout` / `LatinLayout` / `HangulEngKeys`; empty
  string removes the value (an absent toggle value leaves the physical key
  as the toggle).
- Issue #67 adds a fourth value: `HanjaKey`, the candidate-window open/close
  key, ONE `v1:<vk>:<mods>` entry (the maintainer instruction of 2026-10-18:
  the physical Hanja key is the default, not F9). Default `v1:25:0`
  (VK_HANJA = 0x19 = 25 decimal); env override `HUNMIN_TSF_HANJA_KEY` (the
  same empty-means-no-override seam); the interim dialog's edit row follows
  the same grammar, an empty edit saves the default explicitly. An
  unparseable value falls back to the default IN THE ADAPTER rather than
  unbinding the window — the candidate window must stay reachable, and the
  failing entry must never swallow a keystroke (the toggle-binding rule
  above, applied to a single-entry list).
- Compact sizing (ruling: "configuration window ... too large; make both
  compact"), measured on this host 2026-10-17 (Windows 11 26200, the
  display's DPI scale 1.25 with a reported 96 DPI):
  - WinUI exe: 545x674 px on screen (client 525x655 + title bar), fixed
    via `AppWindow.ResizeClient` post-Activate — the ResizeClient input scales
    by the DPI factor on this host ((500,600) request → 625x750 client), so
    the request is measured to land inside the quoted ~480-560 px band.
  - Interim Win32 dialog: template 216x164 dialog units, measured
    493x434 px on screen (client 472x385, per-window DPI 96 on a 125% display
    — the dialog's own DLU conversion lands larger than the raw-DU math
    suggests). The quoted ~340x240 target is a LARGER template in dialog
    units than the existing 216x164, and the on-screen 493x434 px already
    sits in the compact band — kept as the fixed small size, recorded, and
    the exact-quoted-template question is left to the removal commit.
- Also recorded for #68 packaging: publish output is 495 files /
  167.4 MiB fully self-contained (the ruling's ~50 MB+ is confirmed and
  exceeded by the runtime-carrying layout).

## Tier F1 additions (ctest, MSVC-only)

- `tsf-config-test` — env wins, empty env is unset, defaults, save/load round
  trip against an isolated `HKCU\Software\Hunmin\TSF-selftest` (deleted on
  every exit path; never the production store), unterminated-id rejection,
  and slot resolution against the real data dir.
- `tsf-datadir-test` — `HUNMIN_DATA_DIR` cleared, the staged `<module>\data`
  alone must load: the staged `layouts/*.json` count must equal
  `hunmin_ctx_layout_count()` and dubeolsik must compose.
- `tests/conformance/win32/dubeolsik-seosi.trace.in` + `.trace.gold` — a
  dubeolsik replay generated from the tier A `ref-dubeolsik.jsonl` gate (400
  events; gold reviewed, 400/400 commits equal, 153 UTF-8 bytes on both
  sides). Before this, every win32 trace selected the hard-coded sin3-p2 and
  could not detect a layout-wiring regression; the runner pins both slots
  from the trace's own meta so a host's real TIP settings cannot move a
  conformance run.

Measured registration drift fixed the same day (the F1 tier had been
registering zero tests in the documented standalone flow):
`frontends/tsf/CMakeLists.txt` lacked `enable_testing()` — `add_test` was a
silent no-op while ctest printed "No tests were found" and the harness still
built — and the `if(Python3_FOUND)` guard skipped test registration whenever
`-DPython3_EXECUTABLE=...` was passed explicitly, because `find_package` is
skipped when a cache value exists and `Python3_FOUND` is never set.
