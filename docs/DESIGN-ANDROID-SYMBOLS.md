# DESIGN-ANDROID-SYMBOLS — the symbol-layer home for the Android keypad

Status: **design analysis for issue #298, not a ruling.** The maintainer chooses
the symbol-layer home. Requirement pointers: `docs/SPEC-OVERVIEW.md#§A1`,
`docs/SPEC-OVERVIEW.md#§A2`, `docs/SPEC-OVERVIEW.md#§A8`,
`docs/SPEC-FRONTEND.md#§FE1`, `docs/SPEC-FRONTEND.md#§FE2`,
`docs/SPEC-FRONTEND.md#§FE11`, `docs/SPEC-FRONTEND.md#§FE12`,
`docs/SPEC-SCHEMA.md#Layout file`. The lane is `docs/MILESTONES.md` Track 3.

Reproduce the measurements below:

```sh
cmake -B build && cmake --build build
python3 tools/measure-symbol-layer.py --build-dir build
```

The tool writes scratch copies under `build/`, never under `libhunmin/data/`.
With `--out-dir` it writes `measurements.json` and `meta.json` beside it. No
shipped layout, engine source, ABI symbol or golden is changed by this analysis.

Prior art: `docs/ANDROID-KOREAN-IME-SURVEY.md` surveys how open-source Android
IMEs compose Hangul and where they put symbols. Its measured mainstream pattern
is a **separate symbol page** (option B below), not a third per-key layer; the
Hangul shift row is doubled jamo on 두벌식 and may carry symbols on 세벌식.

## 1. What the tree measures today

### 1.1 The two goal layouts carry two rows, not three

The full census is printed by the tool. The two layouts named by the Track 3
goal are:

| layout | keys | `plain` | `shift` | literal outputs | ASCII punctuation reachable |
|---|---:|---:|---:|---:|---:|
| `dubeolsik` | 26 | 26 | 26 | 0 | 0 / 32 |
| `us-qwerty` | 47 | 47 | 47 | 94 | 32 / 32 |

Every shipped layout carries only `plain` and `shift` rows. No layout carries a
third `symbol`, `alt` or `ctrl` row. The ASCII punctuation set is
`string.punctuation`; `us-qwerty` already reaches all of it through its existing
plain/shift literal rows, while `dubeolsik` reaches none of it.

This corrects one premise in the tracking issue: a third per-key row is **not**
engine-free today. The loader and the public key API select only the plain and
shifted rows.

### 1.2 `doctor.py` and the engine disagree about a third row

Scratch probe: add one modifier row to `dubeolsik` key `a`, value `"=@"`, then
run `tools/doctor.py` and feed key `a` through the built engine.

| row added | `doctor.py` | engine key `a` |
|---|---|---|
| `symbol` | rejects: mods must be a subset of `plain`, `shift`, `ctrl`, `alt` | ignored; preedit `ㅁ` |
| `alt` | accepts | ignored; preedit `ㅁ` |

So `symbol` is not a legal modifier today, and the reserved `alt` modifier is
accepted by the linter but not loaded by the engine. A design that uses `alt`
as the symbol row inherits that disagreement unless the loader is changed too.

The current option surface also refuses a symbol-layer option:
`hunmin_ic_set_option` and `hunmin_buffered_preedit_set_option` both return
false for `symbol`, `symbol_layer` and `alt`.

### 1.3 A separate symbol page can be an ordinary layout file

Scratch probe: write `android-symbols.json` with 32 ASCII punctuation literals
mapped to letter/digit key ids, `mode: "latin"`, `engine: "std"`, and the
existing `modern` compose table.

- `doctor.py`: 0 errors.
- The engine loads it as an additional layout.
- Its mode is reported as `latin`.
- Key `a` commits `!` with no preedit.

This is a feasibility probe, not a proposed artefact. It shows that a symbol
page can reuse the engine's literal path with no engine change **if it is
modelled as a layout**. The integration cost is elsewhere: Android's
`LayoutRegistry` lists every native layout by mode, so a `latin` symbol layout
would appear in the Latin settings list unless a new contract hides it. Hiding
it by id in the frontend would be per-layout frontend code (`FE12`).

### 1.4 The current boundary paths deliver differently

