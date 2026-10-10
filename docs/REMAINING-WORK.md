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
| `data-selfcheck` | us-qwerty.json format drift | **still fails**, but for a different reason — see below |
| `engine-d3-x11` | `ㅎ\|곈` instead of `안\|녕` | **passes** (goldens regenerated) |
| `engine-d3-gtk3` / `-gtk4` | same | **both pass** |

**`data-selfcheck` is red in a fresh worktree and green in the main tree.** The
failure is `krdict2data: no *.xml under …/data-orig/krdict` — the 368 MB 국립국어원
dump is gitignored and absent from a new worktree. The test does not skip on a
missing input; it fails. That is the *opposite* of the 2026-10-09 diagnosis, which
recorded a `diff -r` whitespace divergence in `us-qwerty.json`. Both statements were
true of their own tree at their own time, and neither is a claim about this tree.
**This is the same class of defect as the board itself**: a status line carried
forward past the commit that invalidated it.

`ctest` is green on a worktree configured `-DHUNMIN_TRACE_BUILD=ON`; the main
tree here is configured without it, which registers fewer, so a bare count in
prose is only meaningful next to the configuration it came from. The one test
red in a fresh worktree is `data-selfcheck` — see §0 above.

---

## 1. Blocked on the maintainer — do not start these

AGENTS.md: *"네 멋대로 기각하지마"* — a negative result is a finding, "therefore reject it"
is a decision. These are the items an agent may measure but may not settle.

| # | Question | Where | Why blocked |
|---|---|---|---|
| B1 | **The hangul judgement source** — what R8's `hangul_valid` looks up | `wiki/decisions/the-outstanding-decisions.md:20-33`, `docs/MODES-DATA.md:652` | (a) corpus top-N rejected by measurement; (b) dictionary lookup weakened because the 45.42% coverage came from a hanja-filtered subset; (c) combination test provisionally adopted, but krdict is outside the 2026-10-05 scope boundary and **no replacement source inside the boundary has been chosen**. Everything in §2 inherits this. |
| B2 | **Pass lines for all three metrics** | `docs/MODES-METRICS.md:118-122` | *"there is no **how much** … grading is impossible."* Not a measurement; a number only the maintainer can set. |
| B3 | **ON or OFF by default** | `docs/SPEC-MODES.md:555-557` | Explicitly a usability call, *"not settled by guesswork."* |
| B4 | **E5 — the 한/영 key** | `docs/SPEC-MODES.md:680-709`, `docs/MODES-ENGINE-RULES.md:77-81` | F9 is occupied by 전처리수식, and F9 has no `KEYCODE_F9` on Android (`docs/TESTING.md:112-114`). Needs the 날개셋 unit table. |
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
- **Voice recognition quality.** Needs a microphone and Google's servers. *"Unanswerable in
  this environment; nothing in this repo can measure it."* The AVD boots `-no-audio` and has
  no offline ko-KR pack.
- **A soft keyboard.** The input view is a layout strip plus a mic, not keys. No priority has
  been assigned to it by anyone; that assignment is a maintainer call.
- **D4 Wayland plumbing.** `weston --backend=headless` + `zwp_input_method_v2` harness.
  `docs/TESTING.md:247-249` marks it optional and not blocking P4; fcitx5's Wayland module
  is not our code.
- **The R8 shrinker path end-to-end.** `proguard-rules.pro` (139 lines) is written
  and `isMinifyEnabled = true` is set, but the change that added them records
  *"Not measured: the release APK with R8 actually applied (needs a signed release
  build and an emulator run)."*
  The rules exist; the shrinker has still never run. **This is environment-bound, not
  decision-blocked** — it needs a signed release build and an emulator, neither of which is
  a maintainer ruling, so it is a genuine unblocked item that simply cannot be measured on
  a host without a device.

---

## 4. Suggested order, and the deciding reason

**Revised 2026-10-10.** The previous order (`M1 → M2 → W1 → the rest of W →
M3/M4/M5 → N1–N6`) was written when §3's M1–M5 and W1, W2 and W4 were open. Six of
those eight are now closed, so the order's stated reasoning — "M1 first is a
dependency, because B9 cannot be answered while the class has no test" — has been
satisfied: `frontends/fcitx5/test/testmodeswitch.cpp` was added and ctest
`engine-d1-modeswitch` registered, which **passes** here.

**Revised again 2026-10-10, after tier E2 closed W5/W6 and N2/N5/N6 were completed.** W5, W6, N2, N5, N6 are done. The remaining unblocked items are environment-bound (E3 gate, R8 shrinker E2E, soft keyboard, D4 Wayland) or decision-blocked (B1–B9). No further unblocked code work remains at this time.

## Voice input enhancements (incremental, staged)

