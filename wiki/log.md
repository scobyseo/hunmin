# Wiki Log

> Append-only record of **wiki actions** — a page created, folded, marked, or
> lint-fixed. It is not a change log for the project: `docs/HISTORY.md` carries
> what shipped, per phase, and a measurement belongs on the page that holds it.
>
> Entries up to 2026-10-09 also carried the per-measurement narrative, which
> made this file a second home for figures `docs/` owns. They were folded into
> their pages on 2026-10-10 and the log now records only the wiki action.
>
> Format: `## [YYYY-MM-DD] action | subject` — create, update, ingest, query,
> lint, archive. Actions: create, update, lint.

## [2026-10-04] create | Wiki initialised

- Adopted `llm-wiki` in full, on the maintainer's decision, after being shown
  the argument against it (git already provides the immutable-source layer and
  an honest log; the entity/concept split would inflate 16 documents into ~100).
- Placed at `wiki/` **inside the repository**, against the skill's `~/wiki`
  default: a wiki outside git has no history to lose state in.
- Three departures from the skill's schema are recorded in `SCHEMA.md`
  §Deviations — `raw/sessions/` is a first-class layer, `entities/`+`concepts/`
  is not split by kind-of-thing, and Obsidian is not required.
- `tools/wiki-lint.py` written with it. A schema with no checker is a
  preference. One of its checks is mechanical and is the one this wiki is for:
  a retracted page still linked from a live one.

## [2026-10-04] create | First concept and decision pages

- `concepts/the-three-corpora-are-three-registers.md`
- `concepts/the-rules-keep-breaking.md`
- `decisions/decisions-belong-to-the-maintainer.md`
- `decisions/the-outstanding-decisions.md` — `contested: true`, four open
  decisions recorded rather than answered.

## [2026-10-04] ingest | Session 2026-10-04 — the stemming investigation

- `raw/sessions/2026-10-04-stemming-investigation.md` — first session record.
  Kept raw because the *sequence* is the finding: the stemming verdict was
  overturned by the second corpus, and no single number explains that.
- Six mistakes recorded, four of which were breaks of rules written earlier the
  same day.

## [2026-10-04] lint | A delegated research batch returned nothing

- Two subagents dispatched; no output and no running process. The delegation id
  exists only in conversation context, so a silent dispatch failure is
  indistinguishable from a slow one — the parallel-work problem in miniature.
  Recorded in `concepts/the-rules-keep-breaking.md` §The parallel-work failure.

## [2026-10-05] ingest | Sessions 2026-10-02 and 2026-10-03, recovered

- Both sessions predating that day, extracted from the maintainer's messages
  rather than summarising my replies: 1,777 and 826 messages, 87 and 139 of
  them the maintainer's.
- What was recovered and why it mattered: the documentation rule and the
  mistake-list convention, both of which every later session re-derived; the
  hanja research that produced `libhunmin/data/krdict/`; and the requirement
  itself, quoted, including the licence distinction (use vs redistribution) that
  a later session got wrong and used to exclude two corpora from use.
- AGENTS.md's standing instructions were extracted from all hunmin sessions' user
  turns. Three of those sessions were in other directories and contributed
  nothing.

## [2026-10-05] update | AGENTS.md host notes

- Added a section for the machine itself, from sessions that were not about this
  project: never kill a daemon on the auth path, never pkill the session bus,
  `sudo -n` always fails but pty sudo works, `dmesg` is empty unprivileged.
- These cost the most damage of anything recorded that week and existed only in
  a transcript. They now live in `docs/ENVIRONMENT.md`.

## [2026-10-05] lint | Drift audit, and the lint check it owed

- A drift audit compared the maintainer's transcript against every document and
  produced six items; each was re-checked against the tree before being acted
  on, and two turned out to be questions only the maintainer could answer. Both
  are closed on [[2026-10-05-closed-questions]]: the romaja registration's
  comment is correct as it stands (it describes a second fcitx5 configuration
  surface, not a second definition of "English mode"), and the GitHub coding
  corpus was curiosity rather than a requirement.
