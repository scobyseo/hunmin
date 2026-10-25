# Manual checklist — TSF Hanja-key candidate window (issue #67)

Device-bound human pass: the mock harness covers the state machine, a live
TIP covers what it cannot — real hosts, real windows, real caret rects.
Register `hunmin-tsf.dll` (see `frontends/tsf/register-tsf.bat`) and select
the Hunmin profile with Win+Space first.

| # | Step | Expected |
|---|---|---|
| 1 | Focus Notepad (or any edit control), type `dkssud` | overlay shows `안녕` (word buffered, nothing in the document yet) |
| 2 | Press the Hanja key | popup listing `1. 安寧` (and the second sense) under the caret; nothing is inserted |
| 3 | Press Return | `安寧` appears once; no leftover Hangul; popup gone |
| 4 | Type `dkssud`, the Hanja key, press `2` | the second sense's text commits once (same hanja, the meaning differs in the krdict list) |
| 5 | Type `dkssud`, the Hanja key, press Esc, type `d` | popup closes, typing continues (the preedit keeps the word), nothing inserted |
| 6 | Type `dkssud`, the Hanja key, press the Han/Eng key | popup closes, the buffered `안녕` inserts once, language switches to English |
| 7 | Type `rkrkrk` (`가가가` — zero candidates), press the Hanja key | no popup (zero candidates); both phases of the key behave like a pass-through |
| 8 | Type `rk` (`가`, hanja-table fallback list), the Hanja key, PageDown | the popup pages by 10 rows; digits pick from the page |
| 9 | Popup in WordPad (rich edit control) after changing font size | popup face/size follows the edit control's selection font |
| 10 | After a commit, keep typing | composition restarts cleanly, no duplicated first syllable |
| 11 | Run `rundll32 <dll>,HunminSettingsShow` | the dialog appears on screen, centred topmost (`visible=1` in the debug log) |
| 12 | Click 키 입력 beside 한/영 전환 키, press Right Alt | the edit gains `v1:165:0` (VK_PROCESSKEY resolves through IMM32; button label restored) |
| 13 | Click 키 입력, press a physical 한/Hanja key | the IME-eaten key does NOT store `v1:229:0`; the binding records (dialog) or the hint says IME가 가로챕니다 (WinUI exe) |
| 14 | Press the captured toggle key in Notepad | mode toggles per the stored binding; the registry carries the captured grammar |
| 15 | Start menu → Hunmin IME 설정 | the WinUI 3 settings exe opens (fallback: the rundll32 dialog when the exe is not deployed) |

Environment note: in UWP apps and console windows the caret-rect source can
be blank — the popup takes the same hide path as the overlay. Record hosts
where the popup misplaces or vanishes rather than "fixing" locally; the
caret-rect open questions stay in `docs/WINDOWS-TSF-BUFFERING.md`.
