---
title: E5 — modifier + 한/영 carries the special functions, on all three frontends
created: 2026-10-31
updated: 2026-10-31
type: decision
status: active
tags: [decision, modes, engine, fcitx5, android, tsf]
sources:
  - ../../../docs/SPEC-MODES.md
  - ../../../docs/OPTIONS-MEASURED.md
  - ../../../docs/DESIGN-MODES.md
confidence: high
---

# E5 — modifier + 한/영 carries the special functions, on all three frontends

Closes the **assignment half of E5** (`docs/SPEC-MODES.md` §E5, issue #159) that the
2026-10-12 ruling left open: that ruling fixed the 0.1 shipping state
([[2026-10-12-auto-toggle-disabled-by-default-in-0-1]]), not what a single press and a
compound press mean.

**Maintainer, 2026-10-31** (verbatim, in the session that priced the options — issue
#159):

> okay then let's decide to use modifier + han/eng for special functions such as voice
> input or auto-mode switch config ; for all three frontends

and, when asked what the phrasing left unclear about the default and about changing it:

> it is default behavior and we should be able to select a new key composition through
> config dialog.

## What was ruled

1. **The single press stays the manual Hangul/English switch.** That is what the
   physical key does today on Android and TSF, and the ruling does not touch it.
2. **The compound — a modifier held with 한/영 — is the "special function" namespace**,
   not one function. The maintainer's words name two occupants (*such as* voice input,
   auto-mode switch), so the namespace is the decision and the occupants are its
   population.
3. **All three frontends.** Android, TSF and fcitx5 are in scope together, which is
   ReqMode13's "one value the engine holds, three readers" applied to this assignment:
   the value may not be three per-frontend choices that happen to agree.
4. **It is the default, not an option someone opts into** — and **the composition is
   selectable in the config dialog.** So the assignment is *remappable*, and the surface
   is the settings dialog each frontend already has, which answers the E5 sub-question
   "whether the assignment can be changed" and ReqMode13's "it has to be changeable"
   bullet. Per ReqMode13 the dialog must show the composition the engine actually
   honours, not a value the frontend invented.

The two-slot shape the priced options in `docs/OPTIONS-MEASURED.md` §E5-3 described
(option 1 and option 2 are both "single = toggle, compound = special") is what was
chosen; the ruling does not say *which* modifier, and it does not pick between those two
options' collision costs.

## What this does NOT settle

Named here rather than inferred at implementation time, because each one changes what
gets built:

- **Which modifier carries which function.** Android already ships `Shift`+한/영 as
  음성 입력 (`KeyAction.VOICE`, `defaults = Shift+KANA`). If that stays, the auto-switch
  needs a second modifier, and the priced collisions apply: `Alt`/`Ctrl`+한/영 in hangul
  mode is intercepted by TSF's transient-chord rule (ruled 2026-10-18,
  [[2026-10-18-tsf-chord-transient-not-persistent]]), so a Windows default there means
  carving an exception into that ruling or choosing a different modifier.
- **fcitx5's ownership of the key.** `Hangul` is one of fcitx5's default `triggerKeys`,
  so the press never reaches the addon by default. The addon *can* take it
  (`Instance::watchEvent` at `EventWatcherPhase::PreInputMethod`, or by the user removing
  `Hangul` from TriggerKeys) — measured in `docs/OPTIONS-MEASURED.md` §E5-2 addendum.
  Whether an addon may shadow a user's global trigger is a decision, not a measurement.
- **Where a changed composition is stored, given there is no shared store.** Each dialog
  is per platform (Android preferences, TSF registry/env, fcitx5 addon conf) and no store
  crosses them, so "remappable in the config dialog" still needs an answer to: does the
  engine hold the default and each frontend only the override, and how does the dialog
  string it shows stay the one the hook honours (ReqMode13's own condition)? Today three
  stores hold an assignment each (§E5-1), which is the divergence ReqMode13 forbids.
- **Which functions exist per frontend.** Voice is Android's alone today
  (`docs/FRONTEND-EQUIVALENCE.md` §8.3(4)); "for all three frontends" is a rule about
  the *mechanism* being uniform, and it does not by itself create a voice path on
  Windows.
- **The auto-switch state itself.** `grep -rn autoswitch libhunmin/ frontends/` is still
  0: the engine has no such option, so the function the compound would toggle does not
  exist yet, independently of the key that reaches it.

## Why the record matters later

The ruling is a **shape**, and the shape is the part that is expensive to change later:
it says the second meaning of the key is a namespace shared by several functions, and
that the three frontends expose the same one. Everything above is a parameter inside
that shape, so an implementation question that arrives as "which key" is the maintainer's
to answer and one that arrives as "may the engine own the table" is already answered.

The measurement behind it is not repeated here: `docs/OPTIONS-MEASURED.md` §E5-0 through
§E5-5 hold the premise checks, the collision map per platform and the priced options, and
`docs/SPEC-MODES.md` §E5 holds the requirement text.

See [[decisions-belong-to-the-maintainer]] — a ruling recorded because the maintainer
made it, with the sub-decisions it did not make listed rather than absorbed.
