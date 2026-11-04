# Wiki Index

> Content catalog. Every page listed under its type with a one-line summary.
> Read this first to find relevant pages for any query.
> Last updated: 2026-10-25   (no page count: three counters disagreed, so the
> number a reader needs is the one a tool prints — `python tools/wiki-lint.py`)

## Concepts

- [[the-three-corpora-are-three-registers]] — why a stemming verdict reverses
  when the corpus changes, the eight measurements that established it, why the
  three word lists are kept apart, and the one contamination worth knowing.
- [[the-rules-keep-breaking]] — every rule that failed on 2026-10-04 and why
  writing it down did not help; the five instances of a test that was green
  while measuring nothing, and the two defences that exist for them.
- [[where-the-practices-came-from]] — the five working rules this project runs
  on, in the maintainer's words, with the date each was given and the artefact
  it produced.

- [[the-tsf-extraction-round-2026-10-22]] - the 2026-10-22 TSF
  extraction round in one page: eleven stages, `tsf.cpp` 2,320 -> 528,
  and the detach defect left for a ruling.

- [[test-scenario-guide]] - which tier a change touches, and the CI limits each runs under.
- [[ci-android-lessons]] - the seven Android-job fixes and the job split.

## Decisions

- [[2026-11-02-spec-modes-4-splits-into-an-invariant-and-a-metric]] — §4 read as two claims of different kinds: the decidable half became R14
  (A2's child, oracle = the recorded corpus, verified by `engine-off-identity`),
  and the ON half points at ③ unrepresentability instead of restating it; why
  `req-map` could not see the section at all.
- [[2026-11-02-interim-cannot-support-autoswitch-and-on-stops-passthrough]] — two rulings
  from one message: the interim delivery cannot carry the switch (and why it is
  structural), and with the switch on English stops passing through so R8's E1 line
  can judge; what each leaves open is in the page, not here.
- [[decisions-belong-to-the-maintainer]] — the standing instruction that
  measurements are the agent's and rulings are the maintainer's, and the three
  later instances where it bound.
- [[the-outstanding-decisions]] — D1 hangul judgement method, D2 E1 symmetry,
  D3 whether 119 may inform a shipped list, D4 whether this wiki stays.
- [[2026-11-01-b1-retired-the-hangul-judgement-needs-no-source]] — B1 retired by the
  maintainer's "want to select A": the Hangul half is a structure test and the
  English half is shipped, so board §2 waits on implementation, not on a source.
- [[2026-11-01-old-hangul-layouts-get-the-widened-rule]] — the same shape rule with the
  extended jamo sets, used only where a layout composes 옛한글, and the per-layout
  answer derived from the layout data.
- [[2026-10-05-closed-questions]] — the romaja registration is correct as it
  stands, and the coding corpus was curiosity; both closed by the maintainer,
  with the reasoning so neither is re-raised.
- [[2026-10-11-auto-switch-off-by-default]] — B3 ruled: the automatic
  Hangul/English switch is OFF by default until 0.3, and its additional data
  ships separately, the feature enable-able only where the data is present.
  What the ruling does *not* close (D1, E5's assignment, B2) is named.
- [[2026-10-12-versionname-keeps-the-phase-suffix]] — issue #9 ruled: release
  0.1's Android `versionName` stays `0.1.0-p5` (and the keymaps APK
  `0.1.0-p5.5`) — `0.1.0` is the release, `-pN` the build phase. What the
  ruling does *not* close: `versionCode = 1` was never incremented, and no
  tag exists yet.
- [[2026-10-16-release-gate-all-tests-pass]] — the release gate, as the
  maintainer ruled it on 2026-10-16: "on release, all TCs in this repo should
  pass, except data-selfcheck which cannot pass". Encoded in
  `tools/release-gate.sh`. What the measurement added: the tree has **three**
  skips on this host, not one — `modes-corpus-selfcheck` is the same
  corpus-outside-the-repo class, and `engine-android-e2e` skips on the attached
  AVD for want of a focusable Settings field.
- [[2026-10-16-0-1-distribution-channel-github-release]] — issue #7 ruled: a 0.1
  user gets the `.deb` and the APK from a **GitHub Release on tag `0.1.0`**,
  scripted in `tools/release-0.1.sh`. The release and its `.deb` are published
  (2026-10-16). What the ruling does *not* close: the `.deb` is unsigned, the
  signed release APK stays blocked on issue #1, and reachability is a separate
  question — see [[2026-10-16-the-repository-is-private-until-after-0.2]].
- [[2026-10-16-the-repository-is-private-until-after-0.2]] — the maintainer
  ruled `scobyseo/hunmin` stays private through 0.1 and goes public after 0.2, so
  every artefact up to 0.1 is account-gated (asset URL and release page both 404
  unauthenticated). Issue #7 therefore stays open: the channel is published and
  its bytes are correct, but not anonymously reachable.
- [[2026-10-12-auto-toggle-disabled-by-default-in-0-1]] — B4's 0.1 half ruled:
  the auto Hangul/English toggle ships inert in 0.1, by disabling the default
  key composition — no default chord, the dispatch skips the action, the
  settings row shows 미정 (0.3). The E5 assignment itself stays open for 0.3.
- [[2026-10-13-b8-keystore-path-resolved]] — issue #1 (B8) ruled:
  the keystore config lives in gitignored `local.properties`
  (`frontends/android/local.properties`); nothing in committed build config
  hardcodes a host path. Both `app` and `keymaps` resolve
  `storeFile`/`storePassword`/`keyPassword`/`keyAlias` from `local.properties`,
  honor the `HUNMIN_RELEASE_STORE` env override (CI `android-release` job +
  `tools/build-release.sh`), and generate a throwaway release key at build time
  when no keystore is present.
- [[2026-10-13-r8-mode-guessing-is-adaptor-owned]] — R8 mode guessing is
  adaptor-owned; the engine stays ABI-clean and the adaptor (`modeswitch.*`)
  keeps the cross-frontend validity judgement. **Superseded 2026-11-01.**
- [[2026-11-01-the-engine-judges-the-frontend-switches]] — the judgement is the
  engine's and the switch is each frontend's; R9's threshold at three key
  inputs, and the fcitx5 detector retired.
- [[2026-11-01-the-judgement-timing-is-per-key-by-default]] — E6 ruled: both
  judgement timings ship as the `judge_timing` option and the per-key flicker is
  the default; the delivered text is identical either way (64.05% of dictionary
  forms flip back inside the word under the default, with R9's guard applied).
- [[2026-11-01-the-symmetric-rule-ships-and-the-0-3-gate]] — E1 ruled: both lines
  of R8's formula ship (the Hangul reading of Latin-mode keys), and the 0.3 gate is
  the maintainer's mixed sentence `English는 좋은 Language입니다` typed as one stream.
- [[2026-11-01-one-dict-by-default-the-release-page-later]] — issue #172 ruled:
  `Recommends` and one dictionary on every surface; the words stay a package of
  their own as a seam for the *next* dictionary, whose mechanism (the GitHub
  release page) is deferred. The feature is developed and tested with the
  in-tree data, before any distribution mechanism exists.
- [[2026-11-01-syllable-commit-turns-autoswitch-off]] — R6 measured (§BND-7): the
  digit run is the buffered policy's, so the maintainer ruled the two settings
  dependent — reconciled at settings time, off-switched at run time when syllable
  commit is on, and the reason named in the debug output. Autoswitch has no
  settings row in any frontend yet, which the work has to add first.
- [[2026-10-18-tsf-chord-transient-not-persistent]] — issue #101 ruled: a
  CTRL/ALT chord in hangul mode is transient processing only — the pending
  preedit commits through the boundary flush and the chord passes uneaten, but
  the mode word stays hangul; no code change was needed because the shipped
  rule already matches. What the ruling does not close: the tier-F1
  behavioural pin waits on ctrl/alt mods bits trace-spec.md does not define,
  and the chord-chain order ahead of the chord block is recorded, not decided.
- [[2026-10-31-han-eng-modifier-compound-special-functions]] — E5's assignment
  ruled: single press stays the manual Hangul/English switch, and modifier +
  한/영 is the **namespace** for special functions (voice input, auto-mode
  switch), on all three frontends. What it does not close: which modifier
  carries which function, whether the addon may shadow fcitx5's default
  `Hangul` trigger, fixed vs remappable, and the fact that the auto-switch state
  still does not exist in the engine.
- [[2026-10-31-hanja-key-default-f9-was-a-test-scaffold]] — the Hanja key is the
  default trigger for 한자 입력 (EISU / `VK_HANJA` / the `Hangul_Hanja` keysym),
  F9 was a test scaffold and is not the product default, and the binding is
  configurable in the settings dialog. What it does not close: whether F9 stays
  accepted as an alternate (the conformance corpus drives `{F9}`), and the
  engine-vs-frontend store question it shares with the E5 assignment.
- [[2026-10-31-special-functions-engine-resolved-frontend-implemented]] — the
  mechanism behind that assignment: the frontend supplies its key codes, the engine
  owns the function→chord binding, the resolution and the refusal, and it invokes
  a registered frontend host for functions it cannot perform. What it does not
  close: which chord each function gets (the table starts empty and the default is
  still to be chosen once), what a refusal does to the key, and whether the engine
  also owns the `autoswitch` state.
- [[space-emission-ownership]] — **contested.** The measurement is not in
  dispute: the engine declines the space and every frontend forwards it. But
  the log and `docs/REMAINING-WORK.md` B5 record the *decision* as closed with
  no quoted ruling, and the measurement page says it is still the maintainer's.
  Named here, resolved by nobody.

## Raw — sessions

- [[2026-10-03-requirement-shaped]] — the hangul/latin requirement in the
  maintainer's words: the quotations `docs/SPEC-MODES.md` paraphrases or omits,
  why one IM and a mode that follows what is typed, the licence distinction that
  was later restated wrongly, and the standing instruction about prior art.
- [[2026-10-10-mutation-tiers-measuring-nothing]] — the Android mutation tier
  counted its own skips as catches and reported 9/9 in 0.31s with nothing built;
  a red baseline reads as nine working mutations; the sandbox copy filter
  returned an empty copy reporting *0 MB, 100% saved*. The figures are in
  `docs/OPTIONS-MEASURED.md` §AND; what generalises is here.
- [[2026-10-11-b3-off-default-and-the-data-gate]] — the B3 ruling in the
  maintainer's words, and why the second half of the sentence (the separate data
  package, enabled only when present) matters as much as the first: it replaces a
  date with a condition.
- [[2026-10-12-b4-auto-toggle-inert-in-0-1]] — the B4 0.1 ruling session: the
  measurement comment that framed inert-vs-working, the maintainer's two
  sentences, and the three tree facts that make "inert" true — the chord is
  the binding, not the row.
- [[2026-10-02-working-practices]] — **superseded** → [[where-the-practices-came-from]].
- [[2026-10-04-stemming-investigation]] — **superseded** →
  [[the-three-corpora-are-three-registers]], [[the-rules-keep-breaking]].
  Keeps only the grep that settled "commercial software does it this way".
- [[2026-10-10-orca-probe]] — **superseded** → `docs/ENVIRONMENT.md` §Orca GUI.
  A host toolchain fact, not a wiki topic.
- [[2026-10-11-orca-orchestration-path]] — the orchestration layer measured end to
  end: ≈40 turns and two accidental Runs bought by guessing flags that one
  `orca agent-context --json` answers, and the four places the documents were wrong
  about what Orca actually does. Durable artefact: `docs/ORCHESTRATION.md`.
- [[2026-10-12-compact-hud-theme-traps]] — the Compact HUD restyle's four theme
  traps in the order found (badge weight, missing LayoutParams on `setView`,
  `android:background` as a theme item, a Dialog-parented host activity), and an
  E2E assertion that failed on a correct UI. Durable artefact:
  `docs/ANDROID-UI-NOTES.md`.
- [[2026-10-17-tsf-scaffold-milestone-a-verification]] — the pre-close pass
  over issue #64: master had already absorbed the milestone-A scaffold (plus
  the PR-#76 buffering adapter), the MSVC and llvm-mingw build measurements
  on the Windows host, the 51/52 inner-loop ctest result with the
  by-nature `data-selfcheck` failure, and the stale host-fact corrections in
  `docs/ENVIRONMENT.md`, `docs/MILESTONES.md` and `docs/REMAINING-WORK.md`.
- [[2026-10-17-clang-format-retune-option-2]] — issue #77: the ruling (option
  2, steered "ideal for AI workload"), the churn ladder that picked
  `ColumnLimit: 0` (1491 → 1673 → 853 `-w` lines), the `--style=file:<path>`
  trap, and the split-config + ignore-file landing with 51/51 inner-loop
  ctest and pass-level idempotence.
