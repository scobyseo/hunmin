# Windows TSF API notes (reference)

Provenance: summaries quoted from the Microsoft Learn TSF reference
(https://learn.microsoft.com/en-us/windows/win32/tsf/text-services-framework) and
its child pages, and the API catalogue at
https://learn.microsoft.com/en-us/windows/win32/api/_tsf/ (retrieved
2026-10-18 during the issue #67 work). This page is read-only reference for
future work on `frontends/tsf`; it states what the platform offers, not what
the adapter does — the adapter's current shape and the open migration are
tracked in the issue referenced at the bottom.

## Headers of the API surface (https://learn.microsoft.com/en-us/windows/win32/api/_tsf/)

The whole technology lives in seven headers; the ones this adapter already
touches are marked:

- `msctf.h` — the manager, contexts, key sinks, composition, edit sessions,
  profiles (in use);
- `ctffunc.h` — the function-provider family: `ITfFnConfigure`,
  `ITfCandidateList`, `ITfFnSearchCandidateProvider` (in use @ #71/#67);
- `ctfutb.h` — the language bar: `ITfLangBarItemButton`, bar item info (in
  use: the settings button added with issue #67);
- `textstor.h` — the application side of the same protocol, `ITextStoreACP`
  and the `Ts*` anchor/gravity types;
- `inputscope.h` — `InputScope` field classification (Notepad/Word fields
  advertise these; candidate-window behaviour may branch on them);
- `msctfmonitorapi.h` — TSF monitor/status API;
- `ctfspui.h`, `msaatext.h` — speech/special-purpose providers (not used).

## What each surface buys, in one paragraph each

- **Text services & applications.** TSF is the system service for advanced
  text input; a TIP (text input processor) is a COM in-proc server activated
  by the TSF manager (msctf) inside the host process
  (https://learn.microsoft.com/en-us/windows/win32/tsf/text-services-framework).
  Hunmin is a TIP (`ITfTextInputProcessor`), registered under its own CLSID
  and a 0x0412 (Korean) language profile — see WINDOWS-TSF-REGISTRATION.md.
- **Compositions.** "A text service creates a composition by calling
  `ITfContextComposition::StartComposition`. … `StartComposition` returns an
  `ITfComposition` object that the text service keeps a reference to and uses
  to modify and terminate the composition. The text service terminates the
  composition by calling `ITfComposition::EndComposition`"
  (https://learn.microsoft.com/en-us/windows/win32/tsf/compositions). The
  service may implement `ITfCompositionSink` to receive start/update/end
  notifications.
- **Text stores / edit contexts.** All text moves through `ITfContext`
  ranges: `ITfRange::SetText`, `GetSelection`/`SetSelection`, edits only
  inside `RequestEditSession` (`TF_ES_SYNC`/`TF_ES_READWRITE`) edit sessions.
  The adapter already works this way for committed text.
- **Display attributes (the inline preedit styling mechanism).** To let the
  *application* render undetermined text (underline, colour):
  1. `ITfCategoryMgr::RegisterCategory` with `GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER`
     (https://learn.microsoft.com/en-us/windows/win32/tsf/providing-display-attributes);
  2. implement `ITfDisplayAttributeProvider` + `IEnumTfDisplayAttributeInfo` +
     one `ITfDisplayAttributeInfo` per attribute (`TF_DISPLAYATTRIBUTE`
     struct: text colour, background, underline style/colour/weight);
  3. during a read/write edit session: take the `ITfRange` covering the
     composition text, `ITfContext::GetProperty(GUID_PROP_ATTRIBUTE)`,
     `ITfCategoryMgr::RegisterGUID` → `TfGuidAtom`, then
     `ITfProperty::SetValue(ec, range, vtI4Variant)`.
  The Learn page carries the full `CEditSession::DoEditSession` example.

## Why native composition preedit beats the overlay window

The current adapter draws the in-flight preedit on its own topmost overlay
window, positioned from the caret rect. The platform's way (`ITfComposition`
text + a display attribute) puts the preedit INTO the app's document: it
inherently follows the app's fonts, zoom, and text flow, needs no caret-rect
races, and any TSF-aware host (Notepad, Word, terminals hosting TSF) renders
it. The overlay window remains needed only for cases the composition cannot
cover — e.g. apps without TSF support, or the candidate popup itself, which
is not in-document text at all.

## Other interfaces this adapter already exercises

### The langbar item as the mode indicator + config menu (ctfutb.h, issue #103)

**The item was re-shaped (2026-10-20, maintainer steer: the langbar item, not
the system-langbar lanes).** The 2026-10-18 measurement — every `AddItem`
answering `0x80004005` for our item (guidItem F2C70003, BTN_BUTTON alone)
and for a reference probe from a plain thread — recorded the ENVIRONMENT
(blurred earlier); the re-measured sibling fact on the SAME host: a TIP
whose item carries `guidItem = GUID_LBI_INPUTMODE`
({2C77A81E-41CC-4178-A3A7-5F8A987568E6}), style `BTN_BUTTON | SHOWNINTRAY`,
and an implemented `ITfSource` IS accepted by `AddItem` (`hr=0x0`) on this
host. What `AddItem` accepting it does NOT establish is that the Win11 tray
indicator renders or routes clicks to it — measured 2026-10-23, it does not
(see the last bullet). That recipe is the adapter's item now, with one
ruling change (2026-10-22): style `BTN_MENU | SHOWNINTRAY`, no `BTN_BUTTON`:

- `guidItem` MUST be `GUID_LBI_INPUTMODE` — the Win8+/Win11 "input
  indicator" hosts a langbar item only with this GUID; other item GUIDs are
  ignored by the shell's hosted-item path.
- The item exposes `ITfSource::AdviseSink/UnadviseSink` (language-bar.md):
  the language bar installs its `ITfLangBarItemSink` ONTO the item at
  `AddItem`; the item notifies `OnUpdate(TF_LBI_ICON | TF_LBI_TEXT)` at
  every mode boundary so the 한/A badge refreshes without a host restart.
- With `BTN_MENU`, the menu-button routing calls `InitMenu`/`OnMenuSelect`:
  the one-entry config menu ("Hunmin 설정 열기") →
  `hunmin_tsf_launch_settings` (the settings-ui exe). `OnClick` stays
  implemented but is not advertised (the former `BTN_BUTTON` recipe delivered
  left and right clicks as `OnClick(CLK_LEFT / CLK_RIGHT)`).
- `hunmin_tsf_launch_settings()` takes no anchor and resolves the module from
  its own address. The former `void *anchor` parameter let the
  `OnMenuSelect` path pass null, so `GetModuleHandleExW(FROM_ADDRESS, null)`
  failed and the item returned `E_FAIL`: the menu entry appeared but launched
  nothing (found and fixed 2026-10-23; regression
  `TsfLangbarItem.OnMenuSelectOneLaunchesTheSettingsSurface`).
- **The item IS hosted, and the bar renders it (measured 2026-10-29, first
  time: issue #68).** A live Notepad round on the standalone build logged the
  whole install from `Activate` and, immediately after `AddItem`, the bar's own
  render queries:

  ```
  langbar QI hr=0x0
  langbar GetInfo style=0x20002 guid=2C77A81E
  langbar AdviseSink riid=57DBE1A0      <- IID_ITfLangBarItemSink: HOSTED
  langbar AddItem hr=0x0
  langbar GetText hangul=1
  langbar GetIcon hangul=1              <- RENDERED
  ```

  A Han/Eng toggle then re-queried `GetText`/`GetIcon` with `hangul=0`, so the
  한/A badge follows the mode with no host restart. `AdviseSink` is the
  observable language-bar.md names for "the item is installed"; it had never
  been logged before, which is why `AddItem hr=0x0` alone could not settle
  whether the bar hosted the item.
- **What the Win11 indicator actually paints is the PROFILE, not the item
  (measured 2026-10-29).** The tray holds one button per language profile:
  `트레이 입력 표시기 한국어 / Hunmin / 입력 방법을 전환하려면 …`
  (`SystemTray.NormalButton`, 55x60 px) — no per-item slot, and no CTF-hosted
  window of its own, which is why the 2026-10-21 UIA window scan read "nothing"
  and was taken for "not hosted". The item's rendering is consumed by the
  language-bar surface behind that button; its click is still the shell's
  (below), so the two facts stand together: hosted and rendered, not clickable.
- **The install lives in `UpdateLangBarItem(ITfThreadMgr*)` (ruled
  2026-10-29).** That function creates the item, obtains `ITfLangBarItemMgr`
  with `ITfThreadMgr::QueryInterface(IID_ITfLangBarItemMgr)` and calls
  `AddItem`; `Activate` and every mode boundary call it, and `Deactivate`
  obtains the manager the same way for `RemoveItem`. The item no longer
  resolves a manager of its own, and it is no longer built in the constructor —
  nothing can QI it before the service installs it.
- **The Win11 tray indicator routes no click to the item (measured
  2026-10-23, unchanged).** With Hunmin active the indicator's left click opens
  the shell's language list and its right click dismisses that list; neither
  `OnClick` nor `InitMenu` is called. Adding `BTN_BUTTON` to the ruled style did
  not change it, so the extra bit was reverted. The MS IME's `한국어 /
  Microsoft 입력기` tray icon is the IME's own `Shell_NotifyIcon`, not a TSF
  item; the Win11 settings entry needs a different carrier.

**Removed (2026-10-20, "it confuses"):** the `ITfSystemLangBarItemSink`
system-menu extension (its advice had settled on this host's system items
yet the shell tray menu never rendered an extension entry — the tray menu
is built from the language list, not from TSF item sinks) and the own
`Shell_NotifyIconW` tray badge (PR #124 rounds 1-3, the overflow-visibility
iteration stopped). The compartment write stays — correct TSF, zero cost,
honoured wherever a build reads it.


- `ITfThreadMgrEventSink` (activation/deactivation, context push/pop),
- `ITfKeyEventSink` + `ITfKeystrokeMgr::AdviseKeyEventSink` (key routing,
  preserve-on-stale via `ITfKeystrokeMgr::SetPreserveKey`),
- `ITfCompositionSink` (EndComposition on external control of the context),
- `ITfFunctionProvider` → `ITfFnConfigure` (the settings surface, issue #71),
- ctfutb `ITfLangBarItemButton` (language-bar settings button; added with
  issue #67's discoverability work),
- `ITfCategoryMgr` register/unregister of categories and profiles.

## Migration sketch (for the separate tracking issue)

1. First preedit key enters a foreign scope: `RequestEditSession` →
   `ITfContextComposition::StartComposition(esi.empty range, this, &pos)`.
2. Every subsequent engine transition rewrites the composition range text
   (`ITfComposition::GetRange` → `SetText`) inside the same edit session, and
   sets `GUID_PROP_ATTRIBUTE` over the range so the app underlines it.
3. Commit: EndComposition, final `SetText` of the chosen character/word.
4. Cancel/backspace-to-empty: EndComposition then delete the range.
5. Overlay window stays only as the candidate-window renderer and as the
   non-TSF-app fallback path.

## Status

- Overlay-window preedit is the shipped behaviour of 0.2.0 (issue #67).
- The native-composition preedit migration is tracked as its own issue:
  https://github.com/scobyseo/hunmin/issues/98 — do not fold it into a
  settings/candidate PR.
