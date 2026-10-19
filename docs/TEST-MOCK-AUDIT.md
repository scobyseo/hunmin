# TEST-MOCK-AUDIT — which tiers pay for a real framework, and what a mock could take over, measured

**Status: measurements only. No mock is decided, none is rejected, and the
"Recommendation" column records what each row's measurement supports — the
choice is the maintainer's** (issue #83 says the same: "This issue measures
and presents; it does not decide").

Produced 2026-10-19 on branch
`scobyseo/ci-audit-existing-test-harnesses-for-mock-opport` (base `5d4206d` = master
tip), Windows 11 host. The branch modifies nothing but this file;
`docs/REMAINING-WORK.md` and `AGENTS.md` are deliberately untouched here
because parallel sessions patch the same board. Figures come from three
kinds of source, named per figure: **fresh** (measured in this worktree at
`5d4206d` on 2026-10-19), **host** (an earlier measurement on this machine,
tree named), and **CI** (the last executed CI logs, parsed in
`docs/CI-TIMING.md`; no CI job has run since 2026-10-13 — CI-TIMING §6).

## 0. The issue's premises, checked first

The issue's motivation paragraph states premises about the tree. Each was
re-verified before being relied on.

1. **"fcitx5 tier D1 links the addon into a headless test binary and
   instantiates `fcitx::Instance` directly" — verified.**
   `frontends/fcitx5/test/testhunmin.cpp:112-126` constructs a real
   `fcitx::Instance` with `--disable=all --enable=testim,testfrontend,hunmin`
   (`:117-118`) and injects keys through the TestFrontend addon's
   `sendKeyEvent` (`:28-34`). No X server, no D-Bus, no process boundary.
2. **"tier D2 reads `InputPanel` accessors in-process" — verified.**
   `frontends/fcitx5/test/testhunminui.cpp` reads `inputPanel().preedit()`
   (`:59`, `:66`), `clientPreedit()` (`:72`), `candidateList()` (`:191`,
   `:208`, `:221`, `:226`, `:250`, `:409`) and selects a candidate (`:207`) — no UI
   addon is loaded at all (no pixels anywhere in the tier).
3. **"D3 launches stock fcitx5 under Xvfb; E2 boots an emulator" — verified.**
   `tests/e2e/run-d3.sh:279-318` starts Xvfb (+ nested Xephyr when installed)
   and a stock distro `fcitx5` per layout group (`:503`); `tools/android-e2e.sh:83`
   installs the APK on an emulator and types with `input keyevent` (`:317`).
4. **"E1's `RecordingInputConnection` already avoids a device" — verified.**
   `frontends/android/app/src/test/java/org/hunmin/ime/RecordingInputConnection.kt`
   is a pure-JVM `InputConnection` fake: records `commitText`/`setComposingText`/
   `finishComposingText` (`:82-96`) and routes every surrounding-text call into a
   `violations` list instead of answering it (`:100-131`) — invariant I9 is
   checked, not assumed.
5. **"some tiers still boot real frameworks" — verified, and the cost is
   concentrated exactly where the issue expects.** But the audit's first
   finding is the inverse of the issue's framing: **the expensive tiers are
   already mock tiers.** The three tests that cost CI 252 of the linux job's
   269 test-seconds (`engine-android-e1` 148.38 s, `android-mutations`
   84.31 s, `frontend-mutations` 19.36 s — CI-TIMING §1) boot no framework
   process at all; their cost is Gradle and CMake rebuilds, not Xvfb,
   D-Bus or an emulator. The real-framework tiers (D3, E2) are *absent from
   the job the issue opens with*: D3 skips on the runner (no Xvfb, CI-TIMING
   §1), and E2 runs only in the android job.
6. **One premise the issue does not state, which the tree forces on every
   row: tier F is not a mock opportunity — it is a mock already shipped.**
   Issue #80's harness exists: `frontends/tsf/test/mock-tsf.h` (in-memory COM
   doubles, adapter `tsf.cpp` included unmodified), corpus replay through
   `tools/trace-check.py`, plus four config/datadir/settings/candidate test
   binaries. The F row below measures what that mock costs and what it
   structurally cannot see.

