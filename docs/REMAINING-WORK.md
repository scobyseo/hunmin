# REMAINING-WORK — what is left, and in what order

Written 2026-10-09 from a verification pass over `PLAN.md`, `AGENTS.md`, `README.md`,
`docs/*.md` and `wiki/`. Every "still missing" claim below was **checked against the
tree** before being written down, and the check is named. Where a document asserted
something was undone and the tree said otherwise, the document was corrected instead —
those corrections are listed in §5, because "what remains" is only trustworthy if the
"already done" half was true when it was measured.

**Corrected 2026-10-10 (this pass).** Between the original
write and this one, master landed seven changes — and **every one of them closed
items this document still listed as open.** §7.1 names each of them by what it did.
§7 records the seven-item audit, the root cause of why the board went stale while
carrying the correct diagnosis of its own disease, and
the guard now enforcing it. §7's assertion table is the load-bearing part: it is
parsed and executed by `tools/docs-lint.py` check 6 on every run.

**Release lanes (2026-10-11): 0.1 = fcitx5 + Android, 0.2 = Windows, 0.3 = 자동 한/영
전환, 0.4 = 소프트 키보드 (두벌식 + QWERTY + 기호), then polish to 1.0.** Which of the
items below belongs to which lane is
[`MILESTONES.md`](MILESTONES.md) — a rearrangement of this board, not a second copy
of it. Status claims stay here and nowhere else.

**P6 (Windows TSF) is excluded by the maintainer's instruction**: there is no Windows
environment on this host (`docs/ENVIRONMENT.md` batch 4, not installed) and no way to
verify a TIP, so work on it could not be measured. Nothing below depends on it, and
§4 says what that leaves short.

---

## 0. Known failures at HEAD — **updated 2026-10-10, and this table was stale**

Re-measured at this pass's base in a worktree configured `-DHUNMIN_TRACE_BUILD=ON`.
**Every row of the previous version of this table is now false.** The previous
version listed `engine-d3-x11` (and the two GTK variants) as failing on
`dubeolsik dkssuds`; the D3 goldens were regenerated and all three now pass.
`engine-d1-fcitx5` and the `docs-lint` count errors are likewise closed.

