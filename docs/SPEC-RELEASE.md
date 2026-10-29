# SPEC-RELEASE — release, packaging and CI requirements

What must hold for a release to exist and be reproducible. Mechanisms live in
`docs/WINDOWS-INSTALLER.md`, `docs/CI-TIMING.md` and the scripts themselves;
this file states the obligations and points at them.

A **Ruling** is a decision with a date and an issue (quoted verbatim where the
board note carries the words). A **Derived** entry is a rule this document
extracts from a measured board note.

# Requirements

### Artefact identity

### REL1

The release version is `0.2.0`; the `-pN` suffix names the Android
build phase and is never the release number. *Ruling:* issue #9, 2026-10-12 —
"the release is 0.1.0 and the -pN suffix is the Android build phase", superseded
for this release by the maintainer's 2026-10-29 release instruction — "this task
is to release v0.2 ; so open PR, merge, release, and close the issue" — which
sets the version to `0.2.0` in `CMakeLists.txt`, `debian/changelog` and the tag,
with `frontends/android/app/build.gradle.kts` carrying `0.2.0-p5` and
`versionCode = 2`. *Implementation:* `frontends/android/app/build.gradle.kts`,
`docs/MILESTONES.md`. *Verified by:* board row `VERSION-CONVENTION`.

### REL2

The release notes file the publish script uploads is named for the
TAG (`docs/release-notes/<tag>.md`), because the script derives the name from
`$TAG`. *Ruling:* derived, measured 2026-10-16 — the file was committed as
`0.1.md` while the script ran `--notes-file docs/release-notes/${TAG}.md`, which
would have pushed a tag and then failed. *Implementation:*
`tools/release-0.1.sh`. *Verified by:* board rows `RELEASE-PUBLISH-SCRIPT`.

### REL3

Every 0.1 artefact (`.deb`, release APK) is buildable by anyone with
the same documented sequence, not only by the machine that produced it.
*Ruling:* issue #8. *Implementation:* `tools/build-release.sh`.
*Verified by:* board row `RELEASE-SCRIPT`.

### Scripts that can publish

### REL4

A script whose run can publish defaults to PRINTING what it would do:
the irreversible steps are the tag push and the public release, so a bare
invocation must not perform them. *Ruling:* derived, measured 2026-10-16 —
`DRY_RUN` defaulted to 0 while the usage line advertised a dry run, and a bare
`tools/release-0.1.sh` took the real path end to end.
*Implementation:* `tools/release-0.1.sh`. *Verified by:* board row
`RELEASE-PUBLISH-SCRIPT` (`grep DRY_RUN=1`).

### REL5

A publish step never leaves a partial release: a step that can fail
after the tag push is ordered before it, or its failure is recoverable without
moving the tag. *Ruling:* derived from REL2's incident (the pushed tag a re-run
reports as existing). *Implementation:* `tools/release-0.1.sh`.
*Verified by:* `none` — the ordering is reviewed, not asserted.

### The package

### REL6

The installation package stages `libhunmin/data` beneath the
installed DLL's directory, because the loader is module-relative; without it the
installed product regresses to single-syllable Hanja. *Ruling:* the packaging
gate in `AGENTS.md`. *Implementation:*
`docs/WINDOWS-INSTALLER.md#What ships`. *Verified by:* board rows for the
installer; the clean-machine rounds in
`tests/manual-checklists/` (device-bound).

### REL7

The shipped settings surface lives at
`{app}\settings-ui\hunmin-tsf-settingsui.exe` — the publish CONTENT, with no
`publish\` subdirectory — and every resolver must carry that shape beside its
dev shapes, or the installed product silently falls back to the interim dialog.
*Ruling:* measured 2026-10-20 (both resolvers had to be fixed).
*Implementation:* `frontends/tsf/src/settingspath.cpp`,
`frontends/tsf/src/settingslaunch.cpp`. *Verified by:* board rows
`TSF-SETTINGS-SHOW-71`; the installer contract.

### REL8

An uninstall removes the TIP registration it created and leaves no
residue in the installed tree that a reinstall would trip over. *Ruling:*
derived from the installer's uninstall-residue rounds. *Implementation:*
`docs/WINDOWS-INSTALLER.md#What the installer runs`.
*Verified by:* `none` — clean-machine rounds, not ctest.

### CI

### REL9

CI builds the `.deb` and runs the test suite INSIDE the package build
(`debian/rules` `override_dh_auto_test`), not skipped — the issue's verification
requires that. *Ruling:* issue #8, 2026-10-13.
*Implementation:* `.github/workflows/ci.yml`, `debian/rules`. *Verified by:*
board row `CI-DEB-JOB`.

### REL10

An uploaded artefact is staged INTO the workspace first:
`upload-artifact` rejects any pattern containing `..`, and the package build
writes one level above the source tree. *Ruling:* derived, measured 2026-10-13
(PR #54 — the step was red until the artefact was copied in).
*Implementation:* `.github/workflows/ci.yml`. *Verified by:* board row
`CI-DEB-JOB` (upload step).

### REL11

The Windows CI job is disabled, and the block is kept with the
ruling quoted rather than deleted, so "was Windows ever built here, and why did
it stop" stays answerable. *Ruling:* issue #51 item 2 — "for windows, it is not
scope of this version. so in the issue we will disable windows test."
*Implementation:* `.github/workflows/ci.yml`.
*Verified by:* board row `CI-WINDOWS-OUT-OF-SCOPE`. **Coverage lost while it is
disabled:** no job compiles the core with MSVC.

### REL12

`dpkg-shlibdeps` may ignore missing dependency info only for
libraries no package owns (a from-source fcitx5); the flag is not a blanket
waiver and a packaged host must still produce real `shlibs:Depends`.
*Ruling:* derived, measured 2026-10-13 (`--ignore-missing-info` reproduces the
failure with an empty admindir and fixes it; a packaged host still resolves).
*Implementation:* `debian/rules`. *Verified by:* board row `CI-DEB-SHLIBDEPS`.

### Time budgets

### REL13

A job or tier has a ceiling, and it is a rule: the inner loop, the
slow tier and the linux CI job are measured against their budgets before a push
that adds a test. *Ruling:* `AGENTS.md` time budgets.
*Implementation:* `tools/ci-time-budget.sh`, `.github/workflows/ci.yml`.
*Verified by:* `tools/ci-time-budget.sh` itself.

### REL14. Work reaches the tree through an issue, and a decision is the maintainer's

A change is proposed as an issue carrying its measurement, worked on a branch, and
merged by pull request; an agent measures and reports, and records no verdict.
*Ruling:* `AGENTS.md` §Working rules and §GitHub issue tracking.
*Implementation:* `tools/orchestrate.sh`, `docs/ORCHESTRATION.md`.
*Verified by:* `none` — a process requirement; what it buys is read from the
record, not asserted by a test.

## Not requirements (recorded so they are not promoted by accident)

These read like obligations and are measurements a tool derives:

- Test counts, per-label counts and per-run timings — `ctest -N` and the timing
  tool are the authority; `AGENTS.md` forbids the number in prose.
- Job wall-clock figures in `docs/CI-TIMING.md` — they are a dated measurement
  of one configuration.
- The size budget of the published settings app — measured at packaging time
  (`docs/WINDOWS-INSTALLER.md#Size budget, measured at packaging time`).
- Whether a specific CI run passed — a run is evidence, not a requirement.