## 1. The unpacked subject: four different things called "mock"

The issue's "a mock could replace some of that" covers four distinct
patterns this repo already uses. Keeping them apart is what makes the rows
comparable — each tier's question is which pattern it could move *to*, not
whether "a mock" exists.

| Pattern | Existing implementation | What it fakes |
|---|---|---|
| **P1. In-process real framework, fake session** | D1/D2 (`TestFrontend` addon, headless `fcitx::Instance`) | the session (X server, D-Bus, windows) — the framework API is real |
| **P2. Recording fake at the client boundary** | E1 (`RecordingInputConnection`) | the client app; also *traps* forbidden calls |
| **P3. In-memory doubles of the framework API** | F1 (`mock-tsf.h`, `E_NOTIMPL` stubs so an unmodelled adapter call fails loudly — `mock-tsf.h:31-32`) | the framework API itself |
| **P4. Trace replay, diffed against a tier-A-derived corpus** | tier C runners (`frontends/fcitx5/test/trace-emit.cpp`, `tools/android-trace-emit.sh`, `frontends/tsf/test/trace-emit.cpp`) | nothing — it is a real adapter plus an oracle |

Pattern P1 is why D1/D2 are already "mocked" in every sense the issue's
table means: the framework API contract is the thing under test there, and
faking it (P3) would assert the adapter against a guess of the contract
instead. The interesting rows are the ones paying for *processes*: D3 (X
server + fcitx5 daemon + injection pacing) and E2 (emulator boot + APK +
uiautomator).

## 2. Per-tier rows

The deliverable table; each tier then gets its surface and cost spelled out.

| Tier | Current cost | Mock possible? | What it would save | What it would lose | Recommendation (measured, not decided) |
|---|---|---|---|---|---|
| **D1** in-process fcitx5 | sub-second block: D1+D2 together 0.23 s serial on the maintainer's host (TESTING.md, 2026-10-10, 59-test config) | already P1; P3 would fake the contract under test | ~nothing — there is no process to remove | the fcitx5 dispatch semantics (`filterAndAccept`, capability flags, group/layout resolution) | nothing for a mock to take |
| **D2** fcitx5 UI accessors | same 0.23 s block | already P1 (no UI addon loaded) | ~nothing | `setPreedit` vs `setClientPreedit` branch semantics — the branches are fcitx5's | nothing for a mock to take |
| **D3** X11 e2e ×4 variants | 159.94 s serial, host (x11 46.43 + gtk3 48.47 + gtk4 48.63 + candidate 16.41 — TESTING.md, 2026-10-15); **skips on CI** | no — the load-bearing parts are fcitx5's own XIM/GTK code, not ours | the priced speedup lever is pacing + restart ordering, reaching ~22 s per run (D3-PERFORMANCE §3) — and that lever is not a mock | the XIM `FORWARD` path, the two GTK mechanisms, the classicui abort oracle — each already caught a real defect only because it was real | the speedup lever is pacing/restarts (D3-PERFORMANCE §3), not mocking |
| **E0** engine on device | no wall figure exists anywhere; `TIMEOUT 600 s`; CI's only placement runs **before** the emulator boots | no — the premise is bionic, the other libc | nothing to save by mocking — a host mock re-hides the bug the tier exists for | the glibc/bionic divergence class (measured: unstable `qsort` compose dedup, TESTING.md E0) | keep real; the finding is its CI placement (below) |
| **E1** Android JVM | **fresh, this host: 50.6 s** for the whole tier (116/116, warm Gradle, shim build included); CI 148.38 s cold (linux), 185.68 s (deb); android-job copy now ~23 s warm (ci.yml:419-420) | already P2 — the mock shipped with the tier | Gradle is the remaining cost, not the framework | the real JNI shim + real engine — the point of the tier | nothing for a mock to take; its cost lever is cache warmth (CI-TIMING option B) |
| **E2** emulator (e2e + espresso) | e2e **108.8 s / 108.9 s** (host, 2026-10-16, wiki); espresso: 12/12 pass (2026-10-13), no wall figure recorded; emulator boot: never measured | no for its four platform contracts; the tier's *reliability* is the measured problem, not its cost | emulator boot + AVD management | key-routing-to-IME-window, on-screen-text read-back, IME binding, `EditorInfo` secure-field behavior — every one caught a defect a mock cannot see by construction | the open item is the unroot-caused keys-bypass-the-IME flake (wiki 2026-10-16), a key-delivery investigation, not a mock |
| **F1** TSF mock harness | **fresh, this host: 1.92 s total, 7/7 pass** (tier-F1 = 1.89 s·proc, 2026-10-19, this worktree) | already P3 — issue #80 shipped it | — | registry, COM activation, threading, real HWNDs — the `E_NOTIMPL` stub list is exactly the unmodelled surface | the real-session F2/F3 work is the residual, not a mock target |

