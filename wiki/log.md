# Wiki Log

> Append-only record of **wiki actions** ??a page created, folded, marked, or
> lint-fixed. It is not a change log for the project: `docs/HISTORY.md` carries
> what shipped, per phase, and a measurement belongs on the page that holds it.
>
> Entries up to 2026-10-09 also carried the per-measurement narrative, which
> made this file a second home for figures `docs/` owns. They were folded into
> their pages on 2026-10-10 and the log now records only the wiki action.
>
> Format: `## [YYYY-MM-DD] action | subject` ??create, update, ingest, query,
> lint, archive. Actions: create, update, lint.

## [2026-10-04] create | Wiki initialised

- Adopted `llm-wiki` in full, on the maintainer's decision, after being shown
  the argument against it (git already provides the immutable-source layer and
  an honest log; the entity/concept split would inflate 16 documents into ~100).
- Placed at `wiki/` **inside the repository**, against the skill's `~/wiki`
  default: a wiki outside git has no history to lose state in.
- Three departures from the skill's schema are recorded in `SCHEMA.md`
  짠Deviations ??`raw/sessions/` is a first-class layer, `entities/`+`concepts/`
  is not split by kind-of-thing, and Obsidian is not required.
- `tools/wiki-lint.py` written with it. A schema with no checker is a
  preference. One of its checks is mechanical and is the one this wiki is for:
  a retracted page still linked from a live one.

## [2026-10-04] create | First concept and decision pages

- `concepts/the-three-corpora-are-three-registers.md`
- `concepts/the-rules-keep-breaking.md`
- `decisions/decisions-belong-to-the-maintainer.md`
- `decisions/the-outstanding-decisions.md` ??`contested: true`, four open
  decisions recorded rather than answered.

## [2026-10-04] ingest | Session 2026-10-04 ??the stemming investigation

- `raw/sessions/2026-10-04-stemming-investigation.md` ??first session record.
  Kept raw because the *sequence* is the finding: the stemming verdict was
  overturned by the second corpus, and no single number explains that.
- Six mistakes recorded, four of which were breaks of rules written earlier the
  same day.

## [2026-10-04] lint | A delegated research batch returned nothing

- Two subagents dispatched; no output and no running process. The delegation id
  exists only in conversation context, so a silent dispatch failure is
  indistinguishable from a slow one ??the parallel-work problem in miniature.
  Recorded in `concepts/the-rules-keep-breaking.md` 짠The parallel-work failure.

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
  `wiki-lint.py` could not see it ??it checked wikilinks and the index, not
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
  without being run** ??the code was read and the answer inferred. It has now
  been run.
- ??**Space emission is a frontend concern** (measured 2026-10-09). The engine
  declines space and flushes the syllable; every frontend (fcitx5 XIM,
  fcitx5 GTK 3/4, Android) forwards the declined space to the client. No
  engine change is needed or planned. ??**Superseded by
  [[space-emission-ownership]]**, which records that the measurement page still
  calls the design question the maintainer's and that no ruling is quoted
  anywhere. `docs/REMAINING-WORK.md` B5 cites this line; it should re-point.
- **The voice question, answered** ??  [[2026-10-09-voice-recognition-status]]: the launch path reaches the
  platform's recogniser, recognition itself has never been tested, RECORD_AUDIO
  is never requested, and the mic on the input view was a comment rather than an
  assertion.

## [2026-10-09] update | GTK forward path measured

- A real GTK client for tier D3 (`tests/e2e/gtk-harness.py`, run through
  `run-d3.sh` with `HUNMIN_D3_CLIENT=gtk`; ctest `engine-d3-gtk3`,
  `engine-d3-gtk4`). The declined key becomes text on both GTK versions, by two
  different mechanisms ??one of which is an empty function in fcitx5's GTK 4
  module.
- Three ways the harness was green while measuring something else, all now
  checks: `has_focus()` lies under GTK 4; `python3` on `PATH` cannot `import
  gi`; and with `GTK_IM_MODULE=xim` every case passes, because
  `XMODIFIERS=@im=fcitx` routes GTK's XIM module to the same fcitx5.
- Full record: [[2026-10-09-gtk-forward-path]].

## [2026-10-11] update | Character-loss trio, 16K align, fcitx5 Hanja keyboard UX

- **Character-loss trio** ??three Android bugs invisible on this machine (hanja commit
  appended; modifier-only `onKeyDown` reaching the engine flushed in-flight composition;
  Android only copied `layouts`/`compose` and passed no `data_dir` to `hunmin_suggest_count`,
  so `hanja.json` showed zero candidates). Guards now land in tier E1 (3 tests) and the
  16K-alignment check script, and a modes-corpus inventory records that
  `modes-corpus-selfcheck` is already SKIP-not-green because the corpus is not deployable.
- **16K alignment** ??`libhunjni.so` was 4KB-aligned from the separate AGP
  `externalNativeBuild`; pinned `max-page-size=16384` in that CMake. The `${HUNMIN_DATA_PATH}`,
  Korean-locale, and NDK-path facts are environment-local and do not belong in the repo.
- **fcitx5 Hanja UX** ??vertical layout hint, plus keyboard selection (Up/Down/PageUp/PageDown/
  Return/Escape). `CommonCandidateList::moveCursor` rotates over `totalSize()` unless
  `cursorKeepInSamePage` is set (it is not), so Up/Down reaches every candidate; measured
  against a replica, not assumed, and the note in the code says which way the measurement cut.
  I8 budget 465 ??545 (reason recorded, not code shaved). D2 tier verifies.
- **Android conformance cold-run failure, closed** (the "found, not fixed" item
  in the 10-10 entry below) ??two causes behind one red. First:
  `tools/build-host-jni.sh`'s ldd proof entered the runner's stdout, which
  `trace-check.py` parses entirely as JSON; the call site now routes it to
  stderr. That fix exposed the second, which only `dpkg-buildpackage` shows:
  its `export LC_ALL=C` makes Java's stdout charset US-ASCII, so every Korean
  preedit became `?` while the JSON parsed fine ??Python's PEP 540 coercion is
  why the x11 half stayed green beside it. The runner pins UTF-8 by
  declaration now. Both rules are in `docs/trace-spec.md`; deb build passes
  end to end, 14/14 cold.

## [2026-10-10] create | D3 case coverage, mutation tiers, ctest labels

- [[2026-10-10-d3-case-coverage]] ??`engine-d3-x11` reported Passed in 5.01 s
  having run one of its eight cases, and hid a second defect behind the first.
  `docs/D3-PERFORMANCE.md` and `docs/TESTING.md` carry the timings; the evidence
  trail is the wiki page, `docs/HISTORY.md` the entry.
- `tools/android-mutations.sh` counted its own skips as catches: *"PASS: all 9
  planted mutations were caught"*, exit 0, 0.31 s, nothing built. A checkout
  whose suite was already red read the same way. Both closed. Measurements in
  `docs/OPTIONS-MEASURED.md` 짠AND; the two parts that generalise on
  [[2026-10-10-mutation-tiers-measuring-nothing]].
- A `LABELS` property on every `add_test` registration, plus
  `tools/ctest-select.sh`, which prints the selection **and every omitted test**
  before running ??a filtered run that passes otherwise reads exactly like a
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
  question ??*"does this belong in the LLM wiki?"* ??was correct: it is a **host
  toolchain fact**, so per `SCHEMA.md`'s domain rule the durable facts moved to
  `docs/ENVIRONMENT.md` 짠Orca GUI. The measurement page is marked `superseded`,
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
- **Samsung ????key (218)**: System consumes this key before IME receives it; alternative keys (Left Alt+Z, Right Alt, Language Switch) work reliably.
- **Settings dialog crash (Resource ID)**: Fixed AlertDialog.Builder to use default context theme instead of custom theme.

- **Folded.** The 2026-10-04 stemming measurements ??`concepts/
  the-three-corpora-are-three-registers.md` (two homes for one table is how they
  drift). The 2026-10-02 practices ??`concepts/where-the-practices-came-from.md`
  (new; `AGENTS.md` can state each rule in one line but not the reason it
  survives). The five "green while measuring nothing" instances ??  `concepts/the-rules-keep-breaking.md`, with the two mechanical defences. The
  mutation-tier detail ??`docs/OPTIONS-MEASURED.md` 짠AND pointers.
- **Kept against the compression default.** `raw/sessions/
  2026-10-03-requirement-shaped.md` ??seven quotations the requirement rests on
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
  maintainer ruled B3 ??automatic Hangul/English switch is OFF by default until
  release 0.3, and its additional data ships as a separate distribution, the
  feature enable-able only where that data is present. The ruling is quoted
  verbatim in `raw/sessions/2026-10-11-b3-off-default-and-the-data-gate.md`.
- **Consequential updates, not wiki actions:** `docs/SPEC-MODES.md` ReqMode13 and E5
  (the default was undecided in two places; both updated in one pass, because
  updating one is the two-homes defect 짠7 of `docs/REMAINING-WORK.md` describes)
, `docs/REMAINING-WORK.md` B3 row and voice stage 8, `docs/MILESTONES.md` 0.1/0.3
  lanes. D1, E5-assignment and B2 are named as NOT closed by this ruling.
- `wiki/index.md` listed both pages; total pages 21.
## [2026-10-11] create | Orca orchestration path measured; playbook + script

- **New session page:** `raw/sessions/2026-10-11-orca-orchestration-path.md` ??the
  ??0-turn / two-accidental-Runs cost of guessing `orca orchestration` flags that one
  `orca agent-context --json` answers, and the four places the documents were wrong
  about what Orca does (nested placement, `scobyseo/` branch prefix, no `--issue` on
  `worker-start`, worker's terminal absent from `worker-list`'s projection).
  `docs/ENVIRONMENT.md` 짠Orca GUI is corrected on the branch-name claim.
- **New durable artefact:** `docs/ORCHESTRATION.md` (procedure) and
  `tools/orchestrate.sh` (the same sequence, with the branch/base/issue defaults,
  validating every flag against the binary's own schema before use).
- **Consequential, not wiki actions:** `AGENTS.md` gains an Orchestration section that
  points at the procedure and states the rules (acceptance command in the spec, the
  preamble owns `worker_done`, a worker never closes anything).
- **Left for the maintainer, on purpose:** the two orphaned Runs and one
  never-dispatched Task from the earlier session are reported in
  `docs/ORCHESTRATION.md` 짠5 and not reset.
- `wiki/index.md` lists the page; total pages 22.
## [2026-10-11] create | Key settings window measured and fixed; N5's tests found never compiled

- **New measurement page:** `raw/measurements/2026-10-11-key-settings-window.md` ??three
  broken layers (gear button off-screen at 1140 px on a 1080 px screen; the window could
  not receive a key because hardware keys are dispatched to the IME window while an IME
  is current; the save wrote a key nothing read), the two platform facts that shaped the
  harness (`input keyevent` has no `--meta`; `OverlayActivity` is correctly not exported),
  and the numeric binding format with the reason.
- **New durable artefacts:** `frontends/android/app/src/main/java/org/hunmin/ime/KeyBinding.kt`
  and a tier-E2 section in `tools/android-e2e.sh` that drives the real window on the
  emulator and cleans up after itself.
- **Consequential, not wiki actions:** `docs/REMAINING-WORK.md` voice stage 7 (Planned ??  Done) and two new 짠7.5 assertion rows that the tree can contradict.
- **A finding, not a verdict:** N5's `frontends/android/app/src/androidTest/` does not
  compile and no tier builds it; its board row asserts only that the directory is present.
  Filed as its own issue; scope is the maintainer's.
- `wiki/index.md` lists the page; total pages 23.

## 2026-10-11 ??the dock, and a style that measured nothing

- **Measurements, not decisions:** the maintainer's specified toolbar (floating pill,
  48 dp, status left / actions right) implemented as Views rather than Compose, because
  the repository has no Compose dependency and adding one is a build, a startup cost and
  an APK size rather than a rendering choice. Recorded, with the reasoning, in
  `wiki/raw/measurements/2026-10-11-dock-and-the-zero-width-style.md`.
- **Phosphor Icons, at the maintainer's choice**, for three glyphs; the mic is Duotone
  because its 0.2-opacity silhouette is the listening state. MIT terms in
  `debian/copyright`, upstream paths and the conversion in
  `frontends/android/licences/phosphor-icons/README.md`.
- **A finding:** style C rendered no content at all ??`lp(0)` in a *vertical*
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
  key carries 轢?rather than `ic_menu_add`, which said "add something".
- **Not settled, and not decided here:** whether style C earns its place now that the
  dock exists, and whether Material Components is worth taking as a dependency for the
  shapes this draws by hand.
- `wiki/index.md` lists the page.
- **A real defect, found by measurement and fixed:** the dock's `bind()` repainted
  an `ImageView` never added to the view tree, so every mic state change was
  invisible while the log said LISTENING. The a11y dump could not see it because
  the session lasts ~5 s and a dump takes longer than that ??the read landed after
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

## [2026-10-12] create | Compact HUD theme traps; Android UI notes

- **Durable artefact is `docs/ANDROID-UI-NOTES.md`**, the Android analogue of
  `docs/FCITX5-ADDON-NOTES.md`: the style system (dock palette vs `CompactTheme`,
  row anatomy, 0.92-width window) and the traps that cost the restyle its time ??  `AlertDialog.setView()` adopting the view's default `WRAP_CONTENT`
  LayoutParams, `android:background` as a *theme item* leaking to every
  inheriting view (the "onion" of rounded frames, and the starved column with
  the same symptom from a different mechanism), a host activity themed
  `Dialog.Alert` nesting a second alert chrome, and the badge-weight/compresses-
  the-title failure that came first.
