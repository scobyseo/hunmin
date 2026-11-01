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
  `docs/OPTIONS-MEASURED.md` §AND; the two parts that generalise on
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
  mutation-tier detail → `docs/OPTIONS-MEASURED.md` §AND pointers.
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

## [2026-10-12] create | Compact HUD theme traps; Android UI notes

- **Durable artefact is `docs/ANDROID-UI-NOTES.md`**, the Android analogue of
  `docs/FCITX5-ADDON-NOTES.md`: the style system (dock palette vs `CompactTheme`,
  row anatomy, 0.92-width window) and the traps that cost the restyle its time —
  `AlertDialog.setView()` adopting the view's default `WRAP_CONTENT`
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

- **2026-10-12** — `2026-10-11-dock-and-the-zero-width-style` marked
  **superseded**: the maintainer ruled "F5 -> remove!" and the unreachable
  `DockToolbar`/`DockStyle.kt` is deleted (issue #33, branch
  `task/android-design-review`). The page keeps the measurement as
  evidence; its outgoing wikilinks were unlinked so no active page cites a
  retracted one.

## [2026-10-12] create | Issue #9 ruled: versionName keeps the phase suffix

- **New decision.** `decisions/2026-10-12-versionname-keeps-the-phase-suffix.md`:
  the maintainer ruled, answering issue #9, that release 0.1's Android
  `versionName` stays `0.1.0-p5` — `0.1.0` is the release (agreeing with
  `CMakeLists.txt` and `debian/changelog`), and `-pN` is the Android
  build-phase label (P5 the IME, P5.5 the keymaps APK; `docs/ENVIRONMENT.md`
  batch 2). The ruling was given in the worktree session that implemented it,
  not in a separate session record, so the issue is the surviving source.
