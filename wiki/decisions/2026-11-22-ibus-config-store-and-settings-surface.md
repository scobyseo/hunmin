---
title: "IBus settings: five rows, a GSettings store, a separate setup app — ruled, not built"
created: 2026-11-22
updated: 2026-11-22
type: decision
status: active
tags: [ibus, settings, config, decision, fe17, issue-327]
sources:
  - ../../docs/OPTIONS-MEASURED.md
  - ../../docs/SPEC-FRONTEND.md
  - ../../docs/REQ-MAP.md
  - ../../frontends/ibus/src/hunmin_engine.c
  - ../../frontends/ibus/src/hunmin_state.h
  - https://github.com/scobyseo/hunmin/issues/327
confidence: high
ingested: 2026-11-22
---

# IBus settings: the ruling, and the measurement that came after it

## The ruling (maintainer, 2026-11-22)

> 3 -> a로 설정할 수 있어야 함.
>
> 나머지는 추천대로 갈게.

Read against the decision list put to him that session, this settles six things at once:

1. **The five rows, all settable** — Hangul layout, Latin layout, commit unit, automatic
   switch, and the language-key list. The fifth row was the contested one; storing the other
   four was never in doubt.
2. **Store: a GSettings schema** (`org.freedesktop.ibus.engine.hunmin`).
3. **Surface: a separate setup binary**, launched from the panel's Preferences through the
   component XML's `<setup>`, with key *capture* for the fifth row rather than a typed keysym.
