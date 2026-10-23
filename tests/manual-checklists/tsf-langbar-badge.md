# TSF: the language-bar mode badge and the right-click "설정" (issue #67)

**Why both are allowed to be absent, and how to test whether they exist.**
Two mechanisms carry them; each test below tells you WHICH one failed, so a
red step here is measurement, not an unexplained failure.

## Mechanisms, and what "working" means for each

1. **The mode badge — the conversion compartment.** The TIP writes its own
   state into the thread compartment
   `GUID_COMPARTMENT_KEYBOARD_INPUTMODE_CONVERSION`
   (`TF_CONVERSIONMODE_NATIVE` on the hangul side, 0 on latin) at Activate,
   at every Apply, and on every toggle. What the system does with it: the
   language-bar surface relies on this compartment for its 中/英 state
   (documented reasoning: Microsoft Q&A 5865217). The UNVERIFIED half: a
   third-party TIP's tray badge pick-up on this build (26200) — the accepted
   answer confirmed the state per-thread; whether the taskbar badge moves is
   the field measurement. This bullet is board row TSF-CONV-COMP-67.
2. **The right-click "Hunmin 설정 열기" entry** — TSF's menu-extension
   mechanism (`desktop-src/TSF/language-bar.md`, "Extending System Language
   Bar Items"): the adapter advises an `ITfSystemLangBarItemSink` (menu-id 1)
   onto every system langbar item of the host thread; the item that accepts
   the advice calls its own InitMenu when its menu opens and calls
   OnMenuSelect(1) when the user selects ours. This is board row
   TSF-SYSMENU-EXT-67. **RETIRED 2026-10-20: the implementation is removed**
   (board row TSF-ITEM-SINK-67 records the removal) — the advice measured
   dead as an affordance everywhere it settled, and the ruled lane is the
   toolkit's own item (checklist `tsf-langbar-item.md`). The MEASURED RESULT
   below stays as the record of WHY.
3. **The mode badge on our OWN item** — an `ITfLangBarItemButton` (item GUID
   F2C70003: badges = resources 2/3, text = 한/A, right-click menu) is in the
   DLL, and its `AddItem` is REFUSED on this host: a reference item (fresh
   GUID, BTN_BUTTON only) fails identically in an uninvolved thread —
   measured, environmental. This is board row TSF-LANGBAR-67; do not debug
   our item code for it. **RE-shaped 2026-10-20 (issue #103): guidItem is now
   GUID_LBI_INPUTMODE with BTN_BUTTON|SHOWNINTRAY and an implemented
   ITfSource (style re-ruled 2026-10-22: BTN_MENU|SHOWNINTRAY, no BTN_BUTTON) — the shape the sibling jamotong TIP is hosted live with on
   this host; the old "measured, environmental" verdict measured our former
   shape and neither a wrong GUID nor missing ITfSource support was excluded
   by its probe. Measurements from 2026-10-20 on belong to
   `tsf-langbar-item.md`.**

## MEASURED RESULT (2026-10-18, post-reboot re-register, this host)

The guide above was executed. Three facts, with the log lines that measured
them:

1. **Our own item** — `langbar AddItem hr=0x80004005` on every activation,
   including the post-reboot re-registered build: the item lane stays CLOSED
   (already proven environmental by the reference probe).
2. **The menu extension** — `sysmenu advise cls=34745C63 hr=0x0` and
   `cls=ED9D5450 hr=0x0`: the advice SETTLES on the host thread's system
   items (the other two answer `0x80040202`, a single-sink limit). BUT the
   field test says the tray's right-click menu shows NO "Hunmin 설정 열기":
   the shell's tray menu is built by the shell from the language list, not
   from the TSF item's column of sinks; the advised menu would render only
   inside the floating language bar UI, which this build does not display in
   embedded mode. Mechanism measured: advice lands, affordance does not.
3. **The tray badge** — the compartment write runs (no failure lines), and
   the UIA read of the tray's indicator name answers
   `SystemTrayIcon | 트레이 입력 표시기 한국어 Hunmin … 입력 방법을 전환하려면
   Windows 키 + 스페이스를 누르세요.` — the accessible name carries the
   LOCALE and the IME identity but NO han/latin mode state: the shell does
   not read a third-party TIP's conversion compartment for its badge on this
   build. The write is correct TSF and costs nothing; it stays.

**Standing state for milestone D's closure:** the mode display and the
right-click entry are NOT reachable on Windows 11 25H2 (26200) through any
legal TSF/langbar surface; the reachable, verified surfaces are the
Start-menu entry, the language list's Sejong icon (IconIndex=0 fixed), the
interim dialog's hook capture, and the WinUI 3 exe. Post-install
re-measurement travels with issue #99 (the comment lists these rows).

## Test procedure (elevated steps: the maintainer's UAC)

0. **Load the current build.** Run (or accept the UAC for)
   `C:\Users\pc\AppData\Local\hermes\cache\scratch\reinst2-admin.bat`:
   it unregisters and re-registers the current
   `frontends/tsf/build-msvc/Release/hunmin-tsf.dll`, then restarts ctfmon
   (the loaded TIP bytes refresh only on new host processes — start hosts
   AFTER this step). Verify: `reg query "HKLM\SOFTWARE\Microsoft\CTF\TIP\{F2C70001-0000-0000-0000-000000000001}"` shows Enable=1; and the
   `regexit=0` result line lands in the scratch file.

1. **Open a text host with the debug log on.**
   `cmd /c "set HUNMIN_TSF_DEBUG_LOG=1&& start wordpad"` — WordPad is the
    RichEditD2DPT host measured all session; the Store notepad does not
    attach the TIP the same way (measured). Switch into Hunmin (Win+Space)
    and click into the text area.

2. **Step: the ADD failure is recorded, not silent.**
   `tail frontends/tsf/build-msvc/Release/hunmin-tsf-debug.log`:
   - `langbar QI hr=0x0` — the real kernel is present.
   - `langbar AddItem hr=0x80004005` — the measured refusal; the badge/button
     item cannot show through OUR OWN item on this build. GO ON to test the
     other two mechanisms.
   - `sysmenu advise items cls=XXXXXXXX hr=0x…` — the extension advice:
     `0x0` = the system item accepted our menu entry (step 3 then has a
     landing); `0x80004005/80070005` = refused on this thread.

3. **The right-click menu (mechanism 2).** Right-click the tray input
   indicator AND right-click any visible langbar button: look for
   **"Hunmin 설정 열기"**.
   - Clicking it must launch the **WinUI 3** settings exe (its window: layout
     lists + 한/한자 키 capture rows); if the exe is not deployed next to the
     DLL the trace says `launch settings exe=` — the interim rundll32 dialog.
   - No entry + an advise hr above → the host build refuses extensions:
     mechanism 2 is closed on THIS build and the Start-menu entry remains
     the guarantee.

4. **The tray badge (mechanism 1).** With the caret in the WordPad text:
   toggle the mode (the default bare Han/Eng key, or the configured toggle):
   - Expected: the tray's Hunmin entry swaps 한 ↔ A **within the same
     profile** (no Win+Space reselection; the compartment is re-read on the
     tray's next render).
   - If the tray badge does not follow: the third-party TIP's tray pick-up is
     NOT honored on this build (the unverified half stays unverified locally)
     — record one line on issue #99 and stop; the compartment WRITE is
     correct TSF behaviour either way and costs nothing.
   - Windows renders tray badges on FOCUS events too: focus another window
     and back once before concluding.

5. **The interim dialog's capture (independent of the above).**
   `rundll32 <dll>,HunminSettingsShow` — 키 입력 arms ("키를 누르세요…"),
   press a chord (physical 한/한자 record as 25/19!), ESC restores. This is
   the worked-surface fallback for BOTH --type the string and capture paths.

## Decision table

| Step result | Meaning | Next |
|---|---|---|
| 2: AddItem 0x80004005 | the item lane refused (2026-10-18 shape; the RE-shaped item is measured in `tsf-langbar-item.md`, not here) | test 3/4/5 |
| 3: no entry + advise OK | the system item's menu didn't display our entry (WHY the extension lane is removed) | board row TSF-SYSMENU-EXT-67 |
| 3: entry visible | mechanism 2 lives HERE | record on issue #67's comment lane + #99 |
| 4: badge flips | mechanism 1 pick-up CONFIRMED on this build | milestone D's mode display is DONE |
| 4: badge static + focus-flip retry | not honored for third-party TIPs on this build | record on #99; the compartment write stays (costs nothing, correct TSF) |
| 5: capture works | the interim surface is complete | - |
