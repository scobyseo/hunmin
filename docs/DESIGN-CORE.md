# DESIGN-CORE — Core (libhunmin) design decisions

> Design home for multi-place libhunmin decisions, per issue #272 (Phase 3).
> Requirement text lives only in `docs/SPEC-*.md`; cite REQ ids, never restate.
> Local behavioural facts stay as 1–3 line code comments; multi-place rationale
> is gathered here and linked by heading (never `file:line`).

Status: **skeleton** — headings identify decisions the issue measured as repeated
across `libhunmin/include/hunmin.h` and `libhunmin/src/buffered_preedit.c`
(R8, B3, R4-정정, E5, E6, R8-정정-2/3). Content is moved here incrementally as
Phase 3 progresses; until moved, the authoritative rationale may still appear in
code/design docs referenced by `docs/REQ-MAP.md`.

## R8 — switch only when clearly the other language

REQ: `docs/SPEC-MODES.md#§R8`. Design/mechanism: `docs/DESIGN-MODES.md#§1` and
`docs/MODES-DATA.md#§4-7` (figures cited by pointer). Implementation seam:
`libhunmin/src/buffered_preedit.c#judge_boundary`,
`libhunmin/include/hunmin.h#hunmin_buffered_preedit_take_judgement`.

*Phase 3 TODO:* move repeated R8 rationale from core code comments here.

## B3 — autoswitch default/data gate

REQ/design context: `docs/REQ-MAP.md` R13-정정 row; `docs/DESIGN-MODES.md#§3`.

*Phase 3 TODO:* gather B3-related multi-place rationale.

## R4-정정 / buffer-boundary judgement

REQ: `docs/SPEC-MODES.md` R4/R4-정정 family. Design: `docs/DESIGN-MODES.md#§2`.

*Phase 3 TODO:* move repeated boundary/judgement-timing rationale.

## E5 / E6 — special functions and key identity

REQ: `docs/SPEC-MODES.md` E5/E6 (and R13 family). Design: `docs/DESIGN-MODES.md#§3`.

*Phase 3 TODO:* move repeated E5/E6 rationale.

## R8-정정-2 / R8-정정-3

REQ: `docs/SPEC-MODES.md` R8-정정-2 / R8-정정-3. Design: `docs/DESIGN-MODES.md` /
`docs/MODES-DATA.md` as cited by `docs/REQ-MAP.md`.

*Phase 3 TODO:* move repeated validity/reading rationale.

## Notes

- `libhunmin/src/ic.c` parity comments are protected by `CODE_RULES.md`
  `[doc-why-not-what]`; do not move them mechanically.
- `libhunmin/src/tables.c` is generated (exempt).
- Symbol renames break `docs-lint`/`REQ-MAP`/`abi` — Phase 3 does comment moves
  only.


## R8 judgement — judge_boundary implementation

Design/formula: `docs/DESIGN-MODES.md#§1` (REQ R8/R8-정정, R3). This section
records core implementation decisions that were repeated in
`buffered_preedit.c`/`hunmin.h` and are now linked from code.

- **When it runs:** once at the word boundary, only when `autoswitch` is on
  (R13-정정/B3, identity path when off), the layer has a Latin layout to read
  against, `keys >= JUDGE_MIN_KEYS` (R9, key-input count), and the English
  wordset is ready (fail-closed if absent).
- **Hangul-mode line:** compares `english_valid(latin_out)` vs
  `hangul_valid_for(word)`; switches only when Latin is a dictionary word and
  the Hangul reading is not well-formed for the active Hangul layout
  (R8-정정-3 uses widened 옛한글 sets where the layout composes archaic jamo).
  A layout that composes no Hangul abstains.
- **Latin-mode / E1 line:** symmetric check over the Hangul reading in
  `hangul_out` vs the typed Latin word; switches when Hangul reading is
  well-formed and the Latin text is not a dictionary word. Requires Hangul
  syllable validity; a non-Hangul-composing layout cannot produce a valid
  syllable, so it does not switch.
- **Who acts:** engine judges and returns the text to deliver for the
  triggering word (R10); frontend performs the host mode switch via
  `hunmin_buffered_preedit_take_judgement()` after the boundary key.
- **Han/Eng press:** callers that represent an explicit user toggle pass
  `judge=false` so R13 takes precedence over R8 inference.


## R4-정정 cap (word_flush_half)

REQ: `docs/SPEC-MODES.md` R4-정정 (word cap/overflow). Implementation in
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


## E6 judged display (judge_timing)

REQ: `docs/SPEC-MODES.md` §E6 (ruled: default shows judgement per key).
Implementation: `buffered_preedit.c#judged_display`, `judge_timing` option.

- Returns text to **show** for the in-flight word when `judge_timing==1`
  (per-key), or NULL to keep the current rendering. Uses the same
  `english_valid`/`hangul_valid_for` questions as `judge_boundary()` over
  recorded keys and rendered text so display and delivery cannot disagree.
- Does **not** deliver, switch, or record a judgement; delivery/judgement
  still happen at the boundary (`judge_boundary`, `take_judgement`) per R10.
- Cost accepted by ruling: per-key replay is O(keys) (bounded by 256 B word
  cap) and can flicker (measured in `docs/OPTIONS-MEASURED.md` §BND-4).
- Option: `judge_timing` 1 = per key (default), 0 = boundary only.


## B3 autoswitch data gate and default

REQ: `docs/SPEC-MODES.md` R13-정정 / B3 (autoswitch OFF by default; ON requires
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


## R8-정정-2/3 readings and validity

REQ: `docs/SPEC-MODES.md` R8-정정-2/R8-정정-3. Implementation fields/helpers in
`buffered_preedit.c` (`english`, `keys`/`latin_out`/`hangul_out`, replay readers,
`hangul_valid_for`).

- `english` is the modes wordset (NULL until loaded/asked for).
- Hangul layouts cannot produce Latin readings directly; `latin_out` is the
  finished word's keys replayed through the Latin layout (R2 shadow reading).
  `keys` stores (base,shift) slots for the word in flight.
- `hangul_out` is the Hangul reading replayed through the Hangul layout for the
  symmetric/E1 path.
- R8-정정-3: Hangul validity uses widened 옛한글 sets when the active Hangul
  layout composes archaic jamo (`hangul_valid_for`).
