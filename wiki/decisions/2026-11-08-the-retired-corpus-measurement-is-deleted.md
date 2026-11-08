---
title: The retired corpus measurement is deleted; only the method survives
created: 2026-11-08
updated: 2026-11-08
type: decision
status: active
tags: [decision, modes, measurement, provenance]
sources:
  - ../../../docs/SPEC-MODES.md
  - ../../../docs/MODES-MEASUREMENT.md
confidence: high
---

# The retired corpus measurement is deleted; only the method survives

**Ruling (2026-11-08).** The maintainer's words, as recorded when the removal was
made — the message this repository kept is quoted here with its elision marked:

> "most of modes-data is stale; … no need to retain. remove the file and extract
> minimal knowledges to reproduce it with new data and approach."

and, asked how far the deletion reached into the pricing papers:

> "delete the measurement history is my order. just leave how to remeasure with
> new data and new approach."

## What was deleted

- `docs/MODES-DATA.md` and `docs/MODES-METRICS.md` — the corpus description, the
  cap derivation, the licence narrative and every dated reading taken off that
  corpus.
- `docs/SPEC-MODES.md`'s "`valid` is not a dictionary lookup (measured
  2026-10-04)" section, whose conclusion — that "word" means "something in this
  list" — is the definition ReqMode8 replaced.
- The five modes sections of `docs/OPTIONS-MEASURED.md`: §EDW (which dictionary),
  §B1 (whether the blocker existed), §PAS (pass-line arithmetic), §MRO (who owns
  the judgement) and §BND (the boundary parameters). Their rulings survive here
  and in `docs/SPEC-MODES.md`; their measurements do not.

The figures are not lost — git history and this wiki's `raw/` records hold them.
They are no longer a home anybody can cite as current.

## What survives, and where

- **How to measure again**: `docs/MODES-MEASUREMENT.md` — metric definitions
  (§1), pass-line shapes (§2), reproduction commands (§3), corpus sources and
  licences (§4), harness rules (§5) and the named open questions (§6). It
  deliberately carries no figures.
- **The reproduction tools**: `tools/build-modes-sentences.py`,
  `build-modes-corpus.py`, `build-modes-624.py`, `build-modes-119.py`,
  `modes-glued.py`, `modes-cap.py`, `modes-dict-probe.py`, `modes-metrics.py`,
  `jamo-keytable.py` — each takes its corpus as an argument and writes its
  provenance beside its output.
- **The rulings**, each with its own page: the English source
  ([[2026-11-01-the-english-source-is-hunspell-en-us-plus-a-user-dictionary]]),
  the buffer cap
  ([[2026-11-01-the-buffer-cap-is-256-bytes-configurable]]), ownership and the
  three-key guard ([[2026-11-01-the-engine-judges-the-frontend-switches]]),
  judgement timing ([[2026-11-01-the-judgement-timing-is-per-key-by-default]]),
  and the requirement wording a measurement corrected
  ([[2026-11-01-the-reqmode6-tier-examples-are-the-shift-row]]).

## What the removal does not settle

- **The pass lines** for ②, ②b and ③-bis (board B2) — a number only the
  maintainer sets, and the deletion removed the readings that would have been its
  baseline without supplying a replacement.
- **ReqMode9's threshold**, now that `hangul_valid` is a syllable-structure test
  rather than a word test.
- **Whether a new corpus and harness should be built at all**, and on which
  population. `docs/MODES-MEASUREMENT.md` §4 names the licensable candidates and
  the freshness/population trade-off; choosing one is a ruling.

## Why the record matters later

The deletion is the kind that looks like tidying and is not: without this page, a
later session finds a tree with no modes figures and no note explaining why, then
either re-derives the retired measurement from git history believing it current,
or writes a fresh one on the retired definition. The rule the removal encodes is
the one `docs/MODES-MEASUREMENT.md` §5 states: a harness that cannot fail loudly
is not a guard — the same reason
[[2026-11-02-spec-modes-4-splits-into-an-invariant-and-a-metric]] separated the
invariant from the metric in the first place.