Probe: buffer `dkssud` on `dubeolsik` with `us-qwerty` as the Latin slot, then
insert `!`.

| path | what happens | delivery |
|---|---|---|
| mode toggle, Shift+1, toggle back | toggle delivers `안녕`; Shift+1 on `us-qwerty` leaves `!` in the word buffer | the second toggle delivers `!` |
| `external_text("!")` | the pending word is delivered first, then the external text | `안녕!` in one delivery |

The mode-toggle path already works for ASCII punctuation, but it changes the
language mode and needs a return toggle if the next key is Hangul. The
external-text path delivers immediately, but it is the `FE8` path for text that
did not come from the keyboard. Using it for a soft symbol key is a semantic
choice, not a free shortcut: a symbol key is keyboard input, and the shared key
path is where `ReqMode4`'s symbol delimiter and the autoswitch key log live.

### 1.5 The ABI has no row selector

The committed ABI list carries 64 symbols. The key entry points take a base
character plus a shift bit; the classifier's `mods` are the chord bits. There
is no exported query for a key's symbol row and no event field that selects a
third row. Adding one is an append-only ABI act; adding an accepted option name
to the existing `set_option` surface adds no symbol but still changes engine
behaviour.

## 2. The design axes are separate

The issue names three options, but they answer different questions:

1. **Data home** — where the symbol inventory lives: per-key layout rows, a
   separate symbol table/page, or an ordinary layout file.
2. **Selection mechanism** — how a key event selects the symbol row: a
   persistent layer mode, a one-shot modifier, a layout switch, or a direct
   commit outside the key path.
3. **Interaction** — how the user reaches it: a symbol page key, a long-press
   popup, or a mode toggle.

Long-press is an interaction, not a data home. It can sit on top of any data
home that exposes variants. Treating it as a third alternative to the data home
hides the choice that actually changes the schema and engine.

## 3. Comparable designs

### A. Third per-key row in layout data

**Mechanism.** `keys.<id>.symbol` carries a value using the existing primitives
(`Literal | SlotJamo | Ambiguous | Sequence`). The loader stores a second row;
the engine selects it from an event or option; `doctor.py` validates it; goldens
pin the behaviour.

**Selection variants.**

- **A1: new event entry point.** Append an ABI function that takes a row
  selector (or a fuller key event) and feed it through the shared layer. The
  existing `process_key(base, shift)` signatures stay unchanged.
- **A2: engine option.** Accept a `symbol_layer` option on the existing
  `set_option` surface and let `process_key` read it. No new ABI symbol, but
  the engine's accepted option names and key behaviour change. One-shot vs
  persistent reset is an engine rule, not a frontend toggle loop.
- **A3: one-shot vs persistent.** A persistent symbol mode is a layer state; a
  one-shot symbol key resets after the consumed key. The choice affects the
  classifier, the key log and the Android UI state.

**Costs measured or structural.**

- Schema: the loader rejects `schemaVersion > 2`; a new modifier needs a
  versioning decision. Leaving it optional in v2 would let old engines load the
  file and silently ignore the row (the measured `alt` behaviour).
- Loader: `build_keys` ignores every modifier except `plain` and `shift`;
  `hm_layout.keys` is one value per ASCII code. A third row needs storage and
  cleanup.
- Capability: `compute_capability` scans key values for jamo and old-Hangul
  capability; a new row must be scanned too.
- Key log/replay: `keys_push` stores `(base, shift)` and `replay_reading`
  replays through `process_key`. A symbol-row selector must be recorded or the
  autoswitch readings replay the wrong row.
- Peek: `hunmin_ic_peek_key` and `hunmin_buffered_preedit_peek_key` copy the
  ic/layer state; any selection state must be copied too.
- Doctor: `MODS` must accept the new row, and negative fixtures must prove the
  linter and loader agree. The reserved `alt` disagreement is the warning.
- Frontends: Android needs a touch event that carries the selector; fcitx5/TSF
  need a mapping only if the row is exposed there. The engine owns the rule.
- Tests: tier A goldens for the row, tier C trace format extension for the
  selector, Android E1/E2 for the touch path, doctor negative fixtures.

**What it preserves.** `A1`, `FE1`, `FE2`, `FE12`: the symbol set is layout
data, the key rule stays in the engine, and a new layout needs no frontend code.