- The same audit found the requirement stated on 2026-10-03 absent from PLAN.md
  entirely, with a `Status: approved plan (2026-09-26)` line that is the *cause*
  rather than a symptom. It is now P8, in the Goal, and in a status table that
  says the engine side has no code at all.
- **Debt found while writing the audit record:** a page required to carry
  measurement provenance pointed at a measurement that was never made, and
  `wiki-lint.py` could not see it — it checked wikilinks and the index, not
  frontmatter `sources:`. **Paid:** `check_sources_exist` in `tools/wiki-lint.py`
  and `check_wiki_sources` in `tools/docs-lint.py`. Both tools' comments still
  describe the check as owed, which is worth deleting.

## [2026-10-09] update | Three measurements, one symptom that did not reproduce

- **The space question.** The requirement is two-part: space must end the
  composition *and* emit the character. The fix applied on 2026-10-07 was a
  layout-data change that broke every libhangul parity test. Re-measured on both
  frontends: **the symptom does not reproduce at HEAD** with that change
  reverted, so no engine change and no regenerated goldens. Full account:
  [[2026-10-09-where-space-is-emitted]].
- **The fcitx5 check the maintainer asked for on 2026-10-07 was marked done
  without being run** — the code was read and the answer inferred. It has now
  been run.
- ✅ **Space emission is a frontend concern** (measured 2026-10-09). The engine
  declines space and flushes the syllable; every frontend (fcitx5 XIM,
  fcitx5 GTK 3/4, Android) forwards the declined space to the client. No
  engine change is needed or planned. → **Superseded by
  [[space-emission-ownership]]**, which records that the measurement page still
  calls the design question the maintainer's and that no ruling is quoted
  anywhere. `docs/REMAINING-WORK.md` B5 cites this line; it should re-point.
- **The voice question, answered** —
  [[2026-10-09-voice-recognition-status]]: the launch path reaches the
  platform's recogniser, recognition itself has never been tested, RECORD_AUDIO
  is never requested, and the mic on the input view was a comment rather than an
  assertion.

## [2026-10-09] update | GTK forward path measured

- A real GTK client for tier D3 (`tests/e2e/gtk-harness.py`, run through
  `run-d3.sh` with `HUNMIN_D3_CLIENT=gtk`; ctest `engine-d3-gtk3`,
  `engine-d3-gtk4`). The declined key becomes text on both GTK versions, by two
  different mechanisms — one of which is an empty function in fcitx5's GTK 4
  module.
- Three ways the harness was green while measuring something else, all now
  checks: `has_focus()` lies under GTK 4; `python3` on `PATH` cannot `import
  gi`; and with `GTK_IM_MODULE=xim` every case passes, because
  `XMODIFIERS=@im=fcitx` routes GTK's XIM module to the same fcitx5.
- Full record: [[2026-10-09-gtk-forward-path]].

## [2026-10-11] update | Character-loss trio, 16K align, fcitx5 Hanja keyboard UX

- **Character-loss trio** — three Android bugs invisible on this machine (hanja commit
  appended; modifier-only `onKeyDown` reaching the engine flushed in-flight composition;
  Android only copied `layouts`/`compose` and passed no `data_dir` to `hunmin_suggest_count`,
  so `hanja.json` showed zero candidates). Guards now land in tier E1 (3 tests) and the
  16K-alignment check script, and a modes-corpus inventory records that
  `modes-corpus-selfcheck` is already SKIP-not-green because the corpus is not deployable.
- **16K alignment** — `libhunjni.so` was 4KB-aligned from the separate AGP
  `externalNativeBuild`; pinned `max-page-size=16384` in that CMake. The `${HUNMIN_DATA_PATH}`,
  Korean-locale, and NDK-path facts are environment-local and do not belong in the repo.
- **fcitx5 Hanja UX** — vertical layout hint, plus keyboard selection (Up/Down/PageUp/PageDown/
  Return/Escape). `CommonCandidateList::moveCursor` rotates over `totalSize()` unless
  `cursorKeepInSamePage` is set (it is not), so Up/Down reaches every candidate; measured
  against a replica, not assumed, and the note in the code says which way the measurement cut.
  I8 budget 465 → 545 (reason recorded, not code shaved). D2 tier verifies.
