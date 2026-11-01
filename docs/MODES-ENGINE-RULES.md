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

## Allocation (SPEC-MODES E5 — DECIDED 2026-10-31)

The maintainer ruled (verbatim, issue #159; recorded in
`wiki/decisions/2026-10-31-han-eng-modifier-compound-special-functions.md`): a single
press is the manual switch, and **modifier + 한/영 is the special-function namespace** —
voice input, auto-mode switch — as the **default**, **selectable in the config dialog**,
**on all three frontends**.

Three consequences for this document:

- **The value is an engine option, not a layout row.** The earlier question here ("the
  key combination placement can go in as a layout row, and that fits the convention
  better") is now answered by the requirement plus a measurement: the key must behave
  identically in every layout, and no layout can name it — 15 layouts, 47 keys each,
  **0 non-printable keys** (`tools/keymap-fixture.py`). So the hook stays top-level and
  the assignment is a value the engine holds (R13-정정), which the frontends read and the
  dialog writes.
- **It has to be readable as instructions, not computable per frontend.** The dialog
  shows the composition, so the engine owes a way to *describe* it (the R13-정정
  "explainable" bullet), beside the value itself.
- **The per-platform physical key stays the frontend's.** The engine holds the *semantic*
  assignment (which modifier carries which function); each frontend maps it to its own
  keycode (KANA 218 / `LANGUAGE_SWITCH` / bare `ALT_RIGHT`; `VK_HANGEUL`; fcitx5's
  `FcitxKey_Hangul`, which needs the trigger-ownership answer first — collision map:
  `docs/OPTIONS-MEASURED.md` §E5-2).

Still open, and named in the ruling record: which modifier carries which function,
whether the addon may shadow fcitx5's default `Hangul` trigger, and where a changed
composition is stored when no store crosses platforms.

## Where to place it

- It must be a **top-level key hook** rather than a standalone key, so that the requirement
  "identical whichever layout it is pressed in" holds.
- This hook must be processed before inference — pressing the hangul/latin key while inference
  is on is the signal "do not do this automatically".

## The special-function dispatch (implemented 2026-10-31)

Ruled the same day, in the maintainer's words: *"each frontend can have default key code and
custom ones and provide it to engine. then engine processes them and invoke frontend side
interface which implements the feature or processes by itself. if the feature is not
supported at the platform, it can just refuse to service."* Recorded in
`wiki/decisions/2026-10-31-special-functions-engine-resolved-frontend-implemented.md`.

The split it fixes, which is what removes the three coexisting mechanisms:

| who | owns |
|---|---|
| frontend | the platform key codes (default + custom), mapping them to `hunmin_key_event` with `language_key` and `mods`, and the **implementation** of a function it alone can perform |
| engine | the binding `function → chord bits`, the resolution of one event to one function, the refusal, and the functions it can perform itself |

The seam is five append-only symbols (`hunmin.h`): `hunmin_buffered_preedit_special_bind()`
(the frontend seeds the table from its default, then its store), `..._special_chord()` (the
dialog reads what is bound), `..._special_host()` (registers `{user, supports, perform}`),
`..._special()` (resolve and service; returns the function, `0` for not-ours, `-1` for
refused) and `hunmin_special_name()` (one spelling for dialogs and logs). The classifier
stays pure: `classify()` answers `HUNMIN_KEY_LANGUAGE_CHORD` for the gesture and never runs
foreign code — `..._special()` is the one call that can, and the frontend makes it.

Two consequences worth keeping in view: the engine **ships the ruled defaults** (`VOICE = Alt`,
`AUTOSWITCH = Ctrl`, 2026-10-31) so a frontend that seeds nothing still agrees with the other
two, and a frontend that has its own store replaces one entry; and a refusal is `-1`, which the
frontend answers by passing the key through — a refusal that swallowed the press would be a
dead key.

**SHIFT is a special combination with the language key, and nothing else.** The ruling is the
maintainer's, verbatim: *"make shift to be available for special key combination. if it is used
with normal characters, work as capitalize. but with special combination such as shift + han/eng,
it should be able to do special work."* The engine does not widen `HUNMIN_MOD_CHORD` for this:
the event already carries Shift in its own field, and the language-key branch adds it —
`special_want()` = chord bits | SHIFT, called by both `classify()` and `..._special()` so the
gesture recognised and the gesture serviced cannot drift. An ordinary character therefore never
reaches a SHIFT binding (capitalize keeps working), and a chord with no binding — Shift
included — still answers the plain toggle.

### The value the `AUTOSWITCH` function flips (issue #165, 2026-11-01)

R13-정정 says the on/off state is **one** value and the engine owns it, with a key
combination and a settings surface as two paths onto the same state. The state now
exists, and only the state: what the surfaces are wired to is listed here so the gap
is a recorded one and not a surprise.

| what | measured fact |
|---|---|
| the name | `"autoswitch"`, accepted by `hunmin_ic_set_option()` and through the layer by `hunmin_buffered_preedit_set_option()`. Value contract is the one `"caps"` already has: coerced by `!!`, so any non-zero is ON (appended to the ABI, `libhunmin/tests/abi/symbols.txt` 52 → 54) |
| the default | OFF with no option call at all — the B3 ruling of 2026-10-11 (the feature's additional data ships separately and the option is the transition lever) |
| the query | `hunmin_ic_get_option()` / `hunmin_buffered_preedit_get_option()`, returning the value through an out-parameter. An unknown name answers `false` and leaves the caller's int untouched; the `w/W/z/h/H` overrides answer `-1` for "not overridden, the layout default", which is a value and not an unknown name |
| where it lives | a field on `hunmin_ic`, and `attach_ic()` carries it across the ic re-creation a language toggle performs. Without that carry, a settings surface writes ON and the next 한/영 press silently restores OFF — measured red by `KeyRuleTest.TheAutoswitchStateSurvivesALanguageToggle` against the pre-carry code. `hunmin_ic_reset()` does **not** clear it: a preference is not input state |
| the key beside it | already the engine's, unchanged: `hunmin_buffered_preedit_special_chord(l, HUNMIN_SPECIAL_AUTOSWITCH)` answers the ruled default `Ctrl` (Table A, 2026-10-31), so a dialog that shows value + chord spells neither of them itself |
| the transition | `hunmin_buffered_preedit_special()` flips the value when it services `AUTOSWITCH` — the engine's, not each frontend's `perform` (ruled 2026-11-01, below). Only a serviced press flips; `special_peek()` never does |

**The transition is the engine's (ruled 2026-11-01, issue #165).** Verbatim:

> The engine, in hunmin_buffered_preedit_special() before the host's perform — one line,
> one home, all three frontends agree by construction

So `hunmin_buffered_preedit_special()` flips `autoswitch` beside the resolution and *then*
calls the host's `perform`, which stays the platform half (TSF records the request, Android
logs "not yet implemented"). The consequences worth keeping straight:

- **Only a serviceable press flips.** The flip sits after `special_peek()`'s `fn <= 0`
  return, so a refusal (`-1`, no host or `supports()` said no) and a not-ours event (`0`)
  leave the value alone — a platform that cannot honour the state does not move it.
- **`special_peek()` never flips.** It answers a platform "test" phase, and a test that
  changed the state would toggle twice per key press.
- **No frontend writes it from `perform`.** With the engine owning the transition, a
  frontend's `perform` writing the option again would double-flip; what a frontend does
  there is the feature work the data gate enables.
- **The B3 data gate is still unruled, and deliberately not in this path.** Nothing here
  refuses to turn the feature ON when the data it needs is absent, and the key does not
  consult whether that data is present — "where the data gate is checked" is 0.3 design
  (`docs/SPEC-MODES.md` §R13-정정, the default bullet). The value is stored either way.
  `hunmin_ctx_layout_autoswitch_capable()` is **not** that gate, despite the name: it answers
  whether a layout's keys can compose any jamo at all for the judgement to be about
  (`docs/SPEC-MODES.md` §E, the capability table), which is a different question from
  whether the feature's dictionary is installed.

Pinned by `KeyRuleTest.TheAutoswitchOptionIsOneValueTheLayerReadsBack` (default, both
writes, the `!!` coercion, the unknown-name rejection, the chord),
`KeyRuleTest.TheAutoswitchStateSurvivesALanguageToggle` (the carry, both directions, and
`reset()` leaving it alone) and `KeyRuleTest.TheAutoswitchChordFlipsTheOneValue` (peek does
not flip, a serviced press flips both ways, a refusal does not, and VOICE is not a second
writer).

### Windows (TSF) — the second frontend on the dispatch (2026-10-31)

`frontends/tsf` wires the same five symbols. What the adapter owns and what it asks:

- **The physical key with ANY modifiers is the entry point** (`IsLanguageKeyVk`), while
  the plain toggle keeps its old exact-modifier match. The distinction is measured, not
  stylistic: the exact match is what keeps a configured `Shift+Space` binding from firing
  bare, and it is also why a `Ctrl+한/영` press never reached the layer at all until the
  bare-only lookup was added — and why that lookup matches **bare entries only** (a
  configured alias whose key is Space turned every ordinary space into the language key
  once).
- **A host is registered per layer** (`RebuildLayer`), beside the layer's construction.
  `supports()` answers per function: `AUTOSWITCH` is serviceable and the perform records
  the request (the state itself is 0.3's data-gated feature, issue #2 — the same shape
  Android's `toggleAutoHangulEnglish` has), `VOICE` **refuses** because this frontend has
  no recogniser, which is exactly the ruling's "if the feature is not supported at the
  platform, it can just refuse to service". A refusal passes the key: nothing is
  swallowed.
- **The test phase asks the engine, not a guess.** `hunmin_buffered_preedit_special_peek()`
  was added for this: TSF answers `OnTestKeyDown` before `OnKeyDown`, and a host told
  "mine" in the test that then does not receive the down drops the key (the measured
  `dkssud1` defect class). The peek runs the same resolver without calling the host, so
  test and down are one decision — the same role `hunmin_ic_peek_key()` has.
- **`HUNMIN_MOD_GUI` never reaches the engine from TSF**: `hunmin_tsf_keymods_from_state()`
  derives SHIFT/CTRL/ALT only, so a GUI chord bound in the table simply does not fire on
  Windows. Recorded, not fixed — the modifier vocabulary is the frontend's, by the ruling.

Pinned by `TsfConfigTest.TheHanEngKeyCarriesTheSpecialFunctionOnAChord` (performed /
refused / unbound-is-the-toggle, all three) and, on the engine side,
`KeyRuleTest.ThePeekAnswersWhatTheCallWouldDoAndPerformsNothing`.

## Can it be expressed as data

AGENTS.md: "Layouts are data, never code." The earlier reading here was that the key
combination placement could go in as a layout row. **E5's ruling closes that off:** the
key must behave identically in every layout while the hook is the physical 한/영 key,
which no layout names (0 non-printable keys across 15 layouts), and the composition has
to be readable and changeable from the config dialog. So it is an engine-held option
whose *semantics* are data-shaped (a function→modifier table) and whose *storage* is the
existing `set_option` path — not a row in any layout file.

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