### D1/D2 — what the adapter actually touches, and what that pins

The fcitx5 addon's entire framework surface (`frontends/fcitx5/src/engine.cpp`)
is six API families: `ic_->commitString(...)` (`:174`, `:349`, `:394`),
`ic_->inputPanel().setClientPreedit(...)` / `setPreedit(...)` (`:437`/`:439`),
`ic_->inputPanel().setCandidateList(...)` (`:152`, `:547`) and a read of
`candidateList()` (`:243`), `keyEvent.filterAndAccept()` (`:221` and seven
call sites through `:374`), and `updateUI()` (`:420-440`).
D1/D2 exercise every one of them through the real fcitx5 libraries. What
they assert that tier C cannot: the `sendKeyEvent` return value as the
consumption oracle (TESTING.md:566-570 — D2's return value is the only place
consumption is asserted for F9), input-method activation via the profile's
group plumbing (`testhunmin.cpp:36-61`, the `InputMethodGroup` `Layout=` path),
and the caps config option (`testhunmin.cpp:91-104`). All three are fcitx5
integration contracts, not engine assertions — a P3 mock of `fcitx::Instance`
would test our adapter against our own model of fcitx5, and the two recorded
cases where fcitx5 semantics differed from exactly such a model
(`InputMethodGroup` second-item ordering, `fcitx5-remote -s` returning rc=0
while changing nothing — `run-d3.sh:347-352`, `:533-538`) are exactly the
class a mock would have blessed.

### D3 — where the 159.94 s goes, and why the framework is load-bearing

Measured breakdown for one GTK 3 run (~49.7 s, D3-PERFORMANCE): XTEST key
injection 24.4 s (49% — the 40 ms-per-event pacing margin), four fcitx5
restarts 10.2 s (21%), fixed sleeps 9.8 s (20%), harness start→READY 4.5 s
(9%), teardown <1%. The same page lists the three pacing levers with their
costs and explicitly does not decide them.

What the tier asserts that no other tier can, all of it about framework code
that is not ours: the XIM decline/forward path (fcitx5 forwards a declined
key via `XIM_FORWARD_EVENT`, `src/frontend/xim/xim.cpp:664` — the client's
`FORWARD:` lines); the two *different* GTK 3/GTK 4 mechanisms for re-queueing
a declined key (TESTING.md D3-GTK, with the gtk3/gtk4 line citations); and
the classicui `#45` abort oracle that only a drawn window can fail
(`run-d3.sh:849-941`). The issue asks whether the XIM forward test could run
against "a mock XIM client" — the harness already *is* a minimal XIM client
(`tests/e2e/xim-harness.c`); the real dependency is the XIM **server** side
plus the X server. The measured record on faking that side is a bad one: D3
looked green for its first week because libX11's *built-in* XIM answered
locally and decoded keys itself (TESTING.md:495-503) — a local fake of the
XIM contract producing a green run that measured nothing is not hypothetical
here, it is the tier's founding incident.

