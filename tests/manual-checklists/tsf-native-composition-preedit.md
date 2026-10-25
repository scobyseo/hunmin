# Manual checklist — TSF native composition preedit (issue #98)

Device-bound human pass: the mock harness covers the state machine, a live
TIP covers what it cannot — real hosts, real composition ranges, real
display attributes. Register `hunmin-tsf.dll` (see
`frontends/tsf/register-tsf.bat`) and select the Hunmin profile with
Win+Space first.

| # | Step | Expected |
|---|---|---|
| 1 | Focus Notepad, type `dkssud` | the composition shows `안녕` inline in the document (underlined), NOT in a floating overlay window |
| 2 | Keep typing `dkssud` | the composition range grows: `안녕안녕` inline, still underlined |
| 3 | Press Space | the composition ends; `안녕안녕` fuses into the document as ordinary text (no underline); Space reaches the document |
| 4 | Type `dkssud`, then Backspace twice | the composition range shrinks: `안녕안` → `안녕`; no stale preedit visible |
| 5 | Type `dkssud`, then Esc | the composition ends; `안녕` fuses into the document; typing continues |
| 6 | Type `dkssud`, then the Han/Eng key | the composition ends; `안녕` fuses; language switches to English |
| 7 | Type `dkssud`, then the Hanja key | the candidate popup opens over the composition; the composition text is still visible underneath |
| 8 | Press Return (candidate selected) | the candidate text replaces the composition range; the composition ends; no duplicate |
| 9 | Type `dkssud`, then Ctrl+A | the composition ends; `안녕` fuses; Ctrl+A selects all (not types) |
| 10 | Type `dkssud`, then an arrow key (e.g. ←) | the composition ends; `안녕` fuses; the arrow passes through (cursor moves) |
| 11 | Type `dkssud`, then Enter | the composition ends; `안녕` fuses; Enter reaches the document (newline) |
| 12 | Switch to a non-TSF-aware app (e.g. console window) if available | record the host's behaviour: the composition may refuse (fallback to plain insert) or the app may not render it. Record rather than "fix" |

Environment note: the display attribute (underline) requires the app to
support `GUID_PROP_ATTRIBUTE` through the display-attribute provider. Most
modern edit controls (Notepad, WordPad, rich edit) do; console apps and
some UWP controls may not. Record hosts where the underline is missing
rather than assuming the composition is broken.