- **Consequential updates, not wiki actions:** `docs/MILESTONES.md` §0.1 now
  records the convention (the issue's second verification checkbox), and
  `README.md` §Install says which string is the release and which is the
  phase (the issue's "keeps the phase marker" branch). No version string
  changed — the ruling is to keep them.
- **NOT closed by the ruling, named so it is not re-derived:** `versionCode = 1`
  has never been incremented, and no tag or GitHub release exists yet; both
  are release-time acts, not documentation.
- `wiki/index.md` listed the page; total pages 26.

## [2026-10-12] create | B4's 0.1 half ruled: auto toggle inert, default key composition disabled

- **New decision.** `decisions/2026-10-12-auto-toggle-disabled-by-default-in-0-1.md`:
  the maintainer ruled the 0.1 shipping half of B4 (issue #2) — the auto
  Hangul/English toggle is disabled by default in 0.1, and the named mechanism
  is "disable the default key composition". In the tree: `AUTO_TOGGLE` gets no
  default chord (Right Alt + Enter unbound; Enter reaches the editor), the
  service dispatch skips the action so a stale stored binding cannot swallow
  the chord, and the settings row renders disabled with badge 미정 (0.3) —
  the R13-정정 divergence (screen advertising a chord whose action logs "not
  yet implemented") removed. The ruling is quoted verbatim in
  `raw/sessions/2026-10-12-b4-auto-toggle-inert-in-0-1.md`.
- **What it does not close:** the E5 assignment itself (single vs compound),
  B1/D1, B2 — all named on the decision page; the assignment stays a 0.3-lane
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
  `hunmin_suggest_text` reading is refuted — it is a bounded read that does not
  rebuild — and a separate cross-context clobbering of the process-wide
  candidate list is recorded alongside it. **No ruling:** what the page size
  should be, or whether labels should be supplied, is the maintainer's.
- **New procedure.** `tools/repro-f9-candidates.sh`: the full stack under Xvfb
  with classicui enabled, which no ctest tier is. It carries the two findings
  that cost time here: `XK_F9` is 0xFFC6 (0xFFC2 is F5), and `FCITX_LOG_RULE` is
  not read by fcitx5 — the rule is `--verbose`, and `tests/e2e/run-d3.sh`'s
  export of it has never had any effect.
- `wiki/index.md` listed the new page; total pages 29.
- **Ruling applied (issue #45).** The maintainer ruled the F9 candidate page is
  **capped at 10** — fcitx5's label ceiling — rather than holding the whole
  list on one page with `setLabels()`. In the tree:
  `frontends/fcitx5/src/engine.cpp` carries a named `kMaxCandidatePageSize = 10`
  (named because the number is fcitx5's `regularLabelSize`, not a preference),
  and `frontends/fcitx5/test/testhunminui.cpp` asserts
  `pageSize == min(totalSize, 10)` — measured red with the cap reverted
  ("page size is 67"), green with it. Verified end to end with classicui
  enabled: 낙(11) and 간(67) no longer abort, and Page_Down reaches pages 2 and
  3 with the cursor on the first candidate of each, Up wraps to the last, so
  the cap made nothing unreachable. **Not closed by the cap:** no ctest tier
  draws the F9 window, so the abort still cannot regress through `ctest` alone
  and `tools/repro-f9-candidates.sh` runs in no CI job. Recorded in the
  measurement page's "What it did not close".

## [2026-10-13] update | The deb job's last mile (dpkg-shlibdeps) measured; windows job disabled

- **Two CI jobs red, and the causes are unrelated.** Measured on run 38398739533,
  and the same two on `master`: `debian (.deb artefact)` died at `dh_shlibdeps`
  *after* all 63 tests had passed inside the package build —
  `no dependency information found for
  /lib/x86_64-linux-gnu/libFcitx5Core.so.7 (used by .../fcitx5/libhunmin.so)` —
  while `windows` died before compiling a line,
  `cl : command line error D8021: invalid numeric argument '/Wextra'`.
- **The deb cause was measured, not guessed.** dpkg answers that question with
  `dpkg -S <library-file>` plus the owning package's `.symbols`/`.shlibs`, and the
  CI job installs fcitx5 5.1.13 **from source** into /usr: the dependency is
  satisfied, the package metadata does not exist. It is the same fact the job
  already encodes by passing `-d` to `dpkg-buildpackage`.
- **The fix was chosen by measurement, before it was committed.** The CI shape
  reproduces locally with `--admindir` pointing at an empty dpkg database:
  identical error, nonzero exit; `--ignore-missing-info` → rc 0.
  `debian/shlibs.local` does not apply — still fails, in both the soname and the
  absolute-path form the man page allows. So `debian/rules` gained one
  `override_dh_shlibdeps` line and one comment recording what it does not assert.
  It is not a blanket waiver: the flag skips only what cannot be *resolved*, and
  building the .deb on this host (fcitx5 packaged) still produces
  `Depends: ... libfcitx5core7 (>= 5.1.19) ...`. Only on a source-built host do
  those three drop out, leaving `Depends: fcitx5` — unversioned, where the addon
  needs >= 5.1.13. That gap is a packaging decision and is left open.
- **A second deb failure, invisible until the first was fixed.** With
  `dh_shlibdeps` past, the build ran all 63 tests and wrote
  `../hunmin_0.1.0_amd64.deb` — and the *next* step failed:
  `Invalid pattern '../hunmin_*_*.deb'. Relative pathing '.' and '..' is not
  allowed.` `upload-artifact@v4` refuses a pattern reaching above the
  workspace, so the package is now copied into `artifacts/` before the upload.
  Worth writing down for the reason it is a finding rather than a nuisance: a
  job cannot report a second failure until the first one is gone, so "the job
  is nearly green" is a claim about the order defects appear in. The same step
  also printed `tar subprocess returned error exit status 2` — that was EPIPE
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
  `docs/OPTIONS-MEASURED.md` §6.1's "four jobs, no nightly" is corrected
  with them. While the job is disabled, **no job compiles the core with `cl`**.
- **Not done, deliberately, and tracked elsewhere:** whether the F9 candidate
  window gets a ctest tier, a CI job, or neither is a tier-design decision. It
  was item 1 of issue #51; at the maintainer's instruction — "F9 테스트는 별도
  이슈로 등록하고 이 작업 범위에서 제외." — it is now **issue #53**, and its
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
  (issue #53): built the option-A disposition — `engine-d3-candidate`, a
  tier-D3 variant with classicui enabled, so the F9 candidate window is drawn
  for real and the #45 abort is reachable from `ctest`. Measured red with the
  cap reverted: `CommonCandidateList: invalid label idx`, fcitx5 SIGABRT
  (core dumped). Records the three per-case assertions, the two injector
  facts (named keys must be braced — a bare `Return` is six Latin keys; F9
  opens the window asynchronously — a Return at 40ms lands before
  `candidateActive()`), and a Down/Page_Down drift between the #45 wiki
  sentence and the engine's arrow handler, recorded not adjudicated. The
  disposition is the maintainer's to confirm or overturn; see
  [[decisions-belong-to-the-maintainer]].
- `wiki/index.md` listed the page under Raw — measurements.
- `wiki/decisions/2026-10-16-0-1-distribution-channel-github-release.md` — the
  channel ruling issue #7 asked for: a GitHub Release on tag `0.1.0` with the
  `.deb` attached. Records the cost of the three options the issue named as
  comparable, and what the ruling does not close — no tag or release exists, so
  the channel is still unverified from a clean machine; the `.deb` is unsigned;
  the signed APK stays blocked on issue #1. The procedure lives in
  `docs/MILESTONES.md` §0.1, not here.
- `wiki/index.md` listed the page under Decisions.
- `wiki/decisions/2026-10-16-the-repository-is-private-until-after-0.2.md` — the
  maintainer ruled `scobyseo/hunmin` stays private through 0.1, public after 0.2.
  Records the measurement that prompted it (release published, `.deb` correct at
  1,252,740 bytes, asset URL and release page both 404 unauthenticated), that #7
  therefore stays open, and the consequence: 0.2 is the first release whose
  documents are read by a stranger, and the 0.1 `.deb` already ships a
  `README.md.gz` claiming no release existed. `tools/release-0.1.sh` now verifies
  reachability unauthenticated against actual repository visibility.
- `wiki/raw/sessions/2026-10-17-issue-72-buffering-docs-closure.md` — issue #72's
  doc work completed on the maintainer's direction to add no new issue: the
  four `TSF-BUFFERING-72` assertion rows (the board named #72 nowhere while the
  issue's own REMAINING-WORK deliverable was open), and the design-doc drift
  correction — `docs/WINDOWS-TSF-BUFFERING.md` claimed `ITfInsertAtSelection`
  and `EndComposition`, but the shipped adapter inserts via a synchronous
  edit session's `ITfRange::SetText` (`FlushSession`, `DeactSession`) and never
  opens an inline composition; the doc now describes the shipped mechanism.
  TAT measurement and a ctest TSF tier remain open under milestone B (#65),
  recorded on the row, not hidden. The issue close itself is left to the
  maintainer.
- `wiki/index.md` listed the page under Raw — sessions.
- `wiki/raw/sessions/2026-10-17-tsf-scaffold-milestone-a-verification.md` — the
  pre-close verification pass over issue #64: the milestone-A branch was a
  rebase no-op because master had already absorbed the scaffold (plus the
  PR-#76 buffering adapter beyond A), the MSVC and llvm-mingw build
  measurements taken on the Windows host, the 51/52 inner-loop result with
  the maintainer-stated by-nature `data-selfcheck` failure, and the stale
  host-fact corrections it drove through `docs/ENVIRONMENT.md`,
  `docs/MILESTONES.md` and `docs/REMAINING-WORK.md` (with new TSF assertion
  rows). The issue close itself is left to the maintainer.
- `wiki/index.md` listed the page under Raw — sessions.
- `wiki/decisions/2026-10-16-release-gate-all-tests-pass.md` — addendum, not a
  rewrite: the maintainer's 2026-10-17 instruction unregistered
  `data-selfcheck` from ctest (not touched before its own dev stage; red on
  every pass on the Windows host), so the gate's exception list holds
  `modes-corpus-selfcheck` alone and `tools/release-gate.sh` was updated with
  the selftest fixtures re-planted. The ruling quote is kept verbatim.

## [2026-10-17] create | clang-format retuned to the tree's own style, applied once (issue #77)

- `wiki/raw/sessions/2026-10-17-clang-format-retune-option-2.md` — the
  implementation pass over issue #77: the maintainer's ruling (option 2,
  steered "ideal for AI workload") kept verbatim, the churn ladder that
  selected `ColumnLimit: 0` (retuned limit-100 1491 → limit-0 without
  `AllowShort*` 1673 → limit-0 with them 853 `-w` lines; trailing-comment
  alignment measured a no-op under limit 0), the `.clang-format-ignore` probe
  and the `--style=file:<path>` trap, the split configs landing
  (root C core, `frontends/fcitx5`, `frontends/tsf` after `tsf.cpp` fell
  through to the root config once), and the machine-checkable state: 51/51
  inner-loop ctest, idempotent second pass, vendored and generated trees
  excluded. The issue close is the maintainer's; see
  [[decisions-belong-to-the-maintainer]].
- `wiki/index.md` listed the page under Raw — sessions.

## [2026-10-17] update | .clang-format-ignore directory patterns need the ** glob (issue #77)

- `wiki/raw/sessions/2026-10-17-clang-format-retune-option-2.md` — the first
  full `-n` sweep caught the ignore file's directory pattern `data-orig/`
  failing to match files inside the directory; the glob form `data-orig/**`
  does. The rule and the probe measured with hangulctype.c/cJSON.c/tables.c
  are recorded on the session page; the committed `.clang-format-ignore` uses
  the glob form in the same pass.
- `wiki/raw/sessions/2026-10-17-milestone-c-layout-wiring-data-loading.md` —
  milestone C (issue #66): the HKCU layout-pair store mirroring the fcitx5
  config, slot validation by supported+mode, the single-source data rule
  (measured: the additive chain double-loaded a staged tree, 15 files →
  layout_count 30), tier-F1 additions `tsf-config-test` / `tsf-datadir-test`
  and the dubeolsik win32 trace (gold reviewed 400/400 vs tier A), and the
  measured tier-registration drift — the standalone frontends/tsf configure
  had been registering zero F1 tests until `enable_testing()` and the
  executable-keyed Python3 guard landed.

## [2026-10-17] create | Settings dialog — WinUI 3 exe (issue #71)

- `wiki/raw/sessions/2026-10-17-settings-dialog-winui-exe.md` — the maintainer
  ruling executed: the separate WinUI 3 settings exe. Carries the measured
  window sizes (exe 545x674 px, interim dialog 493x434 px — "compact" as a
  number), the ResizeClient-DPI-factor and XamlCompiler host quirks, the one
  binding by key capture ("키 입력 방식"), the publish 495 files / 167.4 MiB
  figure for #68, and the still-open device-bound and maintainer-sizing
  questions.
- `wiki/raw/sessions/2026-10-18-milestone-d-candidate-edit-mode-ux.md` — issue
  #67's milestone D: candidate window + Hanja key store + hook-based key
  capture + the screen-presence defect roots (Mica, ownerless rundll32) +
  Start-menu entry + conversion compartment + system-langbar menu
  extension + the krdict (ko,hanja) dedupe (34,142 → 33,184); overturned:
  the langbar surface measured closed (reference-item probe), the sink
  direction; pending: last-syllable fallback, mode-persist, re-register,
  CI billing.
## [2026-10-18] create | CTRL/ALT chord in hangul mode ruled: transient, no mode switch (issue #101)

- **New decision.** `decisions/2026-10-18-tsf-chord-transient-not-persistent.md`:
  the maintainer ruled in an issue-#101 comment — "will not switch mode;
  transient processing is enough." The shipped chord rule (commit the pending
  preedit through the boundary flush, close the candidate window, pass the
  chord to the app uneaten) already matches, so the ruling adds no code; the
  mode-persist alternative recorded since the milestone-D session is closed.
- What the ruling does not close, recorded on the page: the tier-F1
  behavioural pin (trace-emit.cpp injects only the Shift bit — a ctrl/alt
  chord case waits on the trace-spec.md mods bits), and the chord-chain order
  ahead of the chord block (candidate-window keys see modifiers; the chord
  flush can land on the Ctrl keydown itself) — read off the code, not measured.
- The ruling was given on the issue, not in a session record, like issue #9's
  ruling in the worktree session — the issue is the surviving source.
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
  the tree: option 1b (FetchContent, not vendored — the issue's
  "disqualification" did not survive measurement), the engine tier-A trio
  under gtest, the four TSF runner groups merged into one executable with
  one add_test and the rc-77 probes outside the binary, the never-shrinks
  gate amended (59 → 56 on the MSVC tree), and the two host lessons (the
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
  name. Page count moved 58 → 60.
- `wiki/test-scenario-guide.md`: the tier table contradicted `docs/TESTING.md`;
  replaced with a pointer to `§The vocabulary` and the three specific errors
  named (tier-B, tier-D, E3).
- `wiki/ci-android-lessons.md`: the Recommended CI Flow section and the tier-job
  mapping corrected to match the workflow and `docs/TESTING.md` (automated
  Android tiers are E0/E1/E2, no E3).
- `wiki/index.md`: dropped the hand-maintained page count (points at
  `python tools/wiki-lint.py` instead).
- `tools/docs-lint.py`: the cross-check of index.md's page count removed.
- Also: `docs/REQ-MAP.md` re-pointed after heading renames.

Verified: `python tools/wiki-lint.py` → 60 pages, 0 errors; `python tools/docs-lint.py` → 0 errors.

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
  failure — composed 고 where tier A records commit ㄱ + preedit ㅋ), and the
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

## [2026-10-31] create | E5's assignment ruled: modifier + 한/영, default and remappable (#159)

- `wiki/decisions/2026-10-31-han-eng-modifier-compound-special-functions.md`: the
  maintainer's two sentences verbatim (compound = the special-function namespace, on all
  three frontends; default behaviour, composition selectable in the config dialog), and
  the sub-decisions that were *not* made, listed rather than absorbed.
- Consequential updates, not wiki actions: `docs/SPEC-MODES.md` §E5 (assignment decided
  → `[D]`, remaining parameters named), `docs/MODES-ENGINE-RULES.md` §3 (the hook's
  allocation), `docs/OPTIONS-MEASURED.md` §E5 header row (§E5-3's option 1/2 family is
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
  the bare Hanja key (`docs/SPEC-TSF.md` §TSF9, issue #67) and Android carries
  `KEYCODE_EISU` as a `platformAlternates` entry, while Android's *row* reads "not set"
  because it is an alternate and not a default.

## [2026-10-31] create | The special functions are engine-resolved, frontend-implemented (#159)

- `wiki/decisions/2026-10-31-special-functions-engine-resolved-frontend-implemented.md`:
  the maintainer's paragraph verbatim (frontend supplies key codes, engine processes and
  invokes the frontend interface, an unsupported platform refuses), the five-symbol shape
  that implements it, and the four things it leaves open — the per-function default chord
  most of all, because choosing it per frontend is the divergence the ruling removes.

## [2026-11-01] update | B1's question was answered elsewhere (#161)

- `wiki/decisions/the-outstanding-decisions.md`: D1 carries a dated status — §R8-정정
  ruled option (c) and the English half was ruled by §R8-정정-2, so the table is the
  record of the evidence, not the current definition; D2 records that its krdict/`pos`
  route left the scope boundary and §R8-정정 removed the need for it. Neither is
  retired here: the board row's fate is issue #161, priced in
  `docs/OPTIONS-MEASURED.md` §B1.

## [2026-11-01] rule | B1 retired — the Hangul judgement needs no source (#161)

- `wiki/decisions/2026-11-01-b1-retired-the-hangul-judgement-needs-no-source.md`:
  the maintainer's "want to select A" verbatim. The row had been emptied by
  §R8-정정 (structure test, no lookup) and §R8-정정-2 (the English source, shipped,
  #160 closed) — neither of which the board or MILESTONES mentioned, which is why
  a decided question kept reading as the root of five blocked rows.
- Propagated the same day: board §1's B1 row, §2 retitled and restated per item,
  the voice stage 8 row, the assertion table's decision-blocked comment,
  MILESTONES' "hostage to B1" lines, and PLAN's P8 "judgement source" row.
- Left open and named: the corpus-list delete-or-keep question §R8-정정 wrote down,
  E1's lack of an owner, and §R8-정정's stale "left open" sentence against R9.

## [2026-11-01] rule | 옛한글 layouts get the widened validity rule, per layout (#166)

- `wiki/decisions/2026-11-01-old-hangul-layouts-get-the-widened-rule.md`: the
  maintainer's "it is optional for layouts which support 옛한글" verbatim. The rule
  is unchanged and only the jamo sets widen, so `랴ㅜㄷ` stays invalid while `ᅀᅡ`
  `ᅌᅵ` `ᅀᅡᆶ` `ᄀᆞ` — what the 옛글 layouts actually render — are valid.
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
- Created `decisions/2026-11-01-the-judgement-timing-is-per-key-by-default.md`
  (E6 ruled, issue #163: `judge_timing` 1/0, per key by default, display-only) and
  added it to `index.md`.
- Corrected `decisions/2026-11-01-the-engine-judges-the-frontend-switches.md`:
  R9's threshold unit is **key inputs**, named by the maintainer
  ("3. 키입력 세개로 명확히 하자."), not the `3자` the page had repeated from the
  ruling's wording. Same correction in `docs/SPEC-MODES.md` §R9/§E3 and
  `docs/OPTIONS-MEASURED.md` §MRO/§BND-3.