**What it does not settle.** The symbol inventory, the geometry, and whether
the row is exposed on hardware frontends or only on Android.

### B. Separate symbol page

**B1: frontend-owned grid.** Android draws a symbol grid and commits strings
after `commitWord`, or sends ordinary key events to the Latin layout. No engine
or schema change. The symbol inventory lives in Kotlin constants or a frontend
asset. This is the smallest Android-only change, but adding a symbol requires a
frontend edit, and a direct commit bypasses the shared key path unless the
boundary is modelled explicitly. The surveyed IMEs (HeliBoard, FlorisBoard,
fcitx5-android, open-keyboard) all use a separate symbol page; see
`docs/ANDROID-KOREAN-IME-SURVEY.md`.

**B2: data-owned symbol table.** A new `data/symbols/` JSON file carries the
page inventory; Android reads it from assets; `doctor.py` validates it. The
engine is unchanged. The data home is outside `data/layouts/`, so the project
has two homes for "what characters exist": layout literal rows and the symbol
table. Android packaging needs a new asset subdir, installer extraction and a
doctor check.

**B3: symbol page as an ordinary layout file.** The scratch probe shows this
loads and commits with no engine change. It reuses the engine key path and can
be golden-tested as a layout. The cost is the mode/settings contract: a `latin`
symbol layout appears in the Latin list, and hiding it by id is per-layout
frontend code. A new mode or visibility flag would be a schema change.

**What it preserves.** Zero engine ABI cost for B1/B2; B3 reuses the engine.
Presentation stays in Android.

**What it risks.** `A1`/`FE12` if the symbol inventory is frontend code;
`FE1`/`FE2` if a symbol key commits outside the shared classifier without a
recorded boundary rule; two data homes if layout literals and the symbol table
both claim to define the symbol set.

### C. Long-press popup

**Mechanism.** A touch gesture on a key opens a popup of variants. The popup
reads its variants from the chosen data home: A's third row, B's symbol table,
or the existing shift row.

**Measured boundary.** Without a third row or a separate inventory, long-press
can reach only what the existing rows already expose. On `dubeolsik` that is
jamo, not ASCII punctuation; on `us-qwerty` it is the shifted literal row.

**Costs.** Gesture timing, popup state, accessibility labels, and the
discoverability of a hidden variant. None of these is measured by this
analysis. No engine change is needed if the variants are already reachable;
otherwise C inherits the data-home cost of A or B.

**What it is not.** A data home. It is an interaction that composes with A or B.

### D. Baseline: mode toggle to the existing Latin layout

The current pair already types ASCII punctuation by toggling to `us-qwerty`.
The probe measures the delivery: the Hangul word lands at the first toggle, the
symbol is buffered, and the return toggle delivers it. This is shippable today
for ASCII punctuation, but it changes the language mode and costs two toggles
per insertion if the user returns to Hangul. It does not provide a Korean-mode
symbol grid.

## 4. Cross-cutting contracts

- **`FE1`/`FE2`:** a soft symbol key is keyboard input. If it goes through the
  shared key path, the classifier and delivery rule stay shared. If it commits
  directly, the boundary and symbol-delimiter behaviour must be modelled
  explicitly rather than assumed.
- **`ReqMode4`/`ReqMode8`:** the autoswitch key log currently records
  `(base, shift)`. Any third-row selector must be recorded and replayed, or the
  Latin/Hangul readings drift from the keys the user actually pressed.
- **`FE11`:** the Android frontend stays thin. A symbol grid is presentation;
  parsing, validation and selection logic belong in data/engine seams.
- **`FE12`/`A1`:** no per-layout frontend code. B1's frontend constants are a
  symbol inventory, not a per-layout rule, but they still move layout behaviour
  out of data. B3's hide-by-id is per-layout code unless a schema contract
  replaces it.
- **ABI append-only:** A1 adds symbols deliberately; A2 adds an accepted option
  name without a new symbol. Both are engine changes, and the tracking issue's
  "no engine change" premise does not hold for a third row.
- **Schema versioning:** the loader rejects newer versions. A new modifier
  needs a versioning decision so old engines do not silently ignore a row.
- **Doctor/loader agreement:** the measured `alt` case is the failure mode:
  the linter accepts a row the engine ignores. A new row must be validated and
  loaded together, with negative fixtures.
