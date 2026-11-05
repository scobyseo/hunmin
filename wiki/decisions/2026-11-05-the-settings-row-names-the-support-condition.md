---
title: The settings row names the support condition, 2026-11-05
created: 2026-11-05
updated: 2026-11-05
type: decision
status: active
tags: [decision, modes, tsf, settings, delivery, r13]
sources:
  - ../../docs/FRONTEND-EQUIVALENCE.md
  - ../../frontends/tsf/src/settingsrows.cpp
  - ../../frontends/tsf/test/tsf-settings-test.cpp
confidence: high
---

# The settings row names the support condition, 2026-11-05

**Ruling (2026-11-05, issue #215).** The maintainer chose the qualified-label
shape, with this wording verbatim:

> 자동 한/영 전환 (지원 앱에서만, Ctrl+한/영)

## Why this shape and not a host state

- **The dialog cannot know the delivery.** It is a separate process with no host
  handle, and delivery is TSF-frontend state — the engine has no such concept
  (`grep -rn "delivery\|interim\|legacy" libhunmin/src libhunmin/include` finds
  only unrelated prose). The store is read at `Activate()`, not by the dialog.
- **The standalone path has nothing to ask.** `ITfFnConfigure::Show` runs in a
  live host and knows its own `legacyMode_`, but the rundll32/`show-settings.bat`
  launcher has no TIP at all. Any answer has to be true there too.
- **"What the last run did" is not one fact.** Each host process detects its own
  delivery; a single recorded value would describe one host and be unknown
  before any host has run — a second home that cannot agree with every reader.

So the label states a condition that is true wherever it is read: the switch is
honoured only in supporting apps. It never claims to know the current host.

## What it settles

- **The available label** is `자동 한/영 전환 (지원 앱에서만, Ctrl+한/영)`.
- **The data-missing suffix is unchanged**: `… — 사전 없음: 사용할 수 없음`.
- **The fcitx5 and Android rows stay `자동 한/영 전환`**: they have no interim
  delivery and their chord is already correct.
- **The adapter's state is not touched.** `CTextService::ApplyAutoswitch()` still
  answers the stored value AND the native delivery; this ruling is about what the
  dialog may say, not what the host does.

## What it does not decide

- **Whether a future interim shape could carry the switch.** That stays open in
  `docs/FRONTEND-EQUIVALENCE.md` §2.1, as before.
- **The WinUI settings surface.** This ruling governs the interim dialog's row,
  which is the one wired to `hunmin_tsf_autoswitch_row`. The ruled WinUI exe
  still carries the disabled `자동 한/영 전환: 미정 (0.3)` placeholder and is not
  changed here.

## Related

The delivery ruling this qualifies is
[[2026-11-02-interim-cannot-support-autoswitch-and-on-stops-passthrough]]. The
state it displays remains the engine's, per
[[2026-11-01-the-engine-judges-the-frontend-switches]], and the choice of a
measurement-bearing page rather than a verdict follows
[[decisions-belong-to-the-maintainer]].
