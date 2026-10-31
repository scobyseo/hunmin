# MODES-ENGINE-RULES — Engine rules for hangul/latin inference

Requirements: `docs/SPEC-MODES.md`. This document covers **which state machine those requirements (R4,
R3, R13) are implemented as inside the engine** and its load cost.

Status: **design direction settled, no core implementation.** The load items are
pre-measurement.

> **The adapter-side detector ships today, and it is the opposite structural
> choice from §1.** `frontends/fcitx5/src/language_detect.{h,cpp}` judges by
> holding a second `hunmin_ctx` on a Latin layout and replaying the buffered word
> when the in-flight preedit empties — not "the mode lives in `hunmin_ctx`".
> Since this section was written it gained a tier-D1 test
> (`engine-d1-language-detect`) and a documented contract; what has NOT changed
> is that its answer is discarded by the caller, so nothing user-visible depends
> on it (issue #132). Which design stands is the maintainer's — R3's objection
> (two frontends answering differently) bites the day the answer is used.

---

# 1. Why engine rules (the result of R3)

If inference is left in the adapter, fcitx5 and Android each end up with their own inference engine, and
the two can give different answers. Then the fact of "the same engine" becomes false. So:

- The decision happens inside `libhunmin`.
- The frontend only (a) passes the key and (b) displays preedit/commit and
  (c) **merely relays** the on/off key combination.
- Since the rule is singular, one core golden proves the behaviour of every frontend.

## Where the mode lives

Currently `hunmin_ic` holds only **one** layout (`libhunmin/src/ic.c:19-34`). Putting the mode
in ic means one ic needs two layouts, and the 조합 버퍼 (composition buffer) becomes two as well.
The romaja layout does not use the composition buffer at all, so which one is alive becomes
unknown to ic — contamination risk.

Therefore **the mode lives in `hunmin_ctx`, and one ctx is one session.** Layout switching is
judged by the engine as a language switch and reported via preedit/commit. The frontend keeps supplying
only the layout id. Only one composition buffer exists — this structure is the premise of Plan B.

- The API is append-only. Existing symbols are not touched.
- We expect about 2 new query hooks as symbols (mode query, on/off toggle).
  `mode` is **not added** to the symbol — ABI append-only convention.
- On settling, update `libhunmin/tests/abi/symbols.txt` with the `nm` command.

## The meaning of the layout-support decision changes

fcitx5 and Android filter the layout selection UI with `hunmin_ctx_layout_supported()` (AGENTS.md).
When a mode is added, the meaning of this filter is **extended from "can this engine execute this
layout" to "does this layout belong to this mode"**. Since both frontends go through the same filter
they cannot diverge, but it is a **test target** — if only one side is fixed they diverge
immediately.

---

# 2. 조합 버퍼 (composition buffer) (the implementation of R4)

## Definition

From just after the previous delimiter to the next delimiter. **Not syllable unit.**

## The gap between the current engine and the requirement

| | Current | R4 requirement |
|---|---|---|
| commit point | immediately, per completed syllable | at the delimiter |
| preedit content | one syllable (`jaso_to_string`) | the whole buffer |
| decision point | none | at the delimiter moment, 1 time |

Measurement evidence is in SPEC-MODES R4 (`world` → `재` is finalized on the 4th key).

## The fact that the decision happens only once eliminates the ghost

SPEC-MODES E6 wrote that the ghost arises **only when the decision is moved forward to the
middle**. If we decide once at the delimiter, the display does not change twice.

Therefore ghost prevention and the space-timing decision are **satisfied at the same time.** E6's
mutual exclusivity of the options truly holds at the moment we decide to move the decision forward.

---

# 3. on/off key hook (R13)

`hunmin_ic_set_option()` has a precedent of accepting a preedit-string override
(`ic.c:610`). A new option is added here.

## Allocation (SPEC-MODES E5 — UNDECIDED)

Settled by the user: via a key combination, no UI, identical in all environments.
UNDECIDED: what single input and compound input each do. The F9 constraint
recorded here — preedit-string occupies F9, no AOSP keycode on Android — was
measured 2026-10-31 and does not hold as written: F9 is no layout key, and
`KeyEvent.KEYCODE_F9` is 139. What constrains the allocation is the per-platform
collision map, priced in `docs/OPTIONS-MEASURED.md` §E5-0 and §E5-2 (option list
§E5-3); read that rather than this paragraph when choosing.

## Where to place it

- It must be a **top-level key hook** rather than a standalone key, so that the requirement
  "identical whichever layout it is pressed in" holds.
- This hook must be processed before inference — pressing the hangul/latin key while inference
  is on is the signal "do not do this automatically".

## Can it be expressed as data

AGENTS.md: "Layouts are data, never code." The key combination placement can go in
as a layout row, and going that way fits the convention better. Which way to go is tied to the
E5 decision.

---

# 4. Load — pre-measurement [E]

The cost of the engine invariant change that Plan B (extending the buffer to the delimiter) requires.
**Since nothing has been measured, the following is not an estimate but a list of things to confirm.**

| Item | Why |
|---|---|
| libhangul parity | `hunmin_ic_process()` goes beyond "one key processing" to "word processing". Full re-verification of goldens needed |
| composition unit change | syllable → word. The composition boundary of every layout changes |
| `STR_CAP` 64 truncation | When the buffer overflows it is **silently truncated.** The effective ceiling of the cap (SPEC-MODES E2) |
| preedit length | The point the user warned about directly. Editors differ in reaction (NOT MEASURED) |
| ROMAJA | The only engine that does not use the composition buffer. What does this side look at when deciding |

## How to measure

Parity becomes existing infrastructure — all of `tests/golden/` is tier A. preedit length and
truncation need real editors as targets, and Android does not have a soft keyboard yet
(AGENTS.md P5). **Do not recommend starting implementation without the numbers in this
document.**

---

# 5. References

- Requirements: `docs/SPEC-MODES.md`
- Basis of libhangul parity: `docs/FRONTEND-CONTRACT.md`, AGENTS.md
- Golden regeneration: `tools/regen-goldens.sh`