4. **The fifth row also edits the daemon's trigger list** (`org.freedesktop.ibus.general.hotkey
   trigger`), so the row means "the 한/영 key is this" rather than "of the keys the daemon
   forwards, these switch".
5. **Live apply**: a stored change reaches the running engine without a restart.
6. **A separate binary package** for the surface, so an IBus-only install gains no toolkit.

The requirement is `docs/SPEC-FRONTEND.md` §FE17 and its `docs/REQ-MAP.md` row; the costs, the
five priced options and the measurements behind each are `docs/OPTIONS-MEASURED.md` §IBC. No
requirement text is repeated here.

## What made the options differ

Not preference — three measurements.

- **Capture decides the shape, not the line count.** `tools/check-frontend-invariants.py`
  reports the IBus frontend at 698 of FE11's **1000** code LOC and the fcitx5 addon at 715 (the
  limit was raised from 700 on 2026-11-22, issue #322). An earlier reading of this session put
  the headroom at two lines and made the budget the reason for a separate process; that figure
  came from a worktree seven commits behind `origin/master`, and it is wrong. The reason
  survives the correction for a different and better one: 3(a) makes the fifth row a *captured*
  key, and a capture widget is presentation code, which does not belong in the file FE11
  measures however many lines are free.
- **IBus's own config service does not work on the development distro.** It is shipped on the
  release target (jammy's `ibus` 1.5.26-4 carries `ibus-dconf` and `ibus-memconf`; its
  `libibus-1.0-dev` carries `ibusconfig.h`), but on this host's 1.5.34 no process ever owns
  `org.freedesktop.IBus.Config` — measured twice under a private `dbus-run-session`, with an
  explicit `-c /usr/libexec/ibus-memconf` and with `-c default`. A store that works only on the
  target is a store whose failures are invisible where the code is written.
- **The fifth row is not free on IBus.** `gsettings get org.freedesktop.ibus.general.hotkey
  trigger` lists `Hangul`, so on an XKB `kr` layout the daemon takes the key before the engine
  sees it. This is why item 4 above had to be part of the ruling rather than left to
  implementation.

## The cost the ruling accepted, measured afterwards

`docs/OPTIONS-MEASURED.md` §IBC-5. A dconf-backed store **cannot be isolated from a test by
environment**: a `dconf write` inside a private session bus with a redirected `XDG_CONFIG_HOME`
lands in the developer's own `~/.config/dconf/user` (proved by reading that database back), and
a named database in `DCONF_PROFILE` aborts `dconf` outright. `GSETTINGS_BACKEND=memory`
round-trips only within one process.

So the verification splits: schema, defaults, round trip, the `changed` signal and live apply
are one in-process ctest binary on the memory backend; **persistence across an engine restart
is a CI-only case**, because a runner's dconf is throwaway and a developer's is not. This is the
one place where the repository's rule that a test may not rewrite your tree and the chosen store
pull against each other, and it is recorded rather than smoothed over. If it becomes
unacceptable, the alternative is an ini store plus an environment variable (§IBC-2 option C),
which makes both halves in-process and gives up the schema, the `gsettings` CLI and the
packaging trigger.

## Built, same day

- **The store** (`frontends/ibus/src/settings.c`, `engine-d1-ibus-settings`) and **the window**
  (`frontends/ibus/settings/main.c`, opened from the panel's Preferences through the component
  XML's `<setup>`). Five rows, no OK/Apply, key capture rather than a text field.
- **The fifth row edits the daemon's trigger list** (`store/store.c`): the row's keys leave
  `org.freedesktop.ibus.general.hotkey.trigger` and what was taken is recorded in the schema's
  `managed-triggers`, so it comes back when the key stops being ours. Matching is by keysym, which
  is what makes it work — the daemon's entry for the physical key on XKB `us` is
  `Alt+Release+Alt_R` while the row stores `Alt_R`.
- **The store's plumbing moved to `frontends/ibus/store/`**, shared by the engine and the window,
  leaving `src/settings.c` as the store→engine mapping. Recorded with the measurement (827 of
  1000) because "the number went down when the code moved" deserves to be visible, not tucked away.
- **The default layout is `sin3-p2`**, the same as fcitx5 and TSF. The first attempt got this
  wrong in an instructive way: thirteen tier-D2 cases went red because that tier had been
  inheriting the shipped default, and reverting the default was the wrong repair — the maintainer
  caught it ("sin3-p2에서 테스트가 깨지면 안되는데?"). The fixture now states the layout its key
  sequences belong to, and exactly one case pins the default. Both directions measured.

## Installed and in use (2026-11-22)

The maintainer built the five packages with `tools/build-release.sh deb`, installed them with
`dpkg -i`, and the engine is the ACTIVE one in his session (`ibus engine` → `hunmin`, an engine
process alive on the real session bus, `preload-engines` = `['hunmin']`). His own verdict, after
typing in it: "그냥 잘 동작하고 있으면 괜찮아. 漢字입력도 잘 되네." Hanja input is the part that was
rebuilt inside this frontend rather than inherited from fcitx5, so that sentence is the only
evidence FE16 and the Hanja path have from real use.

Two things the live session showed that the test lanes could not:

- **The fifth row's effect is observable in the daemon's database.** With `switch-keys = ['Alt_R']`
  the engine took `Alt+Release+Alt_R` out of `org.freedesktop.ibus.general.hotkey.trigger` and
  recorded it in `managed-triggers` — the behaviour §IBC-4 describes, happening on a real session
  rather than in a memory backend.
- **The D3 lane went green under it.** `engine-d3-ibus` was red on this host for a
  reason of its own (issue #325's two ibus-host-only defects: the truncated `{F9}`
  substitution producing `actual: TEXT:안녕ㄴ}`, and the expectation dropping the
  trailing in-flight word). After rebasing onto #325's fixes it passes here (40.3 s),
  so the claim "a local red is not evidence about the settings code" is now history
  rather than a live excuse. The lane still runs the engine with
  `GSETTINGS_BACKEND=memory`, so what it does not assert is the store's effect on a
  real database — that half is the live-session observation below.
- **A narrowed row is a trap on the other layout.** That same row leaves `Hangul` in the daemon's
  list, so on XKB `kr` — where the physical key reports `Hangul` (0xff31) — the daemon consumes
  it and the engine never sees it. It works today only because the session's layout is `us`
  (`Alt_R`, 0xffea). The default's two entries are the fix; recorded so a bug report about "한/영
  does nothing after I changed the key" has an answer.

## Still open

- **#327 stays open.** A ruling authorises implementation and verification. The verification now
  has three cases: the store (`engine-d1-ibus-settings`), persistence into the next process
  (`engine-d1-ibus-settings-persist`, on a scratch keyfile store), and the window's rows and
  capture/remove behaviour (`engine-d2-ibus-settings-ui`). The window's pixels are still a manual
  check, and the shipped **dconf** backend itself is unasserted — keyfile buys the isolation, and
  §IBC-5 was corrected to say the narrower thing.
- **The trigger-list edit is built, and its effect on a real session is not measured here**: no
  interactive IBus session was driven, so "한/영 now reaches the engine on `kr`" is the code's
  shape plus the schema write, not an observed keystroke. `engine-d3-ibus` fails on this host
  before the settings work too (measured at `2bdde6d`), so it cannot be the evidence.
- **The key-list grammar** — keysym names, or `modifier+keysym` as IBus spells its own hotkeys —
  is not settled. It is a design detail inside FE17, and it has to be written before the capture
  widget exists.
- **Capturing a key on X11** in a setup process is not measured here: TSF15's rule (capture,
  never type) comes from a Windows host, and its X11 analogue is unproven in this tree.
- **FE11's reading** — whether a second frontend's code counts against its own 700 or the Linux
  frontend is one budget — is still open, and this ruling does not touch it.

## Pages

- [[2026-11-09-the-22-04-release-ships-ibus-only]] — the ruling that made the IBus frontend the
  22.04 artefact, and the packaging shape this surface's own package follows.
- [[2026-11-22-ibus-gloss-option-a]] — the same session's other IBus decision: option A for the
  candidate gloss, and the precedent for recording a ruling with the maintainer's words.
- [[decisions-belong-to-the-maintainer]] — why the ruling is quoted and the analysis is not a
  verdict.