### E0 — the one tier whose premise forbids a mock

`tools/android-golden-check.sh` needs only adb: push `hunmin-sim`,
`libhunmin.so`, the data tree and the goldens (`:54-58`), then diff on
device (`:63`). It exists because engine output depended on the C library
(glibc `ㅃᅠᆸ` vs bionic `ㅃ`, same source and data — the script header), and
the script states it: "a host test cannot catch that class of bug". A mock
of the device would be a libc-native host — the bug class is invisible there
by construction.

The finding this row contributes is not about mocking but about placement:
the android job's "Tier E0 + E1" step runs at `ci.yml:405`, the emulator
boots at `ci.yml:435`, and the script skips (rc 77) when no device answers
(`android-golden-check.sh:39`) — so the android job's only E0 placement runs
before its own device exists. The step's comment claims "No emulator needed
for either: E0 pushes the engine and the goldens over adb" (`ci.yml:406-407`),
but pushing over adb requires exactly the device that does not exist yet at
that point in the job, and the script's own guard says so; the linux job's
unfiltered ctest runs E0 with no device at all. Wherever it runs in CI, E0
is a skip. E0's pass evidence is
the 2026-10-03 maintainer-host AVD run (TESTING.md E0). This host cannot
re-measure it (the emulator cannot boot here — no hypervisor, not an admin;
measured 2026-10-18, workspace AGENTS.md). Whether the step should move
below the boot is a CI-ordering question, and is recorded here as a finding
for the maintainer, not a change.

### E1 — the mock tier that is expensive for a reason the mock cannot touch

The adapter's Android surface (`HunminAdapter.kt`) is two `InputConnection`
calls — `ic_.commitText(...)` (`:231`, `:265`, `:282`, `:286`) and
`ic_.setComposingText(shown, 1)` (`:346`) — plus the service lifecycle in
`HunminInputMethodService.kt` (`onStartInput` at `:258` builds one adapter
per editor; `currentInputConnection` targets at `:364`, `:426`, `:455`,
`:593`, `:734`, `:826`, `:847`). `RecordingInputConnection` models exactly
that surface and traps everything else. The tier runs the **real** engine
through the **real** JNI shim (the same `hunmin-jni.c` the APK ships,
compiled for the host) — the deliberately-unmocked half, because the JNI
byte-format and `hunmin_ic` lifecycle are the contract the adapter relies
on.

Cost, with provenance: CI linux 148.38 s / deb 185.68 s, both cold-Gradle
(CI-TIMING §1); the android job's copy is cache-warm at ~23 s
(`ci.yml:410-420`); maintainer-host `android-unit.sh` warm 22.95 s / cold
126.24 s (CI-TIMING §2). **Fresh on this host, 2026-10-19: the whole tier is
50.6 s** — 116/116 pass, warm Gradle, including the host-JNI shim build
(`HUNMIN_BUILD_DIR=<this worktree>/build-mingw bash tools/android-unit.sh`).
`android-mutations` (nine planted defects, 84.31 s CI linux / 138.45 s deb,
54.0 s host sandbox) rides on this tier's harness. The remaining cost is
Gradle's `--rerun-tasks` design (`tools/android-unit.sh:47-56`), which is
the falsifiability mechanism that caught the "BUILD SUCCESSFUL, all
skipped" failure shape — the cost is the guard.

### E2 — the tier whose measured reliability problem is the real subject

Two scripts, one emulator: `tools/android-e2e.sh` (install `:83`, `ime
enable`/`ime set` `:98-100`, `input keyevent` `:317`, read-back via
`uiautomator dump` `:162`/`:220`/`:323`, `pm clear` `:121`, the W5
`pm grant`/`pm revoke` branches) and the Espresso half
(`HunminEspressoTest.kt`: `UiAutomation.executeShellCommand` `:294`,
`Instrumentation.sendKeySync` `:352`, `onView` assertions `:95`, `:124`,
`:227`).

