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

# Provisional requirements (adopted 2026-10-25, wording to be revisited)

These complete the mapping: every requirement id in the tree has an abstract
parent. The maintainer adopted them so the tree is closed, and will revisit the
wording — so treat a `Provisional` entry as binding in structure and open in
phrasing. Each names an observable outcome and deliberately names no mechanism.

## A8. Composition follows the layout data

> "Hangul input must produce exactly the syllables the selected layout
> prescribes, and no key the layout maps may be lost or duplicated."
> *Provisional — wording to be revisited.*

Design decides the state machine, where composing state lives, when a syllable
finalises, and what backspace and flush do. *Implemented by:*
`libhunmin/src/ic.c`, `libhunmin/src/tables.c`. *Refuted by:* the per-layout
goldens (`engine-golden-*`), `engine-process-key`, `engine-commit-text`.

## A9. A language change only when the reading is unambiguous

> "The input method may change language on its own only when the typed text
> admits exactly one reading; when it admits both or neither, the current mode
> is kept."
> *Provisional — wording to be revisited.*

Design decides what a reading is (dictionary, syllable structure, frequency),
when it is judged, and any threshold. *Children:* `docs/SPEC-MODES.md` R1–R13
with their R7/R8/R13 amendments, and `docs/SPEC-FRONTEND.md#§FE7`.
*Implemented by:* `frontends/fcitx5/src/language_detect.cpp#LanguageDetector`.
*Refuted by:* `none` — the family is designed and not built; `SPEC-MODES` §2
E1–E8 are how it would be evaluated.

## A10. Data carries its terms and its provenance

> "Data distributed with the product must carry the terms it is distributed under,
> and every generated artefact must record how it was produced."
> *Provisional — wording to be revisited.*

Design decides the file formats and where the record lives. *Implemented by:* the
per-directory `libhunmin/data/*/LICENSE.md` and the `meta.json` each generator
writes. *Refuted by:* `none` — the rule is enforced by review, not by a test.

## A11. Install works, uninstall leaves nothing behind

> "Installing must yield a working input method, and uninstalling must remove
> everything the installation created outside the user's own documents."
> *Provisional — wording to be revisited.*

Design decides the package format, the staging layout, the registration
mechanism and where state is stored. *Children:*
`docs/SPEC-RELEASE.md` REL1–REL12, `docs/SPEC-TSF.md#§TSF17`.
*Implemented by:* `tools/build-windows-installer.sh`, `debian/`,
`frontends/tsf/src/registration.cpp`. *Refuted by:* `none` — clean-machine
rounds are device-bound.

## A12. Text that is not typed is delivered verbatim

> "Text that does not come from the keyboard must reach the client exactly as it
> arrived, and must not be altered by the composition rules."
> *Provisional — wording to be revisited.*

Design decides the path, its API, and its ordering against a pending word.
*Children:* `docs/SPEC-FRONTEND.md#§FE8`. *Implemented by:*
`libhunmin/include/hunmin.h#hunmin_buffered_preedit_external_text`.
*Refuted by:* `none` — one frontend exposes the path, so the requirement has one
exerciser.

## A13. A frontend keeps working across versions

> "A frontend built against one version of the library must keep working against
> later versions, and adding capability must never remove or redefine what
> exists."
> *Provisional — wording to be revisited.*

Design decides the symbol policy and how an addition is enforced. *Implemented
by:* `libhunmin/include/hunmin.h#hunmin_buffered_preedit_new` and the committed
symbol list beside it. *Refuted by:* `none` — the symbol diff is a review step
except on the runner-side `abi` test.

## Child index — every requirement id and its parent

Derived from `docs/REQ-MAP.md`, whose `parent` column `tools/req-map-check.py`
enforces (a requirement with no parent, or an abstract requirement with no child,
fails the build). This table is a rendered view, not a second home: change the
map and rerun.

| id | parent | id | parent |
|---|---|---|---|
| FE12 | A1 | TSF13 | A1 |
| FE1 | A2 | FE2 | A2 |
| TSF2 | A2 | FE3 | A2 |
| FE6 | A2 | FE9 | A2 |
| FE11 | A2 | TSF9 | A3 |
| TSF10 | A3 | TSF11 | A3 |
| TSF1 | A4 | TSF3 | A4 |
| FE4 | A4 | TSF4 | A4 |
| FE5 | A4 | TSF5 | A4 |
| TSF6 | A5 | TSF12 | A5 |
| TSF14 | A5 | TSF15 | A5 |
| TSF16 | A5 | FE10 | A6 |
| REL13 | A6 | TSF18 | A6 |
| REL14 | A7 | TSF7 | A8 |
| TSF8 | A8 | R1 | A9 |
| R2 | A9 | R3 | A9 |
| R4 | A9 | R5 | A9 |
| R6 | A9 | FE7 | A9 |
| R7 | A9 | R7-정정 | A9 |
| R8 | A9 | R8-정정 | A9 |
| R9 | A9 | R10 | A9 |
| R11 | A9 | R12 | A9 |
| FE13 | A9 | R13 | A9 |
| R13-정정 | A9 | FE14 | A10 |
| REL1 | A11 | REL2 | A11 |
| REL3 | A11 | REL4 | A11 |
| REL5 | A11 | REL6 | A11 |
| REL7 | A11 | REL8 | A11 |
| REL9 | A11 | REL10 | A11 |
| REL11 | A11 | REL12 | A11 |
| TSF17 | A11 | FE8 | A12 |
| FE15 | A13 | | |
