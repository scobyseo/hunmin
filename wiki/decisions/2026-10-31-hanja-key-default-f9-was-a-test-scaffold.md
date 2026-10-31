---
title: The Hanja key is the default for 한자 입력 — F9 was a test scaffold
created: 2026-10-31
updated: 2026-10-31
type: decision
status: active
tags: [decision, hanja, keybinding, fcitx5, android, tsf]
sources:
  - ../../../docs/SPEC-TSF.md
  - ../../../docs/WINDOWS-TSF-CANDIDATES.md
  - ../../../docs/OPTIONS-MEASURED.md
confidence: high
---

# The Hanja key is the default for 한자 입력 — F9 was a test scaffold

Ties off the F9 question that `docs/SPEC-MODES.md` §E5 carried as a constraint and that
the 2026-10-31 measurement found false in both halves
([[2026-10-31-han-eng-modifier-compound-special-functions]] records the sibling ruling;
the measurement is `docs/OPTIONS-MEASURED.md` §E5-0).

**Maintainer, 2026-10-31** (verbatim):

> f9 as hanja was for test purpose. so set hanja key default for hanja input - it is
> also configurable through setting dialog.

## What was ruled

1. **The Hanja key is the default trigger for 한자 입력**, not F9. On Android that is
   `KEYCODE_EISU` (212) — the 한자 key of a Korean keyboard; on Windows it is
   `VK_HANJA` (0x19); on fcitx5 the `Hangul_Hanja` keysym.
2. **F9's role is a test scaffold**, not a shipped binding. It exists because the F9
   flow was reachable from a test harness (X11 `F9` = `0xFFC6`, the tier-D3-candidate
   round, `tests/e2e/cases-candidate.tsv`) and it is not to be read as the product
   default any more.
3. **The binding is configurable in the settings dialog**, on each frontend, as the
   Han/Eng assignment is (`docs/SPEC-MODES.md` §E5 — same convention: the value is
   shown, changeable, and it is the one the hook honours).

## What this does NOT settle

- **Whether F9 stays accepted as an alternate.** The conformance corpus and two tiers
  drive `{F9}`; if F9 is a scaffold, the corpus can keep using it *provided* a shipped
  binding still accepts it, or the corpus moves to the Hanja keysym. Which of the two is
  an implementation choice, not a requirement, and it decides whether the tier-C traces
  change.
- **Where the value lives.** The ruling says "configurable through the setting dialog",
  the same shape as E5's composition: the value the dialog shows must be the value the
  hook honours, so the engine-vs-frontend store question is shared with
  [[2026-10-31-han-eng-modifier-compound-special-functions]] and is not answered twice.
- **What already matches.** Two of three frontends already default to the Hanja key:
  TSF's `hanjaKey` default is `v1:25:0` (`docs/SPEC-TSF.md` §TSF9, ruled with issue #67),
  and Android carries `KEYCODE_EISU` as a `platformAlternates` entry. Android's *row*
  still reads "not set" because that entry is an alternate and not a default — which is
  the gap this ruling closes.

## Why the record matters later

The F9 sentence in the issue body and in three documents ("F9 is occupied by the
preprocessing expression, and Android has no AOSP keycode for it") was the whole
justification for the earlier shape of E5. The measurement found that the 전처리수식 keys
are the `flags` overrides on z/h/H/w/W and that `KeyEvent.KEYCODE_F9` is 139. This ruling
is the other half: F9 was scaffolding. Together they retire the constraint rather than
working around it, and a later reader who finds `{F9}` in a trace should read it as a
harness key, not as the product's 한자 key.

See [[decisions-belong-to-the-maintainer]].
