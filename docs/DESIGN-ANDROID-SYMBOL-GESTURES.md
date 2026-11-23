# DESIGN-ANDROID-SYMBOL-GESTURES — swipe-up = shift, and the symbol-page picker

Status: **design for issue #312.** The two gesture rules are ruled — their text
lives in `docs/SPEC-ANDROID-KEYPAD.md#§KP7` (clarification 2026-11-23: swipe-up
is identical to shift+key on any key) and `docs/SPEC-ANDROID-KEYPAD.md#§KP10`
(clarifications 2026-11-23: 기호 mode-key swipe-up opens a page picker — emoji /
extended symbols / math symbols — and picking one opens that symbol page).
This document cites those rulings, records the tree facts they land on, and
presents the **open mechanisms** as comparable. It supersedes the removed
`DESIGN-ANDROID-SYMBOL-VARIANTS.md`, which answered a question the maintainer
never asked (the drift record is in the SPEC KP10 section and the wiki session
`2026-11-23-312-symbol-variants`). Parents: `docs/SPEC-OVERVIEW.md#§A1`,
`docs/SPEC-OVERVIEW.md#§A2`.

## 1. The two rules, and what they are not

- **Gesture 1 (KP7):** swipe-up on any text-keypad key = that key's shifted
  form, identical to shift+key. Number-row symbols follow from the layout's
  existing shift row. This is **not** a per-key variant table: no new symbol
  data, no mapping to rule.
- **Gesture 2 (KP10):** swipe-up on the **기호 mode key** opens a page picker;
  picking a page opens its symbol window. The designed Tier 2 panel
  (`docs/ANDROID-KEYPAD-MOCKUP.md` §Symbol mode Tier 2) is the extended-symbols
  content. This is **not** variants on the symbol-character keys, and the
  Tier 2 page is not deprecated — only its button entry is replaced.

Both gestures open things that already exist (the shift path, the designed
pages). KP5's accidental-swipe defences are requirements on both; KP8 bounds
the picker's complexity; KP9 keeps the 기호 key in its mode-key slot in every
mode (tap = Tier 1, swipe-up = picker).

## 2. Tree facts the rules land on (measured 2026-11-23)

- **The shifted-tap path exists.** `KeypadModel` carries `META_SHIFT_ON`
  (AOSP, javap-verified), a one-shot shift (#299), and digit corner hints
  derived from `HunminKeyRegistry`'s shift column (`1<!>` … `0<)>`); a shifted
  digit tap already delivers `! @ # …` through the engine/app path, pinned by
  tier E1/E2. Gesture 1 is therefore **recognition + routing into an existing
  path** — no engine, ABI or composition-schema change, no new symbol data.
- **The 기호 mode key exists** in the text keypads' bottom row (the KP13/KP15
  picture). The symbol screen's bottom row currently lacks the 기호 slot —
  an artefact of the drifted reading; restoring it is part of #312 (KP9).
- **The extended-symbols content is designed**: the Tier 2 panel, 40 symbols
  in 4×10 grouped rows, no editing keys, bottom row `[한/영] [기호 1]`
  (KP6/KP9 rulings; inventory measured + curated in
  `docs/ANDROID-SYMBOL-INTERACTION.md` §Korean special symbols). Its
  remaining-space question stays open (mockup).
- **Prior art for "일반적으로 많이 제공하는" pages**
  (`docs/ANDROID-KOREAN-IME-SURVEY.md`): HeliBoard `symbols`/`more_symbols`,
  FlorisBoard `symbols`/`symbols2`, fcitx5-android picker pages, open-keyboard
  an emoji panel. Symbol pages and emoji are the surveyed common set; a math
  page's content needs a source — the measured 수학/단위 group exists
  (`° ± × − ≈ ℃`, curated `÷ ≠ ≤ ≥ ∞ √ ′ ″`).

## 3. Gesture 1 — open details

Mostly settled by the ruling; what remains is mechanism:

- **Shift semantics.** Swipe-up carries the shift bit per event (identical to
  hardware shift+key), leaving the one-shot shift state untouched —
  recommended, because "동일한 동작" names the shift-held result, not the
  soft one-shot arming. *What would change it:* a maintainer preference that
  swipe-up arm the one-shot instead.
- **KP5 defences** apply as measured in `docs/ANDROID-SYMBOL-INTERACTION.md`
  §Avoiding the accidental swipe: distance/direction/velocity gate,
  return-to-cancel, commit-on-release with preview, adjustable sensitivity.
  Cited, not restated. The corner hints (digits, shifted jamo, frequent keys)
  are already drawn, so the gesture's target is visible before it is used.
- **Symbol-screen keys have no shift meaning** (KP6: every symbol is its own
  button, direct commit). Swipe-up there is undefined for now — recorded
  open, not decided.

## 4. Gesture 2 — open mechanisms, comparable

**Picker presentation** (three items: 이모지 / 확장 기호 / 수식 기호):

| option | shape | KP8 reading | cost |
|---|---|---|---|
| popup above the 기호 key | 3 labelled items, release-on-item | glanceable, recognition; same vocabulary as a long-press popup | occludes the row above while held |
| bottom-row strip | the picker replaces the bottom row until picked/cancelled | no occlusion; items are full keys | a transient layout state; cancel path needed |
| full-screen chooser | a modal page list | room for labels + previews | heaviest; a page to open pages |

Recommendation: **popup**, because three items fit a glance and reuse the
popup vocabulary; *what would change it:* measured occlusion/reach problems
on the narrow screen, or the page list growing past a glance.

**Long-press on the 기호 key:** the same picker (KP10 names swipe *or*
long-press) vs a distinct meaning. Recommendation: **same picker** — one
meaning per key is the KP8-consistent choice.

**Page content and data home** (KP11: app-read data, engine uninvolved):

| page | content | state |
|---|---|---|
| 확장 기호 | the designed Tier 2 panel as-is | design ruled (KP6/KP9); remaining-space question open |
| 수식 기호 | measured 수학/단위 group + curated additions | inventory **open** — a ruling or a usage measurement |
| 이모지 | (i) bridge to the system/androidx emoji picker; (ii) bundled curated frequent set; (iii) full bundled panel | scope **open** — (i) is thinnest and tracks the platform; (ii) is data with its own licence terms; (iii) is a large bundled inventory. Prior art: open-keyboard bundles (ii)-shaped panels |

Recommendation: extended symbols from the ruled panel first; math from the
measured group pending an inventory ruling; emoji as a **bridge or curated
set**, maintainer's choice — a full bundled emoji inventory is a data-licence
and size decision, not a gesture decision. Each page ships as app-read JSON
under `assets/keypad/` with the checker extended per page and an E1 commit
golden (the #297 pattern).

**Return navigation:** each page's bottom row carries the mode keys
(`[한/영] [기호]`, the Tier 2 panel's designed `[한/영] [기호 1]`); picker
cancel is release-outside / return-to-center (KP5). Positional consistency
(KP9) is the binding rule, the exact slots provisional.

## 5. What this does not settle

The picker presentation ruling; the emoji scope and source; the math
inventory; long-press equivalence; swipe-up's meaning on symbol-screen keys;
the shift state machine (capslock unlock, KP7); device-measured gesture
thresholds and error rates (KP5's defences are requirements, their tuning is
measurement).

## 6. Verification plan

```sh
python3 tools/symbol-data-check.py            # extended per page (tier-B)
python3 tools/doctor.py libhunmin/data        # engine data untouched
ctest --test-dir build -j16 -LE slow          # inner loop
ctest --test-dir build -R engine-android-e1   # parse + commit goldens per page
ctest --test-dir build -R engine-android-e2e  # gesture rounds on the AVD (KP5)
```

Gesture 1's regression surface is the existing shifted-tap goldens plus a
swipe-up round per key class (digit, letter, frequent punctuation); gesture 2
adds picker-open / pick / page-commit / return rounds. Test cost is measured
against the budgets before push (`tools/ci-time-budget.sh`).