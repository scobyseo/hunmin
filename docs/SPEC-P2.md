# SPEC-P2 — 신세벌식 P2 engine requirements (living document, P0 edition)

Normative source: pat.im/1136 (팥알). Ground-truth artifacts vendored under
`data-orig/naraeseot/` ( 날개셋 ist XML exported by the designer, 2023-12-09+):

| file | ist name | role |
|---|---|---|
| `sin3-p2.basic.xml` | 신세벌식 P2.ist (2018-04-10 revision) | basic layout |
| `sin3-p2.symbols.xml` | 신세벌식 P2 + 기호 확장 (2024-01-19) | + 6-tier symbol extension, 초성체, CapsLock/W modes, ㄸ/ㅃ/ㅉ batchim combos |
| `sin3-p2.yethangul.xml` | 옛한글 조합 + 기호 확장 (20221224) | + 옛한글 variant composition rules (the '조합' scheme; '확장' scheme withdrawn upstream 2022) |

## ist file anatomy (verified by inspection)

One `<KeyTable name flag from>` with `<Key at value/>` for both unshifted (`0x61`) and
shifted (`0x41`) states. `<GeneratorSetting object="CNgsIme">` holds the engine model:

- `UnitMixTable` — combination rules `unit=CHO|JUNG|JONG a b to` (536 rules across files).
- `VirtualUnitTable` — virtual vowel units: **501=ㅗ, 502=ㅜ, 503=ㅡ, 504=ㆍ** (the
  right-hand "variant" vowels used to build compounds; appear in key expressions as
  `0x1F50000…0x1F80000` and in `E==0x1F6`-style comparisons).
- `AutomataTable default=0 predefined="SEQUENTIAL"` — states: 0 initial, 1 초성,
  2 초성+중성, 3 +종성; transitions by conditions A/B/C (the syllable state machine our
  engine must mirror).
- `Extra/Bksp` — per-state backspace policy (`BySyllable` / `ByUnitStep`).
- symbols/yethangul add: `UserKeyTable`, `UserCompo`, `HanSubstTable`, shortcut, and
  前처리수식 flags (pat.im §2-6: w/W/z/h/H).

### Key-value expressions (the 갈마들이 core)

Values like `D&&!E ? H3|YU : H3|_Q` combine state variables with jamo tokens:
`D` ≈ "syllable expects/accepts jungseong" (automaton ≥1 with composable vowel),
`E` = previously pressed unit code (0x1F5.. = virtual-unit or Jong context tests),
`F` = auxiliary mode (초성체/CapsLock per file). Tokens: `X_` = base unit, `_X` =
jongseong reading, uppercase digraphs = ssang/compound (`GG`=ㄲ, `WA`=ㅘ, `RM`=ㄺ…);
mnemonics follow 날개셋 romanization and MUST be pinned by extraction from UnitMixTable,
not memory (see TODO).

## Verified cross-checks vs kime `sebeolsik-3-90…p2` YAML (2026 transcribe)

Concordant: a(ㅠ/jong ㅇ), b(ㅜ/jong ㅊ), c(ㅔ/jong ㄱ), d(ㅣ/jong ㅎ), e(ㅐ/jong ㅂ),
f(ㅏ/jong ㅍ), g(ㅡ/jong ㄷ), j(ㅇ), k(ㄱ), l(ㅈ), ;(ㅂ), '(ㅌ), z(ㅁ+ㆍ), m(ㅎ),
shift Q/E/R/T→ㅒ ㅐ ㅓ ㅕ, shift A–G→ㅠ ㅖ ㅣ ㅏ ㅡ (right col), Y→×, U→○, M→…,
P→;, "right-vowel" variants for i/o/p match kime's `ㅁㅡ`, `ㅊㅜ`, `ㅋㅗ` pairs via 503/502/501.

**Divergence (ist wins — kime likely stale):** shifted H is `□`(U+25A1) in the
2018-04-10 ist vs `ㄴ` in kime. Log all such deltas here as the table is converted.

## P1 conversion TODOs (ist2data.py)

1. Extract mnemonic→codepoint map from UnitMixTable structure + UnitAbbrTable +
   날개셋 docs; assert bijection against ist used chars; never hardcode from memory.
2. Map AutomataTable A/B/C/D/E/F conditions onto hunmin engine state predicates;
   define the `case` value form in SPEC-SCHEMA (v1.1) and emit it verbatim from ist.
3. Port UnitMixTable → `data/compose/sin3-p2-*.json` (+ modern-full base where shared).
4. Symbol tiers (UserKeyTable in symbols.xml, j+k/l/; × shift → 6 tiers) → reserved
   `seq` primitive;CapsLock/W modes, ㄸ/ㅃ/ㅉ via 2024 combos.