- **Android packaging:** a new layout file ships through the existing `layouts`
  asset extraction. A new `symbols/` subdir needs asset sync, installer
  extraction and a doctor check.
- **Testing:** tier A goldens for engine rows; tier C trace format for the
  selector; Android E1/E2 for touch; doctor negative fixtures. This analysis
  adds no test registration.

## 5. What the measurement does not settle

- The symbol inventory beyond ASCII punctuation: Korean symbols, currency,
  emoji, and whether the page is universal or per-layout.
- Gesture ergonomics, long-press timing, accessibility and landscape geometry.
- The other Track 3 decisions: geometry home, shift behaviour, functional-key
  placement, candidate window, mic button, touch-key data source.
- Whether a symbol page should be a layout mode, a hidden presentation, or a
  separate data table.
- The maintainer's choice.

## 6. Decision request

Issue #298 asks for the symbol-layer home: third layer in layout data vs
separate frontend symbol page vs long-press emulation. The measurement separates
that into a data home, a selection mechanism and an interaction. The deciding
question is whether symbols are **per-key layout behaviour** (A), a **separate
page inventory** (B), or an **interaction over an existing inventory** (C/D).

## Decision (maintainer, 2026-11-22)

**Chosen: B2 — the Android app draws the symbol screen from a data file; the
engine is not involved.** Verbatim: "앱이 데이터 파일로 그리기. 엔진은 관여 안함."
Recorded as `docs/SPEC-ANDROID-KEYPAD.md#§KP11`.

This settles #298's core question: the symbol screen is a **separate symbol
inventory read by the frontend**, not a third per-key row (A) and not an
engine-loaded layout (B3). **No engine or composition-schema change.** The symbol
list stays data (the app reads a data file, not hardcoded constants), honouring
"layout behaviour is data"; its format/validation is a frontend data contract
settled at implementation. The engine's layout/settings list is unaffected (no
visibility contract, unlike B3). The gesture-variant extension is #312 (KP10).

The analysis above stands as the comparable record; the options not chosen (A,
B1, B3, C, D) remain the evidence for why B2 fits. What would reopen it: a future
need to share the symbol screen across frontends (move toward B3).

The symbol-screen build itself is the follow-up implementation to this decision,
not part of #298 (the decision issue).

## Implementation — the data contract (T3-2, issue #297)

The app-read data file is
`frontends/android/app/src/main/assets/keypad/symbols.json` — a committed
frontend asset, deliberately outside `assets/data/` (which is synced from
`libhunmin/data` and belongs to the engine). It carries the symbol screen only:
the three KP6-ruled symbol rows (all 32 ASCII punctuation, grouped `[]{}` and
`,.<>`, `` ` `` leading), the navigation row and the KP9 bottom row as
function-key **ids**. What is deliberately absent, with the reason in the
file's own `x-notes`: the Tier 2 slot (KP10 prefers gesture variants; #312
owns that mapping, on this same file) and the main keypads' frequent-symbol
keys (the frequent set and number-row default are still open,
`docs/ANDROID-KEYPAD-MOCKUP.md`).

The contract has one owner per half:

- **Content** — `tools/symbol-data-check.py`, ctest `keypad-symbol-data`
  (tier-B guard): the ruled arrangement, the closed function-id vocabulary,
  KP9's no-space/enter, the KP6 nav order, and a ban on engine-layout fields
  (KP11: the engine never loads this file). Runs on hosts with no Android SDK.
- **App path** — `frontends/android/app/src/main/java/org/hunmin/ime/SymbolScreen.kt`
  (`SymbolScreen.parse`, `SymbolScreen.commitSymbol`) parses the asset and
  commits a symbol key's character directly; `SymbolScreenTest` (tier E1,
  `engine-android-e1`) is the golden: every symbol in the shipped file, tap →
  exactly that character through `RecordingInputConnection`, nothing
  composing, no I9 violation. These tests need no JNI shim and never skip.
  `commitSymbol` is the seam the soft-keyboard UI sub-issue calls — after
  flushing any pending composition (§4's FE1/FE2 ordering note).

No engine, ABI or composition-schema change was made: `libhunmin/data` and
`libhunmin/tests/abi/symbols.txt` are untouched by #297.
