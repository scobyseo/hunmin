# REQ-MAP — requirement → architecture → design → implementation

Status: **pointer layer.** Every cell is a pointer, not a statement.

This file exists so that **duplicated prose can be deleted from other
documents**: a fact gets one home, and this table says where that home is. If a
sentence here could be deleted without losing information, it should not have
been written — a row that restates a requirement is a second copy of it.

- `requirement` is `pointer — label`: the pointer is read, the label after the
  em dash is for a human scanning the table. Never the requirement text.
- `architecture` / `design` / `implementation` pointers are
  `path#§<section key>`, `path#<heading title>`, or `path#<symbol>`.
  **Section keys, never line numbers** — `docs/` already carries hundreds of
  `file:line` citations that rot at line granularity, and a pointer that rots is
  worse than none.
- `verified by` holds ctest ids only. `none` means no test asserts it.
- `none` in any cell is a **finding**, not a blank to be filled with something
  plausible. Requirement families that are designed and not built carry it.
- `status` is `implemented` / `partial` / `none` / `not implementable` plus the
  measurement that decided it.

The checker is `tools/req-map-check.py` (ctest `req-map`): it fails on a dead
path, an anchor that no longer resolves, an unregistered ctest id, a row with
fewer than seven columns, and a requirement id that a SPEC carries but this
table does not.

Requirement ids come from the SPECs. `docs/SPEC-MODES.md` §1 is the only section
in the tree that carries numbered requirements (R1–R13, plus the two `-정정`
amendments). `docs/SPEC-P2.md` and `docs/SPEC-SCHEMA.md` carry **no requirement
ids at all** — their settled rules are headed, not numbered, and their other
headings are phases and data-contract sections — so there is nothing to map from
them and nothing is invented here. `docs/SPEC-MODES.md` §2 (E1–E8) says of
itself that each item "is an implementation parameter, not a requirement"; those
are parameters of the rows below and are not rows.

Ids marked **(proposed)** do not exist in any SPEC today. They are
`docs/FRONTEND-EQUIVALENCE.md` sections that are stated as rules and behave as
requirements but were never numbered. The ids are proposed here so the rows can
be pointed at; **numbering them in the document itself is the maintainer's
call**, and until it happens these ids live only in this table.

