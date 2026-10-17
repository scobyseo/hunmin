---
title: "Settings dialog — WinUI 3 exe (issue #71)"
created: 2026-10-17
updated: 2026-10-17
type: session
status: active
ingested: 2026-10-17
tags: [session, windows, tsf, settings, winui3, issue-71]
sources:
  - ../../../docs/WINDOWS-TSF-CONFIG.md
  - ../../../frontends/tsf/settings-ui/
confidence: high
---

## What the ruling was, and what ended up executing it

[[2026-10-17-milestone-c-layout-wiring-data-loading]] carried interim work: a
plain-Win32 dialog behind `ITfFnConfigure` (commit 89ea318). The maintainer's
ruling 2026-10-17 resolved the A/B options with **"B — separate WinUI 3
settings exe via Windows App SDK (true Fluent; ~50MB+ installer weight;
milestone E packaging implications)"**, and two later instructions shaped the
exe: the language-bar path surfaces **the exe only**, and both surfaces get
**compact** — "configuration window displayed while developing seems too
large; make both compact".

## Measured facts this session produced

- The interim Win32 dialog — measured while developing — was 493x434 px on
  screen (client 472x385) from a 216x164-DU template; that is where the
  "too large" came from. The compact WinUI exe lands at **545x674 px**
  (client 525x655) via `AppWindow.ResizeClient` applied AFTER the first
  layout pass and calibrated by measurement: on this host the call's input
  scales by the display's DPI factor ((500,600) request → 625x750 client
  px), so a 436x500 request is what lands inside the quoted ~480-560 px
  band. `GetDpiForWindow` still reports 96 — the two numbers disagree on
  this host, and the recorded measurement is the one that counts.
- The publish output, for the #68 weight figure: **495 files, 167.4 MiB**
  self-contained (`WindowsAppSDKSelfContained` + `SelfContained`). The
  ruling's "~50MB+" is confirmed and exceeded by the runtime-carrying
  layout.
- Toolchain quirk worth keeping: `dotnet build` fails with a silent
  XamlCompiler MSB3073 on this host while `MSBuild.exe` surfaces the real
  XAML diagnostics (first real error: `RequestedTheme="Default"` is not an
  `ApplicationTheme` value). A second one: a `ResizeClient` call made during
  layout ended in heap corruption (0xC0000037); post-Activate through the
  DispatcherQueue it is stable.
- The mode-toggle row is ONE binding set by key capture (ruling quoted:
  "한영 전환 키는 하나만 선택해도 되고, 복잡하게 입력하는 게 아니라 키 입력
  방식을 사용하자") — ESC cancels, repeat-bit swallows auto-repeat,
  modifier-only presses are rejected (Android's isModifierKey mirror), and
  the store keeps its `v1:<vk>:<mods>` ';' list grammar with a single entry.
  The adapter's implicit bare Han/Eng platform alternate is untouched: the
  physical key cannot be captured away.
- Exe smoke measured with UIAutomation on this host (registry restored
  after): layout list split by mode (12 hangul + 3 latin rows), capture
  F11 → label `F11 (v1:122:0)`, Apply → `reg query` shows the new
  HangulLayout and `v1:122:0`, relaunch shows both, close exits 0.

## What remains open (not decided here)

- The `ITfFnConfigure::Show` host path needs a registered TIP and a real
  text host to measure the block-on-process behaviour end to end — the
  resolution/exe-only fallback log lines exist and are read from code, the
  language-bar scenario is device-bound.
- The interim Win32 dialog stays reachable via show-settings.bat until its
  removal commit; that commit decides whether the template re-lays to the
  maintainer's quoted ~340x240 DU (a LARGER template than the current
  216x164 DU, smaller on-screen than the exe band? left open with the
  measured 493x434 px recorded).
- Whether ~167 MiB is acceptable for the 0.2 lane packaging (milestone E,
  #68) is the maintainer's sizing decision; the number is recorded where
  the exe is documented (docs/WINDOWS-TSF-CONFIG.md).
