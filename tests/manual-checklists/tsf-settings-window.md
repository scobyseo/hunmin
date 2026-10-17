# Manual checklist — TSF settings window on a real host

**The settings UI is the WinUI 3 exe (`hunmin-tsf-settingsui.exe`,
`frontends/tsf/settings-ui/`, ruling 2026-10-17)**; the interim plain-Win32
dialog is its `show-settings.bat` fallback until removed. The mock harness
(`tsf-settings-test`, tier F1) can drive the COM plumbing — ITfFunctionProvider,
ITfFnConfigure, ApplyConfigStore — because it feeds the adapter real objects.
What only a real desktop session can measure is the modal behaviour, the key
capture and the measured window sizes.

Artifacts: the published exe (`build-settings-ui.bat`), the built
`hunmin-tsf.dll`, the runtime log `<dll dir>\hunmin-tsf-debug.log` gated by
`HUNMIN_TSF_DEBUG_LOG=1` (delete before starting), the exe-side log
`settings-ui\publish\settings-ui-debug.log`, and `reg query`/`regedit` for the
store:

    reg query HKCU\Software\Hunmin\TSF /v HangulLayout

## Sizes (compact — the ruling's numbers, measured on this host)

| Surface | Template / request | Measured on screen (2026-10-17, Win11 26200, 125% display scale) |
|---|---|---|
| WinUI exe | ResizeClient (436, 500) — DPI factor 1.25 applies to the request | **545 x 674 px** (client 525x655 + title bar) |
| Interim Win32 dialog | in-memory template 216x164 DU | **493 x 434 px** (client 472x385, per-window DPI 96) |

The exe's measured width sits inside the claimed compact band (~480-560 px);
the interim dialog stays at this fixed size and is not re-laid further
(removal commit decides the quoted ~340x240 DU exact-template question, a
number LARGER than the current 216x164 DU template).

## Steps — the exe surface

1. `reg query HKCU\Software\Hunmin\TSF` — record the three values
   (`HangulLayout`/`LatinLayout`/`HangulEngKeys`) as the before-state.
2. Launch `frontends\tsf\settings-ui\publish\hunmin-tsf-settingsui.exe`
   (`--data <engine data dir>` to pin the tree; the deployed dir and
   `HUNMIN_DATA_DIR` work the same). The window must open inside the compact
   band above, and list layouts split by mode — hangul rows from
   `mode=="hangul"`, latin rows from `mode=="latin"` (ko names from
   `name.ko`, ids in parenthesis, sorted by id). Measured this host: 12
   hangul + 3 latin rows from `libhunmin/data`.
3. The key row shows the stored binding as a label (`한/영`, `오른쪽 Alt`,
   `Shift+Space`, `F11 (v1:122:0)` …; `설정 없음 (물리 한/영 키)` when the
   store is empty). Press **변경**: the hint turns "키를 누르세요… (ESC
   취소)". Captured keydown contract (measured, UIAutomation):
   - the NEXT keydown stores the binding — F11 → label `F11 (v1:122:0)`;
   - **ESC cancels** — the label reverts to the stored binding, nothing is
     written on Apply;
   - modifier-only presses (Shift/Ctrl/Alt alone) are rejected as the
     binding (the Android isModifierKey mirror): the row keeps waiting;
   - auto-repeat (WasKeyDown) is swallowed — one press one capture;
   - Shift chords land with the shift bit (e.g. Shift+Space → `v1:32:1`).
4. Pick a layout different from the stored one in the hangul list, press
   **적용**: the status line shows **저장했습니다.**. Read back:
   `reg query HKCU\Software\Hunmin\TSF /v HangulLayout` shows the NEW value
   (measured: `sin3-p2-basic` → `dubeolsik`), and
   `reg query ... /v HangulEngKeys` shows the ONE captured entry
   (`v1:122:0` in the measured run).
5. Relaunch the exe: the selection and the captured key label come from the
   store (measured: `dubeolsik` + `F11 (v1:122:0)`). Close the window (X):
   exit 0.
6. Restore the before-state values with `reg add` so the host user's TIP
   settings survive the pass (do this after every smoke run).

## Steps — the TSF-native path (device-bound where noted)

7. `ITfFnConfigure::Show` (language bar / text-service Tools on a real
   host, `HUNMIN_TSF_DEBUG_LOG=1` set first): the log shows
   `ITfFnConfigure::Show pid=...` from the HOST process (notepad etc.), then
   `settings-exe: launching <path>` — the exe, NOT the Win32 dialog, opens —
   and while the exe window is open the host call DOES NOT RETURN (the
   modal property-sheet contract; verify: the host app's UI stays blocked
   for the popup's lifetime). After the exe exits the log shows
   `settings-exe: exited 0`, then `ApplyConfigStore` (the live adapter
   re-reads without a restart). **Needs a registered TIP and a real host —
   the registry/registration steps are in `tsf-register.bat`**; not
   measurable headless: record the exact blocker (no language-bar host in
   a headless session) as device-bound, never as passing.
8. Exe miss (delete/rename `HUNMIN_TSF_SETTINGS_EXE` and the publish dir):
   Show must LOG (`settings-exe: not found`) and return S_OK WITHOUT
   opening the interim dialog — the ruling's "exe ONLY".

## Steps — the interim dialog fallback (until removed)

9. `show-settings.bat` with the exe present launches the exe (measured:
   `hunmin-tsf-settingsui.exe` process appears, no rundll32). With the exe
   absent it falls back to `rundll32 <dll>,HunminSettingsShow`, opening the
   **Hunmin settings** dialog (the measured 493x434 px dialog). Steps 2-6
   apply to it with combos in place of the lists; the apply logs
   `settings: saved hangul=... latin=... keys=...` and Cancel/X close with
   `settings: dialog closed got=-1 applied=0`, no write. Expected row label:
   **자동 한/영 전환: 미정 (0.3)** disabled throughout.

## Notes

- Income to the engine IPC question: an in-flight syllable of a running
  composition is lost across Apply (the ic is rebuilt; documented in the
  `ApplyConfigStore` contract in `frontends/tsf/src/settings.h`).
- The dialogs are not in any automated ctest (blocking windows are the
  manual tier's surface); the pure half — the row split, the store round
  trip and the apply-bridge — is in `tsf-settings-test`.
- Worker note: none of the above is a verdict. A failed step is a finding to
  record in `wiki/`, not a rebuild order.
