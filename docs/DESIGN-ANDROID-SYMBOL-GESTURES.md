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
- **Long-press = swipe-up is the general rule, with per-key exceptions** —
  decided (maintainer 2026-11-23, verbatim in
  `docs/SPEC-ANDROID-KEYPAD.md#§KP7`: "long press도 swipe up과 동일하게 구현해
  두자" and "long press와 swipe up은 일반적으로 동일하지만, 기호 키나, backspace
  등 일부 키에서 동작이 달라지는 거야."). One implementation of the gesture
  pair plus a per-key exception table: 기호 mode key → picker (KP10),
  backspace → word-delete repeat (KP14), further exceptions open ("등").
- **The backspace exception.** Long-press deletes a word, repeating at ~0.2 s
  intervals while held (KP14; the request's stated reason — repeat tapping to
  erase many characters is hard — and all three verbatim rulings are in the
  SPEC). Mechanism: the WORD unit of the existing swipe-left delete path.
  Prior-art premise checked: the surveyed open-source IMEs repeat CHARACTER
  deletion (`docs/ANDROID-KOREAN-IME-SURVEY.md` §5) — word-unit repeat is
  hunmin's differentiating choice; the ~0.2 s interval is a provisional
  tuning constant, not a measured optimum.
- **Symbol-screen keys have no shift meaning** (KP6: every symbol is its own
  button, direct commit). Swipe-up/long-press there is undefined for now —
  recorded open, not decided.
- **Implemented 2026-11-23 (phase 1)**: the pure decision layer
  (`KeypadModel.gestureAction`, `isSwipeUp`, `returnedToOrigin`,
  `WORD_REPEAT_INTERVAL_MS`) and `KeypadView`'s pointer state machine —
  swipe-up arms past the gate and commits on release, return-to-origin
  aborts back to a tap (provisional), long-press fires at the **system**
  timeout, movement past jitter cancels the hold, backspace repeats WORD
  deletion until release. The shift bit is per-event; the one-shot state is
  untouched. Provisional tuning constants: the 60 dp gate (shared with
  KP14), 10 dp jitter, 12 dp return radius — all named in the view, all
  tunable without a decision change. E1 pins the table and gates
  (`KeypadGestureTest`); the device round and the picker popup are the next
  phases.
- **Two device findings (2026-11-23, E2E gesture round)**:
  (A) in Hangul mode the soft 두벌식 pair has no digit keys, so the engine's
  boundary-and-pass dropped the shift bit — shift+1 typed `1`, breaking the
  corner hint's promise; fixed by KP16's decided rule (the digit's touch-shift
  alternate is its drawn hint, via the symbol path — `shiftedDigitAlternate`,
  E1-pinned, Hangul-mode E2E round added). (B) A fast 2-key-height swipe-up
  from the NUMBER row can leave the IME window and be stolen by a system
  gesture (observed once: systemui came forward); 1.5 key heights at 150 ms
  stays over the gate and inside the window. The top row has little headroom
  above it — gate tuning or a direction-aware threshold is a measurement
  item, recorded not decided. (C) The picker path TEXT→EXTENDED bypassed
  `enterSymbols`, so the KP9 return memory kept its stale default and 한/영
  toggled instead of restoring — fixed in `KeypadState.enterExtended`, E1 +
  E2E pinned. (D) `commitSymbolFromTouch` validated against the Tier 1 screen
  only and rejected the extended panel's own symbols — fixed to validate
  against the page the key belongs to. (E) The picker popup is NON-focusable:
  a focusable popup steals the IME's input focus (the standard IME pattern);
  the mode bug measured in the same round was (C), not this — recorded so the
  two are not confused.
- **Shift state machine + symbol-screen navigation implemented (2026-11-23,
  decided)**: shift's swipe-up/long-press LOCK capslock, a tap while locked
  releases (`KeypadState.capsLocked`, the bit holds across keys); on the
  symbol screen swipe-up/down on a symbol key cycle the symbol SETS (the
  pages that have data, wrapping — the recorded reading of "기호 조합"),
  long-press follows the equivalence, swipe-left stays KP14 backspace.
  Swipe-down on text keys and the 기호 key is UNRULED — a NONE cell, not a
  silent copy of swipe-up. E1 pins the table + state; the E2E gesture round
  verifies capslock lock/release and the set cycle on the AVD.
- **Provisional lists shipped (2026-11-23)** — under the maintainer's
  direction "심볼 목록은 일단 구현해 보고 사용성을 평가하면서 개선해보자" and
  "다른 ime가 제공하는 심볼 목록도 참조해 보자" (verbatim in SPEC KP10):
  확장 기호 = the ruled 40 base + 20 additions (6×10, weakest five trimmed);
  수식 기호 = the proposed 40; 이모지 = curated 40 from symbl popular (emoji
  subset, U+FE0F kept); 카모지 = curated 30 (classics the site lacks +
  font-safe site members), a full-width scrollable list with the mode row
  FIXED below (measured: inside the scroll it took 30 rows to reach 기호 1).
  Other IMEs' lists surveyed (`docs/ANDROID-KOREAN-IME-SURVEY.md` §6): the
  shape families match FlorisBoard/HeliBoard popups, no surveyed IME has a
  dedicated math page or kaomoji, bundled curated emoji is shippable prior
  art. Every list is marked PROVISIONAL in its own `source`; the checker
  pins structure + the ruled base, never the unruled assignment. **The
  usability evaluation is the next step** (device use, font verification,
  a Korean chat register would re-rank) — improvements are data edits.
- **Phase 2 implemented (2026-11-23)**: the popup picker above the 기호 key
  (DECIDED), the extended page as app-read data (the designed 40-panel,
  checker-contracted), the 기호 slot restored on Tier 1 (KP9), page state
  TEXT→SYMBOLS→EXTENDED with the KP9 returns. The picker is data-driven: it
  lists the pages that have data — 확장 기호 today; 수식 기호 / 이모지 / 카모지
  appear when their inventories are ruled and shipped, not as disabled
  placeholders. Full E2E green on the AVD (553 s, all rounds incl. the
  gesture round); E1 pins the contracts and the state.

## 4. Gesture 2 — open mechanisms, comparable

**Picker presentation — DECIDED** (maintainer 2026-11-23, verbatim in
`docs/SPEC-ANDROID-KEYPAD.md#§KP10`: "페이지 선택은 기호 키 위에 팝업하자."):
a **popup above the 기호 key**, three labelled items (이모지 / 확장 기호 /
수식 기호). The considered alternatives (bottom-row strip, full-screen
chooser) remain in the branch history as the comparison; the popup's known
cost — it occludes the row above while held — is a presentation detail to
verify on the narrow screen, not a reopen.

**Long-press on the 기호 key — DECIDED:** the same picker, by the KP7
long-press-equivalence ruling (2026-11-23, verbatim in the SPEC).

**Page content and data home** (KP11: app-read data, engine uninvolved):
the inventories are proposed in §5 with reference to symbl.cc, as the
maintainer directed. Each page ships as app-read JSON under `assets/keypad/`
with the checker extended per page and an E1 commit golden (the #297 pattern).

**Return navigation:** each page's bottom row carries the mode keys
(`[한/영] [기호]`, the Tier 2 panel's designed `[한/영] [기호 1]`); picker
cancel is release-outside / return-to-center (KP5). Positional consistency
(KP9) is the binding rule, the exact slots provisional.

## 5. Page inventories — the symbl.cc reference (proposal, not a ruling)

The maintainer named the reference (2026-11-23): "symbl.cc/kr/collections/ 를
참조해서 어떤 기호를 제공하면 좋을지 정해보자." Eighteen collections were
extracted on 2026-11-23 (catalog pages, `/kr/<codepoint>/` anchors → Unicode
characters); the artefact and `meta.json` live outside the repository at
`~/work/hunmin-measurements/symbl-collections-312/`. Three sources are
cross-referenced: the symbl collections (category structure + long tail),
the measured kowiki reading (`docs/ANDROID-SYMBOL-INTERACTION.md` §Korean
special symbols), and the designed Tier 2 panel (KP6/KP9).

**The register finding, stated first: symbl does not catalog the Korean
core.** `《》〈〉「」『』` (the measured top non-ASCII symbols, 525/69/32/26 per
million), `※`, `†‡`, `℃`, `′″` are absent from the fetched collections — it
is a Western-centric catalog. symbl alone would drop the most frequent
Korean prose symbols. It is a reference for **category structure and the
math/sign long tail**, not a replacement for the measured Korean set; the
same register lesson as the corpus measurements applies to the catalog.

### 확장 기호 (extended symbols)

Base: the designed Tier 2 panel, 40 symbols in 4×10 grouped rows — every
member is either measured or use-case-curated, and all but the CJK brackets
and `※` also appear in symbl (punctuation-marks, quotation-marks,
currency-symbols, star-symbols, list-bullets, arrow-symbols,
special-symbols, heart-symbols). Proposed additions, each grounded:

| group | additions | grounding |
|---|---|---|
| 문장부호 | `‥ ―` | symbl punctuation-marks/special-symbols; `―` measured 2.4/M |
| 따옴표 | `« »` | symbl quotation-marks; measured 1.1/M each (nested quotes in Korean prose) |
| 통화 | `¢` | symbl currency-symbols; measured 0.3/M |
| 도형/표시 | `☆ ◆ ◇` | symbl star-symbols/list-bullets; `☆` measured 0.9/M |
| 화살표 | `⇒ ⇔` | symbl mathematical-signs/arrow-symbols |
| 표시 | `§ ¶ № † ‡` | symbl special-symbols (`§ ¶`); `† ‡` measured 1.6/0.4/M (footnotes), not in symbl; `№` use-case |
| 체크 | `✓ ✔ ✗ ✘` | symbl check/popular — chat/notes use case, prose understates |
| 음악 | `♪ ♩ ♬` | not in symbl; chat use case |
| 카드 | `♠ ♣ ♦` | symbl suits-of-the-cards (`♡` already in the panel) |

That is 25 additions → 65 symbols: at the ruled 10 per row, 6 full rows plus
5. **The fork this opens is geometry, not taste**: 65 symbols do not fit the
ruled 4×10 panel. Comparable resolutions: (a) grow the page (7 rows with a
partial last, or trim 5 near-misses — `№ ♬ ⇔ ‥ ✗` are the weakest-grounded —
to 6×10), taller than the text keypad, the narrow-screen cost KP2/KP8 warn
about; (b) keep 4×10 and swap the lowest-value members for the additions
(which to swap is a ruling); (c) paginate the extended page (the mockup's
category-tabs option, KP8 caution). Recommendation: **(a) at 6×10 with the
row cost stated**, because every kept addition is grounded and swapping
loses measured symbols; *what would change it:* a device measurement showing
the taller page hurts reach, or a maintainer preference for a fixed panel
size.

### 수식 기호 (math symbols)

symbl mathematical-signs carries 332 entries — a catalog, not a keypad. The
common subset, cross-referenced with the measured 수학/단위 group and symbl
special-symbols/popular, proposed as 4×10 to mirror the Tier 2 structure:

```
 − ± ÷ × ∗ ≠ ≈ ≡ ≤ ≥        (연산/관계)
 ≪ ≫ ∈ ∉ ⊂ ⊃ ∪ ∩ ∧ ∨        (비교/집합)
 ¬ ∀ ∃ ∴ ∵ ∞ √ ∫ ∂ ∑        (논리/미적분)
 ° ′ ″ ‰ ² ³ ¼ ½ ¾ ℃        (단위/분수)
```

All but `′ ″ ℃` are in symbl; those three are measured Korean needs
(8.0/3.8/3.9 per million). Near-misses for the remaining-space question:
`∏ ∇ ⊕ ⊗ ∛ ∜ ℉ π` (`π` is in symbl popular). The inventory is a **proposal**;
the ruling is the maintainer's.

### 이모지 (emoji)

symbl's emoji collections are mood/nickname sets; its **popular** collection
is a usage-weighted curation (😂❤😍🤣😊🙏💕😭🔥😘👍🥰😎😆😁😉🤔… — site
register, not Korean chat). It gives the bundled-curated option a concrete
starting set (~48 emoji), but the scope choice is unchanged and open:
(i) bridge to the system/androidx picker — thinnest, tracks the platform;
(ii) bundled curated set (symbl popular as a candidate source, licence terms
recorded); (iii) full bundled panel — a data-licence and size decision.
Recommendation: **(i) or (ii)**; a Korean chat corpus would re-rank either.

### Selection criteria — a proposal (2026-11-23, maintainer asked for one)

"기호 데이터 선정 기준을 한번 제안해 볼래?" — the criteria below are a
proposal for the pending inventory rulings (extended additions, math, emoji,
kaomoji), not a ruling. Three hard gates, then ranked evidence:

**Hard gates — a candidate must pass all three:**

| gate | test | why |
|---|---|---|
| G1 font-renderable | device-verified on the AVD, no tofu | a missing glyph breaks the whole symbol (kaomoji: 2,968/9,419 flagged by the block heuristic — the heuristic is a screen, the device is the test) |
| G2 licence + provenance | shippable terms, source + date recorded beside the artefact | the catalog sites are reference only; an artefact without retrievable provenance is not evidence (AGENTS) |
| G3 geometry budget | 10 per row (KP9), page height vs the narrow-screen target size (KP2) | the extended-page fork (grow/swap/paginate) is this gate applied — a ruling, not a criterion |

**Ranked evidence — a candidate ships if any source supports it, strongest
first:**

1. **R1 measured usage** — the kowiki reading (`tools/symbol-frequency.py`).
   The strongest evidence available; register caveat: encyclopedia prose, not
   chat/passwords. Example: `《》` 525/M, `†‡` 1.6/0.4/M (footnotes) ship on R1.
2. **R2 use-case necessity** — registers prose understates: passwords (KP1's
   number row), chat/notes (`※ ₩ ✓ ♡ ★`), maintainer-named items. Marked
   *curated* with the use case stated. Example: `✓✘` (chat), the kaomoji
   classics `(´・ω・｀)` (community use, absent from the reference site).
3. **R3 reference commonness** — symbl collections (incl. `popular`) and the
   surveyed IMEs' pages: the "일반적으로 많이 제공하는" signal. Register
   caveat: Western-centric — the Korean core is absent from symbl, so R3
   alone never drops an R1/R2 item. Example: `¢` (currency collection),
   emoji from `popular` as a starting curation.

**Ordering within a page**: category groups (KP3 search time), frequency
order inside a group where measured — the same rule the variant analysis
used, now applied to page rows.

**Applying it to the open lists**: the 25 extended additions all pass G1
(pending device verification) and G2; `№ ♬ ⇔ ‥ ✗` are the weakest on R1–R3
(the trim candidates for 6×10). The math 40 are R1/R3-supported except
`′ ″ ℃` (R1, Korean needs). Emoji: R3 starting set, a Korean chat corpus
would re-rank. Kaomoji: G1 is the binding gate; curate font-safe classics +
the site's safe subset, provenance recorded.

*What would change the criteria:* a Korean chat/notes corpus measurement
(promotes R2 items to R1), the device font survey (G1 results), or a
maintainer weighting (e.g. R3 above R2 for "commonness first").

## 6. Kaomoji (카모지) — requested 2026-11-23 (KP18)

The maintainer's request (verbatim in `docs/SPEC-ANDROID-KEYPAD.md#§KP18`)
names `https://snskeyboard.com/emoticon/` as the reference. The page was
extracted on 2026-11-23 (all `<pre>` entries; artefact + `meta.json` outside
the repository at `~/work/hunmin-measurements/snskeyboard-kaomoji-312/`).
Findings, measured:

- **9,419 entries** (9,413 unique), a flat list with no category headings —
  the site is a copy-paste catalog, not a taxonomy.
- **Font risk is real**: a codepoint-block heuristic flags **2,968 entries**
  containing characters outside commonly-rendered blocks (Egyptian
  hieroglyphs, rare combining sequences, syllabics). The heuristic is not a
  device measurement — Android font coverage must be verified on the AVD
  before any subset ships; tofu in a kaomoji breaks the whole emoticon.
- **Register gap**: the collection skews decorative SNS-style; classics like
  `(´・ω・｀)`, `(╯°□°)╯︵ ┻━┻`, `(^_^)` are absent while `¯\_(ツ)_/¯` is
  present. A curated set would add the classics from the community repertoire
  (provenance recorded), not mirror the site's skew.
- **Licence**: the strings are community/folk creations, but the site's
  compilation is editorial (ⓒSNS.KEYBOARD). Reference only — curate our own
  set with the source and date recorded, do not mirror the list wholesale.

**What kaomoji change structurally**: they are **multi-character strings**
(average ~15 characters, some 50+), so the single-character symbol contract
does not fit. A string-page kind is needed: entries are whole strings,
committed through the existing direct-commit seam (flush the pending
composition first, then `commitText` — the #297 ordering rule). KP11 holds:
app-read JSON under `assets/keypad/`, engine uninvolved, no ABI change.
Display cannot be a 10-across grid — a kaomoji is wider than a key; the
natural shape is a **scrollable list of full-width rows**, tap to commit
(the reference site's own shape, and Simeji-style kaomoji panels).

Comparable decisions (none ruled):

| question | options | reading |
|---|---|---|
| picker slot | 4th item 카모지 / a tab inside the emoji page | a 4th item keeps the picker→page model uniform and the emoji page single-purpose; 4 items still fit a glance (KP8). A tab hides kaomoji one level deeper |
| inventory | curated subset (~100–300, font-verified, classics included) / full 9.4K + search / categorized tabs | 9.4K is ~400 KB of JSON — bundleable, but scrolling it is unusable without search; a curated set is the shippable first step, growth is data-only |
| issue scope | rides #312 / a separate task issue after #312's mechanism lands | #312 is already large; one PR closes at most one issue. The mechanism (picker, string pages) is #312's; the kaomoji inventory is its own deliverable |

Recommendation: **4th picker item, curated font-verified subset first, a
separate task issue** — because the picker model stays uniform and the
curation (font verification + classics + provenance) is work independent of
the gesture wiring. *What would change it:* a maintainer preference for
kaomoji inside the emoji page, or a usage measurement showing the full list
with search is what users want.

## 6.5 Usability evaluation — the instrument (2026-11-26)

The maintainer is evaluating the shipped lists in real use: "쓰다 보니 입력이
기대대로 안되는 경우가 많이 나타나서, 로깅을 통해 내가 어떤 기대를 했다가 입력이
안되고 수정했는지, 그 이유는 뭔지 파악해 보고 싶네?" Two findings and the
instrument:

- **The existing trace was not enough, and not reachable.** The E3 sink
  (`HunminTraceRecorder`, `tools/e3-trace.sh`) records ENGINE key events only,
  is OFF by default, and its broadcast is debuggable-guarded — the release
  build on the phone had no logging channel at all. The expectation mismatches
  live ABOVE the engine: a swipe recognised as a tap, a symbol from the wrong
  page, a drifted mode — none of those become engine events (KP11 commits are
  direct).
- **The long-press wait was the system's** (`ViewConfiguration`): 500 ms on
  the test phone, 400 ms on the AVD. Maintainer (2026-11-26, verbatim):
  "long-press가 너무 긴데. 200ms 이하로 테스트해보고 싶어." and "설정할 수 있게
  하는 게 제일 좋아. 일단 그 값으로 테스트해 보려고 해." → DECIDED: a setting
  (설정 → 길게 누르기, presets `LONG_PRESS_CHOICES_MS` 100–500 ms, default
  `KeypadModel.LONG_PRESS_TIMEOUT_MS = 200` provisional; E1-pinned, E2E
  derives the default from the source). The view reads the stored value the
  window writes — one value, not two (the KP14 pattern). The trade-off to
  watch: a SLOW swipe can fire long-press before it passes the distance gate;
  the jitter cancel is the defence. If slow swipes become shifts in the
  evaluation, the gate or the timeout moves — recorded, not decided.

**The instrument**: `UiTraceRecorder` → `ui.trace` (a SECOND sink, its own
JSONL dialect; the normative engine trace format is untouched). Rides the
same debuggable-guarded broadcast. Events:

| line | records |
|---|---|
| `gesture` | page, key, kind, dx/dy/duration, recognized (tap/swipe_up/swipe_down/swipe_left/long_press/cancel), action, the gate values — "my swipe-up became a tap" is reconstructable |
| `symbol` | the page id and the whole entry committed (KP11 direct path) |
| `page` | from/to/trigger (tap/picker/cycle/return) |
| `shift` | armed/locked after each change |
| `mark` | a free-form annotation at the moment of interest |

Procedure (debug build on the device; `ANDROID_SERIAL` selects it):

```sh
tools/e3-trace.sh start eval-1        # both sinks on
tools/e3-trace.sh mark "기대: !"       # annotate a moment (optional)
# ... use the keyboard, reproduce the mismatch ...
tools/e3-trace.sh stop
tools/e3-trace.sh pull eval-1         # tests/conformance/eval-1.trace + .ui.trace
```

The expectation itself lives in the evaluator's head; the log captures what
the finger did, what the recognizer decided, and what the app delivered —
the mismatch is the difference. Evaluation pulls are session records, not
tier-C conformance data.

### Evaluation session 2026-11-26 — what the trace found, and the rulings

One phone session (SM-F971N, release-signed debuggable build, case
`usability-eval`): 316 UI gestures, 272 engine keys. Artefacts outside the
repo (`~/work/hunmin-measurements/usability-eval-312-2026-11-26/`, with
`meta.json`).

| # | measured in the trace | maintainer's ruling | outcome |
|---|---|---|---|
| 1 | backspace **committed** the composing word: 9 of 34 backspaces (26%) returned `consumed=false` with a commit; 8 of 39 typed characters were leftover jamo (`ㅅ`, `ㅈㅇ`, `ㅓ`, `ㄱ`, `ㅅㅍㅣ`). Reproduced deterministically: `hunmin-sim --layer word --keys $'rkrk\b\b\b'` → `commit '가'`; the ic path deletes normally | engine fix, both surfaces alike, its own issue | **issue #332**, requirement `docs/SPEC-FRONTEND.md#§FE17` — not fixed here (the Android gesture work is #312) |
| 2 | swipe-left on the **space bar** deleted text (2×, `dx ≈ -161`) | "커서 이동으로 수정하자" | implemented: `KeypadModel.swipeLeftAction` → `CURSOR_LEFT` on space, `DELETE` elsewhere; E1 pins the exception, the E2E round asserts `ab` + swipe-left + `x` → `axb` |
| 3 | two upward-left swipes on `t` (`dx = -63 / -117`) were read as swipe-up and typed `ㅆ` | "이건 시프트를 의도한 게 맞아" | no change: the recognition was correct; recorded so a future reader does not re-open it |

Gesture health in the same session: 300 taps, 3 long-press, 6 swipe-up, 1
swipe-down, 2 swipe-left — every non-tap mapped to its ruled action, all four
symbol pages cycled, symbols and `$` committed. The three long-press events
(`t`, `p`, `w`) each produced the doubled jamo the session needed (ㅆ·ㅖ·ㅉ), so
the 200 ms default was not the problem in that session.

## 7. What this does not settle

The extended-page geometry fork (grow / swap / paginate); the math inventory
ruling; the emoji scope and source; the kaomoji picker slot, inventory and
issue scope; device font coverage for kaomoji; swipe-up's meaning on
symbol-screen keys; the shift state machine (capslock unlock, KP7);
device-measured gesture thresholds and error rates (KP5's defences are
requirements, their tuning is measurement).

## 8. Verification plan

```sh
python3 tools/symbol-data-check.py            # extended per page (tier-B)
python3 tools/doctor.py libhunmin/data        # engine data untouched
ctest --test-dir build -j16 -LE slow          # inner loop
ctest --test-dir build -R engine-android-e1   # parse + commit goldens per page
ctest --test-dir build -R engine-android-e2e  # gesture rounds on the AVD (KP5)
```

Gesture 1's regression surface is the existing shifted-tap goldens plus a
swipe-up round per key class (digit, letter, frequent punctuation); gesture 2
adds picker-open / pick / page-commit / return rounds — all landed in
`tools/android-e2e.sh` (the gesture round; `HUNMIN_E2E_SKIP_TO_GESTURE=1` is
a selection lever for iterating on it, the default runs every round).

**Budget measurement (2026-11-23, `tools/ci-time-budget.sh`)**: inner loop
602.4 s / slow 115.8 s locally — both over, dominated by the PRE-EXISTING
structural fact that tier E2 is not labelled `slow` and runs inside the inner
loop (recorded in the wiki's symbol-layer-options page); the gesture round
adds ~2 min to E2 (553 s full, was ~305 s at T3-3). The CI linux job excludes
the android tiers; the android job carries them. Labelling E2 `slow` (or a
CI-side split of the gesture round) is a CI-structure decision for the
maintainer — recorded, not decided; the round's sleeps are tuned for emulator
reliability and were not shaved to fit a budget the lane already exceeded.