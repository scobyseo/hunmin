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
| FE1 | docs/SPEC-FRONTEND.md#§FE1 — same layout and keys give the same committed text | `libhunmin/src/buffered_preedit.c#hunmin_buffered_preedit_process_key` | `docs/FRONTEND-EQUIVALENCE.md#§1` | `libhunmin/include/hunmin.h#hunmin_buffered_preedit_process_key` | engine-buffered-preedit; engine-key-rule; tsf-mock-tests_conformance_win32_prose.trace.in | implemented |
| FE2 | docs/SPEC-FRONTEND.md#§FE2 — one key rule, in the engine library | `libhunmin/include/hunmin.h#hunmin_buffered_preedit_classify` | `docs/FRONTEND-EQUIVALENCE.md#§5` | `libhunmin/src/buffered_preedit.c#hunmin_buffered_preedit_classify` | engine-key-rule | implemented |
| FE3 | docs/SPEC-FRONTEND.md#§FE3 — no frontend holds a word buffer | `libhunmin/include/hunmin.h#hunmin_buffered_preedit_new` | `docs/FRONTEND-EQUIVALENCE.md#§5` | `libhunmin/src/buffered_preedit.c#hunmin_buffered_preedit_new` | frontend-invariants | implemented |
| FE4 | docs/SPEC-FRONTEND.md#§FE4 — one delivery per word, at a boundary | `libhunmin/include/hunmin.h#hunmin_buffered_preedit_commit_word` | `docs/FRONTEND-EQUIVALENCE.md#§1` | `libhunmin/src/buffered_preedit.c#hunmin_buffered_preedit_commit_word` | tsf-mock-tests_conformance_win32_prose.trace.in; engine-buffered-preedit | implemented |
| FE5 | docs/SPEC-FRONTEND.md#§FE5 — the commit unit is a preference; the text is the same | `libhunmin/include/hunmin.h#HUNMIN_COMMIT_AT_BOUNDARY` | `docs/FRONTEND-EQUIVALENCE.md#§3` | `libhunmin/src/buffered_preedit.c` | engine-buffered-preedit | implemented |
| FE6 | docs/SPEC-FRONTEND.md#§FE6 — caps is per key event; Caps Lock never toggles | `libhunmin/include/hunmin.h#hunmin_ic_set_option` | `docs/FRONTEND-EQUIVALENCE.md#§8` | `libhunmin/src/buffered_preedit.c` | engine-key-rule; engine-buffered-preedit | implemented |
| FE7 | docs/SPEC-FRONTEND.md#§FE7 — Han/Eng maps to one abstract action | `libhunmin/include/hunmin.h#hunmin_buffered_preedit_toggle_language` | `docs/FRONTEND-EQUIVALENCE.md#§4` | `libhunmin/include/hunmin.h#hunmin_buffered_preedit_toggle_language` | none | partial: the symbol exists and no frontend calls it (`docs/FRONTEND-EQUIVALENCE.md#§8`) |
| FE8 | docs/SPEC-FRONTEND.md#§FE8 — text from outside the keyboard is not composed | `libhunmin/include/hunmin.h#hunmin_buffered_preedit_external_text` | `docs/FRONTEND-EQUIVALENCE.md#§8` | `libhunmin/src/buffered_preedit.c#hunmin_buffered_preedit_external_text` | none | partial: only Android has the path; the invariant has one exerciser |
| FE9 | docs/SPEC-FRONTEND.md#§FE9 — what is shown comes from the layer | `libhunmin/include/hunmin.h#hunmin_buffered_preedit_display` | `docs/FRONTEND-EQUIVALENCE.md#§5` | `libhunmin/src/buffered_preedit.c#hunmin_buffered_preedit_display` | frontend-invariants | implemented |
| FE10 | docs/SPEC-FRONTEND.md#§FE10 — one corpus, every frontend | `tools/gen-conformance.py` | `docs/trace-spec.md#Corpus layout` | `tools/trace-check.py` | engine-android-conformance-tests_conformance_android_basic-syllables-case01.trace.in; tsf-mock-tests_conformance_win32_prose.trace.in | partial: TSF runs the buffered unit only (`docs/FRONTEND-EQUIVALENCE.md#§6.1`) |
| FE11 | docs/SPEC-FRONTEND.md#§FE11 — a frontend stays thin | `tools/check-frontend-invariants.py` | `AGENTS.md#§Guidelines` | `tools/check-frontend-invariants.py` | frontend-invariants | implemented: 700 code LOC, the ruled number |
| FE12 | docs/SPEC-FRONTEND.md#§FE12 — no per-layout anything in a frontend | `tools/check-frontend-invariants.py` | `AGENTS.md#§The one idea` | `tools/check-frontend-invariants.py` | frontend-invariants | implemented |
| TSF1 | docs/SPEC-TSF.md#§TSF1 — preedit rides a live composition | `frontends/tsf/src/tsf.h#CompositionWrite` | `docs/WINDOWS-TSF-BUFFERING.md#§Design` | `frontends/tsf/src/composition.cpp#StartComposition` | tsf-test | implemented |
| TSF2 | docs/SPEC-TSF.md#§TSF2 — preedit is transparent with an underline | `frontends/tsf/src/displayattr.h` | `docs/WINDOWS-TSF-API-NOTES.md#§Why native composition preedit beats the overlay window` | `frontends/tsf/src/displayattr.cpp#DisplayAttr` | none | partial: the attribute carries the ruled shape; Notepad still paints its own highlight, and desktop rendering is not ctest-measurable |
| TSF3 | docs/SPEC-TSF.md#§TSF3 — the interim path is detected, not configured | `frontends/tsf/src/tsf.h#CaptureDeadCopy` | `docs/TSF-INTERIM-INPUT-SPIKE.md#§2` | `frontends/tsf/src/composition.cpp#CaptureDeadCopy` | tsf-test | implemented |
| TSF4 | docs/SPEC-TSF.md#§TSF4 — interim text is document text | `frontends/tsf/src/tsf.h#Delivery` | `docs/TSF-INTERIM-INPUT-SPIKE.md#§3` | `frontends/tsf/src/delivery.cpp#InterimDelivery` | tsf-test | implemented |
| TSF5 | docs/SPEC-TSF.md#§TSF5 — Enter is a boundary at the test phase on the interim path | `frontends/tsf/src/tsf.h#FeedEngineKey` | `docs/TSF-INTERIM-INPUT-SPIKE.md#§3` | `frontends/tsf/src/keydispatch.cpp#FeedEngineKey` | tsf-test | implemented |
| TSF6 | docs/SPEC-TSF.md#§TSF6 — the toggle is a configurable binding list | `frontends/tsf/src/keybinding.h` | `docs/WINDOWS-TSF-CONFIG.md#§The mode-toggle bindings` | `frontends/tsf/src/keybinding.h#HUNMIN_TSF_VIRTUAL_HANGEUL` | tsf-test | implemented |
| TSF7 | docs/SPEC-TSF.md#§TSF7 — a chord flushes and passes | `frontends/tsf/src/tsf.h#Delivery` | `docs/WINDOWS-TSF-BUFFERING.md#§Design` | `frontends/tsf/src/keydispatch.cpp#FeedEngineKey` | tsf-test; engine-key-rule | implemented |
| TSF8 | docs/SPEC-TSF.md#§TSF8 — edit keys are keys of passage | `frontends/tsf/src/keydispatch.cpp#DrainAndPass` | `docs/WINDOWS-TSF-BUFFERING.md#§Design` | `frontends/tsf/src/keydispatch.cpp#DrainAndPass` | tsf-test | implemented |
| TSF9 | docs/SPEC-TSF.md#§TSF9 — the candidate window and its key | `frontends/tsf/src/candidates.h` | `docs/WINDOWS-TSF-CANDIDATES.md#§What the adapter supplies` | `frontends/tsf/src/candidates.cpp#hunmin_tsf_hanja_candidates_new` | tsf-test; tsf-mock-tests_conformance_win32_words.trace.in | implemented |
| TSF10 | docs/SPEC-TSF.md#§TSF10 — a whole-word miss falls back to the last syllable | `frontends/tsf/src/candidates.h` | `docs/WINDOWS-TSF-CANDIDATES.md#§What the adapter supplies` | `frontends/tsf/src/candidates.cpp#LastSyllableOf` | tsf-test | implemented |
| TSF11 | docs/SPEC-TSF.md#§TSF11 — interim candidate: in-flight anchor, one character only | `frontends/tsf/src/tsf.h#Delivery` | `docs/WINDOWS-TSF-CANDIDATES.md#§The edit-mode half` | `frontends/tsf/src/delivery.cpp#CandidateCommit` | tsf-test | implemented |
| TSF12 | docs/SPEC-TSF.md#§TSF12 — one settings read order; unresolved ids degrade | `frontends/tsf/src/config.h` | `docs/WINDOWS-TSF-CONFIG.md#§The configuration store` | `frontends/tsf/src/config.cpp#hunmin_tsf_config_load` | tsf-test | implemented |
| TSF13 | docs/SPEC-TSF.md#§TSF13 — one data directory wins | `frontends/tsf/src/datadir.h` | `docs/WINDOWS-TSF-CONFIG.md#§Data loading` | `frontends/tsf/src/datadir.cpp` | tsf-test | implemented |
| TSF14 | docs/SPEC-TSF.md#§TSF14 — the settings surface is out of process | `frontends/tsf/src/settingslaunch.h` | `docs/WINDOWS-TSF-CONFIG.md#§The configuration store` | `frontends/tsf/src/settingslaunch.cpp#hunmin_tsf_settings_exe_run` | tsf-test | implemented |
| TSF15 | docs/SPEC-TSF.md#§TSF15 — a setting appears on both surfaces; keys are captured | `frontends/tsf/src/settingsrows.h` | `docs/WINDOWS-TSF-CONFIG.md#§The configuration store` | `frontends/tsf/src/settingsdialog.cpp#SetWindowsHookExW` | none | partial: both surfaces implement it; judged live, not asserted |
| TSF16 | docs/SPEC-TSF.md#§TSF16 — the language bar item is the TIP's own | `frontends/tsf/src/langbar.h` | `docs/WINDOWS-TSF-REGISTRATION.md#§The regsvr32 flow` | `frontends/tsf/src/langbar.cpp#HunminLbiInputModeGuid` | tsf-test | partial: the handler is asserted; the Win11 tray never calls InitMenu |
| TSF17 | docs/SPEC-TSF.md#§TSF17 — unregistration mirrors registration | `frontends/tsf/src/registration.h` | `docs/WINDOWS-TSF-REGISTRATION.md#§Resulting registry state` | `frontends/tsf/src/registration.cpp#DllUnregisterServer` | none | implemented in code, device-bound: the elevated cycle is a manual checklist |
| TSF18 | docs/SPEC-TSF.md#§TSF18 — the subproject registers its own tests | `frontends/tsf/CMakeLists.txt` | `docs/TESTING.md#§The vocabulary` | `frontends/tsf/CMakeLists.txt` | tsf-test | implemented |
| REL1 | docs/SPEC-RELEASE.md#§REL1 — the release is 0.1.0; -pN names the Android build phase | `docs/MILESTONES.md#§0.1` | `frontends/android/app/build.gradle.kts` | `tools/release-0.1.sh` | none | implemented; board row VERSION-CONVENTION asserts the value |
| REL2 | docs/SPEC-RELEASE.md#§REL2 — release notes are named for the tag | `docs/MILESTONES.md#§0.1` | `docs/release-notes/0.1.0.md` | `tools/release-0.1.sh` | none | implemented; board row RELEASE-PUBLISH-SCRIPT guards the name |
| REL3 | docs/SPEC-RELEASE.md#§REL3 — every 0.1 artefact is buildable from a clean tree | `docs/CI-TIMING.md#§1` | `docs/WINDOWS-INSTALLER.md#§Build it` | `tools/build-release.sh` | none | implemented; board row RELEASE-SCRIPT |
| REL4 | docs/SPEC-RELEASE.md#§REL4 — a publishing script defaults to printing | `docs/ENGINEERING.md` | `tools/release-0.1.sh` | `tools/release-0.1.sh` | none | implemented; board row RELEASE-PUBLISH-SCRIPT greps DRY_RUN=1 |
| REL5 | docs/SPEC-RELEASE.md#§REL5 — a publish step never leaves a partial release | none | `tools/release-0.1.sh` | `tools/release-0.1.sh` | none | none: reviewed, not asserted |
| REL6 | docs/SPEC-RELEASE.md#§REL6 — the package stages data beneath the installed DLL | `docs/WINDOWS-INSTALLER.md#§What ships` | `docs/WINDOWS-INSTALLER.md#§What ships` | `tools/build-windows-installer.sh` | none | implemented; the clean-machine rounds are device-bound |
| REL7 | docs/SPEC-RELEASE.md#§REL7 — the installed settings exe lives at {app}/settings-ui/ | `frontends/tsf/src/settingspath.h` | `docs/WINDOWS-INSTALLER.md#§What ships` | `frontends/tsf/src/settingspath.cpp` | none | implemented; board row TSF-SETTINGS-SHOW-71 |
| REL8 | docs/SPEC-RELEASE.md#§REL8 — uninstall leaves no residue | none | `docs/WINDOWS-INSTALLER.md#§What the installer runs` | `tools/build-windows-installer.sh` | none | none: clean-machine rounds only |
| REL9 | docs/SPEC-RELEASE.md#§REL9 — CI builds the .deb with the suite inside the package build | `.github/workflows/ci.yml` | `docs/CI-TIMING.md#§1` | `debian/rules` | none | implemented; board row CI-DEB-JOB |
| REL10 | docs/SPEC-RELEASE.md#§REL10 — an uploaded artefact is staged into the workspace first | `.github/workflows/ci.yml` | `docs/CI-TIMING.md#§1` | `.github/workflows/ci.yml` | none | implemented; board row CI-DEB-JOB |
| REL11 | docs/SPEC-RELEASE.md#§REL11 — the windows job stays disabled, kept with its ruling | `.github/workflows/ci.yml` | `docs/ENVIRONMENT.md#§Batch 1` | `.github/workflows/ci.yml` | none | implemented; board row CI-WINDOWS-OUT-OF-SCOPE |
| REL12 | docs/SPEC-RELEASE.md#§REL12 — shlibdeps ignores only what no package owns | `debian/rules` | `docs/CI-TIMING.md#§1` | `debian/rules` | none | implemented; board row CI-DEB-SHLIBDEPS |
| REL13 | docs/SPEC-RELEASE.md#§REL13 — time budgets are rules, measured before a push | `AGENTS.md#§Time budgets` | `docs/CI-TIMING.md#§2` | `tools/ci-time-budget.sh` | none | implemented; the tool measures itself |
| A1 | docs/SPEC-OVERVIEW.md#§A1 — layouts are data: various korean and english layouts | `libhunmin/src/schema_loader.c` | `docs/SPEC-SCHEMA.md` | `libhunmin/data/layouts/` | engine-golden-dubeolsik; schema-contract; frontend-invariants | implemented: one golden per layout, and no frontend holds layout code |
| A2 | docs/SPEC-OVERVIEW.md#§A2 — three frontends, one behaviour | `libhunmin/include/hunmin.h#hunmin_buffered_preedit_process_key` | `docs/SPEC-FRONTEND.md` | `libhunmin/src/buffered_preedit.c` | engine-buffered-preedit; engine-key-rule; tsf-mock-tests_conformance_win32_prose.trace.in; engine-android-conformance-tests_conformance_android_basic-syllables-case01.trace.in | implemented: FE1-FE12 hold on all three, with FE7 and FE8 partial as recorded there |
| A3 | docs/SPEC-OVERVIEW.md#§A3 — hanja with word candidates, single char as fallback | `libhunmin/include/hunmin.h#hunmin_suggest_count` | `docs/SPEC-TSF.md#§TSF9` | `libhunmin/src/candidate_tables.c#hunmin_suggest_count` | engine-candidates; tsf-test | implemented |
| A4 | docs/SPEC-OVERVIEW.md#§A4 — both word-level and syllable-level preedit | `libhunmin/include/hunmin.h#HUNMIN_COMMIT_AT_BOUNDARY` | `docs/SPEC-FRONTEND.md#§FE5` | `frontends/tsf/src/delivery.cpp#InterimDelivery` | engine-buffered-preedit; tsf-test | implemented |
| A5 | docs/SPEC-OVERVIEW.md#§A5 — configurable for various aspects | `frontends/tsf/src/config.h#HunminTsfConfig` | `docs/SPEC-TSF.md#§TSF12` | `frontends/tsf/src/config.cpp#hunmin_tsf_config_load` | tsf-test | implemented: one read order; every frontend has its own store |
| A6 | docs/SPEC-OVERVIEW.md#§A6 — every aspect covered, cheaply | `docs/TESTING.md#§The vocabulary` | `docs/SPEC-RELEASE.md#§REL13` | `tools/ci-time-budget.sh` | none | partial: budgets are enforced by the tool, and the suite is label-selected; no single test asserts the coverage claim |
| A7 | docs/SPEC-OVERVIEW.md#§A7 — developed by AI under human instruction | `docs/ORCHESTRATION.md` | `docs/ORCHESTRATION.md` | `tools/orchestrate.sh` | none | process requirement: checked by reading the record, not by a test |