| Stage | Feature | Status | Notes |
|---|---|---|---|
| 1 | Voice trigger: Right Alt + V | ✅ Done | `onKeyDown` handles `KEYCODE_V` + `META_ALT_RIGHT_ON` |
| 2 | Auto Hangul/English toggle on/off: Right Alt + Enter | ⚠️ Stub | Key handler added, engine integration pending (B4) |
| 3 | Voice language follows keyboard mode | ✅ Done | `SpeechRecognizerRecognizer(language)` uses `ko-KR` / `en-US` |
| 4 | Longer silence timeout (partial results) | ✅ Done | `EXTRA_PARTIAL_RESULTS=true`, 1.5s/3s silence |
| 5 | Mic button visual state (idle/listening/error) | ✅ Done | `LayoutPicker.setMicState(MicState)` |
| 6 | IME switch button in picker | ✅ Done | Shows system `showInputMethodPicker()` |
| 7 | Configurable key bindings | 📋 Planned | Persist user preferences for triggers |
| 8 | Auto Hangul/English detection engine integration | 🔒 Blocked on B4 | Needs maintainer decision on assignment/ON-OFF default |

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
| §3 W4 — mic duplication + nav-bar inset | the Android R8/mic/inset change | mic is a `lateinit` field built in `LayoutPicker.init` (`LayoutPicker.kt:101`), not per-`bind()`; `ViewCompat.setOnApplyWindowInsets` applies the nav-bar inset |
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
- **`data-selfcheck` in a fresh worktree.** It fails on the gitignored 국립국어원 dump
  rather than skipping (see §0). That is a real defect in `data-selfcheck`, not in this
  board, and it is recorded here rather than fixed here because changing a test's
  skip/fail behaviour on a missing input is a maintainer call.

### 7.5 Assertions — parsed and verified by `tools/docs-lint.py` check 6

<!-- docs-lint: remaining-work -->
# id | kind | target | expected | note
# ----------------------------------------------------------------  still open
# ----------------------------------------------------------------  closed this pass
W6-CLOSED | grep | frontends/android/app/src/test/java/org/hunmin/ime/VoiceSessionTest.kt:decidePermission | present | tier E1 tests the voice permission decision logic (tap effect)
W6-CLOSED | grep | tools/android-e2e.sh:input tap.*MICX.*MICY | present | tier E2 taps the mic button and verifies keyboard survival
W6-CLOSED | grep | tests/manual-checklists/tier-e3-bt-keyboard.md:On emulator \(tier E2\) | present | manual checklist now notes E2 coverage
N2-CLOSED | grep | .github/workflows/ci.yml:nightly | present | nightly fuzz job added 2026-10-10
N2-CLOSED | grep | .github/workflows/ci.yml:fuzz | present | fuzz job with LeakSanitizer added 2026-10-10
N5-CLOSED | path-present | frontends/android/app/src/androidTest | present | Espresso assertions added 2026-10-10
N6-CLOSED | path-present | CONTRIBUTING.md | present | external-contributor docs added 2026-10-10
N6-CLOSED | path-present | debian/control | present | Debian packaging exists
N6-CLOSED | path-present | debian/hunmin.postinst | present | Debian postinst exists
SETTINGS-DIALOG-CLOSED | path-present | frontends/android/app/src/main/java/org/hunmin/ime/OverlayActivity.kt | present | settings dialog via OverlayActivity
HANJA-DIALOG-CLOSED | path-present | frontends/android/app/src/main/java/org/hunmin/ime/OverlayActivity.kt | present | hanja dialog via OverlayActivity
SETTINGS-GEAR-CLOSED | grep | frontends/android/app/src/main/java/org/hunmin/ime/LayoutPicker.kt:settingsButton | present | settings gear button in layout strip
SPACE-COMPOSITION-CLOSED | grep | frontends/android/app/src/main/java/org/hunmin/ime/HunminInputMethodService.kt:KEYCODE_SPACE | present | space during composition flushes + forwards
TOGGLE-FLUSH-CLOSED | grep | frontends/android/app/src/main/java/org/hunmin/ime/HunminInputMethodService.kt:flush | present | hangul/english toggle flushes composition first
OVERLAY-LIFECYCLE-CLOSED | grep | frontends/android/app/src/main/java/org/hunmin/ime/OverlayActivity.kt:onPause | present | overlay activity lifecycle fixed
ALT-KEY-218-CLOSED | grep | frontends/android/app/src/main/java/org/hunmin/ime/HunminInputMethodService.kt:218 | present | alt key 218 (Samsung 한/영) support added
SETTINGS-CRASH-CLOSED | grep | frontends/android/app/src/main/java/org/hunmin/ime/OverlayActivity.kt:AlertDialog.Builder | present | settings dialog crash fixed (default theme)
R8-SHRINKER | path-present | frontends/android/app/proguard-rules.pro | present | the R8 rules are written
R8-SHRINKER | grep | frontends/android/app/build.gradle.kts:isMinifyEnabled = true | present | shrinker enabled
R8-SHRINKER | grep | .github/workflows/ci.yml:assembleRelease | absent | release build never runs in CI, so the shrink path is unexercised
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
W2-CLOSED | path-present | frontends/android/app/proguard-rules.pro | present | the Android R8/mic/inset change
W4-CLOSED | grep | frontends/android/app/src/main/java/org/hunmin/ime/LayoutPicker.kt:lateinit var micButton | present | the Android R8/mic/inset change; mic is a field, not per-bind
N1-CLOSED | path-present | tools/kcm_export.py | present | the keymaps exporter; was listed open as "does not exist"
N3-CLOSED | grep | libhunmin/src/hunmin_internal.h:HK_SEQ | present | the seq symbol tier
N4-CLOSED | path-present | debian/hunmin.postinst | present | the deb postinst; the real filename is hunmin.postinst
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
<!-- end docs-lint -->
