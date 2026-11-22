---
title: The Ubuntu 22.04 release ships an IBus engine only; fcitx5 is given up for that target
created: 2026-11-09
updated: 2026-11-22
type: decision
status: active
tags: [decision, linux, frontend, packaging, ibus]
sources:
  - ../../../docs/OPTIONS-MEASURED.md
  - ../../../docs/MILESTONES.md
  - ../../../docs/SPEC-FRONTEND.md
confidence: high
---

# The Ubuntu 22.04 release ships an IBus engine only; fcitx5 is given up for that target

**Ruling (2026-11-09, issue #289).** The maintainer had set this work's goal in the
previous message:

> 이 작업의 목표로 ubuntu 22.04 를 위한 배포판을 추가하자.

and, given the measurement that jammy's own `fcitx5` is 5.0.14 while the tree requires
5.1.13 (`docs/OPTIONS-MEASURED.md` §PLT-7), answered the three priced paths with one
sentence:

> 22.04 release는 ibus만 지원하면 돼. fcitx5는 포기.

## What it settles

- **The 22.04 artefact's frontend is an IBus engine** — §PLT-3 option A, built against
  `libibus-1.0-dev` from jammy's **main** pocket (1.5.26-4). That is §PLT-7 path 1, and it
  is the only path this sentence leaves open: path 2 (fcitx5 5.1.13 built from source) is
  what "fcitx5는 포기" retires, and path 3 (core only, no input method) is not "ibus 지원".
- **The 22.04 package carries no fcitx5 dependency at all** — not an archive dependency,
  not a bundled build, not a private prefix. `dpkg-checkbuilddeps` on jammy is to succeed
  with `libibus-1.0-dev` and the core's own dependencies only.
- **The tree therefore owed an IBus frontend before the 22.04 package exists.**
  Until 2026-11-22 nothing in the tree implemented one; the requirement set it had
  to satisfy already existed (`docs/SPEC-FRONTEND.md` FE1–FE15) and the
  host-mechanism price is in §PLT-3 A. **Measured 2026-11-22 (issue #288):** the
  engine now exists under `frontends/ibus/`, builds against 22.04's own libibus
  1.5.26 and 26.04's 1.5.34 from one source, and has its D1/D2 tiers green
  (`docs/TESTING.md`, "The IBus frontend's D1/D2"). The package itself still does
  not exist — this ruling authorises the frontend, not the artefact.

## What it does not settle

- **Whether the other targets keep the fcitx5 addon.** The sentence gives fcitx5 up *for the
  22.04 release*; it says nothing about the dev-distro `.deb` that ships today, and retiring
  that addon is a separate ruling with its own users.
- **The binary package layout.** Whether the IBus engine lives in `hunmin` itself, in a
  second binary package (`hunmin-ibus`), or in a jammy-only package name — and therefore
  what a plain `apt install hunmin` means on each target. §PLT-7 prices the split; the
  sentence does not choose one.
- **The engine's own surfaces**: candidate UI and page ceiling (the IBus panel's lookup
  table or our own window), config backend (GSettings schema or an engine-owned ini), and
  the page size the engine publishes. §PLT-4 items 4–5 stay open.
- **Where the jammy artefact is built.** This host cannot (no container, no root — §PLT-7),
  so a container job, a PPA, or a maintainer host is still a decision, and `AGENTS.md`'s
  three-minute CI ceiling governs whichever job it becomes.
- **Whether IBus is also added where fcitx5 already works.** The ruling is scoped to 22.04;
  a second frontend on the same target is a different question with a different cost.
- **FE11's budget reading** (one budget per frontend, or one for the Linux frontend): the
  shipped adapter is at 660 of 700 code LOC, so a second adapter needs that reading settled
  before it can be written to a limit.

## Why the record matters later

A later session that reads only §PLT-7's three paths will find path 2 still priced and
plausible — build fcitx5 5.1.13 from source for jammy — and may rebuild it, because the
measurement that makes it expensive (jammy ships 5.0.14) is a fact about an archive that
keeps changing. This page is the answer: the maintainer gave that path up deliberately, for
this target, and the IBus engine is what the 22.04 release ships. The measurement itself
stays in §PLT-7 with its commands, so if a future jammy update moves `fcitx5` past 5.1.13 the
premise can be re-read without re-deriving the ruling. See
[[2026-11-08-no-frontend-core-move-the-seam-is-a-contract]] for the seam an IBus engine has to
fit, `docs/OPTIONS-MEASURED.md` §PLT for the measurement this decision lands beside, and
[[decisions-belong-to-the-maintainer]] for why the sentence, and not this page, is the rule.