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
| 0.3 | 자동 한/영 전환 (the modeswitch feature, R8/E5) |
| 1.0 | after a usage-polish pass |

**Lane assignment is a plan, and a plan can be re-cut.** Nothing here settles a
blocked item; B1–B9 remain the maintainer's rulings wherever they are named.

---

## 0.1 — fcitx5 + Android

The release closes with two decisions, two rulings-that-happen-to-be-blocking, and
two measurements. No engine work is on the path.

**Must be decided to ship (from `REMAINING-WORK.md` §1):**

- **B8** — the keystore path contradiction in `build.gradle.kts`. A signed release
  APK cannot exist until the packaging decision names one truth. GitHub issue #1.
- **B3** — ON or OFF by default. This one line decides whether the modeswitch
  feature ships in 0.1 at all: **OFF by default moves R8 and everything under
  `REMAINING-WORK.md` §2 out of the 0.1 lane**, which is what puts 자동 한/영
  전환 in 0.3.
- **B4** — the 한/영 key (E5). Voice-input stage 2 (Right Alt + Enter auto-toggle)
  is a stub in the tree; 0.1 either ships it inert or ships it working. Either is
  shippable; silence is not.
- **B9** — the R8/R3 documentation contradiction. A release must not ship two
  documents describing two architectures as one.

**Must be measured before shipping (environment-bound, not decision-gated):**

- The **R8 shrinker path end-to-end** (`assembleRelease` + emulator run). The rules
  exist and minify is on; the shrinker has never actually run.
- The **tier E3 physical-device sign-off** (three rows,
  `tests/manual-checklists/tier-e3-bt-keyboard.md`). This is the last open line of
  the P5 gate.

**Known limitation if shipped unmeasured:** voice recognition quality — no
microphone on the AVD, nothing in the repo measures it.

## 0.2 — Windows TSF

- The precondition is a **Windows environment**, which this host does not have
  (`docs/ENVIRONMENT.md` batch 4, not installed). No TIP work can be measured
  without a VM or physical machine; that is the blocker, and it is environmental,
  not decisional.
- The engine ABI is append-only and layouts are data, so 0.2 is expected to be a
  frontend-only lane: `docs/SPEC-P2.md`'s "planned TSF frontend" becomes an actual
  frontend. No core work is planned.
- The mingw cross-compile CI job exists (`BUILD_TESTING=OFF`); it proves the core
  compiles on Windows, nothing more.
- B4's 날개셋 unit table is an Android framing. Windows key assignment
  (Alt+Shift / Win+Space class) is a separate decision when 0.2 starts.

## 0.3 — 자동 한/영 전환

This lane inherits **everything §2 of `REMAINING-WORK.md` holds hostage to B1**:

- B1 (the hangul judgement source) is the root; §2's five items are its children
  (③-bis wrong-direction switch metric, E1 symmetry, E2/E3/E6/E7 thresholds,
  third-party correctness 두/세벌식, word-list schema).
- B2 — the pass lines for all three metrics — is the exit gate: the feature is
  shippable when its own metrics have a number the maintainer set.
- B4's key assignment lands here rather than in 0.1.

One consequence worth naming: **soft keyboard sits with no lane.** The instruction
places 사용성 개선 after 0.3, and `REMAINING-WORK.md` records the soft keyboard as
having no assigned priority. Naming it explicitly: it has been assigned no priority
by this re-cut either. Voice stage 7 (configurable key bindings) is the natural
companion whenever that decision is made.

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