5. 옛한글 UserCompo rules → separate compose table; 2 withdrawn 2022-12-24 rules excluded.

## Seed golden cases (from pat.im text; encode in tests/golden/sin3-p2.jsonl)

`cq`→ㄱㅅ(initials across syllables) vs `cQ`→ㄳ batchim; 갭(e a G_) vs 겹; 앞/갚
same-key repeat (ㅏ+ㅍ); 듣/뜯 (ㅡ+ㄷ); 있 (ㅣ+ㅆ repeat); 높/깊/싶/덮/엎 retention
choices; gG→ㄸ eE→ㅃ vV→ㅉ (yethangul table); ㅜㅜ→ᆍ (old-Hangul), right-vowel
compound gating; 초성체 via batchim keys in symbols table; `S-M`×2 → … exclusion
while composing.

## Disambiguation model (settled 2026-09-29, all oracles agree)

Lifted ist conditions are *the* specification once rewritten per these rules
(ist2data + engine):

1. **E virtual-unit tests = variantness, not codepoint.** `E==0x1F5`,
   `E>=0x1F5&&E<=0x1F8`, `E<0x1F4` become `E==<code> && J`, `J`, and
   `E && !J` where engine state `J` = "current jung was typed as a right-side
   variant" (`{"j":…,"variant":true}` lines: `/ o p i` while cho present & jung
   empty). Consequence: ㅘ/웨/외/의(chains from variants) compose; shift-row
   plain ㅗ+ㅏ, ㅡ+ㅣ do NOT (kime compose/dont-compose tests, patim ㅗㅏ/ㅡㅣ,
   docs §2 all pass).
2. **G-gate is pair-aware** (`jongonlyFresh` layouts = files containing `(G=1`):
   after a jong-only syllable, an *unshifted* jong extends only non-modern-ssang
   pairs; `r∈U+11A9..11BB` (ㄳ…ssang-sios) requires the shift mark. →
   cq→ㄱ,ㅅ · cQ→ㄳ · (yet) ㅇ+ㄱ→ᇬ and ㅅ+ㅍ→ᇫ compose ungated.
   dubeolsik/390/ahn/romaja: never gated (libhangul parity untouched — 17/17 ctest).
3. `D` = cho slot (indistinguishable from "last unit" for every shipped trigger;
   noted, harmless). h/H double-tap guards with unknown small unit ids stay
   inert while h=H=0 (default); decode = open item (needs 날개셋 unit table).

Oracle corpora in CI: tests/golden/patim-sin3-p2-basic.jsonl (106 asserted
steps, 3 cases excluded: patim-only araea-djamo chains + their z=ㅢ table
deviation) and hand-derived manual-sin3-p2{,-yet}.jsonl (syllable-formula and
Unicode-name-verified literals). kime tests transcribed as behavior facts.

## P1b status (converted, green)

`sin3-p2-basic` (basic.xml) · `sin3-p2` (symbols.xml, tiers + G-rows) ·
`sin3-p2-yet` (yethangul.xml + `sin3-p2-yet-ohi` 384 transcribed OHI rules,
2022-12-24 deletions). `x-notes` records stripped assignments (`G=` now
semantic via jongonlyFresh; `T=` assignments are the sole stripped form).

## Unit-id conditions and the 2024 받침 rules (settled 2026-09-30)

날개셋 unit ids are **letter-shared** (one small id per jamo letter), but the
AutomataTable slot conditions compare against that slot's *codepoints*, so
`id == codepoint` can never be true at runtime. Conversion policy:

- Every `D/E/F` comparison is checked against explicit tables in
  `tools/jamo.py`: `UNIT_ID_CHO = {0x8A: …}` (ㅇ z+ㅇ tier guard) and
  `UNIT_ID_JONG = {24, 86, 161} → 0x11AE/0x11B8/0x11BD` (the 2024 윗글 받침
  rows: shift-G/E/V `F==24/86/161` = 받침 ㄷ/ㅂ/ㅈ). Rewritten comparisons
  become live; everything else stays as written (always-false) and is
  recorded in `x-notes["inert-ids"]` — including the h=H=0 guarded rows and
  the yethangul tier-guard `D==` groups, which are emit-neutral (branch ==
  else emit), verified row by row. No blind `id + 0x1081` fit.
- Unresolvable `UnitMix` rows are classified `x-notes["dropped"]
  ["unitmix-unresolved"]` (388 yethangul old-cluster rows with extended
  `GD/Z_/V_/X_/NG` names — legacy 날개셋 space superseded by the
  OHI-transcribed table; first-sil old-gul combos come from `sin3-p2-yet-ohi`).

