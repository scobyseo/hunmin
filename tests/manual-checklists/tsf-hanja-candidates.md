# Manual checklist — TSF F9 Hanja candidate window (issue #67)

Device-bound human pass: the mock harness covers the state machine, a live
TIP covers what it cannot — real hosts, real windows, real caret rects.
Register `hunmin-tsf.dll` (see `frontends/tsf/register-tsf.bat`) and select
the Hunmin profile with Win+Space first.

| # | Step | Expected |
|---|---|---|
| 1 | Focus Notepad (or any edit control), type `dkssud` | overlay shows `안녕` (word buffered, nothing in the document yet) |
| 2 | Press F9 | popup listing `1. 安寧` (and the second sense) under the caret; nothing is inserted |
| 3 | Press Return | `安寧` appears once; no leftover Hangul; popup gone |
| 4 | Type `dkssud`, F9, press `2` | the second sense's text commits once (same hanja, the meaning differs in the krdict list) |
| 5 | Type `dkssud`, F9, press Esc, type `d` | popup closes, typing continues (the preedit keeps the word), nothing inserted |
| 6 | Type `dkssud`, F9, press the Han/Eng key | popup closes, the buffered `안녕` inserts once, mode switches to English |
| 7 | Type `rkrkrk` (`가가가` — zero candidates), press F9 | F9 reaches the application unchanged (no popup); repeat to confirm both halves behave |
| 8 | Type `rk` (`가`, hanja-table fallback list), F9, PageDown | the popup pages by 10 rows; digits pick from the page |
| 9 | Popup in WordPad (rich edit control) after changing font size | popup face/size follows the edit control's selection font |
| 10 | After a commit, keep typing | composition restarts cleanly, no duplicated first syllable |

Environment note: in UWP apps and console windows the caret-rect source can
be blank — the popup takes the same hide path as the overlay. Record hosts
where the popup misplaces or vanishes rather than "fixing" locally; the
caret-rect open questions stay in `docs/WINDOWS-TSF-BUFFERING.md`.
