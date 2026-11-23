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
- **Long-press is identical to swipe-up** — decided (maintainer 2026-11-23,
  verbatim in `docs/SPEC-ANDROID-KEYPAD.md#§KP7`: "long press도 swipe up과
  동일하게 구현해 두자. 나중에 필요하면 기능을 다르게 만들지 뭐."). One
  implementation of the gesture pair; a later split is the named exit.
- **The named exception: backspace.** Long-press on backspace deletes a word
  (KP14, decided 2026-11-23, verbatim in the SPEC: "swipe-left 는 backspace로
  사용할 거고, backspace에서 long press는 단어 삭제야.") — the WORD unit of
  the existing swipe-left delete mechanism; swipe-left as backspace is
  reconfirmed by the same ruling.
- **Symbol-screen keys have no shift meaning** (KP6: every symbol is its own
  button, direct commit). Swipe-up/long-press there is undefined for now —
  recorded open, not decided.

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
adds picker-open / pick / page-commit / return rounds. Test cost is measured
against the budgets before push (`tools/ci-time-budget.sh`).