**Engine justification (new behavior):** the 2024 rule is a *받침-only*
buffer receiving a 윗글 choseong (`gG` → preedit ㄸ). In `process_jaso`'s
choseong branch, when the buffer is lone-받침 (`!cho && jong && !jung`),
`ic_combine(jong, ch)` runs against the compose tables first — including
`(ㄷ,旦)→旦`, keyed by the choseong-typed result the VU trio override emits
(`UnitMix 653/656/659 → DD/BB/JJ` choseong). On a hit the 받침 is popped and
replaced (chord stays composing → `gGe` → 때); on a miss the normal
commit+push path runs, unchanged. The pair-aware G-gate mirrors the jong
branch. Safe because no pre-2024 layout has a `(jong, choseong)` compose row
reachable here — the only such rows are these three, so dubeolsik/libhangul
parity is untouched (17/17).

## P2 gate corpora (tests/golden/p2-*.jsonl)

One golden per pat.im/1136 section (PLAN.md:85), wired as
`engine-golden-p2-*` ctest targets. Expected values are the section's
word/rule letters (or normative compose rows); flag/caps cases are pinned
by replay through `hunmin-sim --check` so case meta applies.

Findings recorded while building them:

- `hunmin_ic_reset` did not clear the pending symbol-tier state `T`; a
  multi-case golden whose case ends mid-tier leaked it into the next case.
  Fixed: reset now zeroes `T`.
- Live flags only: `h` on the `F==0x11B8` row (key `e`), `z` (tier rows,
  `z&&D==0x110B` rows), `W` (`W&&D` shift rows), `w` (via the tier guard).
  The `h&&F==<unit-id>` rows are inert (unit ids vs jamo cps) as documented;
  `h` on key `v` (`F==0x11BD`) is shadowed by earlier guards for `av`;
  `H` has no referencing row in any layout — `H-no-op` pins that.
- §2-5 옛한글확장 stays excluded (upstream withdrew the scheme 2022-12-24).
- Typeable-subset gaps, not yet in any compose table: first-sil `ㅈ/ㅊ+ㅆ`,
  `ㅁ+ㅈ→ᅕ`; the `ㅆ` second-cho key is unreachable (key `x` yields `ㅛ` in
  cho-only state). Excluded from corpora; opening items for the 388
  `unitmix-unresolved` old-cluster rows.
## IC input entry points (issue #90: the explicit-modifier append)

The engine's composition input is the case-encoding `char` event
(`hunmin_ic_process(ic, ch)`, libhangul/golden parity). Direction from the
close of #87: option B of `docs/OPTIONS-MEASURED.md` §IC-2 -- append an
explicit-modifier key entry point; the case endpoint moves into the engine.

**This is engine-interface behaviour justification (hard rule), recorded
2026-10-18.** The wrapper reuses the loader's own US-QWERTY shift map
(`schema_loader.c` `BASE[]`/`SHIFTED[]`, one home -- §0-4 of the options doc):
`hunmin_ic_process_key(ic, base, shift)` maps `(base, shift)` to the same
`ch` the legacy call was always handed. No new US table, no layout-JSON
change, no expression or state-machine change; `hunmin_ic_process` is
byte-for-byte unchanged and remains the only call fcitx5 uses (its keysym
arrives pre-resolved).

**The already-resolved rule (specified here, not invented per-host).** Given
`base`:

- When the base is a lowercase letter or one of the 21 punctuation base keys,
  `base` names a physical key: `shift=0` selects the plain row, `shift=1`
  selects the shifted row at `keys[shifted_of(base)]` -- exactly the addressing
  the loader materialises (docs/OPTIONS-MEASURED.md §IC-1).
- When `base` is *itself* an already-shifted form (an uppercase letter, a
  shifted punctuation, or any non-US-QWERTY codepoint), `shifted_of(base)` is
  zero and nothing further is selected: the character reaches the engine as-is
  and the shift bit is idempotent. This is the fcitx5-shaped-host case: a
  frontend whose event already carries the resolved character passes it with
  either shift value and behaviour equals the legacy call.

Guard: ctest `engine-process-key` (tier A) sweeps every shipped layout × every
(base, shift) row and asserts commit/preedit/equality of consumption against
the legacy call, with the legacy character computed from an independent copy
of the shift map so a drift in the loader's map reads red. Consequence
preserved: no tier-A golden or tier-C corpus regen -- an appended entry point
invalidates none of them (options doc §0-2).
