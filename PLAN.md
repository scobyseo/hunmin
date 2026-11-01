# hunmin — portable Korean input engine (PLAN)

Status: **open.** Approved 2026-09-26; reopened 2026-10-05. Engine core license:
LGPL-2.1+. Working name `hunmin`.

The date on the original approval is not the date of this plan's content. A
requirement stated on 2026-10-03 — automatic hangul/latin switching, no IM
change — was written up thoroughly in `docs/SPEC-MODES.md` and **never added
here**, so this file read as complete for a requirement that did not exist when
it was approved. It is now **P8**, near the bottom because it is unstarted, not
because it is optional.
(libhunmin / fcitx5-hunmin / hunmin-keymaps / hunmin-android); rename before first public release.

## Goal

An AI-maintainable Hangul input engine whose full behavior is defined by **data files +
golden tests**, fronted by thin adapters on Linux (fcitx5 addon), Android (own IME),
and later Windows (TSF TIP). 신세벌식 P2 (pat.im/1136) is the flagship layout; all
libhangul-era layouts (2, 2y, 32, 39, 3f, 3s, 3y, ro, ahn) ship as data so others can use them.

**And it decides hangul vs latin for the user** (P8). Typing English while in
Hangul mode should switch to English, and typing Hangul while in English mode
should switch back — **without changing IM**, because the IM change is the part
that costs the user. This is a requirement, not a proposal; the requirements are
R1–R13 in `docs/SPEC-MODES.md` and the decision belongs in the engine so every
frontend gets it.

## Why not the existing options (evidence trail)

