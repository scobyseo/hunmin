# Manual checklist — TSF mode-toggle keys on a real host

Milestone C (#66 / #71), 2026-10-17. The mock harness proves the key-state
mechanics (`tsf-config-test`); this checklist measures what only a real
desktop session can: whether Windows routes the physical keys to the TIP.
Artifacts needed: the registered `hunmin-tsf.dll`, Notepad, and the runtime
log `<dll dir>\hunmin-tsf-debug.log` (delete it before starting so only this
run's lines appear).

Steps:

1. Select Hunmin via Win+Space in Notepad. Type `rk` — the log must show two
   `OnKeyDown` lines and the overlay should render the in-flight syllable.
2. Press the physical **Han/Eng** key once: the log must show
   `toggle key vk=21 repeat=0 -> english` (VK_HANGEUL = 0x15 = 21). If the key
   arrives as anything else, Windows is consuming it (input hotkey system) and
   the finding — not this failure — is the measurement to record.
3. Press it again: `toggle key vk=21 repeat=0 -> hangul`.
4. Hold it down so it auto-repeats: extra `repeat=1` lines, and the mode must
   NOT flip more than once (the `repeat=1` deliveries are swallowed).
5. With `HangulEngKeys` set to `v1:165:0;v1:32:1` (Right Alt + Shift+Space;
   registry `HKCU\Software\Hunmin\TSF` or the settings dialog): press bare
   Right Alt → `toggle key vk=165`; press bare Space → NOT a toggle (Space
   reaches the document); Shift+Space → toggles.
6. If a Korean hardware keyboard is attached, verify the physical 한/영 key
   (step 2 form) and a second chord: with `v1:21:1` configured, Holding Shift
   then pressing 한/영 must toggle while the bare key still does (the exact
   modifier rule) — evidence for the "Han/Eng combined with other keys"
   question in docs/WINDOWS-TSF-CONFIG.md.

Notes:
- The Han/Eng key combined with another action key (Android's voice "Shift +
  KANA" shape) needs no new mechanics: the modifier bits are pinned from the
  thread input state at keydown, so `v1:21:<mods>` is just another chord. What
  remains genuinely unmeasured is Han/Eng-as-pseudo-modifier (holding 한/영
  while pressing a letter), which would need OnKeyUp state tracking — record
  device behavior before building that.
- On an attached Japanese keyboard, the Kana key maps to VK_HANGEUL class on
  Korean systems only by the driver; acceptance is per device (the Android
  precedent "KANA is detected as han/eng" lives in its own keycode space
  (218), not Windows's).