Cost: the only wall figures in existence are **108.8 s / 108.9 s** for
`engine-android-e2e` (host AVD, 2026-10-16, after the `dump_ui` fixes —
wiki/raw/measurements/2026-10-16-android-e2e-keys-bypass-the-ime.md); the
espresso run has 12/12 pass recorded (2026-10-13) but no wall figure; the
emulator boot itself has never been measured as a number. The ctest
properties bound the tiers at `TIMEOUT 1200 s` / `1800 s`
(`CMakeLists.txt:417-435`).

Reliability, measured (same wiki page): after the fixes, two full passes,
then **Failed, Failed, Skipped — 0 of 3** — injected keys reaching the field
as literal ASCII while `dumpsys input_method` shows the IME current,
connected, bound, shown and visible; unroot-caused, with five refuted
hypotheses recorded. This is the audit's strongest datum on the issue's
"cheaper **or more reliable**" question, and it points the opposite way
from mocking: the flake is in *key delivery between the platform and the
IME* — the layer a mock defines away. The four contracts only this tier can
assert (platform routes hardware keys to an IME window; the assertion is
on-screen text from the platform's view hierarchy, not a log; an
`InputMethodService` is *bound*, the defect the Espresso suite caught in
`setUp`; `EditorInfo`-driven secure-field behavior, found as `퓬ㄴ` in a
password field) each caught a real defect that E1's fake cannot see by
construction — three of them are named with their findings in
`android-e2e.sh`'s own header.

### F1 — the mock opportunity, already taken and measured

