---
title: 2026-11-01 modes ownership — ruled, implemented, detector retired
created: 2026-11-01
updated: 2026-11-01
type: session
status: active
tags: [session, modes, r8, r9, fcitx5, tsf, android]
sources:
  - ../../../docs/OPTIONS-MEASURED.md
  - ../../../docs/SPEC-MODES.md
  - ../../../docs/REQ-MAP.md
confidence: high
---

# 2026-11-01 modes ownership — ruled, implemented, detector retired

One session: priced the question in issue #164, took the maintainer's ruling, and
implemented it on all three frontends. The ruling itself is recorded at
[[2026-11-01-the-engine-judges-the-frontend-switches]]; this page is what the work
found and what it left.

## What was created

- **`docs/OPTIONS-MEASURED.md` §MRO** — the pricing. Two of the issue's own premises
  did not hold: the replay mechanism had already moved into the core with #167 (so
  the choice was not "does the layer have the material"), and #136's "no caller in
  any frontend" was stale by two frontends. A third fact was missing from the
  question entirely: fcitx5 was building a **one-language** layer (`nullptr` Latin
  id), so its own copy of the judgement could not have been wired even if someone
  had read its answer.
- **`hunmin_buffered_preedit_take_judgement`** — the engine judges at the boundary
  and the frontend performs the switch. R9's threshold landed at three **key
  inputs** (`JUDGE_MIN_KEYS`), the unit the ruling's "3자" resolves to once the two
  readings of one word are compared (`안녕` is six keys and two syllables).
- **The three acts** — TSF (compartment + langbar), Android (which adapter receives
  the next key), fcitx5 (the layer's switch, after finally getting the English slot).

## What was overturned

- **B9 / the 2026-10-13 adaptor-owned decision.** Kept as a marked page
  ([[2026-10-13-r8-mode-guessing-is-adaptor-owned]]) rather than deleted: the load
  argument it made was answered by the data moving into the core first, not by a
  counter-argument.
- **`frontends/fcitx5/src/language_detect.{h,cpp}`** and its
  `engine-d1-language-detect` case: retired, with six `REQ-MAP` rows re-pointed in
  the same change because `req-map-check` fails on an unregistered id.

## What is pending, with its own home

- **#175** — two TSF defects reported live during the work: punctuation + Enter
  loses the paragraph break on a layout where the punctuation is a layout key
  (`sin3-p2`, all five of `. ? , ! :` measured), and a word duplicated after a
  focus round-trip. The engine is measured innocent on both; the mechanisms are in
  the issue.
- **#176** — fcitx5's ON/OFF settings row, plus the prerequisites recorded there.
- **#179 / #180** — the OFF-identity and cross-frontend switch tests. No D1/D2 case
  drives any frontend's act yet, because the shipped default is OFF.
- **#202** — three Linux-tier reds on a CRLF worktree (`abi`, `krdict-fixture`, and
  the mutation sandbox picking a Windows toolchain cache), all measured.
- **Unfiled, and named here rather than invented as an issue:** no frontend persists
  the ON/OFF state, so a rebuilt layer starts from the engine default; R8's
  symmetric half stays open exactly as the requirement states.

## The lesson worth keeping

The question arrived as "who owns the judgement", and the honest answer was "the
tree has already answered half of it". Measuring the premises first turned a
design argument into a smaller one: the reading existed twice, the validities
existed once, and the formula existed nowhere. See
[[decisions-belong-to-the-maintainer]] for why the paper stops at the options, and
[[the-rules-keep-breaking]] for what happens when a rule is written down without a
measurement behind it.
