# Wiki Index

> Content catalog. Every page listed under its type with a one-line summary.
> Read this first to find relevant pages for any query.
> Last updated: 2026-10-18 | Total pages: 42

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

## Decisions

- [[decisions-belong-to-the-maintainer]] — the standing instruction that
  measurements are the agent's and rulings are the maintainer's, and the three
  later instances where it bound.
- [[the-outstanding-decisions]] — D1 hangul judgement method, D2 E1 symmetry,
  D3 whether 119 may inform a shipped list, D4 whether this wiki stays.
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
  keeps the cross-frontend validity judgement.
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
  `docs/ANDROID-MUTATION-SANDBOX.md`; what generalises is here.
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