- **Android conformance cold-run failure, closed** (the "found, not fixed" item
  in the 10-10 entry below) — two causes behind one red. First:
  `tools/build-host-jni.sh`'s ldd proof entered the runner's stdout, which
  `trace-check.py` parses entirely as JSON; the call site now routes it to
  stderr. That fix exposed the second, which only `dpkg-buildpackage` shows:
  its `export LC_ALL=C` makes Java's stdout charset US-ASCII, so every Korean
  preedit became `?` while the JSON parsed fine — Python's PEP 540 coercion is
  why the x11 half stayed green beside it. The runner pins UTF-8 by
  declaration now. Both rules are in `docs/trace-spec.md`; deb build passes
  end to end, 14/14 cold.

## [2026-10-10] create | D3 case coverage, mutation tiers, ctest labels

- [[2026-10-10-d3-case-coverage]] — `engine-d3-x11` reported Passed in 5.01 s
  having run one of its eight cases, and hid a second defect behind the first.
  `docs/D3-PERFORMANCE.md` and `docs/TESTING.md` carry the timings; the evidence
  trail is the wiki page, `docs/HISTORY.md` the entry.
- `tools/android-mutations.sh` counted its own skips as catches: *"PASS: all 9
  planted mutations were caught"*, exit 0, 0.31 s, nothing built. A checkout
  whose suite was already red read the same way. Both closed. Measurements in
  `docs/ANDROID-MUTATION-SANDBOX.md`; the two parts that generalise on
  [[2026-10-10-mutation-tiers-measuring-nothing]].
- A `LABELS` property on every `add_test` registration, plus
  `tools/ctest-select.sh`, which prints the selection **and every omitted test**
  before running — a filtered run that passes otherwise reads exactly like a
  full run that passed. Reasoning and the three designs not taken:
  [[2026-10-10-ctest-labels]].
- **Mistake, recorded so it is not repeated:** `add_test(NAME x COMMAND y LABELS
  z)` does not set a label. `LABELS` is a property; the wrong form appends two
  argv entries, configure succeeds and `ctest -N` still reports the same total.
- **Found, not fixed:** every `engine-android-conformance-*` test fails on a
  cold `build/libhunjni-host.so`, because `tools/build-host-jni.sh` prints its
  `ldd` proof to stdout and `tools/trace-emit`'s consumer parses line 1 as
  JSON. Deterministic, 3/3 cold runs. Those harness files belong to the Android
  work in another worktree. *(Fixed 2026-10-11, below. The record of how it
  read on 10-10 stays because fixing it exposed a second cause behind the same
  red, and that one was only visible from inside `dpkg-buildpackage`.)*

## [2026-10-10] update | Orca GUI probe, retracted same day

- The Orca worktree probe was first written as two wiki pages. The maintainer's
  question — *"does this belong in the LLM wiki?"* — was correct: it is a **host
  toolchain fact**, so per `SCHEMA.md`'s domain rule the durable facts moved to
  `docs/ENVIRONMENT.md` §Orca GUI. The measurement page is marked `superseded`,
  the session page reframed as the evidence trail, and the skill cites
  `docs/ENVIRONMENT.md`. The reasoning behind the retraction is kept on the
  measurement page, because it is a reusable test and not a host fact.

## [2026-10-11] update | Settings dialog and Hanja dialog fixes

- **Settings dialog crash fixed**: OverlayAlertDialog was using a custom theme that conflicted with AlertDialog. Fixed by using default theme and proper FLAG_ACTIVITY_NEW_TASK launch.
- **Hana dialog working**: Hanja dialog now shows candidates via OverlayActivity with ACTION_SHOW_HANJA. Commit works via commitExternalText.
- **Settings gear button**: Added settings button in LayoutPicker that triggers OverlayActivity with ACTION_SHOW_SETTINGS.
- **Space during composition**: When space is pressed during composition, syllable is committed and space character is emitted.
- **Hangul/English toggle flush**: Mode switch now flushes current composition before switching engines to prevent text loss.
- **Overlay lifecycle fixed**: onPause/onDestroy now respect dialog showing state; dialogs remain visible during configuration changes.
- **Samsung 한/영 key (218)**: System consumes this key before IME receives it; alternative keys (Left Alt+Z, Right Alt, Language Switch) work reliably.
- **Settings dialog crash (Resource ID)**: Fixed AlertDialog.Builder to use default context theme instead of custom theme.