| Test | Was (2026-10-09) | Now (2026-10-10) |
|---|---|---|
| `docs-lint` | 3 errors: "156 ctest tests" | **passes** — the bare count was dropped |
| `engine-d1-fcitx5` | rc 134 abort, CapsLock `J K 1` | **passes** |
| `data-selfcheck` | us-qwerty.json format drift | **fixed 2026-10-13 (issue #46)** — skips (rc 77) when the gitignored krdict dump is absent; see below |
| `engine-d3-x11` | `ㅎ\|곈` instead of `안\|녕` | **passes** (goldens regenerated) |
| `engine-d3-gtk3` / `-gtk4` | same | **both pass** |

**`data-selfcheck` no longer fails in a fresh worktree (2026-10-13, issue #46).**
It used to fail on `krdict2data: no *.xml under …/data-orig/krdict` — the 368 MB
국립국어원 dump is gitignored and absent from a new worktree, and the test did
not skip on a missing input; it failed. Now, with no `*.xml` under
`data-orig/krdict/`, `tools/selfcheck.py` skips that one converter step, copies
the committed table across so `diff -r` cannot report it missing, and exits 77
(`SKIP_RETURN_CODE`), so ctest reports the test **Skipped** instead of red. In
the maintainer's primary tree, where the dump is present, every step runs
exactly as before. The earlier 2026-10-09 diagnosis (a `diff -r` whitespace
divergence in `us-qwerty.json`) and the 2026-10-10 diagnosis (krdict) were both
true of their own tree at their own time; the current state is the skip.

The self-contained guard for the converter's *logic* is ctest test
`krdict-fixture` (tier-B): a committed synthetic fixture under
`tests/fixtures/krdict/` converted into a temp dir and diffed byte-identically
against a committed expected `krdict.json`. The dump-dependent full-dump
determinism run is the maintainer-only `tools/selfcheck-krdict.sh`, which is
deliberately **not** registered with ctest — a run that needs 368 MB cannot
live on the repo's test surface.

`ctest` is green on a worktree configured `-DHUNMIN_TRACE_BUILD=ON`; the main
tree here is configured without it, which registers fewer, so a bare count in
prose is only meaningful next to the configuration it came from. No test is red
in a fresh worktree: `data-selfcheck` reports Skipped where the gitignored
krdict dump is absent — see §0 above.

---

## 1. Blocked on the maintainer — do not start these

AGENTS.md: *"네 멋대로 기각하지마"* — a negative result is a finding, "therefore reject it"
is a decision. These are the items an agent may measure but may not settle.

| # | Question | Where | Why blocked |
|---|---|---|---|
| B1 | **The hangul judgement source** — what R8's `hangul_valid` looks up | `wiki/decisions/the-outstanding-decisions.md:20-33`, `docs/MODES-DATA.md:652` | (a) corpus top-N rejected by measurement; (b) dictionary lookup weakened because the 45.42% coverage came from a hanja-filtered subset; (c) combination test provisionally adopted, but krdict is outside the 2026-10-05 scope boundary and **no replacement source inside the boundary has been chosen**. Everything in §2 inherits this. |
| B2 | **Pass lines for all three metrics** | `docs/MODES-METRICS.md:118-122` | *"there is no **how much** … grading is impossible."* Not a measurement; a number only the maintainer can set. |
| B3 | **ON or OFF by default** ✅ **DECIDED 2026-10-11** | `docs/SPEC-MODES.md:555-557`, `docs/SPEC-MODES.md:701-708` | Maintainer: OFF by default until release 0.3; the feature's additional data ships separately and the feature enables only where that data is present. Ruling quoted verbatim in `wiki/decisions/2026-10-11-auto-switch-off-by-default.md`. E5's *assignment* half remains open (see B4). |
| B4 | **E5 — the 한/영 key** ✅ **DECIDED 2026-10-12 (0.1 half)** | `docs/SPEC-MODES.md:680-709`, `docs/MODES-ENGINE-RULES.md:77-81` | Maintainer: **the feature is disabled by default in 0.1, by disabling the default key composition** — the AUTO_TOGGLE default chord (Right Alt + Enter) is removed, the dispatch skips the action, and the settings row shows 미정 (0.3). Ruling quoted in `wiki/decisions/2026-10-12-auto-toggle-disabled-by-default-in-0-1.md`. What remains open is the **E5 assignment itself** (single vs compound meaning), which lands in the 0.3 lane per `docs/MILESTONES.md`. The F9 constraint (occupied by 전처리수식, no `KEYCODE_F9` on Android — `docs/TESTING.md:280-281`) still applies to that future assignment. |
| B5 | **Space emission is a frontend concern** ✅ | `wiki/log.md:111-112` | Measured 2026-10-09: engine declines space, flushes syllable; fcitx5 XIM/GTK 3/4, Android all forward the declined space. No engine change needed. |
| B6 | **D3 — is a frequency list derived from AIHub 119 a reshaping?** | `the-outstanding-decisions.md:41-47` | Licence question. *"probably no, but it has not been settled and should not be assumed either way."* |
| B7 | **D4 — does the wiki stay?** | `the-outstanding-decisions.md:49-60`, `status: active`, `contested: true` | Adopted *"to be re-evaluated later"*, against three named criteria. |
| B8 | **The keystore path in `build.gradle.kts:77`** | §3 W3 | A hardcoded `/home/scoby/.android/debug.keystore` sits directly under a comment (line 74) saying the keystore lives in gitignored `local.properties`. Two lines apart, and they contradict. Which one is right is a packaging decision. |
| B9 | **The R8 adapter's structural divergence from R3** | §3 M1 | R3 `[D]` puts guessing in the engine; `modeswitch.*` puts it in the fcitx5 addon. One of the two documents is wrong. Not an agent's call. |

---

## 2. Blocked behind B1 — measurable only once the source is chosen

Not blocked on anything else; they are simply downstream of the judgement source.

| # | Work | Where |
|---|---|---|
| M1 | **③-bis wrong-direction switch metric** | `docs/MODES-METRICS.md:66` `NOT MEASURED` — needs the same source R8 does |
| M2 | **E1 — does the symmetric rule hold** (english→hangul)? | `the-outstanding-decisions.md:39` *"Not yet measured."* Was measurable via krdict's `pos`; that route left scope, so it must be re-derived inside the boundary. |
| M3 | **E2 / E3 / E6 / E7** thresholds (buffer cap N, short-word N, flicker) | `docs/SPEC-MODES.md:622-742`, all `[E]` |
| M4 | **Third-party correctness**, both 두벌식 and 세벌식 | `PLAN.md:301` *"not started"* |
| M5 | **Word-list schema** — `docs/SPEC-SCHEMA.md` has no word-list section | `docs/MODES-DATA.md:140-155`. Undecided: `debian/copyright` wording, whether to store pos/frequency/level, 조사/어미 attachment. |

**Naming collision, and it is a real defect in this board.** §2's M1–M5 and §3's
M1–M5 are different items with the same labels. Two sessions reading different
sections would resolve "M1" to different work — which is precisely how a patch
confined to "B5's line" could look correct while leaving five rows wrong. Renaming
them is a **maintainer decision** (which prefix belongs to which section), so this
pass does not rename: it records the collision and leaves both sets of numbers in
place.

---

## 3. Unblocked — can be done now

Ordered so that each step leaves the repo in a state where the next is measurable.

### W — Android frontend gaps (P5)

| # | Work | Verification |
|---|---|---|
| W3 | **B8 above** — the keystore path contradiction. | See §1 B8. |
| W5 | **`RECORD_AUDIO` is never requested**, and cannot be — an `InputMethodService` has no permission hook (javap on android.jar API 35). It must be granted in system settings. `tools/android-e2e.sh:83` installs with `-g`, so **the denied branch has never run**. ✅ **Closed 2026-10-10** — tier E2 now drives both branches: `pm grant` establishes the granted state and asserts the platform reports it held, `pm revoke` establishes the denied state and asserts the platform reports it not held, and after the revoked tap the keyboard is asserted to still type. The dialog itself remains unreachable by construction (AppOps `ignore` is a silent deny after `pm revoke`; the emulator has no SpeechRecognizer), which the new E2 section records as a measured platform fact rather than pretending around. | E2's W5 section (`tools/android-e2e.sh`, three runs green 2026-10-10) |
| W6 | **The mic button tap effect** ✅ **Closed 2026-10-10** | Tier E1 tests `VoiceLauncher.decidePermission` (the tap's decision logic); tier E2 (`tools/android-e2e.sh`) taps the mic button in both granted/revoked states and verifies keyboard survival; manual checklist now notes E2 coverage. |

### N — not started, no blocker

| # | Work | Note |
|---|---|---|
| N2 | **Tier E4 soak / nightly fuzz** ✅ **Closed 2026-10-10** | Nightly fuzz job with LeakSanitizer added to CI (`.github/workflows/ci.yml`), `tools/fuzz-generate-keys.py` created. |
| N5 | **E2 planned extensions** ✅ **Closed 2026-10-10** | Espresso assertions for composing span, candidate view, Hanja F9 flow, secure-field negatives, `inputView` show/hide policy added under `frontends/android/app/src/androidTest/`. |
| N6 | **P7 — Debian ITP, external-contributor docs** ✅ **Closed 2026-10-10** | `CONTRIBUTING.md` added; `debian/` packaging exists (`control`, `copyright`, `changelog`, `rules`, `hunmin.postinst`). Optional fcitx5-android port remains unscheduled. |
| N7 | **D3 candidate-window coverage is unreachable, and that is settled** | Measured twice; two mutations left D3 green both times. Closed by D2 alone. `docs/D3-ROOT-CAUSE.md:242-283`. Listed so it is not re-attempted. |

### Not doable here, recorded so nobody plans around it

- **Tier E3 gate.** `tests/manual-checklists/tier-e3-bt-keyboard.md:7-11` — *"NOT STARTED.
  No device has been signed off."* Three device rows (One UI phone, One UI tablet, AOSP
  Pixel), all unchecked. A manual tier with no automation substitute; `PLAN.md`'s P5 gate
  stays open on this one line.
- **Voice recognition quality.** ✅ **Measured on hardware 2026-10-12** (issue #11): the
  tier E4 run passed once with the maintainer speaking the phrases; an unattended run is
  not possible (person-in-loop tier — no speaker means no speech, so such runs fail
  without measuring recognition). The record lives in
  `tests/manual-checklists/tier-e4-voice.md`. Still not measurable *on this host* — the
  AVD boots `-no-audio` and has no offline ko-KR pack — which is why it is a maintainer
  session, not a ctest row.
- **A soft keyboard.** The input view is a layout strip plus a mic, not keys. No priority has
  been assigned to it by anyone; that assignment is a maintainer call. **Assigned
  2026-10-11: it is release 0.4, goal 두벌식 + qwerty + 기호 입력 지원 — see
  `docs/MILESTONES.md`.**
- **D4 Wayland plumbing.** `weston --backend=headless` + `zwp_input_method_v2` harness.
  `docs/TESTING.md:247-249` marks it optional and not blocking P4; fcitx5's Wayland module
  is not our code.
- **The R8 shrinker path end-to-end.** ✅ **MEASURED 2026-10-11** (issue #4).
  `proguard-rules.pro` (139 lines) is written and `isMinifyEnabled = true` is set; the
  change that added them had recorded *"Not measured: the release APK with R8 actually
  applied (needs a signed release build and an emulator run)."* That gap is now closed.
  The measurement, on this host (AVD `hunmin-e2e`, API 35, x86_64):
  - `./frontends/android/gradlew -p frontends/android assembleRelease` → **exit 0**,
    APK at `frontends/android/app/build/outputs/apk/release/app-release.apk` (1,505,137
    bytes, both ABIs), signed with the hardcoded debug keystore (issue #1's hardcoding —
    it worked; nothing was changed to make it work).
  - `frontends/android/app/build/outputs/mapping/release/` holds `mapping.txt`
    (4,221,158 bytes, 43,236 lines), `configuration.txt`, `seeds.txt`, `usage.txt`.
    The rules behaved as intended: every class named by a `-keep` in
    `proguard-rules.pro` appears in `seeds.txt` and maps to **itself** in `mapping.txt`
    (`org.hunmin.ime.HunminNative -> org.hunmin.ime.HunminNative` etc.), so JNI names
    survive; `usage.txt` shows what R8 did remove under `org.hunmin.ime` — 42 class
    entries of removed members: the `R` inner classes, Kotlin data-class `copy`
    helpers, and unused members of kept classes. No class named by a `-keep` rule was
    dropped, and no JNI or IME-lifecycle surface appears in the removed list.
  - `adb install -r <release apk>` → **exit 0** ("Success"); the service appears in
    `adb shell ime list -a` as `org.hunmin.ime/.HunminInputMethodService`.
  - Composition on the R8 build: with the IME selected and the Settings search field
    focused, `input keyevent` `dkssud` left the field reading exactly `ㅎ곈디` — the
    value tier A's `hunmin-sim --layout sin3-p2 --keys dkssud` record
    (`tools/android-e2e.sh:355-371`) says sin3-p2 produces. The release APK composes
    Hangul on the device.
  - Two host preconditions, recorded so a rerun is reproducible: the engine `.so` per
    ABI must exist first (`./tools/build-android.sh <abi> 24` — the JNI shim's CMake
    FATAL_ERROR names this command), and `ANDROID_NDK` must point at r27 per
    `docs/ENVIRONMENT.md` while AGP's **own** toolchain compiles the shim with the SDK's
    `cmake/3.22.1` + default NDK `26.1.10909125` (Clang 17.0.2). The version skew
    between shim and engine toolchains did not block the build or the run; it is a
    measurement, not a defect I was authorised to change.
  Still open after the measurement: `tools/android-e2e.sh` asserts against the
  **debug** APK by design, and no CI job does an `adb install` of the release
  build — so the release APK is built in CI (job `android-release`, added
  2026-10-13 for issue #8) but never typed into there. The shrinker path itself
  is measured and green on this host. The reproducible procedure and the
  observed run are recorded in
  `tests/manual-checklists/release-r8-shrinker.md`.

**CI itself had never run, and that was measured 2026-10-13 (issue #8).** While
adding the packaging jobs, every CI run on this repository — 38 of them, from
the initial public commit `fbb0c43` (2026-10-10) — was found to have failed with
"This run likely failed because of a workflow file issue". Cause: an unquoted
`if: failure()` (`.github/workflows/ci.yml:138`, and the same at :226), which
YAML 1.1 parses as the boolean `True`; GitHub rejects the whole file. A rejected
workflow file registers no jobs at all, so the failure reads as a file error
rather than a step error, and **no CI job had ever executed once**. Both
occurrences are now quoted (`if: "${{ failure() }}"`). This is the same shape as
the board's own disease: a status line ("CI is green") carried forward past the
commit that invalidated it, and nothing mechanical caught it because a rejected
workflow file never reaches a job whose result anyone checks.

### C — CI coverage of the 0.1 artefacts (issue #8)

| # | Work | Verification |
|---|---|---|
| C1 | **CI builds neither 0.1 artefact** — `assembleDebug` only, no `.deb` job. | The issue's own grep. Closed 2026-10-13 as a tree fact: `.github/workflows/ci.yml` now has job `deb` (`dpkg-buildpackage -us -uc -b` + artefact upload) and job `android-release` (`assembleRelease`, both ABIs, artefact upload, gated on `vars.HUNMIN_BUILD_RELEASE='true'`). `tools/build-release.sh` is the same sequence locally. **The ruling the issue asks for is still open**: whether 0.1 *requires* CI-built artefacts or a local run suffices is the maintainer's call — these jobs make both answers cheap, they do not make one. |
| C2 | **The `deb` job's suite, measured on CI.** | Run 38328467052 (commit c91906a): the deb build reaches the full suite, and the only test red is `data-selfcheck` — the documented §0 gitignored-krdict defect (the D3 desktop tests skip on CI, no X display). Two packaging-tree defects were fixed to get here: the engine `.so` lives in `obj-x86_64-linux-gnu` not `build/` (the Android-tier scripts now read `HUNMIN_BUILD_DIR`, which `debian/rules` exports), and the Gradle wrapper cannot create `/root/.gradle/.../*.lck` under fakeroot (`debian/rules` points `HOME` at a writable dir, recipe-scoped). The `linux` job likewise has only `data-selfcheck` red. The `.deb` artifact upload is gated behind a green suite, so it stays skipped until `data-selfcheck` is resolved — whether a missing input should skip or fail is the maintainer's call (§0, §7.4), not changed here. **Resolved 2026-10-13 (issue #46):** the ruling was made and implemented — `data-selfcheck` now skips (rc 77) without the dump, so this measurement's only red test is gone on both jobs. |
| C3 | **CI had never run — three independent root causes, all measured.** | (a) The workflow file was invalid: the android job carried a job-level `paths:` (a trigger filter, not a job property), so GitHub rejected the whole file and **no job ran on any commit since the initial public one**. The `if: failure()` quoting fixed alongside was a real but separate latent YAML-1.1 boolean bug. The android path filter is now a **job-level `if:`** over `github.event.head_commit.modified` — a per-job `paths:` is not a GitHub feature, and an intermediate `on.push.paths` version gated the *whole* workflow on Android paths (a `debian/rules`/`tools/*.py` change skipped every job), which was measured and reverted. (b) The addon needs fcitx5 >= 5.1.13 (`FCITX_ADDON_FACTORY_V2` engine.cpp:619, `setComment` :461 — both first present in 5.1.13, measured across upstream tags 5.1.7/5.1.9/5.1.13). Ubuntu 24.04's distro fcitx5 is 5.1.7, below the floor, so CI now builds fcitx5 5.1.13 from source (linux + deb jobs) and the `.deb` requires >= 5.1.13. **Consequence for supporting Ubuntu 24.04:** the distro's 5.1.7 is NOT supported — a 24.04 user installs the source-built 5.1.13+. An earlier attempt lowered the floor to 5.1.7 on the theory that only the cmake macro was missing; CI's compile error (`setComment`/`FCITX_ADDON_FACTORY_V2` undeclared) disproved it and the floor was restored to 5.1.13. The local `add_fcitx5_addon` macro fallback (frontends/fcitx5/CMakeLists.txt) still helps a bare 5.1.7 configure but cannot bridge the API gap. |

---

## 4. Suggested order, and the deciding reason

**Revised 2026-10-10.** The previous order (`M1 → M2 → W1 → the rest of W →
M3/M4/M5 → N1–N6`) was written when §3's M1–M5 and W1, W2 and W4 were open. Six of
those eight are now closed, so the order's stated reasoning — "M1 first is a
dependency, because B9 cannot be answered while the class has no test" — has been
satisfied: `frontends/fcitx5/test/testmodeswitch.cpp` was added and ctest
`engine-d1-modeswitch` registered, which **passes** here.

**Revised again 2026-10-10, after tier E2 closed W5/W6 and N2/N5/N6 were completed.** W5, W6, N2, N5, N6 are done. The remaining unblocked items are environment-bound (E3 gate, R8 shrinker E2E, soft keyboard, D4 Wayland) or decision-blocked (B1–B9). No further unblocked code work remains at this time.

**Revised again 2026-10-11: the R8 shrinker E2E is measured** (see the row above; Refs #4). It left the environment-bound list. Remaining environment-bound: the E3 gate (needs a physical device), the soft keyboard (release 0.4), D4 Wayland.

## Voice input enhancements (incremental, staged)

| Stage | Feature | Status | Notes |
|---|---|---|---|
| 1 | Voice trigger | ✅ Done | **Shift + KANA**, by the maintainer's instruction 2026-10-11 (was Right Alt + V); it is now the configurable 기본값 for 음성 입력, so it lives in `KeyAction.VOICE` rather than in `onKeyDown` |
| 2 | Auto Hangul/English toggle on/off: Right Alt + Enter | ✅ Done 2026-10-12 — **disabled by default (inert)** | Maintainer ruling (issue #2): the feature ships inert in 0.1 by disabling the default key composition — no default chord (Right Alt + Enter is unbound, Enter reaches the editor), the service skips the `AUTO_TOGGLE` action even when a stale binding is stored, and the settings row shows 미정 (0.3), disabled. The handler stub remains for the 0.3 lane. |
| 3 | Voice language follows keyboard mode | ✅ Done | `SpeechRecognizerRecognizer(language)` uses `ko-KR` / `en-US` |
| 4 | Longer silence timeout (partial results) | ✅ Done | `EXTRA_PARTIAL_RESULTS=true`, 1.5s/3s silence |
| 5 | Mic button visual state (idle/listening/error) | ✅ Done | `LayoutPicker.setMicState(VoiceState)`; the dock binds it as a green capsule (`res/drawable/toolbar_mic_active.xml`) and styles B and C as a coloured icon |
| 6 | IME switch button in picker | ✅ Done | Shows system `showInputMethodPicker()` |
| 7 | Configurable key bindings | ✅ Done 2026-10-11 | `KeyBinding.kt` + the key settings window. Stored as `v1:<keyCode>:<modifiers>` in `hunmin_keys`; the four triggers are read by `HunminInputMethodService.onKeyDown`. Measured by tier E2's settings-window section: a key captured in the window is persisted, displayed back, and fires in the running IME. GitHub issue #12. |
| 8 | Auto Hangul/English detection engine integration | 🔒 Blocked on B1 | The 0.1 half of B4 was decided 2026-10-12: the feature ships inert in 0.1 (default key composition disabled — `wiki/decisions/2026-10-12-auto-toggle-disabled-by-default-in-0-1.md`). What remains is the engine integration itself, gated on B1 as before, and the E5 key assignment, which lands in the 0.3 lane. The ON/OFF default was ruled 2026-10-11: OFF until 0.3, feature enable-able only when its separately-distributed data is present — `wiki/decisions/2026-10-11-auto-switch-off-by-default.md` |

**Evidence that would change this:** if B1 is answered quickly, §2 becomes the
critical path and should jump ahead of everything here — P8 is the phase with no
engine implementation at all, and it is the maintainer's own stated requirement.

---

## 5. What was corrected while writing this

Every one of these was a document asserting work was undone, checked against the tree, and
found false. Recorded because "what remains" inherits the credibility of the "already done"
half.

| Claim | Where it was | Reality |
|---|---|---|
| 140 tests / 140/140 | `TESTING.md:80,166`, `PLAN.md:145`, `README.md:18`, `AGENTS.md:131`, `D3-ROOT-CAUSE.md:4` | **156** with `-DHUNMIN_TRACE_BUILD=ON`, all passing |
| 104 traces | `TESTING.md:80,118` | **118** `.trace.in`/`.gold` pairs, 3,863 events, 1,969 commit assertions (recounted) |
| `make abi-accept` | `TESTING.md:37` | No Makefile exists. `AGENTS.md:117` already said so. The real command is now written out. |
| `make retrace` | `TESTING.md:22,434`, `trace-spec.md:113` | No Makefile. It is `tools/regen-goldens.sh`. |
| "no NDK-clang job at all" (`grep -c android` = 0) | `TESTING.md:40-41` | That grep is **19**. `.github/workflows/ci.yml:23` defines the `android` job, which installs NDK 27.3.13750724. The claim was made *on the evidence of the grep* and the grep was wrong. |
| 5 CI jobs incl. `nightly`, fuzz, debs, TIP/TAT | `ENVIRONMENT.md:405-413` | **4** jobs: `linux`, `android`, `mingw`, `windows`. No nightly, no fuzz, no deb build, no TIP/TAT. mingw/windows are `-DBUILD_TESTING=OFF`. |
| "Candidate assertions remain impossible until P3 adds candidates" | `TESTING.md:155` | P3 landed 2026-10-03; `testhunminui.cpp:168-200` asserts list, size, kind label and text. |
| "nothing in the addon uses candidates yet" | `FRONTEND-CONTRACT.md:287-291` | Same. |
| P3 has no progress marker; licence gate unstruck | `PLAN.md:136,314` | P3 shipped; the source gate closed 2026-10-02. |
| P1b-2 ⏳ "romaja, symbol tiers, CapsLock/W" | `PLAN.md:131-132` | All three ship. Only `seq` is open. |
| "no keystore config, no version-code scheme" | `AGENTS.md:194-195`, `PLAN.md:196-205`, `README.md:33` | Both were added. R8 rules remain genuinely missing. The hardcoded keystore path is the real defect (B8). |
| Android tier-C skips "because no Android frontend exists" | `AGENTS.md:199`, `TESTING.md:130-133` | The frontend exists. The real gap is no `.gold` and no Android runner. |
| P8 is "the only phase with no implementation at all" | `PLAN.md:291-293` | True of the **core**, and the grep is still empty. But `modeswitch.*` implements R8 in the addon — undocumented, untested (M1). |
| `wiki/index.md:5` "Total pages: 9" | `wiki/index.md` | `tools/wiki-lint.py` counts more; the header predates three 2026-10-09 pages. |
| N2 nightly fuzz job "does not exist" | `REMAINING-WORK.md:114`, `ENVIRONMENT.md:411` | Job added to `.github/workflows/ci.yml` with LeakSanitizer; `tools/fuzz-generate-keys.py` created. |
| N5 Espresso tests "src/androidTest/ does not exist" | `REMAINING-WORK.md:115`, `docs/TESTING.md:315` | `frontends/android/app/src/androidTest/` created with 9 Espresso tests. |

**The pattern worth naming.** Four of these were correct when written and were never
revisited; at least three were *conclusions drawn from a specific measurement that was
itself wrong* (`grep -c android` = 0, the trace count, the test count). The failure is not
carelessness — it is that a measured number gets copied into prose, the number moves, and
nothing notices. That is the argument for §6.

---

## 6. Keeping this from going stale — **superseded by §7, kept because a retracted
claim stays marked**

The instruction was to propose a way to work without document inconsistency. The
observation above says the inconsistency is mechanical: **a figure lives in prose and
nothing checks it.** So the check should be mechanical too, and it should live next to
`tools/doctor.py`, which already gates data.

Proposed, in order of value per unit of work:

1. **`tools/docs-lint.py`, registered in ctest** — assert the small set of facts that
   prose cannot be trusted to hold: the ctest count, the trace count, the `abbreviations`
   rule from `docs/GLOSSARY.md`, and that every `sources:` entry in the wiki resolves.
   Each is a mechanical fact about the tree with exactly one right answer. Registering it
   means a stale count fails a build instead of surviving until a reader notices.
   *This is the one that would have caught most of §5.*

2. **Do not write a figure into two places.** AGENTS.md already says this for
   `docs/MODES-DATA.md` vs `wiki/raw/measurements/`, and the rule is sound — it just is not
   applied to *counts*. Prefer "`ctest` is green" over a bare count, and when a
   number matters, write the command that produces it.

3. **Distinguish a prediction from a status.** §5's candidate line was a *prediction*
   ("impossible until P3 adds candidates") that was true and outlived its subject. When a
   document records something as conditional, mark it conditional in a way a reader can
   check later — "until P3 adds candidates" should have been a link to the commit that
   made it false.

4. **Re-verify on the merge, not on the write.** Some of §5 arrived because two sessions
   wrote about the same tree at different times (this session's own base moved twice
   mid-task). A doc change that touches a count should be re-checked at merge
   time, not authored from a stale checkout.

5. **Keep the "what remains" list honest about corrections.** §5 exists because deleting
   the wrong claim would destroy the evidence that the mistake happened — the same rule the
   wiki already applies to retracted pages. A `REMAINING-WORK.md` that silently drops its
   own history cannot be audited.

**Why this section is superseded.** Items 1–5 were all implemented — `tools/docs-lint.py`
exists and is ctest test `docs-lint` — **and the board still went stale**, in a document
section none of those checks reads. §7 says why, and §7's assertion table is the guard
that closes it. This section stays, marked, for the same reason §5 does.

---

## 7. Why this board went stale anyway, and the guard that now prevents it

### 7.1 What happened, measured

Between this document's original write (2026-10-09) and 2026-10-10, master
received eight changes. Each of the first seven **closed items §3 still listed as
open**; the eighth was documentation only. The table names each change by what it
did rather than by the commit that carried it, so a row stays checkable from the
tree alone:

| Board said open | Closed by | Evidence |
|---|---|---|
| §3 M1 — R8 adapter has no committed test; `grep -rn ModeSwitch frontends/fcitx5/test/` returns nothing | the measurement-hygiene pass | `frontends/fcitx5/test/testmodeswitch.cpp` exists (14 `ModeSwitch` hits), registered as ctest `engine-d1-modeswitch` (`frontends/fcitx5/test/CMakeLists.txt:17`), and **passes** here |
| §3 M2 — `tools/doctor.py` globs `compose/` and `layouts/` only, so `data/{hanja,idioms,krdict}/` is never checked | the measurement-hygiene pass | `doctor.py:331-336` now globs all three dirs, with `check_hanja`/`check_idioms`/`check_krdict` present |
| §3 M3 — `wiki-lint.py` does not check `sources:` targets exist | the measurement-hygiene pass | `wiki-lint.py:144` `check_sources_exist`, called at `:255` |
| §3 M4 — `android-e2e.sh:54` uses `local` at top level | the measurement-hygiene pass | line 54 is now `avd="${HUNMIN_AVD:-hunmin-e2e}"`, inside `if ! "$ADB" get-state`. `grep -n '^\s*local '` finds only 2 hits, both inside functions (`:157`, `:158`) |
| §3 M5 — 29 pyright errors across `tools/*.py` | the measurement-hygiene pass | `pyright tools/` → **0 errors, 0 warnings** |
| §3 W1 — 1 `.trace.in`, **no `.gold`**, no Android runner | the Android tier-C corpus | 14 `.in`/`.gold` pairs in `tests/conformance/android/`; `tools/android-trace-emit.sh`; `TraceEmit.kt`; per-case `engine-android-conformance-*` in `CMakeLists.txt:422` |
| §3 W2 — no R8/proguard rules, `isMinifyEnabled = false` | the Android R8/mic/inset change | `frontends/android/app/proguard-rules.pro` (139 lines); `isMinifyEnabled = true` at `app/build.gradle.kts:86` |
| §3 W4 — mic duplication + nav-bar inset | the Android R8/mic/inset change | one mic per style: style A's `DockToolbar` and styles B/C's `LayoutPicker` each build exactly one, and `bind()` repaints rather than re-creates; `ViewCompat.setOnApplyWindowInsets` applies the nav-bar inset |
| §3 N1 — `tools/kcm_export.py` does not exist; no `kcm*` file anywhere | the keymaps exporter | `tools/kcm_export.py` (10,210 bytes) plus 16 files under `frontends/android/keymaps/` |
| §3 N3 — `seq` symbol tier, "the genuinely open part of P1b-2" | the `seq` symbol tier | `HK_SEQ` in `hunmin_internal.h:18`; `data.c:121-140` parses `{"seq": [...]}`; `ic.c:592` commits it; `doctor.py:57-63` validates it instead of warning |
| §3 N4 — the deb needs a postinst; `debian/` has no `postinst` | the deb postinst | `debian/hunmin.postinst` — note the **real filename is `hunmin.postinst`, not `postinst`**; the old row's own claim named a path that would never have matched the shipped file |
| §3 D3 — the known-failures row for `engine-d3-x11`/`-gtk3`/`-gtk4` | the D3 cultural-case fix | all three **pass** here (x11 measured at 40.7 s, gtk3 46.0 s, gtk4 45.7 s) |
| §3 N2 — "no nightly fuzz job" | the nightly fuzz pass | `.github/workflows/ci.yml` has `nightly-fuzz` job with LeakSanitizer; `tools/fuzz-generate-keys.py` generates random HW-key streams |
| §3 N5 — "src/androidTest/ does not exist" | the Espresso test pass | `frontends/android/app/src/androidTest/` created with 9 tests covering composing span, candidate view, Hanja F9, secure fields, inputView policy |

That is **thirteen rows**, not seven — the count depends on whether the two W rows and the
three D3 test rows are counted individually. Either way, the majority of §3 was false.

### 7.2 Root cause: the board diagnosed its own disease and then caught it

This is not a case of a careless document. §5 already recorded the mechanism — *"a
measured number gets copied into prose, the number moves, and nothing notices"* — and §6
already proposed `tools/docs-lint.py` as the fix. The tool was built and registered. The
board then went stale **exactly the way §6 predicted, in a section no check reads.**

Three measured facts, and the third is the load-bearing one:

1. **`tools/docs-lint.py` exists and is registered** as ctest test `docs-lint`
   (`CMakeLists.txt:174-176`). It checks `PLAN.md`, `README.md`, `AGENTS.md`, `docs/*.md`
   and `wiki/` — and `REMAINING-WORK.md` is in `docs/`, so it *is* scanned. But what it
   scans those files **for** is the ctest count, the trace count, `` `make X` `` commands,
   the abbreviation rule, wiki `sources:` and the wiki index count. **None of those is a
   claim about whether a listed item is open.** The item tables are markdown tables whose
   cells are free-text prose, and no check parses them. So the board's own claims — the
   only claims the board exists to make — were the one class of claim nothing verified.

2. **The closing commits touched the file but not the tables.** The
   measurement-hygiene pass closed five items and changed
   `docs/REMAINING-WORK.md` by **exactly 2 lines** (`git show --stat`:
   `1 insertion(+), 1 deletion(-)`) — B5's line, per its own commit message. That is
   the *correct* behaviour for a commit whose subject is code: the author fixed the
   code and patched the one line they were told about. The D3 cultural-case fix
   closed D3 and touched `REMAINING-WORK.md` by **zero lines**. A commit cannot be
   expected to re-audit a document; that is the job of a check. There was no check.

3. **Even the check that exists is host-only.** `.github/workflows/ci.yml` runs
   `ctest --test-dir build --output-on-failure` in the `linux` job (line 16) and
   `docs-lint` **is** among the registered tests it would run — but
   `grep -n "docs-lint\|wiki-lint\|pyright" .github/workflows/ci.yml` returns **0 lines**,
   so no step invokes it directly and, more to the point, the `linux` job has no
   `--build-dir` argument of its own to hand it: it is configured, so the count check is
   live. The accurate statement is narrower than the 2026-10-09 note implied:
   **`docs-lint` runs in CI as part of the suite; it does not run as a named gate, and
   nothing outside the suite would notice if it were removed from the registration.**

The synthesis: **a status board's claims are the hardest shape to lint, so they were the
shape left unchecked.** Every other class of claim in the tree — counts, commands,
abbreviations, wiki sources — got a mechanical guard because those are easy. "Item X is
still open" is a claim about a *relationship* between the document and the tree, and it
needs the document to state the relationship in a machine-readable form. It did not, so
nothing could check it.

### 7.3 The guard, and what it deliberately does not do

`tools/docs-lint.py` check 6 (`check_remaining_work`) now parses the assertion block below
and verifies **every row against the tree** on each run, inside ctest test `docs-lint`.
The grammar is at the check itself; in short, each row is
`id | kind | target | expected | note` with `kind` in
{`path-absent`, `path-present`, `ctest`, `grep`, `pyright`}.

The design properties that make it a guard rather than a comment:

- **It fails closed.** A missing assertion block, a malformed row, an unknown `kind`, or
  an unrecognised `expected` value are all **errors**, not skips. A row the check cannot
  read is a row it cannot verify, so it reports rather than ignoring.
- **It was observed rejecting.** Before this commit, running it against the *previous*
  version of the board produced `rc=1` with exactly the missing-block error, because the
  board carried no assertions at all. Planting a deliberately stale row
  (`path-absent frontends/fcitx5/test/testmodeswitch.cpp | absent` — false since
  the mode-switch test was added) produces the per-row error naming the file, the
  line and the evidence. A guard never seen rejecting anything is a comment.
- **It refuses a `run` kind.** "Is the R8 shrinker path exercised" and "has a BT-keyboard
  device been signed off" are facts about a *signed release build on an emulator* and a
  *physical device*, not about the committed tree. Asserting them would fire on every
  clean checkout on a machine without a device — the exact false-positive class this
  file's header says gets a guard switched off. Those items are listed in §3 as
  **environment-bound** with the missing measurement named, and are excluded from the
  assertion table by construction.
  (This bullet previously offered "does the mic stay clear of the nav bar" as its
  second example. That was wrong twice over — W4 closed it in the same change that
  added the R8 rules, and the table below carries a `W4-CLOSED` row asserting it,
  so the prose contradicted its own guard.)
- **Maintainer-gated items are `decision-blocked` and excluded.** B1–B9 in §1 are rulings,
  not tree facts, and AGENTS.md forbids an agent from settling them. They therefore carry
  no assertion row and the table says so explicitly, so nobody later mistakes their
  absence from the table for an oversight.
- **`pyright` fails on absence.** If `pyright` is not installed, the row reports
  "pyright did not run" rather than reading a missing tool as a clean tree.

### 7.4 What this guard cannot catch, stated plainly

- **A closed item whose evidence is a measurement rather than a path.** "Is the wrong-direction
  switch metric within tolerance" cannot be asserted here; it is B2's pass line, which only
  the maintainer can set.
- **A claim in prose outside the assertion block.** The block governs the still-open items.
  §5's correction log and §6's superseded proposals are history and are intentionally not
  asserted — a retracted claim stays, marked, and asserting it would be wrong in both
  directions.
- **Two items with the same label.** §2 and §3 both use M1–M5. The guard does not detect
  the collision; renaming is a maintainer decision (see §2).
- ~~**`data-selfcheck` in a fresh worktree.**~~ **Fixed 2026-10-13 (issue #46).**
  It used to fail on the gitignored 국립국어원 dump rather than skipping (§0).
  It now skips (rc 77) when no `*.xml` exists under `data-orig/krdict/`, the
  self-contained guard is ctest `krdict-fixture`, and the dump-dependent run is
  the maintainer-only `tools/selfcheck-krdict.sh`. The earlier judgement —
  "changing a test's skip/fail behaviour on a missing input is a maintainer
  call" — held at the time it was written; the maintainer ruled the change in
  (issue #46), so this line records the ruling's implementation, not a new one.

### 7.5 Assertions — parsed and verified by `tools/docs-lint.py` check 6

<!-- docs-lint: remaining-work -->
# id | kind | target | expected | note
# ----------------------------------------------------------------  still open
# ----------------------------------------------------------------  closed this pass
DESIGN-REVIEW-TRACKED | path-present | docs/ANDROID-IME-REVIEW.md | present | review committed on task/android-design-review (e389970); issue #21 tracks it for the maintainer to weigh — no verdict recorded
DESIGN-REVIEW-F5 | grep | frontends/android/app/src/main/java/org/hunmin/ime/LayoutPicker.kt:buildDock | absent | issue #21 verification: grep -n 'buildDock' frontends/android/app/src/main/java/org/hunmin/ime/LayoutPicker.kt shows no call site (F5 evidence)
DESIGN-REVIEW-F1-F2 | grep | frontends/android/app/src/main/java/org/hunmin/ime/OverlayActivity.kt:Color.parseColor | absent | issue #21 verification: grep Color.parseColor OverlayActivity.kt returns nothing (F1, F2 fills)
DESIGN-REVIEW-F3-F10 | grep | frontends/android/app/src/main/java/org/hunmin/ime/OverlayActivity.kt:dialogWidth | present | issue #21 fill: dialog sized off window size class, not physical pixels (F10); HUD day/night pair shared via hunmin_hud_* tokens (F3)
# ----------------------------------------------------------------  decision-blocked
# B1..B9 are maintainer rulings, not tree facts. AGENTS.md: "네 멋대로 기각하지마."
# They carry no assertion row, by construction, and their absence from this table
# is deliberate rather than an oversight.
W6-CLOSED | grep | frontends/android/app/src/test/java/org/hunmin/ime/VoiceSessionTest.kt:decidePermission | present | tier E1 tests the voice permission decision logic (tap effect)
W6-CLOSED | grep | tools/android-e2e.sh:input tap.*MICX.*MICY | present | tier E2 taps the mic button and verifies keyboard survival
W6-CLOSED | grep | tests/manual-checklists/tier-e3-bt-keyboard.md:On emulator \(tier E2\) | present | manual checklist now notes E2 coverage
N2-CLOSED | grep | .github/workflows/ci.yml:nightly | present | nightly fuzz job added 2026-10-10
N2-CLOSED | grep | .github/workflows/ci.yml:fuzz | present | fuzz job with LeakSanitizer added 2026-10-10
N5-CLOSED | path-present | frontends/android/app/src/androidTest | present | Espresso assertions added 2026-10-10 — **but they do not compile**; see the assertion below and issue #12
KEYBINDINGS | grep | frontends/android/app/src/main/java/org/hunmin/ime/KeyBinding.kt:object KeyBinding | present | configurable key bindings, voice stage 7, 2026-10-11
KEYBINDINGS | grep | tools/android-e2e.sh:the captured chord was persisted | present | tier E2 drives the settings window on the emulator
N6-CLOSED | path-present | CONTRIBUTING.md | present | external-contributor docs added 2026-10-10
N6-CLOSED | path-present | debian/control | present | Debian packaging exists
N6-CLOSED | path-present | debian/hunmin.postinst | present | Debian postinst exists
SETTINGS-DIALOG-CLOSED | path-present | frontends/android/app/src/main/java/org/hunmin/ime/OverlayActivity.kt | present | settings dialog via OverlayActivity
HANJA-DIALOG-CLOSED | path-present | frontends/android/app/src/main/java/org/hunmin/ime/OverlayActivity.kt | present | hanja dialog via OverlayActivity
SETTINGS-GEAR-CLOSED | grep | frontends/android/app/src/main/java/org/hunmin/ime/LayoutPicker.kt:settingsButton | present | settings gear button in layout strip
SPACE-COMPOSITION-CLOSED | grep | frontends/android/app/src/main/java/org/hunmin/ime/HunminInputMethodService.kt:KEYCODE_SPACE | present | space during composition flushes + forwards
TOGGLE-FLUSH-CLOSED | grep | frontends/android/app/src/main/java/org/hunmin/ime/HunminInputMethodService.kt:flush | present | hangul/english toggle flushes composition first
OVERLAY-LIFECYCLE-CLOSED | grep | frontends/android/app/src/main/java/org/hunmin/ime/OverlayActivity.kt:onPause | present | overlay activity lifecycle fixed
ALT-KEY-218-CLOSED | grep | frontends/android/app/src/main/java/org/hunmin/ime/KeyBinding.kt:218 | present | the Samsung 한/영 keycode is handled as a 한/영 platform alternate; moved out of HunminInputMethodService.kt on 2026-10-11 when the four triggers became KeyAction (issue #12), which is what made this assertion fail and point at the move
SETTINGS-CRASH-CLOSED | grep | frontends/android/app/src/main/java/org/hunmin/ime/OverlayActivity.kt:AlertDialog.Builder | present | settings dialog crash fixed (default theme)
R8-SHRINKER | path-present | frontends/android/app/proguard-rules.pro | present | the R8 rules are written
R8-SHRINKER | grep | frontends/android/app/build.gradle.kts:isMinifyEnabled = true | present | shrinker enabled
R8-SHRINKER | grep | .github/workflows/ci.yml:assembleRelease | present | the release build RUNS in CI now (job android-release, added 2026-10-13 for issue #8): assembleRelease + both ABIs + an artefact upload. The assertion changed from absent to present because the tree changed; the job is gated on vars.HUNMIN_BUILD_RELEASE='true' on master and signs with a generated throwaway keystore (B8 unresolved, issue #1), so it proves the release BUILD, not a release SIGNATURE. tools/build-release.sh is the same sequence locally. Still not asserted: the release APK is never installed or typed into in CI -- android-e2e.sh drives the debug APK by design, and no job does an adb install of the release build. That gap is a measurement nobody has taken and it is recorded here rather than closed
W5-CLOSED | grep | tools/android-e2e.sh:pm revoke | present | W5 closed 2026-10-10: E2 drives both permission branches
W5-CLOSED | grep | tools/android-e2e.sh:perm_granted | present | the granted/denied state is read from the platform, not from a log line
W5-CLOSED | grep | tools/android-e2e.sh:still types after a denied mic tap | present | the property that matters: a voice failure must not reach the key path
M1-CLOSED | path-present | frontends/fcitx5/test/testmodeswitch.cpp | present | the measurement-hygiene pass; was listed open
M1-CLOSED | ctest | engine-d1-modeswitch | registered | registered by the measurement-hygiene pass
M2-CLOSED | grep | tools/doctor.py:krdict | present | the measurement-hygiene pass; doctor.py now globs data/krdict
M3-CLOSED | grep | tools/wiki-lint.py:check_sources_exist | present | the measurement-hygiene pass added it
M4-CLOSED | grep | tools/android-e2e.sh:HUNMIN_AVD:-hunmin-e2e | present | the measurement-hygiene pass; the top-level local is gone
M5-CLOSED | pyright | tools/ | clean | the measurement-hygiene pass; 29 errors -> 0
W1-CLOSED | path-present | tests/conformance/android/basic-syllables-case14.trace.gold | present | the Android tier-C corpus; 14 pairs now
W1-CLOSED | path-present | tools/android-trace-emit.sh | present | the Android tier-C corpus added the runner
TC-SHIFTARROW | ctest | engine-android-conformance-shift-arrow-selection | registered | tier-C gate for issue #39, and it CANNOT evaluate Shift+Arrow. Read the next row before believing it.
TC-SHIFTARROW-BLIND | grep | frontends/android/app/src/test/java/org/hunmin/ime/TraceEmit.kt:nativeProcess | present | the Android tier-C corpus bypasses the frontend, so TC-SHIFTARROW is not evidence about issue #39. Measured 2026-10-12: TraceEmit builds a bare hunmin_ic and calls nativeProcess(ic, keyCode), so HunminKeys' keycode mapping, the CapsLock option handoff and the whole metaState path never execute, and `consumed` is the engine's answer rather than the adapter's decision — which is precisely the thing #39 is about. It also drops `mods` outright, so events with mods 0 and mods 1 are the same event. This contradicts trace-spec.md, which requires `mods` to be a platform-native bitfield precisely so "an adapter that cannot see CapsLock cannot be tested for it". Consequence: all 15 Android conformance traces check the engine, not the Android adapter, and the row above should not be cited as Shift+Arrow coverage. NOT fixed here: replaying through HunminEngine.onKey needs android.view.inputmethod.InputConnection loadable in tools/android-trace-emit.sh, whose printed classpath has no android.jar (AGP injects the mockable jar onto the Gradle test task, not onto debugUnitTestRuntimeClasspath), and would require regenerating all 15 goldens for review. Scope and sequencing are the maintainer's call
ARROW-NO-TEXT | ctest | engine-android-e1 | ok | the Shift+Arrow behaviour that CAN be evaluated today, at tier E1, where onKey takes a real metaState: an arrow keycode resolves to no engineChar at all, so it can neither reach the jamo pipeline nor commit text of its own — it may only flush the syllable that was already in flight, which is the documented unmapped-key path. Two tier E1 tests, both verified to fail against a mutation that makes an arrow resolve to a jamo: "an arrow key emits no text of its own and only flushes what was composing" and "an arrow keycode is not a layout key". What is deliberately NOT asserted is `consumed` for Shift+Arrow, under either ruling: whether the IME consumes the chord or passes it through for the editor to extend the selection is the maintainer's decision, and a test asserting either value would pre-empt it
FOCUS-EMPTYPREEDIT | ctest | engine-android-e1 | ok | the last character of a non-empty field is not eaten when the IME takes focus: HunminEngine.updatePreedit skips setComposingText when the preedit is empty and no composition of ours is on screen. Provenance: reported by the maintainer against R5KL808WBPN (2026-10-12) as "the last character disappears the moment a text field with text is selected"; the cause was then read off the code path, not measured — onStartInput builds a fresh engine and calls updatePreedit on it, and the unconditional setComposingText("", 1) is the editor being told to drop the character before the cursor. Verified by the maintainer on that device after the change; the last character of a non-empty field is not eaten when the IME takes focus: HunminEngine.updatePreedit skips setComposingText when the preedit is empty and no composition of ours is on screen. Provenance: reported by the maintainer against R5KL808WBPN (2026-10-12) as "the last character disappears the moment a text field with text is selected"; the cause was then read off the code path, not measured — onStartInput builds a fresh engine and calls updatePreedit on it, and the unconditional setComposingText("", 1) is the editor being told to drop the character before the cursor. Verified by the maintainer on that device after the change; the fix is held by two tier E1 tests, "a fresh engine does not tell the client to compose nothing" (red without the guard) and "a composition IS cleared afterwards" (the other half — the guard must not strand a composition on screen). Note what this did NOT establish: it says nothing about Shift+Arrow, which the maintainer's original report also mentioned and which remains unmeasured (see TC-SHIFTARROW)
W2-CLOSED | path-present | frontends/android/app/proguard-rules.pro | present | the Android R8/mic/inset change
W4-CLOSED | grep | frontends/android/app/src/main/java/org/hunmin/ime/LayoutPicker.kt:private var micButton | present | the mic is a field on LayoutPicker and is never re-created per state change: a ToolbarState update calls bind(), which repaints the views the current style already has, and only a style change rebuilds them. The field became nullable on 2026-10-11 (issue #12) because rebuild() clears every reference first; that is what the phone crash was — a view still parented to the detached row |
DOCK-PAINT-CLOSED | path-absent | frontends/android/app/src/main/java/org/hunmin/ime/DockStyle.kt | absent | the dock builder (with its 2026-10-11 micKey fix, SM-S938N) is removed with the whole unreachable DockToolbar (F5, maintainer ruling 2026-10-12, issue #33). The flat strip keeps the mic-paint lesson: its micButton is a field, never re-created per state change |
VOICE-E4-CLOSED | path-present | tools/android-voice-check.sh | present | the E4 voice check, a manual tier: the script gates our half of the voice path (key found, tap reaches it, LISTENING, the mic PAINTED while listening, return to idle, result committed, nothing died) and a person judges intelligibility. Recognition accuracy is not gated and cannot be — the transcript is the platform recogniser's and recorded audio cannot be replayed into it. Checklist: tests/manual-checklists/tier-e4-voice.md |
VOICE-README | grep | README.md:Known limitation | present | the release-point statement issue #11 asks for: the unmeasured voice-recognition-quality limitation is stated in README §Install, not only in docs/MILESTONES.md §0.1. The measurement itself stays environment-bound (needs hardware; see §3) |
VERSION-CONVENTION | grep | README.md:versionName. is .0\.1\.0-p5 | present | issue #9 ruled 2026-10-12: the release is 0.1.0 and the -pN suffix is the Android build phase (P5 the IME, P5.5 the keymaps APK); README §Install says which string is the release and which is the phase, docs/MILESTONES.md §0.1 records the convention. Ruled value also visible at frontends/android/app/build.gradle.kts:53 (unchanged — the ruling was to keep it). NOT closed by the ruling: versionCode = 1 was never incremented, no tag exists |
N1-CLOSED | path-present | tools/kcm_export.py | present | the keymaps exporter; was listed open as "does not exist"
N3-CLOSED | grep | libhunmin/src/hunmin_internal.h:HK_SEQ | present | the seq symbol tier
N4-CLOSED | path-present | debian/hunmin.postinst | present | the deb postinst; the real filename is hunmin.postinst
CI-DEB-JOB | grep | .github/workflows/ci.yml:dpkg-buildpackage | present | issue #8: CI builds the .deb now (job deb, 2026-10-13). The suite runs inside the package build (debian/rules override_dh_auto_test), not skipped -- the issue's verification requires that. Expected failure mode on a fresh checkout: data-selfcheck (§0), plus the tier-C Android rows when no Android build tree exists. **Updated 2026-10-13 (issue #46):** data-selfcheck now reports Skipped without the gitignored krdict dump, so the tier-C Android rows are the only expected non-pass on a fresh checkout
CI-DEB-JOB | grep | .github/workflows/ci.yml:upload-artifact | present | the .deb is uploaded as a run artefact, so CI is a second place the artefact can be produced -- the ruling issue #8 asks for is whether that is REQUIRED for 0.1 or sufficient alongside a local build, and this row asserts only that the job exists
RELEASE-SCRIPT | path-present | tools/build-release.sh | present | issue #8: the 0.1 artefacts (deb + release APK) are buildable by anyone with the same sequence; it existed only as one local run before. The APK signs with a generated throwaway keystore (HUNMIN_RELEASE_STORE) -- B8's ruling (issue #1) is what would make it release-grade
CI-WORKFLOW-VALID | grep | .github/workflows/ci.yml:\$\{\{ failure\(\) \}\} | present | issue #8 exposed this: an unquoted `if: failure()` parsed as YAML boolean True made GitHub reject the workflow with "Invalid workflow file: ci.yml#L1", so all 38 CI runs since the initial public commit (fbb0c43, 2026-10-10) failed before any job ran -- no CI job had ever executed. Both occurrences are now quoted. A workflow whose file is rejected reports no jobs, which is why the failure read as a file issue and not a step failure
# ----------------------------------------------------------------  decision-blocked
# B1..B9 are maintainer rulings, not tree facts. AGENTS.md: "네 멋대로 기각하지마."
# They carry no assertion row, by construction, and their absence from this table
# is deliberate rather than an oversight.
# §2 M1..M5 need B1's chosen source. §3's R8-shrinker end-to-end needs a signed
# release build and an emulator; the E3 gate needs a physical device. Both are
# environment-bound, not decision-blocked. (This line previously said W5's runtime
# grant needed "an emulator, which this host does not have". False twice: tier E2
# drove both permission branches on this host's AVD, and the AVD is what
# tools/android-e2e.sh already uses.)
DATASELFCHECK-46 | grep | tools/selfcheck.py:KR_ABSENT | present | issue #46: data-selfcheck skips (rc 77) when the gitignored 368 MB krdict dump is absent instead of failing, so a fresh worktree and CI are green. Two other rows carry the rest: krdict-fixture is the self-contained determinism guard (committed fixture under tests/fixtures/krdict/, expected output diffed byte-identically), and the dump-dependent full run is tools/selfcheck-krdict.sh, which is deliberately NOT registered with ctest
DATASELFCHECK-46 | grep | CMakeLists.txt:set_tests_properties\(data-selfcheck PROPERTIES | present | issue #46: data-selfcheck carries SKIP_RETURN_CODE 77, so the skip is reported as Skipped by ctest rather than silently passed
DATASELFCHECK-46 | path-present | tests/fixtures/krdict/01.xml | present | issue #46: the committed synthetic fixture exercises every krdict2data.py branch (Hanja/no-origin skipped, `/`-separated readings -> hanja_alt, Hanja-free readings dropped, pos/level/category, meanings absent, (ko,hanja) dedup counted in meta) with no 368 MB dump present
DATASELFCHECK-46 | path-present | tools/selfcheck-krdict.sh | present | issue #46: the maintainer-only full-dump determinism run; not a ctest test, so the ctest surface stays self-contained
DATASELFCHECK-46 | path-absent | CMakeLists.txt:selfcheck-krdict | absent | issue #46: selfcheck-krdict.sh must NOT appear in the ctest registration — a dump-dependent run cannot live on the ctest surface
F9-CANDIDATE-ABORT | grep | frontends/fcitx5/src/engine.cpp:setPageSize\(candidateCount_\) | absent | issue #45, ruled by the maintainer 2026-10-13: cap the F9 page at fcitx5's label ceiling rather than keep the whole list on one page. The uncapped call is gone -- `kMaxCandidatePageSize` replaced it. The defect it caused: any syllable with 11 or more candidates ABORTED fcitx5 (SIGABRT, exit 134) with `CommonCandidateList: invalid label idx`, because classicui reads one label per candidate on the page (fcitx5-5.1.19 inputwindow.cpp:392-405) and CommonCandidateList holds only 10 labels unless setLabels() is called (candidatelist.cpp:28-41, 556-565). Measured: 10 candidates survive, 11 abort, 67 abort, and 67 at the default page size 5 do not -- the page size was the trigger and the vertical hint a bystander. NOT local: the installed 0.1.0 addon against /usr/share/hunmin aborted identically. Measurements: wiki/raw/measurements/2026-10-13-f9-candidate-window-abort.md
F9-CANDIDATE-ABORT | grep | frontends/fcitx5/src/engine.cpp:constexpr int kMaxCandidatePageSize = 10 | present | issue #45: the ceiling is fcitx5's, not ours -- it is `regularLabelSize`, the label vector fillLabels() pads to. It is a named constant rather than a bare 10 so that a fcitx5 upgrade that moves the ceiling has one place to look, and so the number carries its provenance
F9-CANDIDATE-ABORT | grep | frontends/fcitx5/test/testhunminui.cpp:common->pageSize\(\) <= 10 | present | issue #45: tier-D2 is the tree fact that keeps the abort out. Measured red with the cap reverted ("page size is 67, above fcitx5's label ceiling of 10"), green with it. It asserts `pageSize == min(totalSize, 10)` as well, so the cap cannot silently shrink a list that already fits. Stated plainly because it is the subtle part: this tier loads NO UI addon, so it would not have crashed even without the cap -- it goes green either way on that, and it is the page-size assertion that fails
F9-CANDIDATE-ABORT | path-present | tools/repro-f9-candidates.sh | present | issue #45: the end-to-end check -- full fcitx5 under Xvfb with classicui ENABLED, which no ctest tier is (engine-d2-ui has no UI addon; tests/e2e/run-d3.sh:391 lists DisabledAddons=classicui). Verified after the fix: 낙(11), 간(67), 녹(10) and 푸(1) all open the window and fcitx5 survives. It also settled what the cap costs: Page_Down reaches page 2 and page 3 and the cursor lands on the first candidate of each, Up wraps to the last, Escape closes -- so no candidate became unreachable
F9-CANDIDATE-ABORT | grep | tests/e2e/run-d3.sh:DisabledAddons=classicui | present | issue #45, NOT closed by the cap: no ctest tier draws the F9 window, so the abort cannot regress through `ctest` alone. `tools/repro-f9-candidates.sh` covers it and nothing runs it in CI. Expected `absent` when a tier does
F9-WINDOW-TIER | grep | .github/workflows/ci.yml:repro-f9-candidates | absent | issue #51, item 1, still open: the #45 abort has no CI coverage. This row is written against the cheapest disposition (option B in the issue: wire tools/repro-f9-candidates.sh into CI). If a D3 variant with classicui enabled is taken instead (option A), this row is replaced by a `ctest` row naming the new label, in the same commit -- the assertion grammar cannot express "a tier that does not exist yet" without naming it, and a stale grep that nothing flips is a guard that never fires
CI-WINDOWS-OUT-OF-SCOPE | grep | .github/workflows/ci.yml:issue #51 | absent | issue #51, item 2, still open: the maintainer ruled "for windows, it is not scope of this version. so in the issue we will disable windows test." The `windows` job (.github/workflows/ci.yml:369-374) builds only the core library -- the addon is already skipped there with "Fcitx5Core not found: fcitx5 addon disabled" -- and has been red on master since at least 0d046bc with `cl : D8021 invalid numeric argument '/Wextra'`, because CMakeLists.txt:10 passes GCC flags to MSVC unconditionally. The row is an issue reference, not a `windows-latest` grep, and deliberately so: the assertion grammar can only mark an item open when the TREE answers absent, and `windows-latest` is present today, so a grep on it would have to claim `present` and read as "this is done". This way the row fires when the job is disabled and cites the ruling, whichever disposition is taken. What it does NOT check: that disabling the job is what changed (a comment citing the issue would pass it). NOT in scope for this item: guarding the flag with if(NOT MSVC), which would turn the job green and is a different decision from the one that was ruled
<!-- end docs-lint -->
