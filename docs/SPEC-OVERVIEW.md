# SPEC-OVERVIEW — the abstract requirements

The seven requirements below are the root of the tree: every other SPEC id is a
way of satisfying one of them. They are recorded in the maintainer's words
(2026-10-25), unedited, which is why some are one clause and some are a whole
policy. Each names the SPECs and tests that discharge it — an abstract
requirement that no child satisfies is the finding this file exists to expose.

Numbered as the maintainer numbered them.

# Requirements

## A1. Layouts are data

> "should support various korean layouts and english layouts"

Discharged by the data-driven layout contract rather than by code: a layout is a
JSON file, and adding one needs no engine or frontend change.
*Children:* `docs/SPEC-SCHEMA.md` (the JSON contract), `docs/HOWTO-NEW-LAYOUT.md`
(the procedure). *Implemented by:* `libhunmin/src/schema_loader.c`,
`libhunmin/data/layouts/`. *Refuted by:* `engine-golden-*` (one golden per
layout), `schema-contract`, `frontend-invariants` (I11: a picker that hardcodes
ids is a second place to edit per layout).

## A2. Three frontends, one behaviour

> "should support 3 frontends with identical behavior."

Discharged by putting the behaviour in the library and leaving each frontend an
adapter: one key rule, one word buffer, one delivery rule, one corpus.
*Children:* `docs/SPEC-FRONTEND.md` (FE1–FE12, the common requirements),
`docs/SPEC-TSF.md` (TSF1–TSF18, where only TSF differs), and the fcitx5/Android
adapters. *Implemented by:* `libhunmin/src/buffered_preedit.c`,
`frontends/fcitx5/src/hunminadapter.cpp`, `frontends/tsf/src/keydispatch.cpp`,
`frontends/android/app/src/main/java/org/hunmin/ime/HunminAdapter.kt`.
*Refuted by:* `engine-buffered-preedit`, `engine-key-rule`, the win32 and android
conformance traces, `frontend-invariants`.

## A3. Hanja, word candidates first

> "should support hanja input with word candidates, and also single char as
> fallback"

A lookup is keyed on the whole buffered word; when that misses, the last syllable
is retried alone and only that syllable is replaced.
*Children:* `docs/SPEC-TSF.md#§TSF9` (the window and its key),
`docs/SPEC-TSF.md#§TSF10` (the fallback), `docs/SPEC-TSF.md#§TSF11` (the interim
path's one-character rule). *Implemented by:*
`libhunmin/src/candidate_tables.c#hunmin_suggest_count`,
`frontends/tsf/src/candidates.cpp#LastSyllableOf`. *Refuted by:*
`engine-candidates`, `tsf-test` (candidate group).

## A4. Both preedit units

> "should support both word-level and syllable-level preedit for compatibility"

The commit unit is a preference: the same text either way, delivered at the
boundary or as each syllable is finalised. It exists because classic hosts end a
composition per key and modern ones do not.
*Children:* `docs/SPEC-FRONTEND.md#§FE5` (the unit is a preference),
`docs/SPEC-TSF.md#§TSF1` (composition preedit), `docs/SPEC-TSF.md#§TSF4` (the
interim document path). *Implemented by:*
`libhunmin/include/hunmin.h#HUNMIN_COMMIT_AT_BOUNDARY`,
`frontends/tsf/src/delivery.cpp#InterimDelivery`. *Refuted by:*
`engine-buffered-preedit`, `tsf-test` (`LegacyTest.CommitUnit*`).

## A5. Configurable across the board

> "should be configurable for various aspect"

Every behaviour a user can want differently is a stored setting with one read
order (environment seam → per-user store → cross-frontend default) and one
control per surface that shows it.
*Children:* `docs/SPEC-TSF.md#§TSF12` (the read order),
`docs/SPEC-TSF.md#§TSF15` (both surfaces), `docs/SPEC-FRONTEND.md#§FE5` (the
commit unit as a setting). *Implemented by:* `frontends/tsf/src/config.cpp`,
`frontends/fcitx5/src/config.h`, `frontends/android/.../ImeSettings`. *Refuted
by:* `tsf-test` (config group); the fcitx5 and Android settings are asserted in
their own tiers.

## A6. Every aspect covered, cheaply

> "test coverage should cover all aspect with minimal execution time."

Coverage is a labelled suite where a tier is selected by label, every tier runs
inside a time budget, and a test that cannot fail is treated as no test.
*Children:* `docs/SPEC-RELEASE.md#§REL13` (budgets are rules); the label
vocabulary in `docs/TESTING.md`. *Implemented by:* `tools/ci-time-budget.sh`,
`tools/frontend-mutations.py` (the guards are shown capable of failing).
*Refuted by:* `tools/ci-time-budget.sh` itself, `frontend-mutations`.

## A7. Developed by AI under human instruction

> "can be developed by AI with human instruction"

The working surface is an issue, the unit of work is a worktree, and a decision
is the maintainer's: an agent measures and reports, and records no verdict.
*Children:* the `AGENTS.md` sections "Working rules", "GitHub issue tracking" and
"Requirement traceability". *Implemented by:* `tools/orchestrate.sh`,
`docs/ORCHESTRATION.md`. *Refuted by:* `none` — a process requirement; what it
buys is checked by reading the record, not by a test.