- **libhangul upstream is frozen**: maintainer statements 2024–2026 (issue #31, PR #94):
  no new core keyboards; external keyboards are *data-only*; new engine *logic* is not
  accepted. Its engine assigns jamo slots by Unicode category (`hangul_buffer_set`,
  hangulinputcontext.c) — P2's state-dependent dual-slot keys are inexpressible without
  engine change.
- **3beol fork** (+9,955 engine LOC, 5 copy-pasted ~280-line automatons, layout state in
  core structs, mid-enum ABI break, zero tests, dead since 2019) is the cautionary tale:
  architecture pressure → duplication → rot.
- **kime**: GPL-3, no fcitx5/Windows/Android frontends in current tree, P2 lacks 옛한글 and
  symbol tiers. Reference for verified *semantics* only (clean-room: transcribe behavior
  facts + test expectations as data, never copy code).
- **3beol-style libhangul PPAs break on Ubuntu upgrades** (apt overwrites patched system
  lib). A separate addon + own library is conflict-free by construction.
- **Android KCM** ((keycode+mods)→exactly one Unicode char, AOSP-verified) cannot express
  P2's ambiguity at all → engine must own composition. Fixed-slot layouts (3-90, 3-2) are
  KCM-expressible → side product `hunmin-keymaps` (see P5.5).

## Architecture

```
~/work/hunmin/
  libhunmin/            C ABI, no platform deps. hunmin_ic_*/layout_list/press/commit_text/suggest
    data/layouts/*.json one file per layout; versioned schema; discovered at runtime
    data/compose/*.json vowel/batchim/옛한글 combination rule tables (per layout)
    data/hanja/         dictionary + LICENSE.md (source gate — SETTLED 2026-10-02:
                        libhangul's hanja.txt is **BSD-3**, NOT LGPL-2.1 as PLAN.md
                        previously claimed; only the surrounding libhangul is LGPL-2.1.
                        BSD-3 is the better fit: no LGPL linking conditions on a data
                        file. Verified from the Debian copyright file. Unihan (CC-BY)
                        remains an alternative; kime hanja.txt is GPL → facts only.
                        Vendored at data-orig/hanja/ with a sha256 pin, like every
                        other converter input, so no build step needs the network)
    tools/              ist2data.py (pat.im .ist.xml), kbd2data.py (libhangul XML),
                        kcm_export.py, hunmin-sim (replay CLI), hunmin-doctor (data linter)
    tests/              see TESTING.md (tiers A–C live here)
  frontends/fcitx5/     C++ addon "hunmin"; enumerates layouts via hunmin_layout_list();
                        coexists with stock fcitx5-hangul (distinct addon/input-method names)
  frontends/android/    Kotlin IME (InputMethodService) + JNI; hardware-keyboard first;
                        mic → SpeechRecognizer (ACTION_VOICE_INPUT fallback);
                        one-tap showInputMethodPicker() back to Samsung for touch
  frontends/windows/    P6: TSF Text Input Processor (x86+x64), architecture ref: rime/Weasel
  docs/                 the SPEC set is what an agent reads first, so name it in
                        reading order, not alphabetically:
                          SPEC-P2.md         composition engine, normative (pat.im)
                          SPEC-MODES.md      hangul/latin automatic mode (R1-R13)
                          SPEC-SCHEMA.md     data file contracts
                          MODES-DATA.md      sources, licences, corpus measurement
                          MODES-METRICS.md   metric definitions + pass lines
                          MODES-ENGINE-RULES.md  the judgement state machine
                          MODES-POSTMORTEM.md    what went wrong, numbered
                        then the working set:
                          TESTING.md         tiers A-E, what proves what
                          ENGINEERING.md     how to investigate, what to write down
                          FRONTEND-CONTRACT.md  what each frontend file assumes
                          FCITX5-ADDON-NOTES.md  addon/pitfalls, X11-specific
                          ENVIRONMENT.md     host facts and installed toolchains
                          D3-ROOT-CAUSE.md   why tier D3 looked green while measuring nothing
                          CODE_RULES.md, HOWTO-NEW-LAYOUT.md, GLOSSARY.md,
                          trace-spec.md      the tier C trace format
  PLAN.md / AGENTS.md   this file + agent invariants
```

### Engine invariants (non-negotiable; enforced in review + CI)

1. New layout = data file + golden vectors only. No Rust-free C `switch` per layout ever;
   no layout-specific fields in core structs (use named generic slots).
2. Key value primitives are fixed and closed: `Literal | SlotJamo{L,V,T,compose} |
   Ambiguous{pair,first} | Sequence{trigger→output}`. All P2 behavior (갈마들이, symbol
   tiers via j+k/l/;, 초성체 compat-jamo mode, CapsLock/W modes, ssang finals) expresses
   through these + rule tables + per-layout flags.
3. ABI is append-only; layout identity is a string, never a number; symbol list is tested.
4. Frontends stay thin (< ~700 LOC each): map platform events ↔ hunmin key ids, render
   commit/preedit/candidates, nothing else.
5. Voice/external text enters via the pass-through, never through the jamo
   pipeline. Candidate/suggest seam (built for hanja) is the future host of a
   corrector module (v2).
   ✅ **Implemented 2026-10-03** as `hunmin_ic_commit_text(ic, text)` — note
   the name is `hunmin_ic_*`, not the `hunmin_commit_text()` this invariant was
   originally written with: the result depends on an input context (it commits
   that ic's in-flight composition first, so the two texts come out in the order
   the user produced them), so it cannot be a ctx-level call. Composition, tier
   state and 전처리수식 flags are untouched; text is returned verbatim and
   untruncated rather than through the 64-`ucschar` buffer a single keystroke
   commit uses. Tier A: `libhunmin/tests/commit-text-test.c`
   (`engine-commit-text`), mutation-tested against four breakages: re-composing
   the text, accepting a truncated UTF-8 tail, dropping instead of committing the
   typed syllable, and truncating the sentence.

## Phases

- **P0 ✅** scaffold + ground truth: git, schema, `docs/SPEC-P2.md` distilled from
  pat.im/1136 (+ 2018-04-10 symbol revisions, 20221224 옛한글 조합 ist; ohi.pat.im as
  oracle list; kime transcribed facts). All three `신세벌식 P2*.ist.xml` fetched from
  pat.im and sha256-pinned in `data-orig/naraeseot/`. Local libhangul kbd XMLs
  converted via `kbd2data.py` (9 layouts + 4 compose tables incl. ahn/romaja
  extracted from libhangul's C source).
- **P1** core v0: ✅ **P1a done** — std engine = faithful port of libhangul
  jamo/jaso (tables generated from vendored source), reproducing all 7 reference
  traces byte-exactly + 25-seed parity sweep vs system libhangul; loader/doctor/
  hunmin-sim/ABI tests in ctest (12/12). ✅ **P1b-1**: schema v2 `case` primitive + expression evaluator; `ist2data.py`
  lifts sin3-p2 basic.xml verbatim (47 keys, 25 mix rows); multi-case golden green.
  ✅ P1b-2 — **all four items shipped**, the fourth on 2026-10-09.
  Romaja is ported and parity-verified (`libhunmin/data/layouts/romaja.json`,
  `ref-romaja.jsonl`, `engine-golden-romaja` + sweep); the symbol tiers ship as
  `"tier": 101/102/103` in `sin3-p2.json`; CapsLock and W modes exist as
  `hunmin_ic_set_option(ic,"caps",1)` and the `wWzHh` rules (`ic.c:610-621`).
  `seq` is no longer reserved-and-warned: `HK_SEQ` in
  `libhunmin/src/hunmin_internal.h:18`, parsed at `schema_loader.c:92`, committed at
  `ic.c:592`, and validated rather than warned by `tools/doctor.py:57-63`.
  Oracle-grade patim/kime validation is partly covered by the P2 gate corpora
  below.
- **P2** P2-complete: symbol tiers, ssang finals/초성체 via shift, CapsLock/W rules, 옛한글
  rule file. Gate: one golden corpus per pat.im feature section — ✅ `tests/golden/p2-*.jsonl`
  (`engine-golden-p2-*`, 9 corpora; see `docs/SPEC-P2.md` "P2 gate corpora" for scope notes).
- **P3** hanja: candidate/suggest API + F9 flow; dictionary source/license decision.
  ✅ **Shipped 2026-10-03** (corrected 2026-10-09: this line carried no progress marker
  after it landed). F9 opens the Hanja window; selecting a candidate commits it.
  D2 asserts the candidate list's presence, size, kind label and text —
  `frontends/fcitx5/test/testhunminui.cpp:168-200`. The source gate closed 2026-10-02
  (`libhunmin/data/hanja/LICENSE.md`), so the ⚠️ gate at line 314 below is satisfied and is
  struck there.
  ⚠️ Design constraint from the fcitx5 wiki Q&A: the F9 flow must **not** depend on
  reading or rewriting surrounding text. Many applications do not support it at all
  (XIM clients, SDL, terminals) and it is buggy in browsers and embedded terminals.
  Upstream Hangul uses surrounding only for *temporary* Hanja mode. See
  docs/FCITX5-ADDON-NOTES.md §8c.
- **P4** fcitx5 addon + debs: `dpkg-buildpackage -us -uc -b` (outputs land in `~/work/`
  per workspace convention) → PPA. Gate: tier-D suite. ✅ addon + `debian/` +
  `hunmin_0.1.0_amd64.deb` built; **D1 `engine-d1-fcitx5` and D3 `engine-d3-x11`
  green, D2 `engine-d2-ui` green — the whole suite passes (ctest is green; 140 as
  of the P5 merge, which added `engine-commit-text` and `engine-android-goldens`).
  ✅ **fcitx5 configtool UI (2026-10-03):** the `Layout=` option was a free-text
  field, so picking a layout meant knowing the id (`sin3-p2`) and typing it
  exactly — a wrong id failed later, at keystroke time, with only a log line. It
  is now an `EnumAnnotation`, which configtool renders as a combobox, with the
  rows injected from `hunmin_ctx_layout_*` at `getConfig()` time rather than
  written out in the frontend (AGENTS.md rule 1; invariant I11). Also: a second
  IM registration `Hunmin (Romaja)` (`hunmin-romaja.conf`) on the same addon, so
  Romaja is selectable from the IM list without reconfiguring the layout; and
  `Name[ko]` on both IMs, since the list is read by name. D2 asserts the row
  count against the shipped data and every id against a file on disk, and
  checks `IsEnum`'s *value* — presence alone passed with it set to `False`.
  ⚠️ Not covered: `refreshLayoutList()` on `reloadConfig()` (an addon virtual
  only fcitx5 itself calls, over D-Bus — a testfrontend never does it), so a
  layout added while fcitx5 runs could leave the picker stale. **Measured
  2026-10-03:** the picker is populated once per process, so installing the
  deb adds the IM and the layout rows to disk but a *running* fcitx5 still
  shows the old list — the user sees exactly the "still just Hunmin / sin3-p2"
  report and it is not a packaging bug. `systemctl --user restart fcitx5`
  fixed it. Recurs on every `dpkg -i`; the deb needs a postinst. See
  `docs/FCITX5-ADDON-NOTES.md` §11c-ter.
  D3 was not green
  until 2026-10-03 and the cause was not where it looked: the e2e harness called
  `XOpenIM()`/`XCreateIC()` while linking only `X11::X11`, so the *client* ran
  libX11's built-in XIM against fcitx5's libxcb-imdkit *server*. libX11's XIM
  answers with its own local IM when no server is reachable, so the harness
  decoded every key itself and D3 compared libX11's output against tier A — it
  measured nothing while looking close to green. The harness is now written
  against dkit's `xcb_xim_*` client API. Three further defects surfaced only
  after that: `fcitx5-remote -s hunmin` is a silent no-op that returns rc=0; the
  profile's *second* item wins the default IM regardless of `DefaultIM=`; and the
  layout was written to a config file the addon never reads. Two cases had been
  passing for the wrong reason (empty layout → `sin3-p2` default). Full account
  and measurements in `docs/D3-ROOT-CAUSE.md`; the wrong turns are kept in
  `docs/FCITX5-ADDON-NOTES.md` §11b–§11d.
  ⏳ Still not covered by D3: a UI defect that changes *which* string reaches
  the client without the addon noticing it declined a key. The candidate window
  is deliberately absent, not missing — an F9 case in D3 passed no matter what
  the adapter did (measured: two mutations, D3 green both times, D2 red both
  times), because a client that only sees commits cannot observe consumption.
  See `docs/D3-ROOT-CAUSE.md`; `tests/e2e/cases.tsv` takes expectations only
  from `--trace`, which is what makes a D3 case falsifiable. Consumption is
  unobservable from an XIM client at *any* level — not in the commit stream,
  not in `nKeySym` (structurally 0: fcitx5 sends `LOOKUP_CHARS`, whose XIM
  message has no keysym field, and a declined key never becomes text at all),
  not in the raw KeyPress (delivered before the addon filters) — so the gap is
  closed by D2 alone, not narrowed here.
- **P5** Android IME (route B primary): HW keyboard e2e on emulator CI; mic launch path;
  picker-switch UX; Play/F-Droid packaging prep. Gate: tier-E suite + BT-keyboard manual matrix.
  Status, measured 2026-10-04 — of the four deliverables, **three are done**
  (emulator CI job, mic launch path, picker UX) and **R8 rules are not**. The
  tier-E suite is green (E1 34/34, mutations 8/8, 21 goldens, E2 on the emulator).
  ⚠️ **Partly done, corrected 2026-10-09.** This used to read "no keystore config,
  no version-code scheme, no R8 rules". The release-signing prep added a `release`
  signingConfig and `versionCode = 1` / `versionName = "0.1.0-p5"`, so two of the
  three are done. One defect in that change is recorded and **not** silently
  repaired here, because it is a packaging decision: `build.gradle.kts:75`
  hardcodes `storeFile = file("/home/scoby/.android/debug.keystore")` while the
  comment one line above says *"keystore lives in local.properties (gitignored)"*.
  The two contradict, and AGENTS.md forbids committed absolute paths into a home
  directory. See `docs/REMAINING-WORK.md`.
  ❌ **R8 rules are the remaining code work.** `assembleRelease` builds and
  installs correctly once signed — measured, with a signed release APK ignoring
  the trace broadcast and refusing `run-as`, so the `FLAG_DEBUGGABLE` gate holds.
  `isMinifyEnabled = false` with no `proguard-rules` file means the shrinker path
  has never been exercised; JNI is reached from Kotlin by name, so the JNI shim's
  exported symbols and the `HunminNative` members are exactly what it would remove.
  ❌ **The gate is still open, on the manual half.** Tier E3's checklist and its
  trace tooling are finished and the tooling is proven on the emulator, but **no
  physical BT-keyboard session has been run and no device is signed off**. That is
  the one thing here that no amount of automation substitutes for, which is what
  the tier says of itself.
  ✅ **Mic launch path, 2026-10-04.** A mic button on the input view starts a
  `VoiceSession`, whose result goes to `hunmin_ic_commit_text` — never to
  `hunmin_ic_process`, or spoken syllables would compose a second time
  (invariant 5, mutation-tested). Recognition is an external service and is
  **not** tested; what is tested is the part that is ours, because a voice
  failure escaping into the key path takes the keyboard down with it:
  a recogniser that throws is contained, whitespace-only audio is silence rather
  than invisible committed text, and exactly one result is delivered per attempt
  (`SpeechRecognizer` sends `onResults` then `onError` on some teardowns, and
  the pass-through is destructive). 12 tier E1 tests; the mutation self-test is
  8/8. Tier E asserts the installed package declares RECORD_AUDIO and that the
  keyboard still types. See the Risks entry for the two platform facts that
  forced the design.
  ✅ **Prerequisite held since 2026-10-03:** tier C. All four steps landed —
  `docs/trace-spec.md`, 104 generated traces (3,832 events, 1,968 commit
  assertions) replayed through the real fcitx5 adapter, the keycode-map fixture
  checked against the XKB header *and* `android.jar`, and the shipped-addon trace
  mode with an opt-out gate. The Android corpus is data-only and skips (rc 77) until
  the frontend exists; the keycode table it will be replayed against is measured,
  not assumed — it was wrong for all 47 keys before that was caught.
  ✅ **CI reaches all of it (2026-10-04).** The `android` job in
  `.github/workflows/ci.yml` runs on a GitHub runner: installs the SDK with a
  pinned cmdline-tools build, creates its own AVD, cross-compiles **both** ABIs
  (AGP fails packaging if either is missing, so one is not "half a job"), builds
  the APK, then runs E0+E1+mutations *before* booting the emulator — so a broken
  build fails in seconds rather than after a boot — and E2 after. The AVD name
  comes from `HUNMIN_AVD` rather than being hardcoded, because the runner's
  system image is not the local pixel_6 profile.
  ⚠️ The job is **unproven on a real runner**: it was written and every step was
  reproduced locally, but no GitHub Actions run has executed it. First push is
  the real test.
  ✅ **The layout picker exists (2026-10-04).** `LayoutPicker` is the IME's input
  view: a Spinner listing every layout `hunmin_ctx_layout_supported` allows,
  labelled with its Korean name — the same filter fcitx5's `refreshLayoutList()`
  applies, so the two frontends cannot offer different things. The choice is
  persisted (the service is killed and recreated often, and an unpersisted
  picker silently reverts every time), and a switch flushes the in-flight
  composition rather than dropping it. Tier E1 pins both properties.
  ✅ **The frontend exists and types, measured 2026-10-03.**
  `frontends/android/` is a Gradle/AGP project (AGP 8.6.1, Gradle 8.7,
  compileSdk 35, minSdk 24, `arm64-v8a` + `x86_64`): an
  `InputMethodService`, a Kotlin adapter, a JNI shim, the layout tables shipped
  as APK assets, and four tiers wired into ctest --
  `engine-android-e1` (19 JVM tests against the real engine and a recording
  `InputConnection`), `android-mutations` (5 planted defects, all caught),
  `engine-android-goldens` (21 tier-A corpora on the device, from P5 batch 1)
  and `engine-android-e2e` (the APK on the emulator, typed into with real key
  events, the committed text read back out of a real `EditText` with
  `uiautomator`).
  E2 asserts the field reads exactly `ㅎ곈디` for `dkssud` under `sin3-p2`,
  byte-identical to `hunmin-sim` -- and reads it from the platform's own view
  hierarchy rather than from anything hunmin logged, so a wrong adapter cannot
  pass by being wrong in the same way in its own trace.
  Toolchain installed without sudo, emulator boots headless in ~20 s, an MCP
  server gives 25 adb tools. NDK 27.3.13750724 installed 2026-10-03.
  ✅ **The voice path's engine half exists** as `hunmin_ic_commit_text()`
  (invariant 5), so P5 no longer has to invent a way to get spoken text past the
  jamo pipeline — only a way to call it.
  ✅ **The core cross-compiles and runs on Android, measured 2026-10-03.**
  `cmake/android.toolchain.cmake` + `tools/build-android.sh` produce
  `libhunmin.so` for `arm64-v8a` and `x86_64` with no ndk-build, and
  `tools/android-golden-check.sh` replays **all 21 tier-A goldens on the
  emulator** (ctest `engine-android-goldens`, skips rc 77 without a device).
  Doing that found a portability defect the host could not see: compose-table
  dedup ran after an unstable `qsort`, so which of two disagreeing rows survived
  depended on the libc — glibc said `ᄈᅠᆸ`, bionic said `ㅃ`, on identical source
  and data. Fixed by deduplicating in file order before sorting.
- **P5.5** `hunmin-keymaps`: `kcm_export.py` + data-only APK advertising 3-90/3-2/… KCM
  overlays (`ACTION_QUERY_KEYBOARD_LAYOUTS`); documented OEM-gating caveats.
- **P6** Windows TSF TIP (Weasel topology).
- **P7** distribution: Debian ITP (archive precedent: fcitx5-unikey/varnam/quwei/keyman
  engine-wrapper addons), external-contributor docs, optional later fcitx5-android plugin
  port of the same addon (unscheduled).
- **P8 ⏳** hangul/latin automatic switching — **the requirement, and the only
  phase with no implementation at all.** Stated by the maintainer 2026-10-03:
  type English while in Hangul mode and the mode changes; switching must be
  **without changing IM** ("IM 변경 없이 동작 모드로 한글<->영어 변경이 되는 게 굉장히
  편리해"). Requirements R1–R13 in **`docs/SPEC-MODES.md`**, read that before
  starting anything here.

  Status 2026-10-05, corrected 2026-10-09: **no engine code, and one undocumented
  frontend adapter.** Do not read this as "mostly done" — `grep -rn
  hangul_valid\|english_valid libhunmin/ frontends/` returns nothing, and the core is
  genuinely untouched.

  ⚠️ *(State as of this analysis; superseded 2026-11-01, #164 — the detector below
  is retired and the judgement is the engine's.)* **But this line was also
  incomplete in the other direction.** `frontends/fcitx5/src/
  language_detect.{h,cpp}` implemented an R8 shadow-layout adapter (shipped, wired at
  `hunminadapter.cpp:221`). It keeps a second `hunmin_ctx` on a Latin layout beside the Hangul
  one and replays the buffered word when `hunmin_ic_preedit()` goes empty. It is in
  `master`, it compiles into the addon, **no document mentioned it**, and **no committed
  test covers it** — its verification script lives in the gitignored `out/`, so the
  artefact that caught its own bug is not retrievable. Its design also diverges from R3's
  `[D]` ruling above, which places guessing in the engine. Both the gap and the divergence
  are recorded, not resolved: `docs/REMAINING-WORK.md`.

  | area | state | where |
  |---|---|---|
  | requirements R1–R13 | settled | `docs/SPEC-MODES.md` |
  | judgement source (what "valid" looks up) | **ANSWERED** — a syllable-structure test for Hangul (no source), hunspell `en_US` + a user dictionary for English; B1 retired 2026-11-01 | `docs/SPEC-MODES.md` §R8-정정, §R8-정정-2 |
  | metrics + pass lines | defined, none measured | `docs/MODES-METRICS.md` |
  | engine rules | drafted | `docs/MODES-ENGINE-RULES.md` |
  | third-party correctness | **not started** — must be evaluated against **both** 두벌식 and 세벌식, at minimum 신세벌식 P2 | transcript 2026-10-03 |
  | corpora | three, all outside the repo and all reproducible | `docs/MODES-DATA.md` |

  R3 [D] places the decision in the **engine**, not the frontend adapter, so it
  can serve Android and Windows alike. That supersedes the P5 note below about
  an accessibility-based auto-switch helper, which this makes unnecessary.

## Risks

- P2 ambiguity edge cases → three oracles: pat.im worked examples, kime published test
  expectations (facts), ohi.pat.im spot replays.
- pat.im ist.xml fetch may be blocked → fallback: layout PDF (pat.im/1137) + kime yaml +
  community tables, cross-checked; every divergence logged in SPEC-P2.md.
- Hanja dictionary provenance/license → gate before P3 ships. ✅ **Closed 2026-10-02**
  (struck 2026-10-09; P3 shipped 2026-10-03 without this gate being re-checked). Provenance
  and terms are in `libhunmin/data/hanja/LICENSE.md`; see `PLAN.md` "P2 ambiguity edge
  cases" above for the settled source.
- Samsung devices: Google `SpeechRecognizer` availability varies → dual path.
  ⚠️ **Corrected 2026-10-04 against the platform.** The fallback was written here
  as "SpeechRecognizer + ACTION_VOICE_INPUT", and **that constant does not
  exist**; neither does `RecognizerIntent.LANGUAGE_MODEL_FREE_FORM`, so the
  first implementation referenced both and did not compile. More decisively,
  `InputMethodService` has **no `onActivityResult` hook** (javap on android.jar,
  API 35): an IME cannot receive an activity's result at all, so a
  voice-input activity's text can never be routed back into our engine. The
  fallback is therefore the platform's own recogniser *UI*, whose text reaches
  the focused field by its own route rather than through us — and the fallback
  branch reports `Failed` honestly instead of pretending a result will arrive.
  Corollary, also measured: an IME has **no permission-request hook**, so
  RECORD_AUDIO cannot be requested at runtime and must be granted in system
  settings. "user-selectable" is therefore still open: there is no settings
  surface to put the choice in.
- One-IME Android model → product accepts one-tap switching UX; auto-switch helper is a
  later optional add-on (accessibility-based, fragile).

## Development environment

Single Linux host (this machine) covers **P0–P5**: KVM and uinput already present;
missing tooling installed in phase-scoped batches (clang/libFuzzer, xvfb, python3-evdev
→ P0–P4; JDK+SDK/NDK+emulator → P5; mingw-w64 for local cross-build smoke → tier B).
A **Windows VM is first needed at P6** only (Win11 eval on existing qemu + swtpm, VS2022
C++/ATL + TAT; USB BT dongle passthrough for keyboard checks). Wine is explicitly excluded
(TSF support invalid as a test signal). GitHub Actions (ubuntu / mingw /
android-emulator jobs) is the standing second environment from day one; the
`windows-latest` job that used to sit in that list is **disabled** (issue #51:
"for windows, it is not scope of this version. so in the issue we will disable
windows test."), so MSVC compiles nothing in CI for now — a Windows VM stays a
P6 need either way. Details and exact
install batches: **`docs/ENVIRONMENT.md`**.

## Verification

See **TESTING.md** — tiered from unit goldens to per-platform end-to-end harnesses with a
cross-platform conformance-trace mechanism.
