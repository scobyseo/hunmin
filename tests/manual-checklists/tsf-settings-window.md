# Manual checklist — TSF settings window on a real host

The mock harness (`tsf-settings-test`, tier F1) can drive the COM plumbing —
ITfFunctionProvider, ITfFnConfigure, ApplyConfigStore — because it feeds the
adapter real objects. What only a real desktop session can measure is the
modal dialog itself and the Windows 11 look-and-feel layer (`applyWindows11Look`
logs every DWM HRESULT; the log is the measurement, not a screenshot).
Artifacts needed: the built `hunmin-tsf.dll`, the runtime log
`<dll dir>\hunmin-tsf-debug.log` (delete it before starting so only this run's
lines appear), and `regedit` for reading the store.

Background: the window reached two ways —

1. `rundll32` export: `frontends/tsf/show-settings.bat` resolves the DLL
   clone-independently (beside the script, then the registered InprocServer32
   path) and calls the `HunminSettingsShow` export.
2. The TSF path: the language bar / text-service Tools menu opens
   `ITfFnConfigure::Show`, which runs the same shared
   `settingsDialogRun`.

Steps:

1. Launch `show-settings.bat`. The log must show `HunminSettingsShow entry`,
   `settings: open rows hangul=N latin=M` (row counts from
   `hunmin_ctx_layout_mode`, matching `hunmin_ctx_layout_count`), and a
   top-level dialog titled **Hunmin settings** with no owner (the rundll32
   path has no natural parent).
2. The two combo boxes list only the mode-correct layouts: the hangul combo
   (`IDC_HANGUL_LIST`) carries `dubeolsik` and only hangul-mode ids, the latin
   combo carries `us-qwerty` and only latin-mode ids — store values
   `HangulLayout` / `LatinLayout` preselect. The feature row reads
   **자동 한/영 전환: 미정 (0.3)** and is disabled throughout (ruling quoted in
   `docs/REMAINING-WORK.md` 2026-10-12 — display only).
3. Change both layouts to non-default values, set `HangulEngKeys` to
   `v1:21:1`, press OK. The log must show
   `settings: saved hangul=... latin=... keys=...`, `regedit` must show the
   three values written under `HKCU\Software\Hunmin\TSF`, and the dialog must
   close (EndDialog return 1). Verify the running TIP picks the change up on
   the next Activate — or force it: reopen the dialog, change back, OK; the
   composition of a fresh keystroke must use the new layout without a
   process restart (`ApplyConfigStore` line in the log after the ITfFnConfigure
   path).
4. Set `HangulEngKeys` to a string longer than the store capacity — OK must
   NOT close the dialog silently: the **Toggle key list too long.** warning
   appears and the dialog stays open for fixing.
5. OK with the store unsaveable (deny write by pointing at a read-only root,
   or as the platform permits): the **Failed to write the settings store.**
   warning appears, log shows `settings: save FAILED`, dialog stays open.
   The user's explicit Apply must never silently do nothing.
6. Cancel and the X both close with no write: the registry values must be
   byte-identical to before, log shows `settings: dialog closed got=-1 applied=0`
   (or `applied=0` variant for Cancel) — nothing was written.
7. Windows 11 look (Win11 10.0.26200): rounded corners, Mica backdrop, dark
   mode follows the system, the Variable font applied. Every DWM call's
   HRESULT is logged (`hrBackdrop`, DwmSetWindowAttribute, etc.) — on a
   failure the window must still render (dark fill fallback), and the logged
   hr is the measurement to record, not a reject.
8. The TSF path (needs a registered TIP and a host app whose language bar
   exposes Tools): select Hunmin via Win+Space, open the Tools menu →
   `GetDisplayName` label must launch the same dialog. The log shows
   `ITfFnConfigure::Show pid=...` from the HOST process (notepad etc.), then
   the same entry/save lines as the rundll32 path.

Notes:
- Income to the engine IPC question: an in-flight syllable of a running
  composition is lost across Apply (the ic is rebuilt; documented in the
  `ApplyConfigStore` contract). Verify in step 3: type `rk` so `ㅂ` is in
  flight in a host app, apply a layout change from a second dialog, then
  finish the word — record whether the preedit cleared, and whether the
  partially-composed buffer reconnects around the boundary flush.
- The dialog is not in any automated test (a blocking modal window in a CI
  binary is the manual tier's surface); the pure half — the row split and the
  apply-bridge — is in `tsf-settings-test`.
- Worker note: none of the above is a verdict. A failed step is a finding to
  record in `wiki/`, not a rebuild order.