- **Folded.** The 2026-10-04 stemming measurements → `concepts/
  the-three-corpora-are-three-registers.md` (two homes for one table is how they
  drift). The 2026-10-02 practices → `concepts/where-the-practices-came-from.md`
  (new; `AGENTS.md` can state each rule in one line but not the reason it
  survives). The five "green while measuring nothing" instances →
  `concepts/the-rules-keep-breaking.md`, with the two mechanical defences. The
  mutation-tier detail → `docs/ANDROID-MUTATION-SANDBOX.md` pointers.
- **Kept against the compression default.** `raw/sessions/
  2026-10-03-requirement-shaped.md` — seven quotations the requirement rests on
  exist nowhere in `docs/`, and a paraphrase drifts.
- **New.** `decisions/space-emission-ownership.md`, `contested: true`: the log
  and `docs/REMAINING-WORK.md` B5 record the space-emission question as closed
  with no quoted ruling, and the measurement page says it is still the
  maintainer's. Named, not resolved.
- **Dropped as pure timeline:** the `~/.hermes/state.db` extraction recipe, the
  cirrussearch hour, the drift audit's per-item commands, the four D3-GTK
  harness construction steps, and every session-message count.
- **Commit hashes removed** from the wiki; prose names the change instead.
- `tools/wiki-lint.py --strict` and `tools/docs-lint.py` both clean.
## [2026-10-11] create | B3 ruled: auto-switch OFF by default, data-gated

- **New decision.** `decisions/2026-10-11-auto-switch-off-by-default.md`: the
  maintainer ruled B3 — automatic Hangul/English switch is OFF by default until
  release 0.3, and its additional data ships as a separate distribution, the
  feature enable-able only where that data is present. The ruling is quoted
  verbatim in `raw/sessions/2026-10-11-b3-off-default-and-the-data-gate.md`.
- **Consequential updates, not wiki actions:** `docs/SPEC-MODES.md` R13 and E5
  (the default was undecided in two places; both updated in one pass, because
  updating one is the two-homes defect §7 of `docs/REMAINING-WORK.md` describes)
, `docs/REMAINING-WORK.md` B3 row and voice stage 8, `docs/MILESTONES.md` 0.1/0.3
  lanes. D1, E5-assignment and B2 are named as NOT closed by this ruling.
- `wiki/index.md` listed both pages; total pages 21.
## [2026-10-11] create | Orca orchestration path measured; playbook + script

- **New session page:** `raw/sessions/2026-10-11-orca-orchestration-path.md` — the
  ≈40-turn / two-accidental-Runs cost of guessing `orca orchestration` flags that one
  `orca agent-context --json` answers, and the four places the documents were wrong
  about what Orca does (nested placement, `scobyseo/` branch prefix, no `--issue` on
  `worker-start`, worker's terminal absent from `worker-list`'s projection).
  `docs/ENVIRONMENT.md` §Orca GUI is corrected on the branch-name claim.
