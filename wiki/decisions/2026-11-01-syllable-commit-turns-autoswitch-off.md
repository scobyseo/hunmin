---
title: The buffer and the autoswitch depend on each other — syllable commit turns autoswitch off, 2026-11-01
created: 2026-11-01
updated: 2026-11-01
type: decision
status: active
tags: [decision, modes, settings, frontends]
sources:
  - ../../docs/SPEC-MODES.md
  - ../../docs/OPTIONS-MEASURED.md
confidence: high
---

# The buffer and the autoswitch depend on each other — syllable commit turns autoswitch off, 2026-11-01

The measurement of issue #171 (`docs/OPTIONS-MEASURED.md` §BND-7) ended with a
fact that is a design question, not a number: ReqMode6's digit run is held by the
**buffered** commit policy, and under `HUNMIN_COMMIT_PER_SYLLABLE` the same run is
delivered one digit per key. So the buffer and the automatic Hangul/English switch
are not independent settings, and it was put to the maintainer as such.

**Maintainer, 2026-11-01, verbatim:**

> 버퍼를 사용하는 것과 자동변환은 네가 말한대로 의존성이 있으니, 설정 시에 의존성을
> 맞춰서 설정해야 하고, 동작 시에도 음절단위 commit이 켜져있으면 자동변환을 끄고,
> 그 내용을 디버그 출력에 적어주는 게 가장 적절한 동작으로 보인다.

Read as three obligations, in the order the sentence gives them:

**The implementation is issue #207** (task, opened 2026-11-01); this page holds the
ruling, the issue holds the work and its acceptance commands.

1. **At settings time, the dependency is reconciled when the value is written.**
   Turning 음절 단위 commit on writes autoswitch = off in the same act, rather
   than leaving a stored ON that the runtime will ignore.
2. **At run time, syllable commit off-switches autoswitch.** A layer built with
   `HUNMIN_COMMIT_PER_SYLLABLE` has no autoswitch, whatever the stored value
   says — the state itself is turned off, not merely left inert.
3. **The debug output says so.** When autoswitch is off for this reason, the
   diagnostic names the reason, so "nothing happened" is distinguishable from
   "the feature is off".

What the ruling does **not** fix, and what a ruling on it would have to say:

- **Where the diagnostic goes.** The engine has no log channel (it writes
  `stderr` warnings from the loaders only); the three frontends have their own
  (`FCITX_INFO`, TSF's `hunmin_logger`, Android's `Log`). One engine-side reason
  string plus one log line per frontend keeps the value in one home (the shape
  ReqMode13 already chose for the state itself), and is testable in tier A without
  building a frontend. The alternative — each frontend deciding the reason
  itself — is three copies of one rule.
- **What the settings surface shows.** Hiding the autoswitch row, disabling it, or
  leaving it enabled and relying on the reconcile is a UI choice per platform.

Measured surface this has to land on (2026-11-01, grep, not a run): the commit
unit exists in all three frontends — fcitx5 `commitBehavior`, TSF `commitUnit`,
Android's `commit_behavior` preference — and **autoswitch has no settings row
anywhere**; it lives in the engine (`hunmin_ic`'s `autoswitch`, read/written
through `hunmin_ic_set_option` / `hunmin_buffered_preedit_get_option`, default off
per [[2026-10-11-auto-switch-off-by-default]] and toggled by the
`HUNMIN_SPECIAL_AUTOSWITCH` chord). So item 1 has a prerequisite: the state needs
a settings row and a persisted value in each frontend before a write can be
reconciled.

The requirement text this depends on is `docs/SPEC-MODES.md` §ReqMode6 (what the buffer
holds) and §ReqMode13 (the one on/off state); the record of how the measurement got
here is [[2026-11-01-the-judgement-timing-is-per-key-by-default]]'s sibling
paper, `docs/OPTIONS-MEASURED.md` §BND-7. As always the ruling authorises the work;
it does not close the question of whether the ON state should pin the policy in
some other way, which stays open. See
[[decisions-belong-to-the-maintainer]] for why this page quotes rather than
paraphrases.