- **On-device verification facts recorded so the next session does not
  re-derive them:** production builds admit neither `adb root` nor `su`, so
  `OverlayActivity` is reached by the gear tap only; wireless adb corrupts
  `exec-out screencap` PNGs (capture to file, then `pull`); a dual-display
  Samsung's default `screencap` display is not the one `uiautomator` sees
  (`-d <display-id>`); stale stacked `OverlayActivity` windows make a correct
  fix look wrong (force-stop before trusting a screenshot).
- **`tools/android-e2e.sh` went red on a correct UI**: its combined-string
  assertions matched the pre-restyle row shape, and the restyle split title
  and badge into two TextViews. Updated in the same branch as the structure
  they assert. See `wiki/raw/sessions/2026-10-12-compact-hud-theme-traps.md`.

- **2026-10-12** ??`2026-10-11-dock-and-the-zero-width-style` marked
  **superseded**: the maintainer ruled "F5 -> remove!" and the unreachable
  `DockToolbar`/`DockStyle.kt` is deleted (issue #33, branch
  `task/android-design-review`). The page keeps the measurement as
  evidence; its outgoing wikilinks were unlinked so no active page cites a
  retracted one.

## [2026-10-12] create | Issue #9 ruled: versionName keeps the phase suffix

- **New decision.** `decisions/2026-10-12-versionname-keeps-the-phase-suffix.md`:
  the maintainer ruled, answering issue #9, that release 0.1's Android
  `versionName` stays `0.1.0-p5` ??`0.1.0` is the release (agreeing with
  `CMakeLists.txt` and `debian/changelog`), and `-pN` is the Android
  build-phase label (P5 the IME, P5.5 the keymaps APK; `docs/ENVIRONMENT.md`
  batch 2). The ruling was given in the worktree session that implemented it,
  not in a separate session record, so the issue is the surviving source.
