# Windows dev environment checks for TSF (record)

**Purpose:** record what to verify in a Windows dev environment for the TSF frontend, with emphasis on findings/changes from the 2026-11-08 #272 session in this worktree. This is a checklist, not a design decision.

**Scope/limits:** TSF is MSVC/Windows-only in practice here; this Linux worktree did **not** register or run TSF ctest targets (`ctest -N -R tsf` returned none in `build/`). Changes in this session were comment/doc/ID-consolidation heavy and included `libhunmin/include/hunmin.h` and `libhunmin/src/buffered_preedit.c` comment refactors plus wide `ReqMode*` text renames touching TSF sources/tests. Behavior-change intent: **none** for those edits, but Windows/MSVC verification is required.

## 1. Environment/build prerequisites (pointers)

- `docs/ENVIRONMENT.md` — “Windows development environment (this host, measured 2026-10-16)” and “Batch 4 — P6 (Windows TSF TIP)”.
- `frontends/tsf/CMakeLists.txt` — standalone TSF configure/build shape, MSVC `/utf-8`, static core discovery, data staging, tier-F1/test registration when `WIN32 && MSVC && BUILD_TESTING`.
- `tools/build-msvc.bat` / `tools/build-windows-installer.sh` (if used in your flow) — follow repo scripts as authoritative procedures.

## 2. Build and automated checks (Windows/MSVC)

Run in a configured Windows build environment (VS2022 BuildTools/SDK as per `docs/ENVIRONMENT.md`):

```powershell
# Core + TSF (shape depends on local build layout; adapt to your build dir)
cmake -S . -B build-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build-msvc --config Release
ctest --test-dir build-msvc -N
```

Then run TSF-registered tests/tiers (names may vary by configure; use `ctest -N` as source of truth):

```powershell
ctest --test-dir build-msvc -R tsf --output-on-failure
ctest --test-dir build-msvc -R tier-F1 --output-on-failure   # if registered
```

**Accept:** TSF tests registered by that configure pass; no new compile/link failures from `hunmin.h` comment changes or ReqMode text migration.

## 3. Session-specific checks (#272, 2026-11-08)

### 3.1 ReqMode ID migration touched TSF files (text/comments/strings)

Global migration renamed `R1-R15` / `R*-정정*` → `ReqMode1..15` (fully collapsed corrections). TSF-related files changed in this migration include (non-exhaustive): `frontends/tsf/src/{composition,config,keydispatch,settingsdialog,settingsrows}.*`, `frontends/tsf/test/{tsf-judgement-test,tsf-config-test,...}`. Verify:

- No user-visible string regressions where text intentionally references old IDs in UI/logs (if any).
- No test-name/label assumptions broken by text changes.
- `ReqMode` replacements did not corrupt non-SPEC tokens (anomaly risk noted in session: `\bR\d+\b` replacement was broad; build/targeted Linux tests passed).

Suggested grep sanity on Windows checkout/worktree:

```powershell
git grep -n '\bR8-정정\b|\bR13-정정\b|\bR4-정정\b|\bR7-정정\b' -- frontends/tsf docs/SPEC-MODES.md docs/REQ-MAP.md
```

**Accept:** no stale SPEC-MODES correction IDs remain where `ReqMode*` was intended (except historical quoted text only if intentionally preserved — none intended by this migration).

### 3.2 SPEC/REQ-MAP/design consolidation

- `docs/SPEC-MODES.md` now uses `ReqMode1..15`; corrections merged into base req sections; REQ-MAP rows for ReqMode4/7/8/13 collapsed (traceability preserved, no design-change intent). `tools/req-map-check.py` passed on Linux.
- `docs/FRONTEND-EQUIVALENCE.md` and `docs/FRONTEND-CONTRACT.md` are stubs; frontend design moved to `docs/DESIGN-FRONTEND.md`.
- `docs/REMAINING-WORK.md` TSF autoswitch board assertions were retargeted to `docs/DESIGN-FRONTEND.md` (`43502e3`, `af61177`) because stubs broke docs-lint greps. `docs-lint` on Linux now reports the pre-existing M5-CLOSED/pyright error only (plus warnings).

**Windows check:** run docs guards in the Windows environment if they are part of that workflow:

```powershell
python tools/req-map-check.py
python tools/wiki-lint.py
python tools/docs-lint.py
```

**Accept:** `req-map-check`/`wiki-lint` pass. `docs-lint` should not introduce *new* TSF/FRONTEND-EQUIVALENCE stub-related errors beyond the known pre-existing M5-CLOSED/pyright issue (confirm against current `REMAINING-WORK.md`).

### 3.3 API/header comment refactors (no contract/behavior change intended)

This session condensed comments in:
- `libhunmin/include/hunmin.h`: `hunmin_key_action`, `hunmin_buffered_preedit_take_judgement`, `latin_text`, English word-list APIs (contracts preserved; stale “judgement not implemented” note corrected), and earlier autoswitch data-gate wording.
- `libhunmin/src/buffered_preedit.c`: ReqMode4 symbol delimiter classification, `hangul_valid_for`, `boundary` timing/contract notes (correctness-critical facts preserved).

**Accept:** MSVC build clean; TSF tests that exercise judgement/autoswitch/English word loading/boundary behavior (`tsf-judgement*`, config/settings/autoswitch-related suites as registered) still pass. Pay attention to any tests sensitive to English word-list search/load contracts (`load_english`/`load_english_shipped`, B3 fail-closed behavior) and `take_judgement` consumption semantics.

### 3.4 P5/D1 not implemented (inventory only)

`docs/DESIGN-PHASE5-CORE-MOVE.md` gained a step-1 inventory (`928b40d`). **No frontend core-move code changes** were made in this session. If any TSF refactor is attempted next, follow D1 constraints there and re-run FE/TSF tests.

## 4. TSF functional/manual checks (existing checklists)

Use the repo’s manual checklists where relevant to the changes you are validating:

- `tests/manual-checklists/tsf-native-composition-preedit.md`
- `tests/manual-checklists/tsf-hanja-candidates.md`
- `tests/manual-checklists/tsf-langbar-item.md`
- `tests/manual-checklists/tsf-langbar-badge.md`
- `tests/manual-checklists/tsf-mode-toggle.md`
- `tests/manual-checklists/tsf-settings-window.md`
- `tests/manual-checklists/tsf-unregister-cleanup.md`
- `tests/manual-checklists/tsf-installer-fresh-vm.md` (release/installer scope)

Also review recent TSF measurement/design records when judging regressions (not re-measurement mandates): `docs/WINDOWS-TSF-BUFFERING.md` (incl. autoswitch live measurement/issues #216/#221/#222 context), `docs/SPEC-TSF.md`, `docs/DESIGN-FRONTEND.md` Part A/B.

## 5. Known non-#272 items to keep separate

- `docs-lint` pre-existing: `docs/REMAINING-WORK.md:551` M5-CLOSED/pyright assertion (environment/tooling dependent). Do not attribute to #272/ReqMode migration without evidence.
- TSF autoswitch/interim limitations and live-measured issues (#221, #222 history/fixes as recorded) are tracked in existing docs/REMAINING-WORK rows; re-evaluate against current tree, not assumptions.

## 6. Reporting

When checking on Windows, record:
- exact build command/config, compiler/SDK version, commit SHA;
- `ctest -N` summary and TSF/tier-F1 results;
- any failures with test name, command, and log;
- whether failures reproduce on the pre-session baseline commit if bisecting (session introduced many doc/ID/comment changes; isolate carefully).
