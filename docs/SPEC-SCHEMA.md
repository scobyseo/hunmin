# hunmin layout data contract (schemaVersion 1..2)

Files: `libhunmin/data/layouts/*.json` (one per keyboard layout) and
`libhunmin/data/compose/*.json` (jamo combination tables). Layouts are **discovered at
runtime** from configured directories; adding a layout never touches code.
All files are UTF-8 JSON, deterministically ordered, validated by `tools/doctor.py`.

## Layout file

| field | meaning |
|---|---|
| `schemaVersion` | int; loaders reject values newer than supported |
| `id` | canonical string id, `[a-z0-9.-]+`, must equal filename; **never numeric in ABI** |
| `name` | `{"en":..., "ko":...}` display names |
| `aliases` | legacy ids (e.g. libhangul `"2"`, `"39"`) |
| `engine` | `"std"` (libhangul-portable jamo composition), `"h3"` (std + `case` expression values, 날개셋 semantics), or `"romaja"` (sequence engine, P1+) |
| `mode` | `"hangul"` or `"latin"` — which half of the two-mode IME this layout belongs to. **Required.** Read by the engine and exposed as `hunmin_ctx_layout_mode(ctx, i)`; the Android settings window splits its layout list on it (issue #12). Absent or unrecognised means `"hangul"` in the engine so a typo costs one mis-filed row rather than the layout — `tools/doctor.py` is what makes the typo fail instead. `romaja` is `hangul`: it is typed in Latin but composes Hangul, and the mode says what the layout *produces*. |
| `libhangulType` | provenance for std engines: `jamo`, `jamo-yet`, `jaso`, `jaso-yet` — selects the exact algorithm ported from libhangul (defaults to `jaso` when absent) |
| `source` | provenance string (normative origin of the arrangement) |
| `compose` | ordered ids of `data/compose/` tables this layout uses |
| `flags` | optional per-layout bools, extended only via new primitive (see PLAN.md) |
| `keys` | map key-id → modifier map (subset of `plain`, `shift`, ctrl/alt reserved) |

### Key ids
Single characters are physical positions in the US layout **named by their unshifted
char**: base ids `a`–`z`, `0`–`9`, and the 21 punctuation chars `` ` - = [ ] \ ; ' , . / ``.
A shifted letter/digit/punct is the same base id with `"shift"` — e.g. `Q` becomes
`q.shift`. Special names: `space`, `enter`, `backspace` (shift column on these is legal).
Frontends map platform events (XKB keysym / Android keycode / Windows VK) to
`(base id, modifier)`; those tables are tier-C tested, not data.

### Values
- `null` or absent key/mod → **unmapped** (libhangul "null entry" semantics:
  flushes any composition, key not consumed, nothing committed; frontends pass it
  through). Distinct from `null`-vs-literal is critical: `"=a"` etc. commit the char.
- `"=c"` → literal char `c` committed directly (`"=="` = the char `=`). Any non-jamo
  codepoint (incl. symbols like `“`, `✳`, `※`) must use this form — the loader must
  not range-reject literals by codepoint.
- one **jamo char string** → slot inferred from Unicode block: cho `U+1100–115E`;
  jung `U+1161–11A7`, `U+A960–A97C`, `U+D7B0–D7C6` (Jamo Ext-A/B, 옛한글);
  jong `U+11A8–11FF`, `U+D7CB–D7FB`. Fillers `U+115F/U+1160` are illegal in data.
- `{"j": jamo, "compose": false}` → jamo that never absorbs a following vowel
  (P2's right-side pure jung, `$vowel` in kime notation).
- `{"amb": [j1, j2], "first": j}` → dual-slot key; slots must differ; resolution rules in
  `docs/SPEC-P2.md` (kime-verified semantics).
- `{"case": {"if": expr, "then": <value>, "else": <value>}}` (schemaVersion 2) →
  condition-resolved value. `expr` is the lifted 날개셋 expression grammar over engine
  predicates: identifiers `D` (cho slot occupied — content codepoint), `E` (jung slot
  codepoint), `F` (jong slot occupied — codepoint); operators `!`, `&&`, `||`, `==`,
  `!=`, `>=`, `<=`, `>`, `<`, parentheses; numeric literals are Unicode codepoints
  (converter rewrites 날개셋 virtual IDs 501-504 to the unified vowels ᅩᅮᅳᅠ).
  `then`/`else` are any value form (no nesting limit beyond engine support).
  Semantics per `docs/SPEC-P2.md`; engine evaluates with the current buffer state
  before dispatching to the composition machine. `engine` field must be `"h3"`.
- `{"seq": ...}` → reserved for symbol-tier sequences (j + k/l/;); doctor warns until the
  engine implements it (P2 tiers arrive with the symbols ist conversion).

## Compose file
```json
{"schemaVersion":1,"id":"modern","source":"...","items":[{"a":"ᄅ","b":"ᄀ","r":"ᆰ"}]}
```
Jamo-pair → result rules; `(a,b)` unique. Slot context (which pairs are cho/comp/jong) is
applied by the engine the same way libhangul applies its tables. Bundled v0:
`modern` (libhangul default), `modern-full` (libhangul yetgeul set),
`ahn`, `romaja` (extracted from libhangul's C arrays — upstream ships no XML for them).

## Hanja files (`data/hanja/`, `data/krdict/`, `data/idioms/`)

Three separate schemas, each `{"version":…,"schema":"hunmin.<name>/…","entries":…,
"meta":{…}}`. They are **not** layout or compose files. Each carries its own licence, so each is
its own directory; merging them would drag CC-BY-SA 2.0 KR (`krdict`) under the LGPL
parts of the project.

Consumed by `hunmin_suggest_count()` and friends (`libhunmin/src/suggest.c`), which
binary-searches each table. **The sort order of every file is therefore load-bearing:**
it must be sorted by the field the engine searches — `entries` key for `hanja`, `ko`
for `krdict`, `reading` for `idioms`, all in UTF-8 byte order. `idioms` was first
generated sorted by `hanja`, which left every idiom lookup silently missing until a
tier-A test caught it. Each converter now sorts by its own search key and the
ordering is asserted by `libhunmin/tests/suggest-test.c`.

Candidate order returned to a caller: idioms, then words, then bare syllable
candidates. Glossed senses therefore lead a lookup. `krdict` is not length-gated —
it has 527 one-character entries (`壽` "a long life", `手` "a game of go" for `수`)
and they are real words.

| dir | schema | keyed by | licence |
|---|---|---|---|
| `hanja/` | `hunmin.hanja/2` | single syllable → ordered candidate objects | BSD-3 (Korean gloss) + Unicode v3 (English gloss, tags) |
| `krdict/` | `hunmin.krdict/1` | whole word/phrase → Hanja + meaning | CC-BY-SA 2.0 KR |
| `idioms/` | `hunmin.idioms/1` | four-character idiom → reading + meaning | MIT |

### `hunmin.hanja/2` (was `/1`)

Version 2 changed the candidate value from a bare Hanja string to an object so the
gloss that libhangul already carried (and `hanja2data.py` used to drop) is shipped,
plus an English gloss and a conservative usage tag from Unihan 18.0.0:

```json
{"한":[{"v":"韓","ko":"나라 이름 한, 한나라 한","en":"fence; surname; Korea"},
       {"v":"仮","ko":"假의 略字","en":"falsehood","jp":true}, …]}
```

- `v` — the Hanja (required, single char).
- `ko` — Korean gloss, from libhangul `hanja.txt` (may be `""`).
- `en` — English gloss, from Unihan `kDefinition` (may be `""`).
- `jp` / `cn` — usage tag, present only when the converter could assign it without
  a false positive: `jp` = Japanese shinjitai (no variant fields + Joyo/Jinmeiyo +
  not used in Korean); `cn` = simplified Chinese (`kTraditionalVariant` names
  another Hanja + not used in Korean at all). `家`/`脚` are never tagged; `价`/`国`
  /`学` are missed (Korea uses them in personal names) rather than mislabelled.
  A wrong tag is worse than a missing one.

`load_hanja` (`libhunmin/src/suggest.c`) builds `detail` as `"ko | en [tag]"`,
dropping empty halves and the bar, so a frontend renders it verbatim with no new
ABI symbol. The candidate `entries` key is still the sort key and upstream
frequency order is unchanged. Coverage (top-64 window): 39% Korean gloss,
+42% English-only, ≈80% any gloss, up from 39%. Provenance (`input_sha256`, licence,
Unihan version) is pinned in `meta`; see `docs/HANJA-GLOSS-DESIGN.md` for the
measurement that chose this design and the options rejected.

Order is meaningful in all three: it is upstream frequency order, reproduced rather than
re-derived (`hanja/` reimplements libhangul's `merge.py`; see Provenance below).
⚠️ `hunmin-doctor` lints `hanja/`, `krdict/` and `idioms/` (`tools/doctor.py`
`check_hanja`/`check_krdict`/`check_idioms`), so the CI data gate reaches them.

## Reference trace format (goldens / tier C)
JSONL: line 1 `{"meta":{...}}`; then one line per event
`{"in":{"ch":"q"}|{"key":"backspace"},"commit":"…","preedit":"…"}`
read after each engine step — exactly how frontends consume an IME. Generated from the
authoritative engine via `tools/gen-golden-libhangul.c` (`make goldens`, CI check
`goldens-reproducible`).

## Provenance / licensing
`data-orig/libhangul/` vendors libhangul 0.2.0's kbd/combination XML + `hangulkeyboard.h`
(LGPL-2.1+, © Choe Hwanjin et al.) as converter inputs. Keyboard **arrangements** are
functional data; each layout's `source` field records its normative origin for review.

`data-orig/hanja/` vendors libhangul 0.2.0's hanja dictionary — **BSD-3-Clause**, not
LGPL — together with the frequency and compatibility tables and the upstream `merge.py`
whose sorting algorithm `tools/hanja2data.py` reimplements. The shipped `hanja.txt` is
unsorted because upstream sorts at build time, so candidate order is reproduced here.
See `libhunmin/data/hanja/LICENSE.md`.