- **Consequential updates, not wiki actions:** `docs/MILESTONES.md` 짠0.1 now
  records the convention (the issue's second verification checkbox), and
  `README.md` 짠Install says which string is the release and which is the
  phase (the issue's "keeps the phase marker" branch). No version string
  changed ??the ruling is to keep them.
- **NOT closed by the ruling, named so it is not re-derived:** `versionCode = 1`
  has never been incremented, and no tag or GitHub release exists yet; both
  are release-time acts, not documentation.
- `wiki/index.md` listed the page; total pages 26.

## [2026-10-12] create | B4's 0.1 half ruled: auto toggle inert, default key composition disabled

- **New decision.** `decisions/2026-10-12-auto-toggle-disabled-by-default-in-0-1.md`:
  the maintainer ruled the 0.1 shipping half of B4 (issue #2) ??the auto
  Hangul/English toggle is disabled by default in 0.1, and the named mechanism
  is "disable the default key composition". In the tree: `AUTO_TOGGLE` gets no
  default chord (Right Alt + Enter unbound; Enter reaches the editor), the
  service dispatch skips the action so a stale stored binding cannot swallow
  the chord, and the settings row renders disabled with badge 誘몄젙 (0.3) ??  the ReqMode13 divergence (screen advertising a chord whose action logs "not
  yet implemented") removed. The ruling is quoted verbatim in
  `raw/sessions/2026-10-12-b4-auto-toggle-inert-in-0-1.md`.
- **What it does not close:** the E5 assignment itself (single vs compound),
  B1/D1, B2 ??all named on the decision page; the assignment stays a 0.3-lane
  item per `docs/MILESTONES.md`.
- **Consequential updates, not wiki actions:** `docs/SPEC-MODES.md` E5 (0.1
  shipping-state bullet), `docs/REMAINING-WORK.md` B4 row, voice stage 2 and
  stage 8, `docs/MILESTONES.md` 0.1 lane; `KeyBindingTest` pins the binding-layer
  behaviour (no default, stored binding still matches, dispatch skip is the
  service's).
- `wiki/index.md` listed both pages; total pages 28.

## [2026-10-13] update | F9 candidate window measured, then capped (issue #45)

- **New measurement.** `raw/measurements/2026-10-13-f9-candidate-window-abort.md`
  (issue #45): F9 on a syllable with 11 or more candidates aborts fcitx5 with
  `CommonCandidateList: invalid label idx` (SIGABRT, exit 134). The page size is
  set to the candidate count (`frontends/fcitx5/src/engine.cpp:509`) and
  classicui reads one label per candidate on the page, while a
  `CommonCandidateList` carries 10 labels unless `setLabels()` is called.
  Threshold measured, not remembered: 10 candidates survive, 11 abort; 67
  candidates with the default page size 5 do not abort. The vertical layout hint
  is a bystander (Horizontal throws at the same index). Not local: the installed
  0.1.0 addon against `/usr/share/hunmin` aborts identically. The issue's
  `hunmin_suggest_text` reading is refuted ??it is a bounded read that does not
  rebuild ??and a separate cross-context clobbering of the process-wide
  candidate list is recorded alongside it. **No ruling:** what the page size
  should be, or whether labels should be supplied, is the maintainer's.
- **New procedure.** `tools/repro-f9-candidates.sh`: the full stack under Xvfb
  with classicui enabled, which no ctest tier is. It carries the two findings
  that cost time here: `XK_F9` is 0xFFC6 (0xFFC2 is F5), and `FCITX_LOG_RULE` is
  not read by fcitx5 ??the rule is `--verbose`, and `tests/e2e/run-d3.sh`'s
  export of it has never had any effect.
- `wiki/index.md` listed the new page; total pages 29.
- **Ruling applied (issue #45).** The maintainer ruled the F9 candidate page is
  **capped at 10** ??fcitx5's label ceiling ??rather than holding the whole
  list on one page with `setLabels()`. In the tree:
  `frontends/fcitx5/src/engine.cpp` carries a named `kMaxCandidatePageSize = 10`
  (named because the number is fcitx5's `regularLabelSize`, not a preference),
  and `frontends/fcitx5/test/testhunminui.cpp` asserts
  `pageSize == min(totalSize, 10)` ??measured red with the cap reverted
  ("page size is 67"), green with it. Verified end to end with classicui
  enabled: ??11) and 媛?67) no longer abort, and Page_Down reaches pages 2 and
  3 with the cursor on the first candidate of each, Up wraps to the last, so
  the cap made nothing unreachable. **Not closed by the cap:** no ctest tier
  draws the F9 window, so the abort still cannot regress through `ctest` alone
  and `tools/repro-f9-candidates.sh` runs in no CI job. Recorded in the
  measurement page's "What it did not close".

## [2026-10-13] update | The deb job's last mile (dpkg-shlibdeps) measured; windows job disabled

- **Two CI jobs red, and the causes are unrelated.** Measured on run 38398739533,
  and the same two on `master`: `debian (.deb artefact)` died at `dh_shlibdeps`
  *after* all 63 tests had passed inside the package build ??  `no dependency information found for
  /lib/x86_64-linux-gnu/libFcitx5Core.so.7 (used by .../fcitx5/libhunmin.so)` ??  while `windows` died before compiling a line,
  `cl : command line error D8021: invalid numeric argument '/Wextra'`.
- **The deb cause was measured, not guessed.** dpkg answers that question with
  `dpkg -S <library-file>` plus the owning package's `.symbols`/`.shlibs`, and the
  CI job installs fcitx5 5.1.13 **from source** into /usr: the dependency is
  satisfied, the package metadata does not exist. It is the same fact the job
  already encodes by passing `-d` to `dpkg-buildpackage`.
- **The fix was chosen by measurement, before it was committed.** The CI shape
  reproduces locally with `--admindir` pointing at an empty dpkg database:
  identical error, nonzero exit; `--ignore-missing-info` ??rc 0.
  `debian/shlibs.local` does not apply ??still fails, in both the soname and the
  absolute-path form the man page allows. So `debian/rules` gained one
  `override_dh_shlibdeps` line and one comment recording what it does not assert.
  It is not a blanket waiver: the flag skips only what cannot be *resolved*, and
  building the .deb on this host (fcitx5 packaged) still produces
  `Depends: ... libfcitx5core7 (>= 5.1.19) ...`. Only on a source-built host do
  those three drop out, leaving `Depends: fcitx5` ??unversioned, where the addon
  needs >= 5.1.13. That gap is a packaging decision and is left open.
- **A second deb failure, invisible until the first was fixed.** With
  `dh_shlibdeps` past, the build ran all 63 tests and wrote
  `../hunmin_0.1.0_amd64.deb` ??and the *next* step failed:
  `Invalid pattern '../hunmin_*_*.deb'. Relative pathing '.' and '..' is not
  allowed.` `upload-artifact@v4` refuses a pattern reaching above the
  workspace, so the package is now copied into `artifacts/` before the upload.
  Worth writing down for the reason it is a finding rather than a nuisance: a
  job cannot report a second failure until the first one is gone, so "the job
  is nearly green" is a claim about the order defects appear in. The same step
  also printed `tar subprocess returned error exit status 2` ??that was EPIPE
  from `dpkg-deb --contents | head`, not a broken package, and it masked the
  step's exit status into the bargain; it now goes through a file.
- **Ruling applied (issue #51, item 2).** "for windows, it is not scope of this
  version. so in the issue we will disable windows test." The job carries
  `if: false` with the ruling quoted beside it, and the block is kept rather than
  deleted so "was Windows ever built here, and why did it stop" stays
  answerable. The `/Wextra` flag is deliberately **not** guarded with
  `if(NOT MSVC)`: that would turn the job green, which is a different decision
  from the one ruled. No other job in the workflow depends on it.
- **Where the matrix is now narrower, stated where it had been claimed otherwise.**
  `docs/ENVIRONMENT.md` said the `windows-latest` job "covers the MSVC matrix"
  and that "CI carries the burden from day one" for P6; its job table was two
  jobs out of date. `docs/TESTING.md`'s ABI bullet claimed "CI builds the core
  on linux-gcc, mingw and MSVC". All three are corrected, and
  `docs/OPTIONS-MEASURED.md` 짠6.1's "four jobs, no nightly" is corrected
  with them. While the job is disabled, **no job compiles the core with `cl`**.
- **Not done, deliberately, and tracked elsewhere:** whether the F9 candidate
  window gets a ctest tier, a CI job, or neither is a tier-design decision. It
  was item 1 of issue #51; at the maintainer's instruction ??"F9 ?뚯뒪?몃뒗 蹂꾨룄
  ?댁뒋濡??깅줉?섍퀬 ???묒뾽 踰붿쐞?먯꽌 ?쒖쇅." ??it is now **issue #53**, and its
  board row is untouched and still asserts `absent`. #51 keeps only the windows
  job, and its title says so. See
  [[2026-10-13-f9-candidate-window-abort]] for what the cap did not close, and
  [[decisions-belong-to-the-maintainer]] for why one of these two is recorded as
  a ruling and the other as a measurement.

## [2026-10-13] rule | B8 keystore path contradiction resolved (issue #1)

- **Ruling applied (issue #1 / B8).** The maintainer ruled the
  keystore config lives in gitignored `local.properties`
  (`frontends/android/local.properties`); nothing in committed build config
  hardcodes a host path. Both `app` and `keymaps` resolve
  `storeFile`/`storePassword`/`keyPassword`/`keyAlias` from `local.properties`,
  honor the `HUNMIN_RELEASE_STORE` env override (CI `android-release` job +
  `tools/build-release.sh`), and generate a throwaway release key at build time
  when no keystore is present. Verified: `grep -rn "/home/scoby"`
  across `frontends/android/*.kts` -> `NONE`; both modules parse under Gradle.
  Ruling recorded in `wiki/decisions/2026-10-13-b8-keystore-path-resolved.md`.
- `wiki/index.md` listed the page; total pages 29.

## [2026-10-15] create | F9 candidate window gets a ctest tier (issue #53)

- **New measurement.** `raw/measurements/2026-10-15-f9-candidate-ctest-tier.md`
  (issue #53): built the option-A disposition ??`engine-d3-candidate`, a
  tier-D3 variant with classicui enabled, so the F9 candidate window is drawn
  for real and the #45 abort is reachable from `ctest`. Measured red with the
  cap reverted: `CommonCandidateList: invalid label idx`, fcitx5 SIGABRT
  (core dumped). Records the three per-case assertions, the two injector
  facts (named keys must be braced ??a bare `Return` is six Latin keys; F9
  opens the window asynchronously ??a Return at 40ms lands before
  `candidateActive()`), and a Down/Page_Down drift between the #45 wiki
  sentence and the engine's arrow handler, recorded not adjudicated. The
  disposition is the maintainer's to confirm or overturn; see
  [[decisions-belong-to-the-maintainer]].
- `wiki/index.md` listed the page under Raw ??measurements.
- `wiki/decisions/2026-10-16-0-1-distribution-channel-github-release.md` ??the
  channel ruling issue #7 asked for: a GitHub Release on tag `0.1.0` with the
  `.deb` attached. Records the cost of the three options the issue named as
  comparable, and what the ruling does not close ??no tag or release exists, so
  the channel is still unverified from a clean machine; the `.deb` is unsigned;
  the signed APK stays blocked on issue #1. The procedure lives in
  `docs/MILESTONES.md` 짠0.1, not here.
- `wiki/index.md` listed the page under Decisions.
- `wiki/decisions/2026-10-16-the-repository-is-private-until-after-0.2.md` ??the
  maintainer ruled `scobyseo/hunmin` stays private through 0.1, public after 0.2.
  Records the measurement that prompted it (release published, `.deb` correct at
  1,252,740 bytes, asset URL and release page both 404 unauthenticated), that #7
  therefore stays open, and the consequence: 0.2 is the first release whose
  documents are read by a stranger, and the 0.1 `.deb` already ships a
  `README.md.gz` claiming no release existed. `tools/release-0.1.sh` now verifies
  reachability unauthenticated against actual repository visibility.
- `wiki/raw/sessions/2026-10-17-issue-72-buffering-docs-closure.md` ??issue #72's
  doc work completed on the maintainer's direction to add no new issue: the
  four `TSF-BUFFERING-72` assertion rows (the board named #72 nowhere while the
  issue's own REMAINING-WORK deliverable was open), and the design-doc drift
  correction ??`docs/WINDOWS-TSF-BUFFERING.md` claimed `ITfInsertAtSelection`
  and `EndComposition`, but the shipped adapter inserts via a synchronous
  edit session's `ITfRange::SetText` (`FlushSession`, `DeactSession`) and never
  opens an inline composition; the doc now describes the shipped mechanism.
  TAT measurement and a ctest TSF tier remain open under milestone B (#65),
  recorded on the row, not hidden. The issue close itself is left to the
  maintainer.
- `wiki/index.md` listed the page under Raw ??sessions.
- `wiki/raw/sessions/2026-10-17-tsf-scaffold-milestone-a-verification.md` ??the
  pre-close verification pass over issue #64: the milestone-A branch was a
  rebase no-op because master had already absorbed the scaffold (plus the
  PR-#76 buffering adapter beyond A), the MSVC and llvm-mingw build
  measurements taken on the Windows host, the 51/52 inner-loop result with
  the maintainer-stated by-nature `data-selfcheck` failure, and the stale
  host-fact corrections it drove through `docs/ENVIRONMENT.md`,
  `docs/MILESTONES.md` and `docs/REMAINING-WORK.md` (with new TSF assertion
  rows). The issue close itself is left to the maintainer.
- `wiki/index.md` listed the page under Raw ??sessions.
- `wiki/decisions/2026-10-16-release-gate-all-tests-pass.md` ??addendum, not a
  rewrite: the maintainer's 2026-10-17 instruction unregistered
  `data-selfcheck` from ctest (not touched before its own dev stage; red on
  every pass on the Windows host), so the gate's exception list holds
  `modes-corpus-selfcheck` alone and `tools/release-gate.sh` was updated with
  the selftest fixtures re-planted. The ruling quote is kept verbatim.

## [2026-10-17] create | clang-format retuned to the tree's own style, applied once (issue #77)

- `wiki/raw/sessions/2026-10-17-clang-format-retune-option-2.md` ??the
  implementation pass over issue #77: the maintainer's ruling (option 2,
  steered "ideal for AI workload") kept verbatim, the churn ladder that
  selected `ColumnLimit: 0` (retuned limit-100 1491 ??limit-0 without
  `AllowShort*` 1673 ??limit-0 with them 853 `-w` lines; trailing-comment
  alignment measured a no-op under limit 0), the `.clang-format-ignore` probe
  and the `--style=file:<path>` trap, the split configs landing
  (root C core, `frontends/fcitx5`, `frontends/tsf` after `tsf.cpp` fell
  through to the root config once), and the machine-checkable state: 51/51
  inner-loop ctest, idempotent second pass, vendored and generated trees
  excluded. The issue close is the maintainer's; see
  [[decisions-belong-to-the-maintainer]].
- `wiki/index.md` listed the page under Raw ??sessions.

## [2026-10-17] update | .clang-format-ignore directory patterns need the ** glob (issue #77)

- `wiki/raw/sessions/2026-10-17-clang-format-retune-option-2.md` ??the first
  full `-n` sweep caught the ignore file's directory pattern `data-orig/`
  failing to match files inside the directory; the glob form `data-orig/**`
  does. The rule and the probe measured with hangulctype.c/cJSON.c/tables.c
  are recorded on the session page; the committed `.clang-format-ignore` uses
  the glob form in the same pass.
- `wiki/raw/sessions/2026-10-17-milestone-c-layout-wiring-data-loading.md` ??  milestone C (issue #66): the HKCU layout-pair store mirroring the fcitx5
  config, slot validation by supported+mode, the single-source data rule
  (measured: the additive chain double-loaded a staged tree, 15 files ??  layout_count 30), tier-F1 additions `tsf-config-test` / `tsf-datadir-test`
  and the dubeolsik win32 trace (gold reviewed 400/400 vs tier A), and the
  measured tier-registration drift ??the standalone frontends/tsf configure
  had been registering zero F1 tests until `enable_testing()` and the
  executable-keyed Python3 guard landed.

## [2026-10-17] create | Settings dialog ??WinUI 3 exe (issue #71)

- `wiki/raw/sessions/2026-10-17-settings-dialog-winui-exe.md` ??the maintainer
  ruling executed: the separate WinUI 3 settings exe. Carries the measured
  window sizes (exe 545x674 px, interim dialog 493x434 px ??"compact" as a
  number), the ResizeClient-DPI-factor and XamlCompiler host quirks, the one
  binding by key capture ("???낅젰 諛⑹떇"), the publish 495 files / 167.4 MiB
  figure for #68, and the still-open device-bound and maintainer-sizing
  questions.
- `wiki/raw/sessions/2026-10-18-milestone-d-candidate-edit-mode-ux.md` ??issue
  #67's milestone D: candidate window + Hanja key store + hook-based key
  capture + the screen-presence defect roots (Mica, ownerless rundll32) +
  Start-menu entry + conversion compartment + system-langbar menu
  extension + the krdict (ko,hanja) dedupe (34,142 ??33,184); overturned:
  the langbar surface measured closed (reference-item probe), the sink
  direction; pending: last-syllable fallback, mode-persist, re-register,
  CI billing.
## [2026-10-18] create | CTRL/ALT chord in hangul mode ruled: transient, no mode switch (issue #101)

- **New decision.** `decisions/2026-10-18-tsf-chord-transient-not-persistent.md`:
  the maintainer ruled in an issue-#101 comment ??"will not switch mode;
  transient processing is enough." The shipped chord rule (commit the pending
  preedit through the boundary flush, close the candidate window, pass the
  chord to the app uneaten) already matches, so the ruling adds no code; the
  mode-persist alternative recorded since the milestone-D session is closed.
- What the ruling does not close, recorded on the page: the tier-F1
  behavioural pin (trace-emit.cpp injects only the Shift bit ??a ctrl/alt
  chord case waits on the trace-spec.md mods bits), and the chord-chain order
  ahead of the chord block (candidate-window keys see modifiers; the chord
  flush can land on the Ctrl keydown itself) ??read off the code, not measured.
- The ruling was given on the issue, not in a session record, like issue #9's
  ruling in the worktree session ??the issue is the surviving source.
- `wiki/index.md` listed the page; total pages 43.

## [2026-10-19] create | TSF native composition preedit (issue #98)

- Session page for the phase that replaced the overlay-window preedit with
  a native TSF composition (inline text + display attribute), including the
  rebase re-verification (both key-path change-sets coexist; tier-F1 re-run
  green) and its two corrections: the fallback case's delivery channel and
  the unsound "7/7 measured" claim. The leftover minor issue (Notepad's
  composition highlight over the preedit) is noted on the issue for the
  revisit.

## [2026-10-19] create | The test-framework migration, ruled and shipped (issue #109)

- Session page for the day the framework question became a ruling and then
  the tree: option 1b (FetchContent, not vendored ??the issue's
  "disqualification" did not survive measurement), the engine tier-A trio
  under gtest, the four TSF runner groups merged into one executable with
  one add_test and the rc-77 probes outside the binary, the never-shrinks
  gate amended (59 ??56 on the MSVC tree), and the two host lessons (the
  :UNINITIALIZED cache suffix that revived the #86 hunmin.lib failure;
  -static for llvm-mingw test binaries). The fcitx5 trio is deliberately
  absent, tracked as issue #118.

## [2026-10-20] create | Legacy interim input path (issue #119)

- Session page for the ruled legacy support: the runtime detection (the first
  mid-word host termination), the Delivery seam, the test-phase boundary drain,
  the app-caret rule, the one-char Hanja ruling, the #103 tray core layered
  under the dispatcher, and the dead overlay remnants stripped. Overturned on
  the way: the EXP-A space row (the eaten-space artifact) and the tier-F1
  mock's 24px fixture blind spot.

## [2026-10-22] create | TSF components: the DIP+SAP stages run (issues #67, #103)

- Session page for the 2026-10-22 stages run (all include-.cpp seams retired
  in both directions, the contracts in tsf.h, the key-state mock a registered
  provider) and issue #103's closure; the shell's remaining SRP load
  (2,320 lines, eight TSF interfaces) is the measured basis for the
  clean-code follow-on issue.

## [2026-10-22] create | TSF langbar item as a tear-off (stage 4, issue #127)

- Session page for the maintainer-chosen option A: `LangBarItem` becomes a
  tear-off implementing `ITfLangBarItemButton` + `ITfSource`; the shell's
  QI, AddItem/RemoveItem and mode notification forward to it. Measured
  correction to the issue text (10 bases, 30 STDMETHOD bodies before) and
  the host re-measure still owed.

## [2026-10-22] create | TSF settings split by responsibility (stage 5, issue #127)

- Session page for the maintainer-chosen option A: the 979-line settings TU
  becomes `settingsrows`, `settingsdialog`, `settingslaunch` and
  `settingsprovider`, with `settings.cpp` left as the rundll32 export; the
  symbol moves re-pointed three board rows, and the two new cross-TU calls
  anchor on their own module addresses.

## [2026-10-22] create | Activate/Deactivate extraction and the langbar style ruling (S5 first cut, issue #127)

- Session page for the maintainer's directive: the naked blocks in
  `Activate`/`Deactivate` become private helpers, and the langbar item's
  style is ruled to `BTN_MENU|SHOWNINTRAY` (no `BTN_BUTTON`). The detach no-op
  exposed by the extraction is recorded as TSF-LANGBAR-DETACH-127, not fixed.

## [2026-10-22] create | DllRegisterServer/DllUnregisterServer blocks extracted (S5, registration)

- Session page for the same directive applied to the registration exports:
  seven naked scopes become file-static helpers called in their original
  order, with the unregister side mirrored and the tree deletions left inline
  in the documented reverse order. No order, argument or log line changed; the
  S4 move into its own TU is recorded as still owed.

## [2026-10-22] update | Registration TU landed (S4)

- Same session page updated: the helpers, exports, class factory and
  DllGetClassObject moved to registration.cpp with the shared GUIDs in
  ids.cpp; tsf.cpp 2,096 -> 1,659 and the standalone TSF configure now uses
  /utf-8, clearing C4819.

## [2026-10-22] update | Display-attribute provider landed (S7)

- New page: [[2026-10-22-tsf-displayattr-extraction]]. displayattr.cpp (185)
  carries the provider objects and the DisplayAttr atom channel; tsf.cpp
  1,659 -> 1,545; tier-F1 4/4.

## [2026-10-22] update | Data source extracted (F)

- New page: [[2026-10-22-tsf-datadir-extraction]]. datadir.cpp (122) carries
  hunmin_ctx_from_module and the layout-slot check; tsf.cpp 1,492 -> 1,384;
  tier-F1 4/4.

## [2026-10-22] update | Composition machinery extracted (C)

- New page: [[2026-10-22-tsf-composition-extraction]]. composition.cpp (457)
  carries the termination handler, the three edit sessions, the
  write/end/detach half and the engine/boundary drain; the session classes
  stay nested in tsf.h and are defined there. tsf.cpp 1,384 -> 945;
  tier-F1 4/4.

## [2026-10-22] update | Key path extracted (K)

- New page: [[2026-10-22-tsf-keydispatch-extraction]]. keydispatch.cpp (386)
  defines the test/down/up handlers, predicates, toggles, DrainAndPass,
  HarvestFinalised and FeedEngineKey; tsf.cpp 945 -> 575; tier-F1 4/4.

## [2026-10-22] update | QI answers tabled (Q)

- New page: [[2026-10-22-tsf-qi-table]]. One kQiRoutes table now feeds the QI
  trace and the dispatch; log text and answer order unchanged; tsf.cpp
  575 -> 597; tier-F1 4/4.

## [2026-10-22] update | Settings-exe resolver unified (S)

- New page: [[2026-10-22-tsf-settings-resolver]]. Maintainer ruling
  "deployed-first": env > <dll>\settings-ui\publish > <dll>\settings-ui > the
  two checkout climbs, one resolver in settingspath.cpp (100). Installed
  Start-menu shortcuts now target the WinUI exe. tsf.cpp 597 -> 571;
  tier-F1 4/4.

- New page: [[the-tsf-extraction-round-2026-10-22]]. The eleven
  2026-10-22 TSF session pages fold into one subject page (stage table,
  what the round established, what it does not); each stays as a
  `superseded` stub carrying its own one-line finding. 715 live lines of
  one-stage narration become ~40 plus eleven stubs.

## [2026-10-25] lint | Two wiki orphans join the checks (issue #133)

- `wiki/test-scenario-guide.md` and `wiki/ci-android-lessons.md` gained
  frontmatter (title/created/updated/type/status/tags/sources), two outbound
  wikilinks each, and an `index.md` entry.
- `tools/wiki-lint.py`'s page set widened to include root-level content pages
  (`wiki/`); framing pages (`index.md`, `log.md`, `SCHEMA.md`) are excluded by
  name. Page count moved 58 ??60.
- `wiki/test-scenario-guide.md`: the tier table contradicted `docs/TESTING.md`;
  replaced with a pointer to `짠The vocabulary` and the three specific errors
  named (tier-B, tier-D, E3).
- `wiki/ci-android-lessons.md`: the Recommended CI Flow section and the tier-job
  mapping corrected to match the workflow and `docs/TESTING.md` (automated
  Android tiers are E0/E1/E2, no E3).
- `wiki/index.md`: dropped the hand-maintained page count (points at
  `python tools/wiki-lint.py` instead).
- `tools/docs-lint.py`: the cross-check of index.md's page count removed.
- Also: `docs/REQ-MAP.md` re-pointed after heading renames.

Verified: `python tools/wiki-lint.py` ??60 pages, 0 errors; `python tools/docs-lint.py` ??0 errors.

## [2026-10-27] create | The #107 mock-surface session page

- `wiki/raw/sessions/2026-10-26-tsf-mock-unmodelled-surfaces.md`: the
  registration seam and why it covers the registry, the four surfaces now
  observed (registration, compartment, lang-bar manager, key-sink advice),
  the measurements (tier-F1 49 -> 64 cases, the mutation guard, the
  pyright red that named the tool and not the claim) and the two items left
  for a ruling: #145 and #146.
- `wiki/index.md`: one sessions entry.

Verified: `python tools/wiki-lint.py` and `python tools/docs-lint.py`,
both 0 errors.

## [2026-10-27] create | The #145 Deactivate teardown fix

- `frontends/tsf/src/tsf.cpp` + `tsf.h`: `UnadviseSinks()` withdraws the
  `ITfThreadMgrEventSink` advice (with the cookie `Activate` was handed) as
  well as the key-event sink, and a new `ReleaseThreadMgr()` drops the
  manager reference; `Deactivate()` calls it last, so the language-bar
  `Detach` runs against a live manager and `RemoveItem` is reached.
- `frontends/tsf/test/tsf-session-test.cpp`: the two cases that encoded the
  defects flipped to assert the pairing (`DeactivateUnadvisesTheThreadManagerEventSink`,
  `DeactivateRemovesTheLangBarItem`), plus a refused-advice variant proving
  no `UnadviseSink(0)` is issued when the manager never advised;
  `mock-tsf.h` gained `adviseSinkHr` for it.
- `docs/REMAINING-WORK.md`: two rows re-pointed and re-stated, the
  TSF-LANGBAR-DETACH-127 note flipped to fixed; tier-F1 4/4 green.
- `wiki/raw/sessions/2026-10-27-tsf-deactivate-teardown-fix.md`: one page.

Verified: `python tools/wiki-lint.py` and `python tools/docs-lint.py`,
both 0 errors.

## [2026-10-31] create | Milestone F: the win32 corpus replays the eight x11 tier-A families (#69)

- `tests/conformance/win32/`: eight new `*.trace.in` + `*.trace.gold` pairs,
  one per tier-A family x11 replays (`p2-2-1-basic`, `dkssud-combo`,
  `p2-2-3-chosung`, `p2-2-4-1-yetrules`, `p2-2-4-2-eung`, `p2-3-swapped`,
  `manual-sin3-p2`, `manual-sin3-p2-yet`), batched per family. Corpus 3 -> 11
  traces / 106 cases / 688 events over four layouts; tier-F1 12/12 green on
  `build-msvc` Release; each gold identical to the `.in` expectations from
  tier A on all 688 events.
- `tools/gen-conformance.py`: a caps case is now EXCLUDED for win32 instead of
  being emitted at `mods: 0` with a caps-on expectation (the blanket
  `unsupported -= {"caps"}` washed the refusal it had just recorded; measured
  failure ??composed 怨?where tier A records commit ??+ preedit ??, and the
  exclusion is written into `meta.excluded`.
- `docs/trace-spec.md`: `meta.excluded` documented as a meta field.
- `docs/TESTING.md` tier F1: the corpus breadth, the measured counts and the
  caps gap.
- `docs/REMAINING-WORK.md`: five rows (TSF-CORPUS-69, TSF-CAPS-EXCL-184,
  TSF-DELIVERY-MODES-185, TRACE-LAYOUTS-186, TRACE-X11-PARITY-187).
- Filed #184 (caps-on sequences, ruling), #185 (both TSF delivery modes),
  #186 (11 of 15 layouts uncovered), #187 (x11 omits cases win32 replays).
- `wiki/raw/sessions/2026-10-31-milestone-f-win32-conformance-corpus.md`: one page.

Verified: `python3 tools/docs-lint.py --build-dir build-msvc` (0 errors) and
`python3 tools/wiki-lint.py` (0 errors).

## [2026-10-31] create | E5's assignment ruled: modifier + ???? default and remappable (#159)

- `wiki/decisions/2026-10-31-han-eng-modifier-compound-special-functions.md`: the
  maintainer's two sentences verbatim (compound = the special-function namespace, on all
  three frontends; default behaviour, composition selectable in the config dialog), and
  the sub-decisions that were *not* made, listed rather than absorbed.
- Consequential updates, not wiki actions: `docs/SPEC-MODES.md` 짠E5 (assignment decided
  ??`[D]`, remaining parameters named), `docs/DESIGN-MODES.md` 짠3 (the hook's
  allocation), `docs/OPTIONS-MEASURED.md` 짠E5 header row (짠E5-3's option 1/2 family is
  what was chosen; the modifier-per-function slot stays open), `docs/REMAINING-WORK.md`
  B4 (the "assignment still open" clause is superseded).
- No implementation: the tree has no `autoswitch` option and no compound hook on any of
  the three frontends, and this box has no cmake, no Windows host, no device.

## [2026-10-31] create | The Hanja key is the default; F9 was a test scaffold

- `wiki/decisions/2026-10-31-hanja-key-default-f9-was-a-test-scaffold.md`: the
  maintainer's sentence verbatim, the three parts of what it rules, and the two things
  it leaves open (F9 as an accepted alternate, because the conformance corpus drives it;
  and the store question it shares with the E5 assignment).
- What already matched, recorded rather than re-implemented: TSF's `hanjaKey` default is
  the bare Hanja key (`docs/SPEC-TSF.md` 짠TSF9, issue #67) and Android carries
  `KEYCODE_EISU` as a `platformAlternates` entry, while Android's *row* reads "not set"
  because it is an alternate and not a default.

## [2026-10-31] create | The special functions are engine-resolved, frontend-implemented (#159)

- `wiki/decisions/2026-10-31-special-functions-engine-resolved-frontend-implemented.md`:
  the maintainer's paragraph verbatim (frontend supplies key codes, engine processes and
  invokes the frontend interface, an unsupported platform refuses), the five-symbol shape
  that implements it, and the four things it leaves open ??the per-function default chord
  most of all, because choosing it per frontend is the divergence the ruling removes.

## [2026-11-01] update | B1's question was answered elsewhere (#161)

- `wiki/decisions/the-outstanding-decisions.md`: D1 carries a dated status ??짠ReqMode8
  ruled option (c) and the English half was ruled by 짠ReqMode8, so the table is the
  record of the evidence, not the current definition; D2 records that its krdict/`pos`
  route left the scope boundary and 짠ReqMode8 removed the need for it. Neither is
  retired here: the board row's fate is issue #161, priced in
  `docs/OPTIONS-MEASURED.md` 짠B1.

## [2026-11-01] rule | B1 retired ??the Hangul judgement needs no source (#161)

- `wiki/decisions/2026-11-01-b1-retired-the-hangul-judgement-needs-no-source.md`:
  the maintainer's "want to select A" verbatim. The row had been emptied by
  짠ReqMode8 (structure test, no lookup) and 짠ReqMode8 (the English source, shipped,
  #160 closed) ??neither of which the board or MILESTONES mentioned, which is why
  a decided question kept reading as the root of five blocked rows.
- Propagated the same day: board 짠1's B1 row, 짠2 retitled and restated per item,
  the voice stage 8 row, the assertion table's decision-blocked comment,
  MILESTONES' "hostage to B1" lines, and PLAN's P8 "judgement source" row.
- Left open and named: the corpus-list delete-or-keep question 짠ReqMode8 wrote down,
  E1's lack of an owner, and 짠ReqMode8's stale "left open" sentence against ReqMode9.

## [2026-11-01] rule | ?쏇븳湲 layouts get the widened validity rule, per layout (#166)

- `wiki/decisions/2026-11-01-old-hangul-layouts-get-the-widened-rule.md`: the
  maintainer's "it is optional for layouts which support ?쏇븳湲" verbatim. The rule
  is unchanged and only the jamo sets widen, so `?담뀥?? stays invalid while `???
  `?뚡뀿` `??■넼` `??? ??what the ?쏄? layouts actually render ??are valid.
- Measured before choosing the shape: six of fifteen layouts can emit an archaic
  jamo (a key-value scan that reads only JSON strings under-reports at five,
  because the values are nested `case` expressions), and the composer never
  precomposes one, so the widened rule had to be a grammar over conjoining jamo.
- The per-layout answer is derived from the layout's own keys and combination
  table, so no schema field was added; SPEC-SCHEMA's layout section says so.

## [2026-11-01] create | The engine judges, the frontend switches

- Created `decisions/2026-11-01-the-engine-judges-the-frontend-switches.md` on the
  maintainer's ruling in issue #164, quoted verbatim, with the threshold it set and
  the principle stated beside it.
- Marked `decisions/2026-10-13-r8-mode-guessing-is-adaptor-owned.md` superseded
  (kept, not deleted: the 2026-10-13 decision is what the question had to be asked
  against) and added it to `index.md`.
- Created `raw/sessions/2026-11-01-modes-ownership-retires-the-detector.md`
  (the #164 session: what it created, what it overturned, what it left pending
  with an issue number each) and added it to `index.md`.
- Created `decisions/2026-11-01-the-symmetric-rule-ships-and-the-0-3-gate.md`
  (E1 ruled and implemented; the mixed sentence is the 0.3 gate) and added it to
  `index.md`.
- Created `decisions/2026-11-01-the-judgement-timing-is-per-key-by-default.md`
  (E6 ruled, issue #163: `judge_timing` 1/0, per key by default, display-only) and
  added it to `index.md`.
- Corrected `decisions/2026-11-01-the-engine-judges-the-frontend-switches.md`:
  ReqMode9's threshold unit is **key inputs**, named by the maintainer
  ("3. ?ㅼ엯???멸컻濡?紐낇솗???섏옄."), not the `3?? the page had repeated from the
  ruling's wording. Same correction in `docs/SPEC-MODES.md` 짠ReqMode9/짠E3 and
  `docs/OPTIONS-MEASURED.md` 짠MRO/짠BND-3.
- Created `decisions/2026-11-01-one-dict-by-default-the-release-page-later.md`
  (issue #172 ruled: "Recommends.", one dict by default because there is no
  additional distribution mechanism, the GitHub release page as the candidate
  for one, and the feature testable before that exists) and added it to
  `index.md`. The packaging followed the ruling: `debian/control` says
  `Recommends`, and `data-split` now fails if it says `Suggests` or `Depends`.


## [2026-11-01] create | Syllable commit turns autoswitch off

- The maintainer's ruling on the dependency issue #171's measurement exposed
  (짠BND-7: the digit run belongs to the buffered policy): reconcile the two
  settings when they are written, switch autoswitch off at run time while
  syllable-unit commit is on, and name the reason in the debug output. Quoted
  verbatim; the page records the three obligations and the two things it leaves
  open (where the diagnostic goes, what the settings row shows).

## [2026-11-02] create | The interim delivery cannot support autoswitch, and ON stops the English passthrough

- The two rulings from the maintainer's message that answered issue #175's findings,
  quoted verbatim: the interim delivery cannot carry the switch (structural ??the word
  leaves the layer per key, so there is nothing whole to judge), and with the switch ON
  English stops passing through so ReqMode8's E1 line reaches the engine. The page records
  what each authorises and the three things neither decides (the settings row on a host
  that ignores it, #215; the live cost of routing English keys, #216; the separate
  syllable-commit dependency, #207).

## [2026-11-02] update | The auto-toggle is live on Android (issue #174)

- Implemented the 0.3-lane feature the 2026-10-12 page had left inert: the
  service's dispatch fires `AUTO_TOGGLE`, the press writes the engine's
  `autoswitch` value through `HunminAdapter.setAutoswitch` (the read-back, never
  the request), and the settings list reports the engine's value and the
  engine's chord. Measured at tier E1 with a designed case each way plus the
  control and a data-absent fixture (`AutoSwitchTest`, `AutoSwitchGateTest`).
- The 2026-10-12 decision's mechanism sentence expected the row to be re-enabled
  "by giving it a default again"; the default chord stays empty instead, because
  the assignment is the engine's chord per
  [[2026-10-31-han-eng-modifier-compound-special-functions]]. That delta is
  recorded in place on
  [[2026-10-12-auto-toggle-disabled-by-default-in-0-1]], which now carries a
  dated supersession section rather than a rewritten body.

## [2026-11-02] create | SPEC-MODES 짠4 splits into an invariant and a metric

- Created `decisions/2026-11-02-spec-modes-4-splits-into-an-invariant-and-a-metric.md`
  from the #179 ruling ("臾몄옣??紐낇솗?섍쾶 諛붽퓭蹂쇨퉴? 洹몃윭怨??섏꽌 ?붽뎄?ы빆 ?꾩씠?붾?
  遺숈씠?좎? ?섏옄"): 짠ReqMode14 added to `docs/SPEC-MODES.md` 짠1 with its `REQ-MAP` row
  (parent A2, `engine-off-identity`), 짠4's two bullets rewritten to point at ReqMode14
  and at ??in `docs/MODES-METRICS.md` 짠1.3, and the reading-order table extended.

## [2026-11-02] update | SPEC-MODES 짠4 splits into an invariant and a metric

- ReqMode15 added (`The switch costs no word`), 짠4 removed and `# 5` renumbered to
  `# 4`, `docs/REQ-MAP.md` at 81 rows, and the verification landed: the
  judgement corpus is now driven through the off path as pairs
  (`hunmin-sim --off-check`, 120 files / 4617 keys / 18752 comparisons) plus
  `LayerTest.TurningTheSwitchOffMidWordLosesNothing`. The page carries the four
  messages in order.

## [2026-11-03] create | the x11 corpus replays every tier-A case (issue #187)

- Created `raw/sessions/2026-11-03-x11-corpus-replays-every-tier-a-case.md` and
  added it to `index.md`: the issue's premise (corpus generated before the
  goldens gained cases) measured wrong ??`git log` shows one commit per golden
  since the import, and commit c773fdd had dropped 22 traces deliberately for
  word buffering (#74), the same set win32 then replayed.
- The fix is a field, not a re-emit: `tools/gen-conformance.py` derives
  `commit_mode: syllable` from the golden (a commit on a consumed key) and
  `frontends/fcitx5/test/trace-emit.cpp` runs under that policy, so the 22
  traces match tier A verbatim ??87 -> 109 traces, 263 events, `.in` identical
  to `.gold` on all of them, the 87 previously committed traces regenerating
  byte-identically.
- Added `tools/conformance-coverage.py` and ctest `conformance-coverage`
  (`tier-C;guard`): every tier-A case of every family the corpus claims has a
  trace, every trace belongs to a recorded family, and `--regen` reproduces
  the corpus from goldens. Verified falsifiable by deleting a trace and by
  planting a stray one.
- Recorded, not resolved: `engine-d1-judgement` is red at this commit on this
  host (the ReqMode9 control case delivers `?뤿맭` where the corpus expects `key`,
  judgement data present), and the `slow` tier measures 94 s against its 60 s
  budget with `engine-d3-edit` at 95 s alone.

## [2026-11-03] update | p2-2-2-tiers is one case, not eight

- Merged the eight tier probes into a single 25-event case after measuring that
  they were not independent in any way that mattered: every one already ended
  with an empty buffer, and `hunmin-sim --check` replays the merged golden
  identically (`docs/SPEC-P2.md` 짠P2 gate corpora carries the finding).
- The split's only products were failure attribution and a grep-able map of
  which UserKeyTable cells are sampled; the tier-leak bug it once caught needed
  a case ending MID-tier, which none of these did. Corpus 109 -> 102 traces,
  same 263 events; tier-C 126 -> 120 tests; the android and win32 dialects
  unaffected.
- `tools/conformance-coverage.py` learned the unsplit spelling
  (`<stem>.trace.in`, no `-caseNN`) that a one-case golden produces, still
  falsifiable both ways.

## [2026-11-03] update | tier C's coverage metric, measured instead of assumed

- Added `tools/tier-c-wiring-coverage.py`: per dialect and layout it reports keys
  pressed vs mapped, consumed vs passed, modifier states, commit policies and
  which tier-A families are replayed at all. A report, not a gate ??a gap is a
  statement about the corpus, and widening it is a decision.
- `docs/TESTING.md` tier C now carries the metric and the measured table, and
  the sentence "the 102 traces prove the adapter applies it" is gone: the x11
  dialect never declines a key (0 of 263 events), presses 24/47 쨌 14/47 쨌 19/47
  layout keys, and reaches none of `0`, `2`-`9`. android has 4 declined events,
  win32 198; the dialects replay 9 / 8 / 1 families.
- Board: `TIER-C-WIRING-DECLINE` and `TIER-C-WIRING-DIGITROW`, both `path-absent`,
  so the missing artefacts are machine-checked rather than described.
- Also measured, and NOT done: `tools/merge-golden-cases.py` shows a space
  boundary makes every corpus golden mergeable with identical replay (patim
  included). Merging keeps every event, so it changes file count, not coverage.

## [2026-11-03] update | the corpus is one trace per family

- Merged every corpus family's cases behind a space boundary with
  `tools/merge-golden-cases.py` (one merged case per engine-option set; it
  refuses to write unless `hunmin-sim --check` replays the result identically).
  Corpus 102 traces -> 10, events 263 -> 355, `tier-C` 120 tests -> 28, every
  `.in` identical to its `.gold`.
- **A prediction was wrong:** I said merging would change file count and not
  coverage. Measured after: x11 `passed` went 0 -> 92, because the boundaries
  are declined keys ??the pass-through half of FRONTEND-CONTRACT invariant 3 is
  now observed on x11 for the first time. Key coverage (24/47 쨌 14/47 쨌 19/47)
  is unchanged, as predicted: a merge cannot press a key the corpus never
  pressed, so issue #229's digit-row gap stands.
- `manual-sin3-p2-yet` keeps two traces (one case runs with `caps` on); the
  board's `TRACE-X11-PARITY-187` row re-points at the merged file and
  `TIER-C-WIRING-DECLINE` narrows to arrows / BackSpace-on-empty / unmapped
  characters.

## [2026-11-03] update | ?몃쾶??理쒖쥌's ten cases become one file

- a1ba784 added ten per-case traces for ?몃쾶??理쒖쥌 ??the family the corpus had
  no trace for at all ??against the policy the previous entry established. The
  policy is one trace per family, so they were merged the same day:
  `tools/merge-golden-cases.py tests/golden/case-sebeolsik-final.jsonl --write`
  reports 10 cases -> 1 with `hunmin-sim --check` replaying it identically, and
  the nine interior boundaries are the merge's declined spaces.
- Verified rather than assumed: the merged trace's 62 events are the ten old
  traces' 53 events plus 9 boundaries, and its `meta.delivery.total` is byte-
  identical to the concatenation of the ten old per-case totals
  (`?덈뀞?쒓?媛믩쭖?롢뀊?귙뀤?졼뀘?뽧뀬?쒌뀑?꿔꽯?덀꽴??230??졹넫媛.4"~`). Corpus 20 traces ->
  11, events 408 -> 417, `passed` 100 -> 109 (all boundary spaces); key
  coverage unchanged at 22/47, as it must be.
- Cost, stated: a red tier-C test now names an event index instead of
  `case01`/`case02`. The per-case labels carried the expected text the event
  already carries.
- `docs/TESTING.md`, `docs/REQ-MAP.md` (ReqMode14's `verified by` id) and the board's
  `TRACE-X11-BOUNDARY-220` / `TIER-C-WIRING-DECLINE` rows re-point at the one
  file; `req-map`, `docs-lint` and `wiki-lint` all green.

## [2026-11-03] update | ?먮쾶??and ?좎꽭踰뚯떇 P2 get designed families

- Same shape for the other two layouts whose identity is an ARRANGEMENT:
  `tests/golden/case-dubeolsik.jsonl` and `case-sin3-p2.jsonl`, eleven cases
  each, from `tools/gen-case-golden.py` (expectations read back from the engine,
  never typed), each merged to ONE trace by `tools/merge-golden-cases.py` ??the
  one-file-per-family policy, applied before the files could multiply.
- ?먮쾶??s identity is the opposite arrangement and, for the frontend tier, the
  thing no family stated: it maps 26 letters and NOTHING else, so its digits,
  shifted digits and `.` are DECLINED and reach the client. P2's is the
  媛덈쭏?ㅼ씠 core (`cq` two initials vs `cQ` one 寃밸컺移? `s` is ??in the jung slot
  and ??in the jong slot), 醫낆꽦 on its own letter key, the 2024 諛쏆묠 rule, and a
  number row spent on literal digits.
- Measured: corpus 11 traces / 417 events -> 13 / 589; x11 `passed` 109 -> 159
  (the ?먮쾶??family's declined characters are the new ones); key coverage
  sin3-p2 24/47 -> 32/47, dubeolsik 0 -> 14/26, sebeolsik-final 22/47 unchanged.
  Both new families replay under tier A's own policy, so `.in` == `.gold`; the
  word-buffered model stays the sebeolsik family's assertion.
- Board: `TIER-C-WIRING-DECLINE` now records two kinds of declined key (space and
  the characters a layout does not map) with an arrow and BackSpace-on-empty
  left; `TIER-C-WIRING-DIGITROW` narrows to `4`-`9`. `docs/HOWTO-NEW-LAYOUT.md`
  짠4 carried a contradiction with the merge policy ("a golden that concatenates
  cases is wrong") and now states the boundary rule instead.

## [2026-11-03] fix | the golden schema's `"shift": true` is interpreted

- The field has been written since #90 (`0d6e6cb`, 2026-10-18): `\s<char>`
  records `{"ch": base, "shift": true}` in `--trace`. Nothing ever read it ??  `git log -S'"shift"' -- tools/gen-conformance.py` is empty, and
  `hunmin-sim --check` fed `hunmin_ic_process(ic, ch)` without the modifier.
  `docs/SPEC-SCHEMA.md`'s reference trace format documented neither spelling.
- That was **unimplemented, not decided**. The record says the corpus keeps the
  resolved-character spelling (`docs/OPTIONS-MEASURED.md` 짠IC-2-B: "keeps
  emitting the legacy shape unless a dialect wants both"; the fcitx5 row: "its
  case *is* the row selector"), and `docs/SPEC-P2.md` 짠"IC input entry points"
  makes `(base, shift)` and the resolved character equal by construction ??  which `ctest engine-process-key` asserts over every layout and row. Nothing
  says a checker may ignore the field.
- Fixed in both readers: `tools/sim.c --check` routes a shifted event through
  `hunmin_ic_process_key(ic, ch, 1)`, and `tools/gen-conformance.py` resolves
  the field into the native character (`shift_char`), so tier C carries
  `FcitxKey_E`, not `e`.
- Fixture: `tests/golden/shift-field.jsonl` (dubeolsik, e/???? r/????
  o/???? plus its x11 trace, wired as tier-A `engine-golden-shift-field` and
  through `conformance-coverage`. Falsified both ways: the pre-fix
  `hunmin-sim` binary reports 6 MISMATCH lines on it, and a converter patched
  to ignore the field aborts the adapter (`commitString: ?? where ??was
  expected). The corpus's spelling is unchanged ??resolved characters still.

## [2026-11-03] create | tier C's x11 corpus takes the digit row and the declined BackSpace

- New session page for issue #229's remaining half; it records the two generator
  defects the work exposed (`consumed` for a named key, and the `{"ch":"\u0008"}`
  spelling of BackSpace) and why a base-0 key still cannot be generated.
- Board rows re-measured in `docs/REMAINING-WORK.md`: `TIER-C-WIRING-DECLINE` and
  `TIER-C-WIRING-DIGITROW` now assert present artefacts, `TIER-C-WIRING-ARROW`
  is new and stays `path-absent` on a ruling nobody has made.

- **2026-11-04** ??one coverage golden per layout (issue #227): the unit is
  (jamo, slot), the generator refuses to publish below `--require-modern`, and
  four probe defects were found by numbers that would not move ??a misaligned
  batched answer, a two-key 珥덉꽦 truncated to its first key (??= ??twice), the
  literal `\` key dropped from the alphabet (?몃쾶??no-shift's ??, and a
  hand-written compose pair beating the measured one. Recorded because two of
  them had been reported to me as layout facts, and neither was. New pages:
  [[2026-11-04-coverage-golden-per-layout]]. Board rows added:
  `COVERAGE-SLOT-MODERN-227`, `COVERAGE-SLOT-PARTIAL-227`.

- **2026-11-04** ??the shared edit-key note at the repository root was
  untracked, unowned and already executed: seven sequences (EK1-EK7), six of them
  asserted on fcitx5, Android and TSF, and the note's own "not yet" cells had
  been filled without anyone updating it. Recorded as
  [[2026-11-04-shared-edit-key-catalogue-was-executed]], the root file deleted
  (no history was lost: it had none), and two board rows added ??  `EDITKEY-CATALOGUE-95` and `EDITKEY-EK7-TSF`, the one real gap, which stays a
  ruling rather than an agent's test. Also noted: the note's E1-E7 labels collide
  with the automatic-switch experiments, so the record uses EK1-EK7.

- **2026-11-04** ??issue #246's eight red `HunminAdapterTest` cases became one
  real defect and seven wrong tests: two typed `dkss` and expected `?덈뀞` (the
  layer shows `?덀꽩`), two counted `sendKeyEvent` payloads the adapter never
  sends (pass-through is the return value), one read `.last()` off an empty list,
  one reached the boundary with one BackSpace instead of four, and two asserted
  a candidate window this frontend does not have (F9 has no AOSP keycode).
  The real one: the layer's `toggle_language()` commits the pending word and
  Android discarded it, so a ????press lost the word being composed ??TSF and
  fcitx5 both insert it. Fixed, and `no-toggle-delivery` is the tenth planted
  mutation. Tier E1: 156/156 green in 9 s; `android-mutations` 10 caught + 1
  recorded gap. The `slow` tier is over its 60 s budget with or without the new
  mutation ??recorded, not re-decided. New page:
  [[2026-11-04-tier-e1s-eight-reds-were-seven-tests]]; board rows
  `EDITKEY-CATALOGUE-95`, `AND-SPECIAL-159`, `AND-SETTINGS-E1-177` re-measured.

## [2026-11-05] create | the settings row names the support condition (issue #215)

- Decided by the maintainer: the TSF settings row reads
  `?먮룞 ?????꾪솚 (吏???깆뿉?쒕쭔, Ctrl+????`. The dialog has no host handle and
  delivery is frontend state, so the label states the condition instead of
  guessing the running host's mode; the data-missing suffix is unchanged and the
  fcitx5/Android labels stay `?먮룞 ?????꾪솚`. Recorded in
  `docs/FRONTEND-EQUIVALENCE.md` 짠2.1, pinned by
  `TsfSettingsTest.TheAutomaticSwitchRowComesFromTheEngine`.
- New page: [[2026-11-05-the-settings-row-names-the-support-condition]].
  Updated the 2026-11-02 page's open-question bullet to point at it. Board row
  `TSF-AUTOSWITCH-ROW-HOST-215` re-pointed; `AUTOSWITCH-TSF-ROW-175` re-measured.

## [2026-11-08] create | the retired modes measurement is deleted (#272)

- Maintainer's order, verbatim: "delete the measurement history is my order. just
  leave how to remeasure with new data and new approach." Executed in one change:
  `docs/MODES-DATA.md` and `docs/MODES-METRICS.md` deleted, the superseded
  2026-10-04 judgement measurement removed from `docs/SPEC-MODES.md`, and the five
  modes sections of `docs/OPTIONS-MEASURED.md` (짠EDW, 짠B1, 짠PAS, 짠MRO, 짠BND)
  deleted. `docs/MODES-MEASUREMENT.md` keeps the metric definitions, pass-line
  shapes, reproduction commands, corpus terms and harness rules, and carries no
  figures.
- The maintainer quotes those papers held are not lost with them: four were
  already recorded in decision pages, and the three that were not ??the English
  source, the buffer cap and ReqMode6's tier wording ??got pages in this change.
  New pages: [[2026-11-08-the-retired-corpus-measurement-is-deleted]],
  [[2026-11-01-the-english-source-is-hunspell-en-us-plus-a-user-dictionary]],
  [[2026-11-01-the-buffer-cap-is-256-bytes-configurable]],
  [[2026-11-01-the-reqmode6-tier-examples-are-the-shift-row]]. Index updated.
- Board rows that cited the deleted sections were re-pointed in the same change;
  `docs-lint`, `req-map` and the cheap ctest selection stay green.


## [2026-11-08] create | D1 closed, and no modes measurement until the maintainer says so

- **D1 (frontend core-move):** the maintainer closed it as "no core move, record
  the seam as a contract". `docs/DESIGN-PHASE5-CORE-MOVE.md` gains Step 4 (the
  ruling, why the inventory supports it, what would reopen it) and is closed as a
  plan; `docs/DESIGN-FRONTEND.md` 짠11 is the contract ??what the core owns versus
  what stays a frontend's. New page:
  [[2026-11-08-no-frontend-core-move-the-seam-is-a-contract]].
- **Measurement:** asked which corpus a fresh run should use, the maintainer ruled
  "no measurement until i mention. current rule - using english dict only for
  auto-switch is enough for me." Recorded in `docs/MODES-MEASUREMENT.md` 짠6 and in
  board row B2, which becomes unblocked-by-nobody. New page:
  [[2026-11-08-no-modes-measurement-until-the-maintainer-says-so]].
- **Tooling:** the two-stage case-golden flow (`gen-case-golden.py` then
  `merge-golden-cases.py --write`) is now documented in the generator's usage and
  in `docs/TESTING.md`, including what `--check` can and cannot compare, so the
  "merged family reported stale" surprise is documented rather than re-diagnosed.
- Verification: docs-lint, req-map, wiki-lint and `ctest -j16 -LE slow` green.


## [2026-11-08] update | the measurement ruling is scoped to 0.3, and B2 stops gating it

- Clarified by the maintainer the same day: *"i mean enough for 0.3"* ??so the
  statement that `english_valid` as an `en_US` lookup is the whole of the
  judgement's data is a 0.3-lane statement, not a claim about 0.4 or 1.0.
- That collided with `docs/MILESTONES.md` 짠0.3, which made B2's pass lines the
  lane's exit gate. Asked which governs, the maintainer retired B2 as the gate:
  0.3 exits on the mixed-sentence gate (`English??醫뗭? Language?낅땲?? in one
  stream) and nothing else; the pass lines move past 0.3. Recorded in
  `docs/MILESTONES.md` 짠0.3, `docs/REMAINING-WORK.md` B2, and
  [[2026-11-08-no-modes-measurement-until-the-maintainer-says-so]].

## [2026-11-09] decision | the 22.04 release ships IBus only

- The maintainer set the goal ??*"???묒뾽??紐⑺몴濡?ubuntu 22.04 瑜??꾪븳 諛고룷?먯쓣
  異붽??섏옄"* ??and, after the measurement that jammy's `fcitx5` is 5.0.14 while the
  tree requires 5.1.13, ruled: *"22.04 release??ibus留?吏?먰븯硫??? fcitx5??  ?ш린."* Recorded in
  [[2026-11-09-the-22-04-release-ships-ibus-only]]; the measurement it lands
  beside is `docs/OPTIONS-MEASURED.md` 짠PLT-7.
- Consequence in the tree: the 22.04 package takes 짠PLT-7 path 1 (core + IBus
  engine, `libibus-1.0-dev` from jammy main), carries no fcitx5 dependency, and
  the IBus frontend becomes the work item before that package can exist. The
  jammy package inventory (ibus 1.5.26-4, fcitx5 5.0.14-1, cmake 3.22.1) and the
  three paths are in 짠PLT-7 with their commands.
- Left open and named in the page: the binary package layout, whether the
  dev-distro `.deb` keeps the fcitx5 addon, candidate UI and page ceiling, config
  backend, where the jammy artefact is built, and FE11's budget reading.
- Verification: the IBus engine's portability was measured, not assumed ??the
  29-line probe compiles against jammy's 1.5.26 headers and, under this host's
  1.5.34 daemon, registers, is spawned and commits into the GTK client
  (`HUNMIN_PROBE_ENGINE=??tools/ime-host-probe.sh` ??PASS).

## [2026-11-09] decision | 22.04 packaging names, no fcitx5 redistribution, and a CI-shaped dev VM

- The maintainer answered the priced option list: *"1-4 源뚯???沅뚭퀬???濡?*, then
  *"5. release ?쒖뿉??CI?먯꽌 ?섎㈃ ?섎굹, 媛쒕컻 以묒뿉????linux box??vm???섎굹
  ?ㅼ튂?댁꽌 媛쒕컻?섏옄. vm??理쒕???github CI ?섍꼍怨??숈씪?섍쾶 留뚮뱾?댁꽌 CI?먯꽌
  ?ㅽ뙣媛 諛쒖깮?섏? ?딄쾶 ?섏옄."* and *"6. fcitx5??諛고룷?섏? ?딅뒗??"* Recorded in
  [[2026-11-09-22-04-packaging-names-and-the-ci-shaped-dev-vm]].
- Consequence: jammy ships `hunmin-ibus` beside an unchanged `hunmin`; fcitx5 is
  never bundled for any target; release artefacts come from CI; development moves
  to a VM modelled on the runner.
- Measured while acting on item 1: the tier-C model cannot be repaired as a
  projection rule ??the layer commits at a word-ending symbol under both policies,
  and tier A makes that event look like a digit commit, so any local rule that
  catches `.` also fires on `1`. The three repairs and their costs are 짠PLT-8 in
  `docs/OPTIONS-MEASURED.md`; the outcome (the adapter delivers there) is decided,
  the mechanism is not.
## [2026-11-21] create | modes failures, judgement principles, and domain-mode idea

- Created [[2026-11-21-modes-failures-and-the-domain-mode-idea]]: distilled intent, four live failures with code paths, four corrective principles, five QAS scenarios, a dictionary-coverage probe, and the domain-mode options (recorded, not ruled).
- Updated [[2026-10-05-closed-questions]] with the maintainer's 2026-11-21 request to re-review the closed coding-corpus question; the exclusion stands and no new verdict is recorded.

## [2026-11-22] create | nightly fuzz LSan error path

- Created [[2026-11-22-nightly-fuzz-lsan-error-path]] from the run 44895832326
  artifact: one malformed `--keys` token produced the whole reported leak set,
  while the normal exit was clean. The page keeps the before/after readings,
  the generator grammar fix, and the scheduled-run verification still owed.

## [2026-11-22] create | symbol-layer options for issue #298

- Created [[2026-11-22-symbol-layer-options]]: the third-row "no engine change"
  premise was measured false against the built loader and key API; the probe
  procedure is `tools/measure-symbol-layer.py`, and the comparable designs are
  `docs/DESIGN-ANDROID-SYMBOLS.md`. No ruling is recorded.
- Baseline findings left open: the inner loop ran tier E2 because E2 is not
  labelled `slow`, and one Espresso run hit an emulator disappearance race
  before passing alone. `docs/TESTING.md`'s stale doctor-coverage warning was
  recorded as a contradiction, not silently resolved.

## [2026-11-22] create | Android Korean IME prior-art survey

- Created [[2026-11-22-android-korean-ime-survey]] and
  `docs/ANDROID-KOREAN-IME-SURVEY.md`: HeliBoard/FlorisBoard/fcitx5-android/
  Unexpected Keyboard/open-keyboard compose Hangul; 두벌식 is universal, 천지인 is
  rare, and the mainstream symbol mechanism is a separate symbol page. Read for
  facts, copied nothing (GPL-3/Apache-2.0/LGPL-2.1/MIT recorded per app).
- Recorded the maintainer's verbatim 두벌식 clarification in
  `docs/MILESTONES.md` Track 3 and the survey doc; corrected the "third row, no
  engine change" premise there to point at the measured engine cost. No ruling.

## [2026-11-22] create | symbol/number interaction cost mid-Hangul

- Created [[2026-11-22-symbol-interaction-cost]] and
  `docs/ANDROID-SYMBOL-INTERACTION.md`: the 두벌식 keypad body in HeliBoard/
  FlorisBoard is jamo only (shift/long-press = doubled jamo), digits come from an
  optional number row, symbols from a separate page. Interaction-count table for
  eight mechanisms, with the trade-off each pays.
- Checked the ISO 25010 premise: usability/operability is the relevant
  characteristic, interaction count is an efficiency proxy (ISO 9241-11), not a
  literal clause; time/space/error are the counterweight. Recorded the
  maintainer's pain verbatim. No ruling.

## [2026-11-22] update | symbol interaction: search time and mis-taps

- Refined [[2026-11-22-symbol-interaction-cost]] and
  `docs/ANDROID-SYMBOL-INTERACTION.md` with the maintainer's follow-up: the core
  cost is finding the symbol (visual search), and crowded keys on a narrow screen
  cause mis-taps. Added two axes (discoverability, target size), the prior art
  (HeliBoard dedicated comma/period + grouped symbol page, FlorisBoard adjustable
  key spacing, long-press variant clusters), a geometry-derived target-width
  table vs the 48 dp guideline, and the trade-off triangle. No ruling.

## [2026-11-22] create | Android keypad criteria KP1–KP3 moved into a SPEC

- Created `docs/SPEC-ANDROID-KEYPAD.md` and [[2026-11-22-android-keypad-criteria]]:
  the maintainer's keypad criteria are now requirements KP1 (104-key symbols at
  minimal touch + physical positions, number-row digit↔symbol pairing for
  passwords), KP2 (space/enter/backspace separated, compact, non-physical; one
  shift), KP3 (frequency placement + visual emphasis, extended to a whole-keypad
  hierarchy: characters 진하게, numbers/edit 연하게). Verbatim quotes in the SPEC.
- Added KP1–KP3 rows to `docs/REQ-MAP.md` (parents A1/A2) and the SPEC-OVERVIEW
  child index; implementation/verification `none` (mechanism open, #298).
- Measured: `us-qwerty.json` already holds all 32 ASCII punctuation at physical
  key ids and the number-row digit→shift-symbol pairs, so KP1 Tier 1 needs no new
  symbol data. Recorded the fork: prior art categorises the symbol page, KP1 asks
  for physical positions. No ruling.

## [2026-11-22] update | KP4 direction, symbol-frequency tool, number-row pairing

- Added KP4 to `docs/SPEC-ANDROID-KEYPAD.md` + REQ-MAP + overview index: the
  maintainer's stated direction — Hangul/English/symbol input separated, very
  frequent symbols (`, . ? "`) default-visible in Hangul/English mode. Exact
  symbol set/geometry still open (#298).
- Added `tools/symbol-frequency.py` (counts P*/S* per million, writes meta.json;
  raw text / JSONL / wiki-dump). Finding: local corpora are word-frequency lists
  with no symbols. First prose reading (kowiki 3000 pages stripped) ranks
  `. , ( ) - : " '` first; password symbols low in prose. Recorded in
  `docs/ANDROID-SYMBOL-INTERACTION.md` with caveats (register, `~ | =` markup).
- Compared number-row digit↔symbol mechanisms (long-press/shift/swipe/double-tap)
  on the password case; long-press fits the number row (no competing long-press
  meaning), cost is hold latency. Analysis, not a verdict; updated
  [[2026-11-22-android-keypad-criteria]].

## [2026-11-22] update | KP5 swipe-up + accidental-gesture defences + display

- Added KP5 to `docs/SPEC-ANDROID-KEYPAD.md` + REQ-MAP + overview index: number-row
  symbols by long-press AND swipe-up; an unintended swipe-up must be preventable
  (user error protection); symbol display an open question.
- Analysed accidental-swipe avoidance from Unexpected Keyboard (Pointers/Gesture/
  Config, read for semantics): adjustable distance threshold (swipe_dist_px),
  return-to-center cancel (Roundtrip), commit-on-release + preview, direction
  quantisation; plus velocity gate, movement-vs-time vs long-press, no glide
  conflict on the number row. Display: digit 진하게 + faint corner hint toward the
  swipe direction + popup preview. In `docs/ANDROID-SYMBOL-INTERACTION.md` and
  [[2026-11-22-android-keypad-criteria]]. Analysis, not a verdict.

## [2026-11-22] create | provisional keypad mockup + remaining-decision inventory

- Created `docs/ANDROID-KEYPAD-MOCKUP.md` + `docs/android-keypad-mockup.html` +
  rendered PNG, and [[2026-11-22-android-keypad-mockup]]: drew Hangul/English/
  symbol screens from KP1–KP5 (chrome headless render, NanumSquareRound). Answer
  to "can we draw it yet": yes, provisionally — the picture surfaces the open
  decisions rather than ruling them.
- Inventoried #298's core open decision (symbol-layer data home: A third row / B
  separate page / B3 layout / popup data) and the adjacent Track 3 decisions
  (frequent set, number-row default, geometry, shift state, gesture defaults,
  emphasis, frequency source, Tier 2, candidate/mic). The mockup embodies
  B3 + popup data + dedicated frequent keys as one candidate. No verdict.

## [2026-11-22] update | KP6 symbol-page nav keys, Korean Tier 2 symbols, ~` fix

- Added KP6 to `docs/SPEC-ANDROID-KEYPAD.md` + REQ-MAP + overview index: symbol
  page needs no shift (symbols shown directly); navigation keys instead
  (arrows/Tab/Home/End/PgUp/PgDn); Korean-frequent special symbols (Tier 2) to be
  found and made inputtable.
- Measured the non-ASCII symbol ranking from kowiki prose
  (`tools/symbol-frequency.py`): · 《》 〈〉 「」 『』 ‘ ’ “ ” – — … → ° ± × ○ ℃ lead;
  proposed a grouped Tier 2 set with curated additions (※ ₩ ♡ ★ …) flagged
  unmeasured in chat. In `docs/ANDROID-SYMBOL-INTERACTION.md`.
- Clarified the ~` question: a text-number-row mockup gap (digits 1–0 only); both
  chars are in us-qwerty and on the symbol page. Updated the mockup HTML+PNG
  (symbol page shift removed + nav keys; 4th Tier 2 panel) and
  `docs/ANDROID-KEYPAD-MOCKUP.md` + [[2026-11-22-android-keypad-mockup]]. No verdict.

## [2026-11-22] decision | symbol-page arrangement: grouped []{} order

- Maintainer ruling (verbatim): "[]{} 순서로 결정하자." The symbol page shows all
  32 ASCII punctuation as separate buttons (no shift), grouped order `[]{} `
  applied consistently to `<>` (`,. <>`), ` leading row 2 before `-`. Layout:
  `!@#$%^&*()` / `~-_=+[]{}\| / `;:'",.<>/?`. The partner-adjacent alternative
  (`[{ ]}`, `,< .>`, physical-key pairing) was considered and not chosen.
- Recorded in `docs/SPEC-ANDROID-KEYPAD.md` KP6 (deliberation quotes kept as
  history), mockup HTML+PNG re-rendered with the decided layout,
  `docs/ANDROID-KEYPAD-MOCKUP.md` wireframe, [[2026-11-22-android-keypad-mockup]].
  This sub-decision is settled; data home / nav-key set / Tier 2 inventory remain
  open (#298).

## [2026-11-22] update | KP7 swipe-up universal substitute; nav-key order decided

- Added KP7 to `docs/SPEC-ANDROID-KEYPAD.md` + REQ-MAP + overview index: swipe-up
  on a key reaches its alternate, substituting for long-press/shift (number→symbol,
  ,→?, .→", shift→capslock, letter/jamo→shifted). Grounded in Unexpected Keyboard's
  press/swipe key-value model and FlorisBoard ShiftKeyBehavior (capslock by
  double-tap/cycle). Recorded the glide-typing conflict on letter keys (hunmin has
  no glide today) and the open shift-unlock detail. In
  `docs/ANDROID-SYMBOL-INTERACTION.md`.
- Nav-key order decided (verbatim): "home pgup pgdn end 순서로 변경." Editing
  cluster now Home·PgUp·PgDn·End (Tab leading, ⌫ trailing). Mockup shift keys show
  a ⇪ capslock swipe hint; HTML+PNG re-rendered; mockup doc +
  [[2026-11-22-android-keypad-mockup]] updated. No verdict beyond the maintainer's
  stated rulings.

## [2026-11-22] decision | glide ruled out; KP8 swipe directions bounded by learnability

- Maintainer ruling (verbatim): "글라이드 기능은 불편하기만 해서 추가할 생각이 없어."
  Glide typing will not be added, so the KP7 swipe-up-on-letters glide conflict is
  resolved (no competing gesture). Updated SPEC KP7 + REQ-MAP status.
- Added KP8 to `docs/SPEC-ANDROID-KEYPAD.md` + REQ-MAP + overview index: a key may
  carry different functions per swipe direction, but bounded by learnability
  (verbatim: "너무 복잡하면 사람이 기억하고 쓸 수 없더라"). Analysis in
  `docs/ANDROID-SYMBOL-INTERACTION.md`: avoid the Unexpected Keyboard 16-direction
  extreme; recognition over recall via visible per-direction hints (KP3),
  consistent cross-key mapping, a small direction cap. Mockup stays swipe-up only
  (the simple end). Direction count / functions / hints open. No verdict.

## [2026-11-22] update | KP9 positional consistency, symbol-page functional keys, Tier 2 10/row

- Added KP9 to `docs/SPEC-ANDROID-KEYPAD.md` + REQ-MAP + overview index: same key
  in the same position across modes (fewer mis-taps); symbol page drops space and
  enter, keeps backspace, a return-to-original-mode key labelled 한/영 + the Tier 2
  button take the mode-key slots, arrows go in the space-bar slot. Tier 2 uses 10
  keys/row by keeping the 8-key category groups and moving 2 bottom-row symbols
  up into each (verbatim: "기존에 8개로 묶은 이유가 있을텐데, 맨 아래줄에 있는 기호
  2개씩을 각 줄로 옮기는 방식이 더 깔끔해.").
- Mockup HTML: symbol Tier 1 bottom row now [한/영][Tier 2][←↑↓→] (no space/enter),
  Tier 2 button removed from the symbol row; Tier 2 panel reflowed to 4 rows of 10
  with the same functional layout; PNG re-rendered. Mockup doc wireframes updated.
  Exact geometry provisional; positional consistency + space/enter removal binding.
  No verdict beyond the maintainer's stated direction.

## [2026-11-22] update | Tier 2 has no editing keys

- Maintainer (verbatim): "tier2에는 편집키가 필요없어." Tier 2 is a symbol picker:
  the navigation cluster, arrows and backspace are removed; correction happens in
  the text mode. Its bottom row is just the mode-return keys (한/영, 기호 1) in the
  same slots as Tier 1. Recorded in SPEC KP9; mockup HTML Tier 2 nav row + arrows
  removed (bottom 한/영/기호 1 only), PNG re-rendered, mockup doc wireframe updated.
  No verdict.

## [2026-11-22] update | Tier 2 remaining space — open options analysed

- Question (verbatim): "Tier2의 남은 공간을 뭘로 채우면 좋을까?" Recorded comparable
  options in `docs/ANDROID-KEYPAD-MOCKUP.md` (no ruling): a fifth symbol row (corpus
  has 194 non-ASCII symbols but the prose tail past 40 is thin), a frequent/recent
  row (KP3, needs state), larger keys/taller rows (KP2 error protection), category
  tabs (KP8 complexity caution), an emoji access key. Deciding question: are 40
  symbols enough for the target population; the measured prose tail is thin, so
  rare symbols have diminishing returns vs ergonomics or a frequency row.

## [2026-11-22] decision | KP10 gesture selection over Tier 2 button; extension split to #312

- Maintainer (verbatim): "심볼 키에 대한 swipe 동작이나 longpress로 어떤 기호를 입력할지
  선택하는 게 tier2 버튼을 두는 것보다 나아 보인다. 일단 이렇게만 정리해 두고 이 기능을
  확장하는 건 별도 이슈로 만들자."
- Recorded KP10 in `docs/SPEC-ANDROID-KEYPAD.md` + REQ-MAP + overview index: swipe/
  long-press on a symbol key preferred over a Tier 2 button/page; Tier 2 symbols
  become gesture variants of Tier 1 keys (deprioritises the KP6/KP9 Tier 2 page).
  #298 records the direction only.
- Created **#312** `[android] extend symbol selection via swipe/long-press on symbol
  keys` (labels task/enhancement) for the extension: variant mapping, data home,
  per-key gesture set bounded by KP8. Mockup doc notes the Tier 2 panels are the
  considered alternative, not the chosen path. No verdict beyond the stated direction.

## [2026-11-22] decision | symbol screen data home ruled: app-read data file (B2)

- Maintainer ruling (verbatim): "앱이 데이터 파일로 그리기. 엔진은 관여 안함."
  #298's core question is settled: the Android symbol screen is drawn by the app
  from a data file, engine uninvolved (option B2). No engine/composition-schema
  change; the symbol list stays data (app-read), format/validation a frontend
  contract at implementation. Avoids A's engine cost and B3's settings-list
  visibility contract.
- Recorded KP11 in `docs/SPEC-ANDROID-KEYPAD.md` + REQ-MAP (parent A1) + overview
  index; Decision section in `docs/DESIGN-ANDROID-SYMBOLS.md`; mockup doc core
  decision marked decided (B2); created
  [[2026-11-22-symbol-screen-app-read-data-file]] (wiki/decisions). The symbol
  screen build is follow-up implementation; gesture variants are #312. #298's
  decision phase is complete; closure is the maintainer's.


## [2026-11-22] decision | IBus candidate gloss option A ruled + implemented (FE16, #319)

- maintainer: "A로 구현해서 평가해 보자. 못 쓰겠으면 B로 바꾸자." →
  [[2026-11-22-ibus-gloss-option-a]] 기록. sink_candidates가 aux text로
  하이라이트 후보 글로스를 커서 따라 게시(빈 글로스/닫힘 시 숨김). 후보
  텍스트는 bare 한자 유지 — 공유 기대값 불변, ABI 변경 없음.
- 평가: D2 aux 시그널 assertion green(내용=state自身の detail accessor,
  커서 추적, Escape/커밋 시 클리어). 실데몬 버스 프로브(tools/ibus-gloss-probe.sh,
  패널 활성화): engine→daemon→Panel UpdateAuxiliaryText에 실제 글로스 문자열
  ("회의 등에서…" / "옳을 가 | may, can…"), GTK3 패널 창 렌더(xwininfo),
  Down+Return이 candidate 1 정상 커밋. D3 회귀 green.
- 프로브 발견: 데몬 기본 panel/config 경로 실패→명시 지정, memconf 패널
  빈 엔진 목록 abort→gsettings 시드, IBus 프라이빗 버스→주소 파일 모니터,
  Xvfb 킵맵 모델 필요(pc105), stray ']'는 프로브 아티팩트(엔진 커밋 이후,
  D3 게이트는 클린). 사용성 판정은 maintainer 몫 — 스크린샷 첨부.

## [2026-11-22] decision | IBus 글로스 A 사용성 판정: "잘 동작하네" (maintainer)

- 실사용 세션(Xephyr+gnome-text-editor+패널, sin3-p2)에서 maintainer 직접
  평가 → A(aux text) 사용 가능 확정. B는 불필요(명의된 폴백으로 유지).
- 평가는 물리 한자 키로 수행 — 트리거를 F9+Hangul_Hanja+Control_R 3종으로
  확장한 당일 지시 반영. 물리 키의 keysym은 버스 캡처로 격리하지 못함
  (패널 트래픽만 캡처) — 3종 모두 지원·D2 고정이라 판정은 유효.
- [[2026-11-22-ibus-gloss-option-a]]에 판정 절 추가, REQ-MAP FE16 갱신.
## [2026-11-22] update | IBus evaluation coverage parity with fcitx5 (issue #316)

- Inventoried every fcitx5 evaluation family against IBus; the table lives in
  `docs/TESTING.md` (§IBus ↔ fcitx5 evaluation coverage), one home. Added D1
  mode-persistence / autoswitch-rebuild / symbol-boundary cases, D2 vfunc
  cases (focus_out, reset, candidate_clicked, paging, declines, mode property,
  CapsLock composition), a tier-C `ibus-trace-emit` replaying the SAME x11
  corpus and `.gold` files, and D3 `engine-d3-ibus-edit` / `-candidate` on the
  GTK client. CI gates fail on a skipped or absent IBus lane.
- Probed the XIM client first and dropped it on measurement: IBus's XIM server
  sends compound text (three designations), commits a stale preedit snapshot
  beside the real commit, and routes a teardown flush into the next case.
  Recorded in [[2026-11-22-ibus-coverage-parity]] so the probe is not repeated.
- Fixed two shared-harness defects the lanes exposed: `xtest-type.py` sent no
  Shift for shifted symbols (`^` typed `6`), and the edit loop leaked the
  reused GTK harness across a layout change. Both frontends benefit.
- Budget: the local baseline was already over the inner/slow budgets before
  this change; the delta is measured and reported on the issue, not ruled on.

## [2026-11-22] update | fcitx5 퇴역 시 CI 빌드 시간 — 실측 A/B (maintainer 질문)

- maintainer가 IBus 평가 충분 후 fcitx5 퇴역을 계획하며 PR 빌드 시간 변화를
  질문. 로컬 A/B(러너 동등 조건, -j4, FCITX5 ON/OFF): 빌드 36.6→20.6s(−44%),
  스위트 33.5→15.6s(−53%), 160→131 테스트. IBus 레인은 이미 PR 잡에 있으므로
  교체는 거의 순수 감산. CI 스텝 실측 기반 투영: PR 잡 ~164–173s → ~90–100s,
  그리고 캐시 미스 시 197s fcitx5 빌드 소멸 → 3분 상한이 캐시 적중에 의존하지
  않게 됨. fcitx5에만 있는 것(mutation 티어, GTK4/XIM D3)은 퇴역 기준의
  입력으로 사실만 기록 — [[2026-11-22-fcitx5-retirement-ci-cost]].

## [2026-11-22] update | IBus 글로스 표시 경로 검증 + "못 한다" 정정 (FE16)

- maintainer 요구: "ibus로 전환하기 위해서는 그 기능을 지원할 방법을 찾아야 해"
  → SPEC-FRONTEND FE16 + REQ-MAP 행(roadmap)으로 기록.
- 1.5.26 소스 검증: ibus_engine_update_auxiliary_text 존재, panelbinding이
  candidate panel로 라우팅, candidatepanel의 m_aux_label이 실제 렌더.
  데이터 경로는 이미 존재(hunmin_state_candidate_detail + cursor 변경 시
  candidates 콜백). 대안 B(후보 텍스트에 풀이 삽입)와 비교 표 제시.
- 정정: 앞선 "IBus는 comment 슬롯이 없어 못 한다"는 주장은 틀렸음 — 슬롯이
  없는 것과 표시 경로가 없는 것은 다른 명제. 엔진의 현재 드롭은 선택이지
  불가능이 아니었음. [[2026-11-22-ibus-gloss-paths]]에 기록.
- fcitx5 5.1.7 폴백도 검증: classicui가 auxDown/customLabel 렌더.
## [2026-11-22] update | T3 issues aligned to the KP11 ruling, and #297 built

- Maintainer instruction (verbatim): "T3-1의 결정과 나머지 T3 issue들의 내용이 일치하지
  않으면 지금 고치자. T3-1의 결정으로 대체할 거야." KP11 (`#298`) replaces the
  pre-decision text in #285, #297 and #312: no third per-key symbol row, an
  app-read data file, no engine/ABI/schema change; #297 retitled to the app-read
  symbol data and #312's data-home question closed against KP11.
- Corrected three doc sentences that still called the data home open
  (`docs/SPEC-ANDROID-KEYPAD.md` §KP10, `docs/ANDROID-KEYPAD-MOCKUP.md` closing
  paragraph, `docs/REQ-MAP.md` KP10 row).
- Built #297: `assets/keypad/symbols.json` (KP6's ruled rows, function ids only),
  `tools/symbol-data-check.py` (ctest `keypad-symbol-data`, tier-B guard),
  `SymbolScreen.kt` + `SymbolScreenTest.kt` (tier E1 commit-path golden). KP11's
  REQ-MAP row moved from `none` to `partial` with those pointers.
- Fixed two pre-existing reds the work uncovered, both unreachable while tier E1
  was already failing: `HunminAdapterTest.every layout key has a keycode` scanned
  `layouts/meta.json` (no `keys` object), and the `always-consume` mutation
  anchor had drifted from the trace call's current shape. Created
  [[2026-11-22-t3-2-symbol-data-and-issue-alignment]]; index updated.

## [2026-11-22] create | T3-3 soft keyboard UI session record

- Created [[2026-11-22-t3-3-soft-keyboard-ui]]: issue #299's keypad build, the
  KP12 ruling recorded verbatim, the seven measured mistakes, and the E1/E2
  results. Index updated.

## [2026-11-22] create | IBus 설정 저장소·UI 판정 (#327)

- Created [[2026-11-22-ibus-config-store-and-settings-surface]]: the maintainer's ruling
  ("3 -> a로 설정할 수 있어야 함", "나머지는 추천대로 갈게") read against the decision list of
  that session — five settable rows, a GSettings store, a separate setup binary behind the
  panel's `<setup>`, the key list also editing the daemon's trigger list, live apply, its own
  binary package. Requirement `docs/SPEC-FRONTEND.md` §FE17, pricing
  `docs/OPTIONS-MEASURED.md` §IBC. Index updated.

## [2026-11-22] update | IBus 설정 저장소·창 구현 (#327)

- [[2026-11-22-ibus-config-store-and-settings-surface]] updated with what was built: the store
  (`settings.c`, ctest `engine-d1-ibus-settings`), the window opened from the panel's
  Preferences, the capture path, and the default-layout correction — including the wrong repair
  I made first (reverting the default instead of fixing the tier) and the maintainer's sentence
  that caught it.

## [2026-11-22] update | IBus 트리거 목록 편집과 저장소 분리 (#327)

- [[2026-11-22-ibus-config-store-and-settings-surface]] updated: the fifth row now removes its
  keys from the daemon's global trigger list and gives them back (matching by keysym, because the
  daemon's physical-key entry is `Alt+Release+Alt_R`), and the store's plumbing moved to
  `frontends/ibus/store/` with the budget measurement recorded next to the reason.

## [2026-11-22] update | 창 위젯과 영속성 검증 (#327)

- [[2026-11-22-ibus-config-store-and-settings-surface]] updated: the widget test
  (`engine-d2-ibus-settings-ui`) and the cross-process persistence case
  (`engine-d1-ibus-settings-persist`, scratch keyfile backend) close two rows
  docs/TESTING.md listed as gaps. Corrected §IBC-5's "persistence is CI-only": switching the
  test's backend, not the product's, is what buys isolation; what stays uncovered is dconf itself.

## [2026-11-22] update | 설치 후 실사용 확인 (#327)

- [[2026-11-22-ibus-config-store-and-settings-surface]] updated with the post-install state: the
  engine is the active one in the maintainer's session, his verdict on real typing ("잘 동작",
  한자 입력 포함), and the two things only a live session could show — the trigger list being
  edited in a real database, and a narrowed fifth row leaving `Hangul` to the daemon on a `kr`
  layout.

## [2026-11-23] create | T4-1 prefix-match measurement session (#300)

- `raw/sessions/2026-11-23-t4-1-prefix-match-measurement.md` — the instrument,
  the three pinned registers, the three stated premises that did not hold, and
  the coverage-expansion hypothesis the measurement refuted. Figures live once
  in `docs/OPTIONS-MEASURED.md` §HPX; the page carries none.

## [2026-11-23] create | 312 symbol gesture variants session record

- Created [[2026-11-23-312-symbol-variants]]: issue #312's provisional Tier 2
  variant mapping (shape-family hosts, frequency order), the comparable designs
  in docs/DESIGN-ANDROID-SYMBOL-VARIANTS.md, the structure-only contract +
  selfcheck in tools/symbol-data-check.py, and the E1 parse lane. The kowiki
  reading was re-run and reproduced; no verdict recorded. Index updated.

## [2026-11-23] update | 312 drift corrected — variant mapping removed, rulings recorded

- [[2026-11-23-312-symbol-variants]] marked superseded and rewritten as the
  drift record: the issue body's "variants on symbol-character keys" reading
  was agent paraphrase; the maintainer's three verbatim clarifications
  (swipe-up = shift+key on any key; 기호 mode-key swipe-up → emoji/extended/
  math page picker → symbol page) are now in SPEC KP7/KP10. The variant
  implementation was removed; the corrected design is
  docs/DESIGN-ANDROID-SYMBOL-GESTURES.md. Index updated.

## [2026-11-24] create | T2-3 G3 expectability (#295)

- Created [[2026-11-24-t2-3-g3-expectability]]: the unit a mid-word display flip
  needed (`docs/MODES-MEASUREMENT.md` §2.5 — rendered language, raw ON arm, no
  language on delimiter/digit keys), the 37% gap between the repaired and raw arms
  that caught the first counter, E6's `judge_hold` option and why the default was
  not moved ([[2026-11-01-the-judgement-timing-is-per-key-by-default]]), and the two
  pinned-corpus runs: 2,546 → 477 and 1,687 → 304 flips at an identical `q`, with
  the digit-flip-back count falling 94%. Index updated.

## [2026-11-26] create | the first usability session, and what the trace found

- Created [[2026-11-26-first-usability-session]]: one phone session with the new
  UI trace sink (316 gestures / 272 engine keys). Backspace committing the
  composing word (9/34, reproduced in the oracle, split to #332 / FE18), the
  space bar's swipe-left deleting instead of moving the cursor (ruled, fixed,
  E1 + E2E), and a swipe-up whose "failure" was the intended shift. Index updated.
## [2026-11-27] create | T2 방향: 세 가지 제안과 이슈 트리와의 간극

- Created [[2026-11-27-t2-direction-and-the-three-proposals]]: the maintainer's
  worry that T2 is grading the shipped design instead of proposing, building and
  testing alternatives, checked against #284's scope and Decisions list (eleven
  measurement decisions, no design proposals) and #295's verification line. Records
  the three proposals verbatim (hint / explicit trigger / stem+particle), which of
  the committed G3 pieces survive each, the T1/T3 surfaces they would reuse (host
  candidates + FE16 gloss, FE17's five rows, the E5 chord namespace, KP14
  gestures), the repair-cost arithmetic showing `k` is not the lever, and three
  priced experiments. Also lands `tools/modes-mixed-tokens.py` and
  `docs/MODES-MEASUREMENT.md` §1.5 for proposal 3's population, which the sentence
  corpora drop by construction. Index updated.

## [2026-11-27] update | T2 목적을 기록하고 선행 사례를 조사

- [[2026-11-27-t2-direction-and-the-three-proposals]] updated with §8 (prior art:
  Mozc's keymap, Rime's `recognizer/patterns`, the Korean IMEs on this machine — and
  the premise that got corrected before it became prose: `Tab`'s meaning in our Linux
  hosts is unmeasured) and §9 (what #295 should become: keep the measurement, make G3
  a regression line, hold `judge_hold`, file the comparison as its own issue).
- Recorded Track 2's **purpose** in `docs/MILESTONES.md` §0.4 Track 2 and mirrored it
  into issues #284 (the track), #295 (G3) and #293 (G4): each issue names the design
  it serves, carries its cost and falsifier, and leaves the ruling to the maintainer.
- Created `docs/MODES-PRIOR-ART.md`.

## [2026-11-27] create | 자동 변환이 내 타이핑을 재작성한 사례 (#295 후속)

- Created [[2026-11-27-personal-rewrites]]: `tools/modes-rewrites.py` (the rewrite
  collector, ctest `modes-rewrites-selfcheck`) driven over `~/.zsh_history` and the
  maintainer's quoted Korean. 214 rewritten tokens of 3,379 (6.33%) in 1,608
  commands, 138 commands affected (8.6%), 149 judgement rewrites from 52 distinct
  tokens (`opencode` 60, `kime` 10, `pwd` 8 → 퍟), plus 65 downstream tokens typed in
  the wrong mode after a switch — 65/65 follow an earlier rewrite on the same line.
  Korean side: 6/940 (0.64%) and **none** carried a judgement, i.e. abstention, not
  rewriting.
- The IBus personal key log gained the `judged` field (`docs/trace-spec.md`), set
  where the engine acts on a judgement, compiled out of shipped builds, asserted by
  `engine-d1-ibus-trace-judged`; `trace-optout-ibus` still proves a shipped engine
  carries neither the sink nor its env var.

## [2026-11-27] update | T2의 열린 결정을 한 곳에 (D5–D9)

- [[the-outstanding-decisions]] gained D5–D9, in dependency order, for the
  automatic switch: what "more data" means and when it is enough (D5, blocks the
  rest), which design is prototyped first, whether the user dictionary is a Track 2
  lever, what #295 is from now on (and `judge_hold`'s disposal), and whether a live
  session may be logged. Recorded, not answered — each is the maintainer's.

## [2026-11-27] update | 트레이스 작업과 핸드오프

- [[2026-11-27-t2-direction-and-the-three-proposals]] §10 added: what a new
  session needs to continue — branch/worktree, the six runnable pieces, the one
  blocked step (`sudo tools/ibus-trace-enable.sh enable` + `ibus restart`), where
  the log lands, what to run when it does, and the two drifts not to repeat.
  Also fixed the enable script's first real failure: it wrote through a running
  engine ("Text file busy"); every install now writes beside the target and
  renames over it.
