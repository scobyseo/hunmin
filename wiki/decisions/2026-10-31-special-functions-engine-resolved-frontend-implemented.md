---
title: The special functions are engine-resolved and frontend-implemented
created: 2026-10-31
updated: 2026-10-31
type: decision
status: active
tags: [decision, engine, keybinding, fcitx5, android, tsf]
sources:
  - ../../../docs/SPEC-MODES.md
  - ../../../docs/MODES-ENGINE-RULES.md
  - ../../../docs/OPTIONS-MEASURED.md
confidence: high
---

# The special functions are engine-resolved and frontend-implemented

Settles the mechanism behind the assignment ruled the same day
([[2026-10-31-han-eng-modifier-compound-special-functions]]): *how* a compound press
becomes a function, now that the compound press is a required gesture on all three
frontends and each frontend had grown its own version of the answer.

**Maintainer, 2026-10-31** (verbatim):

> i think that each frontend can have default key code and custom ones and provide it to
> engine. then engine processes them and invoke frontend side interface which implements
> the feature or processes by itself. if the feature is not supported at the platform, it
> can just refuse to service. current implementation has different implementation mechs per
> frontend, so that duplicated implementation seems to coexist.

## What was ruled

1. **The frontend supplies the platform key codes** — its default and any custom ones —
   because only it knows its own event stream. Nothing in the engine names a keycode.
2. **The engine processes them**: it owns the binding (function → chord), the resolution of
   one key event to one function, and the refusal.
3. **The engine invokes a frontend-side implementation** (a host interface) for functions it
   cannot perform itself, and **performs the engine-native ones itself**.
4. **An unsupported function is refused**, not emulated: a platform with no recorder says
   so, and the press is not serviced.
5. **The per-frontend duplicate mechanisms are the thing being removed** — Android's
   `KeyAction` enum + preferences store, TSF's config lists + `keybinding.cpp` matcher, and
   fcitx5's absence of either are not three legitimate implementations of one rule.

## The shape that implements it

`libhunmin/include/hunmin.h` gained five append-only symbols and two types (the ABI list is
`libhunmin/tests/abi/symbols.txt`, regenerated with the `nm` command the abi test prints):

```
hunmin_special_function        NONE / VOICE / AUTOSWITCH / LIMIT
hunmin_special_host            { user, supports(), perform() }
hunmin_buffered_preedit_special_bind(l, fn, mods)   the frontend seeds it
hunmin_buffered_preedit_special_chord(l, fn)        the dialog reads it
hunmin_buffered_preedit_special_host(l, host)       the implementation
hunmin_buffered_preedit_special(l, ev)              resolve + service
hunmin_special_name(fn)                             one spelling for dialogs and logs
```

`hunmin_buffered_preedit_classify()` stays pure and keeps answering
`HUNMIN_KEY_LANGUAGE_CHORD` for the gesture; `..._special()` is the one call that can run
someone else's code, and the frontend makes it — the engine does not bury a callback inside
the classifier.

## What this does NOT settle

- **Which chord each function gets.** The table starts empty and the frontend seeds it, so
  the default modifier per function is still a value someone has to choose — and choosing it
  per frontend is exactly the divergence R13-정정 forbids, so it wants one answer, not three.
  Whether the engine should ship a default table the frontends may override is open.
- **What "refuse to service" does to the key.** The implementation reads it as: the engine
  answers `-1`, and the frontend **passes the key through** rather than swallowing it —
  a refusal that eats the press would be a silent dead key. That is a reading of the
  ruling, not a second ruling.
- **Whether the engine also owns the `autoswitch` state** the function flips. Today the
  option does not exist (`grep -rn autoswitch libhunmin/` → 0), so `AUTOSWITCH` resolves
  and is serviced by whoever registered a `perform` for it.
- **The two-sided tie between a function and its platform mapping.** Android's 한/영 arrives
  as three keycodes, TSF's as `VK_HANGEUL`, fcitx5's as a keysym it currently never sees. The
  engine's vocabulary has one flag (`language_key`) for all of them, which is deliberate —
  but it means the engine cannot tell which physical key produced the press, and a design
  that needs that distinction would have to widen the event.

## Why the record matters later

The ruling removes a class of defect rather than one instance: before it, "is this chord a
setup function?" was answered in three places that could disagree, and the settings dialogs
had three opportunities to advertise a key the hook did not honour. The engine-resolved
shape makes that disagreement impossible by construction, and the price is named above —
the default chord has to be chosen once, and the platform keeps the part only it can know.

Measurement and provenance for the engine change: `docs/OPTIONS-MEASURED.md` §E5-6;
requirement: `docs/SPEC-MODES.md` §E5; design: `docs/MODES-ENGINE-RULES.md` §3.

See [[decisions-belong-to-the-maintainer]].