| REQ | requirement (one sentence) | architecture | design | implementation | verified by | status |
|---|---|---|---|---|---|---|
| R1 | docs/SPEC-MODES.md#§R1 — the cost being removed | none | none | none | none | not implementable: a problem statement, no behaviour to build |
| R2 | docs/SPEC-MODES.md#§R2 — one IM, a layout pair per language | `docs/architecture.md#§1`; `docs/architecture.md#§4` | `docs/SPEC-SCHEMA.md#Layout file` | `libhunmin/include/hunmin.h#hunmin_ctx_layout_mode`; `frontends/fcitx5/src/config.h#latinLayout` | schema-contract; engine-d1-language-detect | partial: the layout `mode` field and the fcitx5 layout pair exist; no frontend delivers a Latin mode to the client |
| R3 | docs/SPEC-MODES.md#§R3 — one judgement rule, one owner, every frontend | `docs/architecture.md#§3`; `docs/FRONTEND-EQUIVALENCE.md#§1` | `docs/MODES-ENGINE-RULES.md#§1` | `frontends/fcitx5/src/language_detect.cpp#LanguageDetector::judge` | engine-d1-language-detect | partial: adaptor-owned decision taken 2026-10-13; fcitx5 only — Android and TSF hold no detector (`docs/FRONTEND-EQUIVALENCE.md#§8`) |
| R4 | docs/SPEC-MODES.md#§R4 — judge once, at the end of the buffer | `docs/architecture.md#§2` | `docs/MODES-ENGINE-RULES.md#§2` | `libhunmin/src/buffered_preedit.c#hunmin_buffered_preedit_process_key` | engine-buffered-preedit | partial: the word buffer and its boundary exist as the shared layer; **no judgement is made at the boundary** and the engine still commits per syllable |
| R5 | docs/SPEC-MODES.md#§R5 — OFF + English commits per character | none | none | none | none | none: no OFF state exists to be per-character in |
| R6 | docs/SPEC-MODES.md#§R6 — ON holds digit runs in the buffer too | none | none | none | none | none: the layer buffers a word, nothing holds a digit run as one buffer unit |
| R7 | docs/SPEC-MODES.md#§R7 — ambiguous keeps the current mode | `docs/architecture.md#§3` | `docs/MODES-ENGINE-RULES.md#§1` | `frontends/fcitx5/src/language_detect.h#LanguageDetector` | engine-d1-language-detect | partial: an empty answer means "no switch", but "ambiguous" is only "shorter than the threshold" — there is no both-valid / both-invalid test |
| R7-정정 | docs/SPEC-MODES.md#§R7-정정 — both error directions weigh the same | none | `docs/MODES-METRICS.md#§1.4` | none | none | none: a scoring criterion; the metric it names is not measured and its pass line is undecided (E4) |
| R8 | docs/SPEC-MODES.md#§R8 — switch only when clearly the other language | `docs/architecture.md#§3` | `docs/MODES-ENGINE-RULES.md#§1`; `docs/MODES-DATA.md#§4-7` | `frontends/fcitx5/src/language_detect.cpp#LanguageDetector::judge` | engine-d1-language-detect | partial: replay works, the `valid` lists do not exist, and the answer is **discarded at the call site** — no mode is ever delivered |
| R9 | docs/SPEC-MODES.md#§R9 — short words are never switched | `docs/architecture.md#§3` | `docs/MODES-ENGINE-RULES.md#§1` | `frontends/fcitx5/src/language_detect.h#LanguageDetector` | engine-d1-language-detect | partial: `minLen` behaves as a guard; the threshold N itself is undecided (SPEC-MODES E3) |
| R10 | docs/SPEC-MODES.md#§R10 — a switch is not retroactive | none | none | none | none | none: nothing switches, so nothing can be retroactive; the call site discards the judgement |
| R11 | docs/SPEC-MODES.md#§R11 — a person fixes a misjudgement by hand | none | none | none | none | none: an acceptance criterion, not a behaviour |
| R12 | docs/SPEC-MODES.md#§R12 — success is a keystroke count | `docs/MODES-METRICS.md#§1` | `docs/MODES-METRICS.md#§2` | none | none | none: metrics ② and ③ are unmeasured and no pass line is set (E4) |
| R13 | docs/SPEC-MODES.md#§R13 — on/off by key combination, with a UI as well | `docs/architecture.md#§2` | `docs/MODES-ENGINE-RULES.md#§3` | `frontends/android/app/src/main/java/org/hunmin/ime/KeyBinding.kt#AUTO_TOGGLE`; `frontends/android/app/src/main/java/org/hunmin/ime/OverlayActivity.kt#TBD_0_3` | engine-android-e1 | partial and disabled: the Android action and its settings row exist, the default chord was removed for 0.1 (issue #2) and the row shows 미정 (0.3); fcitx5 and TSF have neither surface |
| R13-정정 | docs/SPEC-MODES.md#§R13-정정 — one engine-owned state, two surfaces | `docs/architecture.md#§2` | `docs/MODES-ENGINE-RULES.md#§3` | none | none | none: no `autoswitch` option exists anywhere in the tree (`grep -rn autoswitch` → 0 hits), so no frontend has a state to surface |
| FE-1 (proposed) | docs/FRONTEND-EQUIVALENCE.md#§1 — same layout and keys give the same output | `docs/FRONTEND-EQUIVALENCE.md#§1`; `docs/architecture.md#§5` | `docs/FRONTEND-EQUIVALENCE.md#§5` | `libhunmin/src/buffered_preedit.c#hunmin_buffered_preedit_process_key`; `frontends/fcitx5/src/hunminadapter.cpp#HunminState` | engine-buffered-preedit; engine-d1-fcitx5; tsf-test | partial: commit and preedit come from one layer on all three frontends, and `docs/FRONTEND-EQUIVALENCE.md#§5.2` records that literal per-key preedit equality cannot hold under a free commit boundary — the assertion is committed-plus-preedit |
| FE-2 (proposed) | docs/FRONTEND-EQUIVALENCE.md#§2 — TSF interim and native agree per key | `docs/FRONTEND-EQUIVALENCE.md#§2` | `docs/FRONTEND-EQUIVALENCE.md#§6` | `frontends/tsf/src/delivery.h#hunmin_tsf_deliveries_register` | tsf-mock-tests_conformance_win32_prose.trace.in; tsf-mock-tests_conformance_win32_words.trace.in | implemented: both delivery modes run the same corpus |
| FE-3 (proposed) | docs/FRONTEND-EQUIVALENCE.md#§4 — one abstract Han/Eng action | `docs/FRONTEND-EQUIVALENCE.md#§4` | `docs/FRONTEND-EQUIVALENCE.md#§5` | `libhunmin/include/hunmin.h#hunmin_buffered_preedit_toggle_language` | none | partial: the layer symbol exists and **no frontend calls it** (`docs/FRONTEND-EQUIVALENCE.md#§8`); each adapter still owns its own mechanism |
| FE-4 (proposed) | docs/FRONTEND-EQUIVALENCE.md#§6 — one corpus, every frontend, both units | `docs/FRONTEND-EQUIVALENCE.md#§6` | `docs/trace-spec.md#Corpus layout` | tools/gen-conformance.py | engine-android-conformance-tests_conformance_android_basic-syllables-case01.trace.in; tsf-mock-tests_conformance_win32_prose.trace.in | partial: the format and the emitters are shared; TSF runs the buffered unit only (`docs/FRONTEND-EQUIVALENCE.md#§6.1`) and the commit-unit axis is asserted per frontend instead |
| FE-5 (proposed) | docs/FRONTEND-EQUIVALENCE.md#§7 — a live TSF run leaves nothing registered | none | `docs/FRONTEND-EQUIVALENCE.md#§7` | tools/tsf-sandbox.ps1 | none | implemented, unverifiable by ctest: the lane is Windows Sandbox and no test asserts it; the measurement is the `pass=1` probe run recorded in `docs/FRONTEND-EQUIVALENCE.md#§7` |