- [[2026-10-17-issue-72-buffering-docs-closure]] — issue #72's doc work
  finished at the direction to add no new issue: the four `TSF-BUFFERING-72`
  assertion rows the board was missing (it carried zero rows naming #72), and
  the design-doc drift corrected on the way — the shipped adapter inserts via
  `RequestEditSession` + `ITfRange::SetText`, never `ITfInsertAtSelection`,
  and never opens an inline composition. What the pass did NOT establish:
  TAT and a ctest TSF tier, still open under milestone B (#65).
- [[2026-10-17-milestone-c-layout-wiring-data-loading]] — issue #66's
  milestone C: the layout pair comes from the `HKCU\Software\Hunmin\TSF`
  store (env seam → registry → fcitx5-mirrored defaults), slots validated by
  supported+mode; one data source wins (the additive chain double-loaded a
  staged tree, 15 files counted as 30); tier F1 gained `tsf-config-test`,
  `tsf-datadir-test` and a dubeolsik win32 trace — and the measured finding
  that the standalone frontends/tsf configure had been registering ZERO
  tier-F1 tests (no `enable_testing()`, plus the Python3_FOUND guard).
  #71 (the dialog, subtasked to this issue) is its own worktree.
- [[2026-10-17-settings-dialog-winui-exe]] — issue #71's exe work under the
  2026-10-17 ruling: the maintenance requirement measured as a NUMBER, not a
  verb — the compact WinUI exe lands 545x674 px, the layover interim Win32
  dialog 493x434 px — plus the ResizeClient-DPI-factor and XamlCompiler-only
  MSBuild.exe host quirks, the capture-one-binding key row, the publish
  figure 495 files / 167.4 MiB for the #68 packaging weight.

## Raw — measurements

- [[2026-10-16-android-e2e-keys-bypass-the-ime]] — **two findings.** Closed:
  tier E2 had been asserting nothing, because `dump_ui` was defined below its
  first call *and* its validity check (`wc -l` on a single-line XML dump) could
  never pass — every tap and every read failed, and a skip is exit 77, so the
  tier reported 100% passed over zero assertions. Fixed; E2 then ran to
  completion twice (108 s each). Open and **not root-caused**: injected keys now
  reach the field as literal ASCII while dumpsys says the IME is current,
  connected, bound, shown and visible. Five hypotheses tested and refuted, each
  recorded.

- [[2026-10-15-f9-candidate-ctest-tier]] — issue #53's option A, built and
  measured: `engine-d3-candidate` draws the F9 candidate window for the first
  time in any ctest tier (classicui enabled in `run-d3.sh`'s candidate mode),
  and with the #45 cap reverted it fails with the exact abort —
  `CommonCandidateList: invalid label idx`, fcitx5 SIGABRT. Records the three
  per-case assertions (survival, window-drawn geometry, expected commit), the
  the two injector facts (named keys must be braced; F9 opens the window
  asynchronously), and why `Down` and `Page_Down` both commit 肝 after 간 —
  not a drift but a coincidence: 肝 sits at index 1 (a krdict word sense) and
  index 10 (a hanja), so cursor-move and page-move land on the same
  character. Dumping the indices is what separates the two.
- [[2026-10-13-f9-candidate-window-abort]] — F9 with 11 or more candidates aborts
  fcitx5 with `CommonCandidateList: invalid label idx`: the page size is set to the
  candidate count and classicui reads a label per candidate, but a
  `CommonCandidateList` only carries 10 unless `setLabels()` is called. The
  vertical hint is a bystander; the installed addon aborts the same way; and the
  two tiers that cover this path are each structurally unable to see it.
- [[2026-10-10-d3-case-coverage]] — `engine-d3-x11` reported Passed in 5.01s
  having run **one** of its eight cases: the XIM harness drained the runner's
  case list off stdin. It hid a second defect — XIM harness reuse carries a
  preedit between cases — because the one case that ran is the one that cannot
  inherit anything.
- [[2026-10-09-gtk-forward-path]] — the declined key over the GTK IM module:
  it arrives on GTK 3 and GTK 4, by two different mechanisms, one of which is an
  empty function; and the three ways this harness was green while measuring
  something else.
- [[2026-10-09-where-space-is-emitted]] — who emits the space character,
  measured on both frontends: the reported symptom does not reproduce at HEAD,
  and the tier that should have caught it could not see the key at all.
- [[2026-10-09-voice-recognition-status]] — the unanswered voice question,
  measured on the emulator: the launch path reaches the platform's recogniser
  and recognition has never been tested, RECORD_AUDIO is never requested, and
  the mic on the input view was a comment rather than an assertion.
- [[2026-10-10-ctest-labels]] — why the ctest suite carries tier + `mutation`/
  `slow` labels, the three designs not taken, and a silent defect that is not
  fixed: the Android tier-C runner fails on a cold host JNI shim because a build
  script prints to stdout.
- [[2026-10-11-dock-and-the-zero-width-style]] — superseded 2026-10-12: the
  floating dock the maintainer specified (Views over Compose, measured;
  Phosphor glyphs, MIT, three files) was removed as unreachable (F5, issue
  #33); the page keeps the evidence, including the adjacent finding that
  style C rendered nothing at all: its rows were added to a vertical
  LinearLayout with `width = 0, weight = 0`, which is zero pixels.
- [[2026-10-11-key-settings-window]] — the Android key settings window, measured on
  the emulator: the button that opens it was off-screen, the window could not receive a
  key even when open (hardware keys go to the IME, not the activity), and the save was a
  write-only sink. Plus the adjacent finding that N5's nine Espresso tests have never
  compiled and no tier runs them.
- [[2026-10-05-drift-audit]] — the transcript-versus-documents audit of
  2026-10-05: eight claims re-checked against the tree, what it could not
  determine, and why a document can be accurate about a feature with no code.
- [[2026-10-10-orca-worktree-probe]] — **superseded** →
  `docs/ENVIRONMENT.md` §Orca GUI. Kept for the reasoning behind the retraction.

## Raw — upstream

*(none — external sources are read for semantics, never copied; see `SCHEMA.md`
and the note on kime being GPL-3)*

## How to use this

```sh
tools/wiki-lint.py            # errors are mechanical; advisories are not
tools/wiki-lint.py --strict   # what docs-lint runs
```

The one check that is not advisory: a `void` or `superseded` page still linked
from an `active` page. That is the failure this wiki was created for.- [[2026-10-18-milestone-d-candidate-edit-mode-ux]] — issue #67's milestone D:
  the candidate window (whole-word anchor), the Hanja-key default and its
  store value, key capture via a thread keyboard hook (the WM_KEYDOWN route
  measured dead in a modal dialog), the on-screen defects rooted (Mica,
  ownerless rundll32 not displayed by WS_VISIBLE alone), the Start-menu
  entry, the conversion-compartment mode write, the system langbar menu
  extension, and the (ko,hanja) krdict dedupe; pending: last-syllable
  fallback, mode-persist ruling, live re-register, CI billing.
- [[2026-10-19-tsf-native-composition-preedit]] — issue #98 closed the
  overlay: the preedit is a native `ITfContextComposition` (caret range,
  per-transition rewrite, boundary end, fused text), underlined through a
  registered display-attribute provider; the rebase onto #106's key-path
  migration re-verified, and the re-verification corrected the fallback
  case's delivery channel and the unsound "7/7" claim; left minor: Notepad
  paints its composition highlight over the preedit under both attribute
  encodings measured (noted on the issue).
- [[2026-10-19-test-framework-migration]] — issue #109 ruled option 1b
  (GoogleTest + gmock via FetchContent) and the migration shipped the same
  day: the engine tier-A trio and the four TSF runner groups (merged into
  one executable, one add_test, the rc-77 probes in a wrapper outside the
  binary); the FetchContent "disqualification" overturned by measurement;
  the fcitx5 trio deferred to issue #118 (unverifiable on this host).
- [[2026-10-20-legacy-interim-input-path]] — issue #119's ruled legacy
  support shipped on the interim route the spike measured: runtime detection
  (the first mid-word host termination flips the service, no composition),
  a Delivery seam (NativeDelivery = the #98 composition untouched;
  InterimDelivery owns the span + posted writes), the test-phase boundary
  drain (msftedit never calls OnKeyDown for an uneaten printable key), the
  app's-caret-not-ours rule, the one-char Hanja ruling, the #103 tray core
  layered under the new dispatcher, and the dead overlay remnants stripped;
  overturned: the EXP-A space row (the eaten-space artifact) and the
  tier-F1 mock's blind spot (the 24px capture EDIT rejected EM_REPLACESEL
  growth).

- [[2026-10-22-tsf-component-refactor]] - the DIP+SAP stages 1-3 run on
  the maintainer's steer: all include-.cpp seams retired in both directions
  (the tier-F1 harness LINKS the component TUs like the DLL), the contracts in
  tsf.h (TsfCore, Delivery + the factory/registry pair, CandBackend, Candidates,
  HunminTsfSnapshot), the key-state mock a REGISTERED provider, the old
  #define-private-public white box replaced by Snapshot(); board rows
  re-pointed; tier-F1 4/4 and docs-lint 0 errors; shell still 2,320 lines with
  eight TSF interfaces on CTextService - the measured basis for the SRP
  follow-on.

- [[2026-10-22-tsf-langbar-tearoff]] - stage 4 of the clean-code pass
  (issue #127): the language-bar item becomes a tear-off COM object
  (`langbar.h`/`langbar.cpp`), the shell drops `ITfLangBarItemButton` and
  `ITfSource` (7 -> 5 TSF interfaces, 30 -> 19 STDMETHOD bodies), and QI/AddItem/
  RemoveItem/Update forward to the item; tier-F1 4/4 and docs-lint 0
  errors; the hosted tray presence is a re-measure owed, not asserted.

- [[2026-10-22-tsf-activate-deactivate-extraction]] - the S5 first cut:
  `Activate`/`Deactivate` naked blocks become `AdviseSinks`,
  `UnadviseSinks`, `DetectLegacyMode`, `FlushEngineIntoWordbuf` and
  `LangBarItem::Attach`/`Detach`; `DeactSession` reuses `InsertBufferedWord`;
  the langbar style is ruled `BTN_MENU|SHOWNINTRAY` (no `BTN_BUTTON`); a
  detach-ordering defect is recorded, not fixed.

- [[2026-10-22-tsf-settings-split]] - stage 5 of the clean-code pass
  (issue #127): the 979-line settings TU split on the maintainer's option A
  into `settingsrows`, `settingsdialog`, `settingslaunch` and
  `settingsprovider`, leaving `settings.cpp` as the 25-line rundll32
  export; two file statics became prefixed cross-TU calls and three board
  rows were re-pointed; tier-F1 4/4 and docs-lint 0 errors.

- [[2026-10-22-tsf-registration-extraction]] - the S5 registration cut:
  `DllRegisterServer`'s seven naked scopes become file-static helpers
  (`RegisterComServer`, `RegisterTipKey`,
  `RegisterInputProcessorProfile`, `PinProfileIconIndex`,
  `ActivateInputProcessorProfile`, `RegisterTipCategories`,
  `CreateStartMenuShortcut`) called in their original order, with
  `DllUnregisterServer` mirrored; S4 then moved the block, both exports and
  the class factory to `registration.cpp` with the shared GUIDs in `ids.cpp`.

- [[2026-10-22-tsf-displayattr-extraction]] - the S7 display-attribute cut
  (issue #127): the provider objects and the `DisplayAttr` GUID-atom channel
  moved to `displayattr.cpp` (196), the measured underline shape unchanged;
  `tsf.cpp` 1,659 -> 1,492, tier-F1 4/4.

- [[2026-10-22-tsf-datadir-extraction]] - the F data-source cut (issue
  #127): `hunmin_ctx_from_module` and the layout-slot check moved to
  `datadir.cpp` (122), shared by the shell, the dialog and the tests;
  `tsf.cpp` 1,492 -> 1,384, tier-F1 4/4.

- [[2026-10-22-tsf-composition-extraction]] - the C composition cut (issue
  #127): the termination handler, the three edit sessions, the
  write/end/detach half and the engine/boundary drain moved to
  `composition.cpp` (457) as nested-class definitions, no new seam;
  `tsf.cpp` 1,384 -> 945, tier-F1 4/4.

- [[2026-10-22-tsf-keydispatch-extraction]] - the K key-path cut (issue
  #127): test/down/up, the predicates, toggles, `DrainAndPass`,
  `HarvestFinalised` and `FeedEngineKey` moved to `keydispatch.cpp` (386)
  as member definitions; `tsf.cpp` 945 -> 575, tier-F1 4/4.

- [[2026-10-22-tsf-qi-table]] - the Q QI table (issue #127): one
  `kQiRoutes` list feeds the trace tag and the dispatch, replacing two
  parallel if/else chains; the log text and answer order are unchanged;
  `tsf.cpp` 575 -> 597, tier-F1 4/4.

- [[2026-10-26-tsf-mock-unmodelled-surfaces]] - the mock harness reaches
  the surfaces it had only stubbed (issue #107): one seam wider than the
  issue asked for (the registry is destructive to leave unseamed), four
  surfaces observed, and the finding that the mocks saw what code comments
  had carried for a week - #145 (two Deactivate defects) and #146 (two
  mirror questions).
- [[2026-10-27-tsf-deactivate-teardown-fix]] - the #145 fix: `UnadviseSinks()`
  withdraws both advices and the new `ReleaseThreadMgr()` drops the manager
  reference last, so the language-bar `RemoveItem` is reached; the two cases
  that encoded the defects flipped, with the pre-fix tree measured red
  against them.
- [[2026-11-03-x11-corpus-replays-every-tier-a-case]] - issue #187: the 22 per-case
  traces the word-buffering reduction dropped come back under the policy each one
  needs (`meta.commit_mode`), 87 -> 109 traces with every `.in` identical to its
  `.gold`; the issue's "stale generation" premise measured wrong — c773fdd had
  dropped them deliberately — and ctest `conformance-coverage` now fails when a
  case goes missing again. Then 109 -> 102 by merging `p2-2-2-tiers`' eight
  short probes into one case, and 102 -> 10 by merging every other family behind a
  space boundary: 355 events, and the boundaries being DECLINED keys took x11's
  pass-through coverage from 0 to 92. The ten 세벌식 최종 probes then entered as
  ONE file too (11 traces, 417 events, 109 passed), because the policy is one
  trace per family and the merge already knew how to keep the nine boundaries.
  두벌식 and 신세벌식 P2 then got the same designed-family treatment (13 traces,
  589 events, 159 passed): 두벌식's number row is not the layout's at all, so its
  digits are declined keys and the pass-through half of the contract is now
  observed on a character, not only on whitespace.
- [[2026-11-01-modes-ownership-retires-the-detector]] - the #164 session: §MRO's
  pricing, the ruling implemented on all three frontends (engine judges, frontend
  switches), `language_detect.*` retired with six `REQ-MAP` rows re-pointed, and
  the two TSF defects the live session found routed to #175.
- [[2026-11-03-tier-c-takes-the-digit-row-and-a-declined-backspace]] - issue #229's
- [[2026-11-04-coverage-golden-per-layout]] - one coverage golden per layout
- [[2026-11-04-shared-edit-key-catalogue-was-executed]] - a 2026-10-25 note
  found untracked at the root: seven edit-key sequences, already asserted on all
  three frontends except EK7 on TSF, and the E1-E7 label collision it exposed.
  (issue #227): the slot unit, the gate that refuses a thin file, and the four
  instrument defects that every number that would not move exposed.
  second half: one designed case (`0123456789 \b`) for each x11 layout, so the
  digit row is pressed in full and every layout declines a BackSpace into an
  empty buffer (x11 598 -> 661 events, 162 -> 185 declined). Generating it
  exposed two `tools/gen-conformance.py` defects that no committed trace could
  reach: `consumed` for a named key was read off the previous preedit, and a
  golden's `{"ch":"\u0008"}` spelling was not a BackSpace. The base-0 key
  (arrow/Return) still cannot be generated — tier A has no encoding for it — and
  is now its own board row, `TIER-C-WIRING-ARROW`.
- [[2026-10-31-milestone-f-win32-conformance-corpus]] - milestone F: the win32
  corpus grew 3 -> 11 traces by porting the eight tier-A families x11 replays
  (106 cases / 688 events, tier-F1 12/12); the work exposed a generator that
  emitted a caps-on case at `mods: 0` and asserted caps-off text, now excluded
  with `meta.excluded`; four gaps filed as #184-#187.
- [[2026-10-22-tsf-settings-resolver]] - the S settings-exe resolver
  (issue #127): the maintainer's deployed-first ruling unifies the three
  candidate lists in `settingspath.cpp` (100); an installed Start-menu
  shortcut now targets the WinUI exe; `tsf.cpp` 597 -> 571, tier-F1 4/4.