Issue #80's mock shipped: `frontends/tsf/test/mock-tsf.h` holds in-memory
doubles of the interfaces `tsf.cpp` consumes (`MockContext`, `MockRange` with
a real `SetText` splice at `:62-70`, composition-channel recording for the
issue-#98 native composition at `:57-67`), stubbing the rest with
`E_NOTIMPL` so an adapter call the mock does not model fails loudly instead
of silently succeeding (`:27-32`). The harness includes the adapter source
unmodified (`frontends/tsf/test/CMakeLists.txt:41-54`).

**Fresh cost, this worktree at `5d4206d`, 2026-10-19: 7/7 pass, tier-F1 =
1.89 s·proc, total 1.92 s** (`ctest .` in the standalone MSVC configure of
`frontends/tsf`; each test sub-second — sibling-worktree logs of 2026-10-18
agree). What the mock structurally cannot see is the other half of tier F:
registry/COM activation, the live TIP session in real applications
(verified by hand in Notepad, 2026-10-17 — WINDOWS-TSF-REGISTRATION.md), TAT
stress (F2) and the install matrix (F3) — none of which is a ctest tier
today. The residual real-framework surface is already the *only* part of
tier F that exists outside the mock; a second mock layer has nothing left to
absorb.

## 3. The cross-platform question: which D/E assertions are engine assertions?

The issue asks which D/E assertions are "actually engine assertions that
could move to tier C". Measured against the tree, that migration has
largely already happened — with one asymmetry worth naming:

- The corpus is **generated** from tier A goldens (`tools/gen-conformance.py`),
  so no engine assertion lives in D/E in the first place; tier A stays the
  single source of truth for every D3/E1 expectation (`run-d3.sh:577-591`
  derives expected streams at run time from `hunmin-sim --trace`).
- The **x11** corpus replays through the real fcitx5 addon in-process —
  adapter-level conformance at tier C.
- The **win32** corpus replays through the real `tsf.cpp` over the mock —
  adapter-level conformance at tier C.
- The **android** corpus does **not** reach the adapter. `TraceEmit.kt`'s
  header says "replays it through the real Android adapter", but the code
  drives `HunminNativeBridge.nativeProcess(ic, keyCode)` directly
  (`TraceEmit.kt:143`), and the JNI shim's own comment says the runner
  replays "engine-character events" through the legacy call
  (`frontends/android/app/src/main/cpp/hunmin-jni.c:193`). The Kotlin
  adapter's KeyEvent→layout mapping is asserted only at E1. An adapter-level
  android runner (HunminAdapter + `RecordingInputConnection` as the sink)
  would be a *new* tier-C assertion, not a D/E migration — the android
  corpus (15 files, 35 events — IC-INPUT-OPTIONS.md §0 item 2)
  currently proves the engine through the shim, one layer below the adapter.

So the honest answer to the cross-platform question: D1/D2's remaining
assertions are framework contracts (§2 D1/D2), D3's are framework-internal
paths, E2's are platform contracts — and the one adapter-mapping gap runs
E1→C, not D/E→C.

## 4. Provenance

- **Fresh (this worktree, `5d4206d`, 2026-10-19):** F1 tier 1.92 s total
  (7/7) via `ctest .` in `frontends/tsf/build-msvc` (standalone MSVC
  configure, engine built with `build-msvc`-style recipe into
  `build-msvc/hunmin.lib`); E1 tier 50.6 s (116/116) via
  `bash tools/android-unit.sh` with `HUNMIN_BUILD_DIR` pointing at this
  worktree's llvm-mingw build; inner loop `ctest -j16 -LE
  'slow|mutation|tier-E0|tier-E2'` = 52 tests, 4.14 s, with exactly one red —
  `builds-android`, the documented spawn-broken-`.sh` baseline on this host
  (workspace AGENTS.md, measured 2026-10-18), not caused by any change (this
  branch ships none). 58 tests register in this worktree's mingw configure
  (`ANDROID_HOME` exported at configure).
- **Host (this machine, earlier, trees named):** D3 serial figures and the
  D3-candidate 16.41 s — TESTING.md (maintainer's Linux host, 2026-10-10 and
  2026-10-15); D3 component breakdown — D3-PERFORMANCE (same host,
  2026-10-10); D1+D2 0.23 s and tier-C 1.26 s — TESTING.md per-label table
  (maintainer's host, 2026-10-10, 59-test config); E2 108.8 s/108.9 s and
  the 0-of-3 reliability series — wiki/raw/measurements/2026-10-16;
  espresso 12/12 — TESTING.md (2026-10-13); `android-unit.sh` warm 22.95 s /
  cold 126.24 s — CI-TIMING §2.
- **CI (last executed logs, no run since 2026-10-13):** linux ctest 269 s
  and the per-test figures 148.38 / 84.31 / 19.36 s — CI-TIMING §1; the
  android job's warm ~23 s E1 — `ci.yml:410-420`.
- **Read but not executed (no figure exists, and this host cannot produce
  one):** the emulator cannot boot here (no hypervisor, not an admin —
  workspace AGENTS.md, measured 2026-10-18), so E0's on-device wall time and
  the E2 emulator-boot time are unmeasured anywhere; the ctest properties
  (`TIMEOUT 600/1200/1800`) are the only published bounds. The name of the
  test that would measure them red if it existed: none exists — a
  boot-time-recording wrapper around the emulator step is the instrument
  that would.
- **Not re-derived here:** the CI log parsing behind CI-TIMING §1 (its
  provenance lists the exact job ids). The corpus counts were re-derived
  fresh (`ls`/`grep -c` over `tests/conformance/`, 2026-10-19): x11 118
  files / 3,863 events — matches TESTING.md tier C; android 15 files / 35
  events — matches IC-INPUT-OPTIONS.md §0 item 2; **win32 corrected: 3
  files / 452 events, not the 2 / 52 that IC-INPUT-OPTIONS.md records** —
  its figure predates issue #66's traces (`90cf059` added the third file),
  so that document's corpus count has drifted from the tree and this
  paragraph is the correction of record for it.
- **Deliverable-specific checks:** no test was deleted or weakened — the
  branch's entire diff is this file (verifiable: `git diff --stat master
  HEAD` is this document). The audit covers D1–D3, E0–E2 and F as the issue's
  verification line requires; E0's row exists although the issue's candidate
  table omitted it (its own verification section demands E0–E2).
