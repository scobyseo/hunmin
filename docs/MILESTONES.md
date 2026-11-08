# MILESTONES — the release plan

Written 2026-10-11. The ordering below is the **maintainer's instruction**, recorded
verbatim:

> 일단 fcitx + android 초판으로 0.1을 만들 생각이고 윈도우는 0.2, 한영 자동 변환은
> 0.3으로 할까 해. 그 후 사용성 개선을 거쳐 1.0으로 가는 방향이야.

This file assigns the open items of `docs/REMAINING-WORK.md` to release lanes. It is
a **rearrangement, not a status board**: it claims nothing is done or open — those
facts live in `REMAINING-WORK.md` and its assertion block, and a second home of the
same claim is how they drift apart (AGENTS.md, "Do not write a figure into two
places"). Item ids (B1–B9, §2's items, W/N rows) are cited, never restated.

| Release | Target |
|---|---|
| 0.1 | fcitx5 + Android first edition |
| 0.2 | Windows TSF frontend (P6) |
| 0.3 | 자동 한/영 전환 (the language detection feature, R8/E5) |
| 0.4 | 소프트 키보드 — 두벌식 + QWERTY + 기호 입력 |
| 1.0 | after a usage-polish pass |

The 0.4 lane was added by the maintainer on 2026-10-11, after the first cut of this
file recorded the soft keyboard as laneless. Its goal, stated by the maintainer:
두벌식 + qwerty + 기호 입력 지원.

**Lane assignment is a plan, and a plan can be re-cut.** Nothing here settles a
blocked item; B1–B9 remain the maintainer's rulings wherever they are named.

---

## 0.1 — fcitx5 + Android

The release closes with four decisions and two measurements. No engine work is on
the path. All four decisions are recorded below — this lane named B3, B4, B8 and
B9 — so what is left on it is the measurement list further down and
`REMAINING-WORK.md` §2, which is the 0.3 lane's.

**Must be decided to ship (from `REMAINING-WORK.md` §1):**

- ~~**B8** — the keystore path contradiction in `build.gradle.kts`. A signed
  release APK cannot exist until the packaging decision names one truth. GitHub
  issue #1.~~ **DECIDED 2026-10-13**: the keystore resolves from
  `HUNMIN_RELEASE_STORE`, else gitignored `local.properties`, else a throwaway
  key generated at build time. Ruling in
  `wiki/decisions/2026-10-13-b8-keystore-path-resolved.md`. What the ruling left
  to the maintainer: whether `local.properties` is the permanent canonical
  keystore, and whether the throwaway-key fallback stays.
- ~~**B3** — ON or OFF by default.~~ **DECIDED 2026-10-11: OFF by default until
  0.3**, so the language detection feature and `REMAINING-WORK.md` §2 leave the
    0.1 lane. The ruling and the separate-data-package
  direction are in `wiki/decisions/2026-10-11-auto-switch-off-by-default.md`.
- ~~**B4** — the 한/영 key (E5). Voice-input stage 2 (Right Alt + Enter
  auto-toggle) is a stub in the tree; 0.1 either ships it inert or ships it
  working. Either is shippable; silence is not.~~ **DECIDED 2026-10-12: 0.1
  ships it inert, by disabling the default key composition** — no default chord
  (Right Alt + Enter is unbound; Enter reaches the editor), the service skips the
  `AUTO_TOGGLE` action even against a stale stored binding, and the settings row
  shows 미정 (0.3), disabled. Ruling quoted in
  `wiki/decisions/2026-10-12-auto-toggle-disabled-by-default-in-0-1.md`.
  **SUPERSEDED 2026-11-02 (issue #174):** the engine half landed (#164/#165/#169)
  and Android implements the feature, so the dispatch fires and the row reports
  the engine's value; the inert shipping state no longer describes the tree.
  The **E5 assignment itself** (single vs compound meaning) is not decided by
  this and lands here in 0.3 as before.
- ~~**B9** — the R8/R3 documentation contradiction. A release must not ship two
  documents describing two architectures as one.~~ **DECIDED 2026-10-14: R8 is
  adaptor-owned** — one judgement rule, one owning component, identical on every
  frontend; `docs/SPEC-MODES.md` R3 is requirement-only and `docs/architecture.md`
  records the owner and the mechanism. Ruling quoted in
  `wiki/decisions/2026-10-13-r8-mode-guessing-is-adaptor-owned.md` (issue #3,
  `a5c46dc`). R8's engine integration leaves with it — and it now waits on the
  judgement's implementation (#166, #168), not on a source: B1 retired 2026-11-01.

**Must be measured before shipping (environment-bound, not decision-gated):**

- ~~The **R8 shrinker path end-to-end** (`assembleRelease` + emulator run).~~ **Measured
  2026-10-11** — green; see the row in `REMAINING-WORK.md` §3 and
  `tests/manual-checklists/release-r8-shrinker.md` (issue #4, merged). It left this lane.
  What remains true of it: `tools/android-e2e.sh` drives the **debug** APK by design and
  no CI job does an `adb install` of the release build — the release APK is built in CI
  (job `android-release`, issue #8) but never typed into there — and the release
  `signingConfig` still hardcodes a keystore (#1), so a CI release build proves the build,
  not a releasable signature (both tracked: #8, #1).
- The **tier E3 physical-device sign-off** (three rows,
  `tests/manual-checklists/tier-e3-bt-keyboard.md`). This is the last open line of the
  P5 gate.

**Known limitation if shipped unmeasured:** voice recognition quality — no
microphone on the AVD, nothing in the repo measures it.

**Version-string convention (ruled 2026-10-12, issue #9):** the release is
`0.1.0` everywhere — `CMakeLists.txt` and `debian/changelog` carry it bare.
The Android `versionName` keeps its phase suffix (`0.1.0-p5`, and
`0.1.0-p5.5` for the keymaps APK): the `-pN` label is the Android build phase
(`docs/ENVIRONMENT.md` batch 2 — P5 the IME, P5.5 the KCM APK), not a
different release. A user comparing an installed APK against the .deb reads
the `0.1.0` part as the release and the `-pN` part as "built during phase N".
The convention is restated in `wiki/decisions/2026-10-12-versionname-keeps-the-phase-suffix.md`.

**Distribution channel for 0.1 (decided 2026-10-12, issue #7):**
GitHub Release on tag `0.1.0` with `hunmin_0.1.0_amd64.deb` attached. The
reproducible method is:

```sh
# clean checkout, gate is the tier-D ctest suite (PLAN.md:156)
cmake -B build && cmake --build build
ctest --test-dir build -N
dpkg-buildpackage -us -uc -b     # hunmin_0.1.0_amd64.deb
git tag 0.1.0
gh release create 0.1.0 --title "hunmin 0.1.0" --notes-file docs/release-notes/0.1.0.md
gh release upload 0.1.0 hunmin_0.1.0_amd64.deb
```

The procedure is scripted in `tools/release-0.1.sh` (dry-run with `--dry-run`,
or `--yes` to build/tag/release/upload). The `.deb` is unsigned (`-us -uc`) and
is only published after the tier-D gate passes; the signed, minified release
APK remains blocked on issue #1 (B8 keystore path).

## 0.2 — Windows TSF

- **This is the first release intended to reach people who do not already have
  access to the repository.** The repository is **private until after 0.2**
  (maintainer ruling 2026-10-16), so every artefact published up to and including
  0.1 is account-gated: the GitHub Release on tag `0.1.0` exists and its
  `.deb` is the one built at that tag, but the asset URL returns 404
  unauthenticated and the release page is not readable. 0.2 is where the tree's
  user-facing words are read by a stranger for the first time, which makes the
  state of `README.md`, `docs/release-notes/` and the packaged
  `/usr/share/doc/hunmin/README.md.gz` part of the 0.2 definition of done, not
  incidental prose. Measured 2026-10-16; recorded in
  `wiki/decisions/2026-10-16-the-repository-is-private-until-after-0.2.md`.
- The precondition was a **Windows environment**, marked absent in
  `docs/ENVIRONMENT.md` batch 4. **Superseded 2026-10-16/17:** the Windows 11
  host itself carries VS2022 BuildTools 14.44 + Windows SDK 10.0.26100, the
  core and the milestone-A TSF DLL both build natively there (measured again
  after the PR-#76 merge); see `docs/ENVIRONMENT.md` §"Windows 0.2 development
  environment". TIP registration/activation stays device-bound.
- The engine ABI is append-only and layouts are data, so 0.2 is expected to be a
  frontend-only lane: `docs/SPEC-P2.md`'s "planned TSF frontend" becomes an actual
  frontend. No core work is planned.
- The mingw cross-compile CI job exists (`BUILD_TESTING=OFF`); it proves the core
  compiles on Windows, nothing more.
- B4's 날개셋 unit table is an Android framing. Windows key assignment
  (Alt+Shift / Win+Space class) is a separate decision when 0.2 starts.

## 0.3 — 자동 한/영 전환

The lane now has its shape from the 2026-10-11 ruling
(`wiki/decisions/2026-10-11-auto-switch-off-by-default.md`): OFF by default, and
**the feature's additional data ships as a separate distribution — the feature is
enable-able only where that data is present.** That sentence is the lane's design
direction; the mechanism is new work:

- **Data-package distribution** — packaging, licence isolation (the krdict
  CC-BY-SA precedent in `debian/copyright` and `libhunmin/data/krdict/LICENSE.md`
  shows the pattern), and **presence detection**: how the engine/frontend learns
  the data is installed. `hunmin_ic_set_option()` is the precedent for the on/off
  option itself (`docs/SPEC-MODES.md` R13); the presence half now has one too —
  `hunmin_ctx_autoswitch_data_present()` plus the refusal in the option writer and
  the chord, issue #169, `docs/DESIGN-MODES.md` §3. What is still new work is
  the distribution itself (#190): nothing ships the data separately yet.
- The lane inherits **all five items of `REMAINING-WORK.md` §2**, restated 2026-11-01
  after B1's retirement:

- They are no longer children of a source question. §2's five items (③-bis
  wrong-direction switch metric, E1 symmetry, E2/E3/E6/E7 thresholds, third-party
  correctness 두/세벌식, word-list schema) now wait on the judgement's implementation
  (#166, #168), on a harness (#181), or on their own rulings (#163, #180) — E1 has
  since been ruled and implemented (§E1, 2026-11-01), so none of them is ownerless.
- B2 — the pass lines for all three metrics — is the exit gate: the feature is
  shippable when its own metrics have a number the maintainer set.
- **The feature's own gate, ruled 2026-11-01:** *"내 예제가 동작하는 것까지를 0.3
  release gate로 보자."* The maintainer's mixed sentence
  `English는 좋은 Language입니다` typed as one key stream must come out in the languages
  the person meant — Hangul to English for the two English words and English to
  Hangul for the three Korean ones (E1). It is pinned by
  `WordSetTest.TheZeroThreeGateMixedSentenceWorksInOneStream`, which fails when
  either direction is removed; the sentence's expectations are the engine's own
  rendering per word, not typed Hangul.
- B4's key assignment lands here rather than in 0.1.

One consequence worth naming, and then correcting: the first cut of this file found
**soft keyboard with no lane** — `REMAINING-WORK.md` recorded it as having no
assigned priority, and that re-cut did not assign one. On 2026-10-11 the maintainer
gave it one: it is release **0.4**, below. Voice stage 7 (configurable key bindings)
remains unplaced; whether it joins the 0.4 lane is the same decision the lane now
makes concrete.

## 0.4 — 소프트 키보드 (두벌식 + QWERTY + 기호)

The maintainer's goal, verbatim: 두벌식 + qwerty + 기호 입력 지원. This was the
item `REMAINING-WORK.md` §3 recorded as "no priority has been assigned to it by
anyone" — the assignment is now made, and the board pointer at the top of that file
reflects the plan, not the item rows (status claims stay there; this lane cites).

**What already exists, measured against the tree:**

- **Layouts as data.** `dubeolsik.json` (26 keys) and `us-qwerty.json` (47 keys)
  are in `libhunmin/data/layouts/`, each key carrying a `plain` and a `shift`
  layer. The engine maps a key event to jamo/literal; it does not care whether the
  event came from hardware or a touch. AGENTS.md's one idea — layout behaviour is
  data — is exactly what makes this lane mostly presentation work.
- **The composition engine and eojeol buffer** already serve the hardware path on
  Android (`LayoutPicker.kt` renders the layout strip today; it is a strip plus a
  mic, not keys).
- **Keymap export tooling** (`tools/kcm_export.py`, 16 files under
  `frontends/android/keymaps/`) exists for hardware keymaps and is a candidate
  source for key-cap labels — unverified whether it covers labels or only codes.

**The design question the lane owns — and the data says it is real:**

Three-goal support means **three layers per key** (plain / shift / 기호), but the
layout JSON encodes only `plain` and `shift` (measured: 47/47 keys in `us-qwerty`
carry both, nothing carries a third). So 기호 입력 needs a decision before any
pixel is drawn, and the honest options are comparable:

1. **A symbol layer in the layout data** — a third per-key entry (e.g. `alt` or
   `symbol`), still data, still no engine change; the engine already takes key
   primitives (`Literal | SlotJamo | Ambiguous | Sequence`). Cost: a schema change
   to `docs/SPEC-SCHEMA.md` territory and `doctor.py` validation, and every layout
   file grows.
2. **A separate symbol layout/page in the frontend** — the keyboard shows a
   symbol grid that sends ordinary key events or direct commit strings. Cost: the
   symbol set lives in the frontend or a new data file, not in `data/layouts/`;
   two homes for what "what symbols exist" means.
3. **Hardware-convention emulation** (long-press on a key yields its shifted/symbol
   variant, Android-style) on top of option 1.

Which is the optimum is a design analysis for the lane to write, not a ruling here
(AGENTS.md: analysis presents designs as comparable; the maintainer chooses). What
the measurement above does settle is that **doing 0.4 as "just render the JSON" is
not sufficient** — the third layer does not exist in the data today.

**Scope boundaries implied by the goal:**

- 두벌식 + QWERTY only, so 0.4 does not block on 세벌식/sin3-p2 key-grid ergonomics;
  but because layouts are data, shipping more is a content decision, not a code one.
- Android first (where "soft keyboard" names a real gap: the input view today).
  Whether fcitx5 needs anything (onboard etc.) is out of scope unless named.
- QWERTY means the Latin path through the engine must survive touch: `us-qwerty`
  is marked "Latin, no composition" in its own name field — the commit path for
  direct Latin text from touch needs at least one golden test.
- Shift state, auto-capitals, and gesture (swipe) input are not in the stated goal;
  they enter only if the lane's spec asks.

**Prior-art note:** absence of prior art in this repo is a gap, not a verdict —
Android IMEs universally ship a soft keyboard, so this lane closes a standard
capability, and the interesting question is only which layer model the data should
carry.

## 1.0 — after usage polish

Open by definition; nothing may be asserted here yet.

---

## What this plan does not do

- It settles no B-item. B1–B9 are rulings and stay the maintainer's
  (AGENTS.md: "네 멋대로 기각하지마").
- It does not duplicate status. If `REMAINING-WORK.md` and this file ever disagree
  about *whether something is open*, `REMAINING-WORK.md` wins — this file only
  says *which release* a thing is planned for, and the disagreement itself is a
  finding to report, not to resolve here.
