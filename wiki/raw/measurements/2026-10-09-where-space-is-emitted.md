---
title: Measurement — where the space character is emitted, and the reported symptom does not reproduce
created: 2026-10-09
updated: 2026-10-10
type: measurement
status: active
ingested: 2026-10-09
tags: [measurement, space, fcitx5, android, testing, correction]
sources:
  - ../../../tests/e2e/run-d3.sh
  - ../../../tests/e2e/cases.tsv
  - ../../../tests/e2e/xim-harness.c
  - ../../../docs/TESTING.md
  - ../../../tools/android-e2e.sh
confidence: high
---

# Measurement — where the space character is emitted

**What this is.** The record behind leaving `libhunmin/data` untouched. A session
on 2026-10-07 reported that pressing space completed the composition but did not
type the space, and the fix applied for it — adding `space`/`enter`/`backspace`
to all 13 layout key tables — broke every libhangul parity test. This
re-measures the symptom on both frontends, on the tree as it stands, and records
that it does not reproduce.

The requirement, in the maintainer's words:

> "스페이스 입력 시에 조합 완성이 되고 스페이스도 함께 입력되어야 하는데, 조합
> 완성만 되네. fcitx에서도 같은 이슈가 있는지 확인해야해."

Two things, not one: the space must **end the composition** *and* the space
**character must be emitted**. Which layer owns the second half was the open
question. **Whether it has since been answered as a *decision* is a separate
matter and does not agree with the measurement** — see
[[space-emission-ownership]], which records the disagreement and resolves
nothing. What follows is only what was measured.

## The requirement in the engine's terms

Tier A already answered half of it, and said so plainly once asked:

```
$ hunmin-sim --layout dubeolsik --keys 'dkssuds ' --trace
{"in":{"ch":" "},"commit":"ㄴ","preedit":"","consumed":false}
```

`consumed: false` — the engine declines the space. It flushes the in-flight
syllable (`commit: ㄴ`) and emits nothing for the key itself. So the character
cannot come from the engine; it has to come from the input method handing the
key back to the client. That was available before anything was measured on a
device.

`consumed` is emitted by this measurement's own change — until then
`hunmin_ic_process`'s return value was discarded and consumers re-derived it
from the layout's key table (`tools/gen-conformance.py:221`).

## Android — measured, the symptom does not reproduce

`tools/android-e2e.sh` installs the debug APK, types `dkssud` and reads the
field back from the platform's own view hierarchy with `uiautomator dump`:

| keys | field reads | |
|---|---|---|
| `dkssud` | `ㅎ곈디` | matches tier A |
| `dkssud` + SPACE | `ㅎ곈디<space>` | composition ended **and** space emitted |
| `rk` + SPACE | `딭ㄱ<space>` | same |
| SPACE, empty buffer | `<space>` | emitted |
| DEL | deletes one character | reaches the client |

This is the tree **with the layout-data change reverted**. Both requirements
hold without it.

## fcitx5 — measured, and the harness could not have measured it

The maintainer asked for fcitx5 to be checked as well. The 2026-10-07 session
marked that item done; it had read `frontends/fcitx5/src/engine.cpp` and
inferred. No run. So it was run here.

`tests/e2e/run-d3.sh` (ctest `engine-d3-x11`) is the full stack: stock distro
fcitx5 under Xvfb, keys injected with XTEST, commits collected from a real XIM
client. On `dkssuds<space>`, dubeolsik:

```
ok: [dubeolsik/dkssuds ] -> COMMIT:안|COMMIT:녕|COMMIT:ㄴ|FORWARD:<space>
```

Same shape as Android: `COMMIT:ㄴ` is the composition ending, `FORWARD:` is the
character arriving at the client.

### The harness was blind, and said so in a way that looked like a finding

`tests/e2e/cases.tsv` carried this rule:

> "no pass-through keys (no space / unmapped rows) so the XIM COMMIT stream
> equals the sim commit stream"

That reads as a property of the design. It was a property of the harness.
`tests/e2e/xim-harness.c` registered only `commit_string` and `disconnected`,
so dkit's `forward_event` callback was NULL and `clientprotocolhandler.c:502`
discarded every forwarded key without noticing — the tier whose job is the full
stack could not see the one key whose ownership was in dispute. A claim in
`docs/TESTING.md` that consumption was *"unobservable from an XIM client at any
level"* was corrected: it was unobservable **from our harness**.

**This is the mistake worth not repeating.** A measurement gap was recorded as a
property of the thing being measured, in a comment, in the file that would have
caught it. The rule looked like a considered design decision and would have been
inherited by the next session as one. The tell was available at the time and was
not used: a harness that cannot see a key cannot be the evidence that the key
behaves correctly.

## What was wrong on the way to the number

All three in code written for this measurement, all found by the result being
implausible rather than by reading:

- **`XIM_FORWARD_EVENT` is overloaded.** fcitx5 5.1.19 has two paths that send a
  key back to an XIM client: `src/frontend/xim/xim.cpp:664` forwards what the
  IM declined, and `src/lib/fcitx/instance.cpp:1672` re-forwards an *accepted*
  key for `KeyEventOrderFix`. The name suggests only the first. The second
  cannot reach an XIM client — gated on `CapabilityFlag::KeyEventOrderFix`,
  which the XIM server never sets (`xim.cpp:270`) — but that was verified, not
  assumed, because the first run showed a forward for every *consumed* key.
- **Every key produced two forwards.** `xtest-type.py` sends a press and a
  release; hunmin declines every release; a release inserts nothing. Measured:
  `jk1` produced exactly three forwards for three keys — the release count.
- **`FORWARD:<empty>` before `COMMIT:각`** in sin3-p2 `kfcQ`. That was
  `Shift_L`, sent around an uppercase key by the injector, not `Q` at all: a
  keysym with no codepoint.

## What is asserted, and what is not

`FORWARD:` asserts two things and it is worth being exact about which:

1. **Measured, ours:** the input method handed the space to the client.
2. **Modelled locally:** the character. A real toolkit answers a forwarded key
   by running it through its own default input context, which inserts it. The
   harness does that. Where the space becomes text on a real desktop is
   GTK/Qt's behaviour — measured for GTK in
   [[2026-10-09-gtk-forward-path]].

So the boundary of this measurement is drawn at the toolkit, and it is drawn on
the harness in the open rather than hidden inside it.

## Not established here

- **Non-XIM fcitx5 frontends.** GTK is measured (next page). **Qt and Wayland
  `text-input-v3`** reach the same `InputContext::forwardKey` but convert it by
  their own means, and must not be assumed to behave like XIM.
- **Whether space *should* be the engine's to emit.** This record establishes
  that it is not, at HEAD, for want of a layout mapping — not that the design
  is right.
- **The voice question**, asked in the same 2026-10-07 message
  ("아직 음성 인식 쪽은 동작하지 않나?") — **answered since**, in
  [[2026-10-09-voice-recognition-status]].
- **13 tier-C tests fail** for want of `.gold` files, from a data-only change.
  Untouched here; they were failing before this measurement, and
  `docs/REMAINING-WORK.md` §0 carries the current state.