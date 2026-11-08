# DESIGN-CORE — Core (libhunmin) design decisions

> Design home for multi-place libhunmin decisions (issue #272, Phase 3).
> Requirement text lives only in `docs/SPEC-*.md`; cite REQ ids, never restate.
> Local behavioural facts stay as 1–3 line code comments; multi-place rationale
> is gathered here and linked by heading (never `file:line`). Figures/measurements
> live in their authoritative homes and are cited by pointer.

**Status:** in progress — P3 moved key judgement/autoswitch/cap/display/reading
rationale from `buffered_preedit.c`/`hunmin.h` (comments only). `ic.c` parity
comments are protected by `CODE_RULES.md` `[doc-why-not-what]`; `tables.c` is
generated (exempt).

## Related decisions (gathered)

| Area | REQ | Design/mechanism | Implementation notes |
|---|---|---|---|
| ReqMode8/E1 judgement at boundary | ReqMode8, ReqMode8, ReqMode3, ReqMode9, ReqMode10 | `docs/DESIGN-MODES.md#§1`, `docs/MODES-DATA.md#§4-7` | `judge_boundary`, `take_judgement`, ReqMode9 `JUDGE_MIN_KEYS`, per-syllable => no retroactive judgement |
| B3/ReqMode13 autoswitch state & gate | ReqMode13, ReqMode13 | `docs/DESIGN-MODES.md#§3` | `autoswitch` option, `hunmin_ctx_autoswitch_data_present`, `special_peek`/`special` for `HUNMIN_SPECIAL_AUTOSWITCH` |
| ReqMode4 word cap/overflow | ReqMode4, ReqMode4 | `docs/DESIGN-MODES.md#§2` | `word_flush_half`, UTF-8 boundary cut, no judgement on flushed half |
| E5 special chords | E5, ReqMode13 family | `docs/DESIGN-MODES.md#§3` | `special_mods`/`special_host`, one resolver for peek+perform |
| E6 judged display timing | E6, ReqMode8, ReqMode10 | `docs/DESIGN-MODES.md#§1` (display/policy context), `docs/OPTIONS-MEASURED.md` §BND-4 | `judge_timing`, `judged_display` (display only) |
| ReqMode8/3 readings & validity | ReqMode8, ReqMode8 | `docs/DESIGN-MODES.md#§1`, `docs/MODES-DATA.md` | replayed `latin_out`/`hangul_out`, `hangul_valid_for` widened sets |

## ReqMode8 judgement — judge_boundary implementation

Design/formula: `docs/DESIGN-MODES.md#§1` (REQ ReqMode8/ReqMode8, ReqMode3). This section
records core implementation decisions that were repeated in
`buffered_preedit.c`/`hunmin.h` and are now linked from code.

- **When it runs:** once at the word boundary, only when `autoswitch` is on
  (ReqMode13/B3, identity path when off), the layer has a Latin layout to read
  against, `keys >= JUDGE_MIN_KEYS` (ReqMode9, key-input count), and the English
  wordset is ready (fail-closed if absent).
- **Hangul-mode line:** compares `english_valid(latin_out)` vs
  `hangul_valid_for(word)`; switches only when Latin is a dictionary word and
  the Hangul reading is not well-formed for the active Hangul layout
  (ReqMode8 uses widened 옛한글 sets where the layout composes archaic jamo).
  A layout that composes no Hangul abstains.
- **Latin-mode / E1 line:** symmetric check over the Hangul reading in
  `hangul_out` vs the typed Latin word; switches when Hangul reading is
  well-formed and the Latin text is not a dictionary word. Requires Hangul
  syllable validity; a non-Hangul-composing layout cannot produce a valid
  syllable, so it does not switch.
- **Who acts:** engine judges and returns the text to deliver for the
  triggering word (ReqMode10); frontend performs the host mode switch via
  `hunmin_buffered_preedit_take_judgement()` after the boundary key.
- **Han/Eng press:** callers that represent an explicit user toggle pass
  `judge=false` so ReqMode13 takes precedence over ReqMode8 inference.

## ReqMode8/3 readings and validity

REQ: `docs/SPEC-MODES.md` ReqMode8/ReqMode8. Implementation fields/helpers in
`buffered_preedit.c` (`english`, `keys`/`latin_out`/`hangul_out`, replay readers,
`hangul_valid_for`).

- `english` is the modes wordset (NULL until loaded/asked for).
- Hangul layouts cannot produce Latin readings directly; `latin_out` is the
  finished word's keys replayed through the Latin layout (ReqMode2 shadow reading).
  `keys` stores (base,shift) slots for the word in flight.
- `hangul_out` is the Hangul reading replayed through the Hangul layout for the
  symmetric/E1 path.
- ReqMode8: Hangul validity uses widened 옛한글 sets when the active Hangul
  layout composes archaic jamo (`hangul_valid_for`).

## B3 autoswitch data gate and default

REQ: `docs/SPEC-MODES.md` ReqMode13 / B3 (autoswitch OFF by default; ON requires
modes data). Implementation: `ic`/`buffered_preedit` autoswitch option,
`hm_modes_data_present`, `hunmin_ctx_autoswitch_data_present`,
`judge_boundary` identity path, and `special_peek`/`special` for
`HUNMIN_SPECIAL_AUTOSWITCH`.

- Default OFF is the identity path (no reading/lookup/delivery change).
- ON is refused (and chord refuses) when modes/English word data is absent;
  frontends read the engine's answer, they do not re-implement the gate.
- A serviced autoswitch special press flips the engine-owned state; refused
  or unrelated events leave state unchanged.

## E5 special-function chords

REQ: `docs/SPEC-MODES.md` §E5. Implementation: `special_mods`/`special_host`,
`hunmin_buffered_preedit_special_{peek,set_mods,set_host}`.

- Function→chord mapping is seeded by frontend defaults/store; zero/absent
  means not bound (platform refuses unimplemented function).
- Peek is the test/claim path; `special()` performs and must resolve the same
  gesture (one resolver, two entry points).

## E6 judged display (judge_timing)

REQ: `docs/SPEC-MODES.md` §E6 (ruled: default shows judgement per key).
Implementation: `buffered_preedit.c#judged_display`, `judge_timing` option.

- Returns text to **show** for the in-flight word when `judge_timing==1`
  (per-key), or NULL to keep the current rendering. Uses the same
  `english_valid`/`hangul_valid_for` questions as `judge_boundary()` over
  recorded keys and rendered text so display and delivery cannot disagree.
- Does **not** deliver, switch, or record a judgement; delivery/judgement
  still happen at the boundary (`judge_boundary`, `take_judgement`) per ReqMode10.
- Cost accepted by ruling: per-key replay is O(keys) (bounded by 256 B word
  cap) and can flicker (measured in `docs/OPTIONS-MEASURED.md` §BND-4).
- Option: `judge_timing` 1 = per key (default), 0 = boundary only.

## ReqMode4 cap (word_flush_half)

REQ: `docs/SPEC-MODES.md` ReqMode4 (word cap/overflow). Implementation in
`buffered_preedit.c#word_flush_half`.

- Bounds what the frontend renders (word + in-flight syllable) at `l->cap`;
  delivers the first half when exceeded so preedit stays bounded and the tail
  keeps composing.
- Does **not** judge the flushed half (`judge_boundary` runs only at
  `boundary()`); later judgement applies to the remainder. `keys_clear()`
  drops the reading's raw material, so the boundary reads only the tail's
  keys (measured: `docs/OPTIONS-MEASURED.md` §BND-2).
- Cuts at a UTF-8 lead byte boundary (never fragments a syllable).
- Drops the Latin key log on flush rather than splitting; a half-word reading
  is not meaningful, so fail-closed avoids wrong switches.
- On allocation failure, leaves the word whole (does not drop text already
  decided for delivery).

## Notes

- Symbol renames break `docs-lint`/`REQ-MAP`/`abi` — P3 moves rationale only.
- A requirement with no implementation/verification is a finding (see
  `docs/REQ-MAP.md`); do not smooth it over here.
