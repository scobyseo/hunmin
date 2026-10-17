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
- CapsLock is no longer a toggle and is IGNORED in Korean input mode: the
  fed char derives from the VK and the Shift bit alone (the engine's input
  convention is case-encoding — hunmin.h "case encodes shift" — so a
  caps-derived case would corrupt a composition), and the TSF side does not
  wire the engine `caps` option either (fcitx5 does, engine.cpp:327, for the
  yethangul P variable; the parity there is a separate maintainer decision).

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