- **New durable artefact:** `docs/ORCHESTRATION.md` (procedure) and
  `tools/orchestrate.sh` (the same sequence, with the branch/base/issue defaults,
  validating every flag against the binary's own schema before use).
- **Consequential, not wiki actions:** `AGENTS.md` gains an Orchestration section that
  points at the procedure and states the rules (acceptance command in the spec, the
  preamble owns `worker_done`, a worker never closes anything).
- **Left for the maintainer, on purpose:** the two orphaned Runs and one
  never-dispatched Task from the earlier session are reported in
  `docs/ORCHESTRATION.md` §5 and not reset.
- `wiki/index.md` lists the page; total pages 22.
## [2026-10-11] create | Key settings window measured and fixed; N5's tests found never compiled

- **New measurement page:** `raw/measurements/2026-10-11-key-settings-window.md` — three
  broken layers (gear button off-screen at 1140 px on a 1080 px screen; the window could
  not receive a key because hardware keys are dispatched to the IME window while an IME
  is current; the save wrote a key nothing read), the two platform facts that shaped the
  harness (`input keyevent` has no `--meta`; `OverlayActivity` is correctly not exported),
  and the numeric binding format with the reason.
- **New durable artefacts:** `frontends/android/app/src/main/java/org/hunmin/ime/KeyBinding.kt`
  and a tier-E2 section in `tools/android-e2e.sh` that drives the real window on the
  emulator and cleans up after itself.
- **Consequential, not wiki actions:** `docs/REMAINING-WORK.md` voice stage 7 (Planned →
  Done) and two new §7.5 assertion rows that the tree can contradict.
- **A finding, not a verdict:** N5's `frontends/android/app/src/androidTest/` does not
  compile and no tier builds it; its board row asserts only that the directory is present.
  Filed as its own issue; scope is the maintainer's.
- `wiki/index.md` lists the page; total pages 23.

## 2026-10-11 — the dock, and a style that measured nothing

- **Measurements, not decisions:** the maintainer's specified toolbar (floating pill,
  48 dp, status left / actions right) implemented as Views rather than Compose, because
  the repository has no Compose dependency and adding one is a build, a startup cost and
  an APK size rather than a rendering choice. Recorded, with the reasoning, in
  `wiki/raw/measurements/2026-10-11-dock-and-the-zero-width-style.md`.
- **Phosphor Icons, at the maintainer's choice**, for three glyphs; the mic is Duotone
  because its 0.2-opacity silhouette is the listening state. MIT terms in
  `debian/copyright`, upstream paths and the conversion in
  `frontends/android/licences/phosphor-icons/README.md`.
- **A finding:** style C rendered no content at all — `lp(0)` in a *vertical*
  LinearLayout is zero pixels wide, not "take the leftover", which is what it means in a
  horizontal one. Measured on the phone: the IME window's a11y tree had the rows and no
  children, and a raw pixel of the band was the window background. Fixed with
  `MATCH_PARENT`; styles A and B were never affected, which is why it survived the crash
  fix that preceded it.
- **A crash fixed and verified on the device** (SM-F971N, Android 17):
  `IllegalStateException: The specified child already has a parent` on a style change,
  because `rebuild()` left every button parented to the detached row. Style A is the
  default and `applyStyle()` short-circuits, which is why it looked like "every other
  style crashes" rather than "the rebuild crashes". Fresh views per rebuild now; the mic
  key carries 漢 rather than `ic_menu_add`, which said "add something".
- **Not settled, and not decided here:** whether style C earns its place now that the
  dock exists, and whether Material Components is worth taking as a dependency for the
  shapes this draws by hand.
- `wiki/index.md` lists the page.
- **A real defect, found by measurement and fixed:** the dock's `bind()` repainted
  an `ImageView` never added to the view tree, so every mic state change was
  invisible while the log said LISTENING. The a11y dump could not see it because
  the session lasts ~5 s and a dump takes longer than that — the read landed after
  the state had ended. Validated the replacement pixel gate against both states of
  the same screenshot: `#43A047` listening, `#F3EDF7` idle.
- **Tier E4 written** (`tools/android-voice-check.sh`, `tests/manual-checklists/tier-e4-voice.md`):
  the script gates our half of the voice path and a person judges intelligibility,
  because `SpeechRecognizer` cannot be replayed and an accuracy threshold would be
  a network-day test. C turned out to already exist: `HunminEngineTest` gates
  invariant 5 at E1, so E4 tests the path around it.
- **Still missing:** a completed E4 run end to end. The script asks a person for a
  focused editor by design and refuses otherwise; each of its state and pixel checks
  has been measured by hand on SM-S938N, the loop has not.
