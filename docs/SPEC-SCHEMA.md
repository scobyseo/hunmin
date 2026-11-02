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

**No field declares whether the modes judgement can run on a layout.** That is
derived from the file's own `keys` and `compose` tables at load time
(`schema_loader.c` `compute_capability`) and answered by
`hunmin_ctx_layout_autoswitch_capable` / `hunmin_ctx_layout_old_hangul`: a layout
whose keys emit no jamo composes no Hangul, and one that emits a jamo outside the
modern 19/21/28 needs `hunmin_hangul_valid_old()`. The derivation is deliberate —
a declared field is one more thing a new layout can forget, and it can contradict
the tables it sits beside. `docs/SPEC-MODES.md` §R8-정정-3 is the requirement.

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

Consumed by `hunmin_suggest_count()` and friends (`libhunmin/src/candidate_tables.c`), which
binary-searches each table. **The sort order of every file is therefore load-bearing:**
it must be sorted by the field the engine searches — `entries` key for `hanja`, `ko`
for `krdict`, `reading` for `idioms`, all in UTF-8 byte order. `idioms` was first
generated sorted by `hanja`, which left every idiom lookup silently missing until a
tier-A test caught it. Each converter now sorts by its own search key and the
ordering is asserted by `libhunmin/tests/candidate-tables-test.cpp`.

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

`load_hanja` (`libhunmin/src/candidate_tables.c`) builds `detail` as `"ko | en [tag]"`,
dropping empty halves and the bar, so a frontend renders it verbatim with no new
ABI symbol. The candidate `entries` key is still the sort key and upstream
frequency order is unchanged. Coverage (top-64 window): 39% Korean gloss,
+42% English-only, ≈80% any gloss, up from 39%. Provenance (`input_sha256`, licence,
Unihan version) is pinned in `meta`; see `docs/HANJA-GLOSS-DESIGN.md` for the
measurement that chose this design and the options rejected.

Order is meaningful in all three: it is upstream frequency order, reproduced rather than
re-derived (`hanja/` reimplements libhangul's `merge.py`; see Provenance below).
⚠️ `hunmin-doctor` lints `hanja/`, `krdict/`, `idioms/` and `modes/` (`tools/doctor.py`
`check_hanja`/`check_krdict`/`check_idioms`/`check_modes`), so the CI data gate reaches
them. **This list is the one home for "which directories are linted":** a new data
directory without a check here is data the gate does not reach.

## English word list (`data/modes/`)

The data behind SPEC-MODES R8-정정's `english_valid` (ruling 2026-11-01:
hunspell `en_US` + a user dictionary; `docs/OPTIONS-MEASURED.md` §EDW is the
measurement behind that choice).

`data/modes/en-US.json` — a JSON **array of strings**, lowercase, sorted,
`[a-z]` plus an internal `'` or `-`. Generated by `tools/en-us2data.py` from
hunspell `en_US.dic` + `.aff` with one level of affix expansion; never
hand-edited. An object form `{"words": [...]}` is accepted by the loader so a
later schema can carry metadata beside the words.

`data/modes/meta.json` — written by the same run: input SHA-256s, the declared
SCOWL version, the expansion method, the counts (stems → expanded → shipped,
plus what was dropped and why), and what the list does **not** represent
(capitalisation, inflections past one affix level, compounds, the Korean-typed
latin strings of MODES-DATA §4-7d).

`data/modes/LICENSE.md` — SCOWL's permission notice and the `.aff`'s BSD, with
the attribution the notice requires. This directory is **not** ShareAlike like
`krdict/`; it ships apart from the base engine data because the ruling that
created the feature makes its data a separate distribution (B3). Which
mechanism carries that today, per surface, is
`docs/OPTIONS-MEASURED.md#§EDW-4`.

### What the artefact stores, and what it deliberately does not

An entry is a **word form and nothing else**: no part of speech, no frequency,
no level, no per-word annotation of any kind, and no ordering beyond the sort
above. The question SPEC-MODES R8-정정's `english_valid` asks is membership of
one finished buffer, and a field nothing reads is a field a later session adds
and then starts trusting.

The two questions `docs/MODES-DATA.md` §3 left open — whether the list stores
pos/frequency/level, and whether josa (조사, particle)/eomi (어미, ending)
attachment is modelled — were questions about a **Hangul** word list. Under
R8-정정 the Hangul side is judged by composition and needs no list, so they do
not apply to this artefact and are not deferred to a later schema revision.

That makes the object form's extension point narrow, and it is worth saying how
narrow: it may carry metadata only together with the code that reads it. A fact
*about* the list — provenance, method, counts, exclusions — belongs in
`meta.json`; the word array carries words.

### What the lookup does, and what it does not

The lookup's policy belongs to this section, not to the loader's implementation:
`wordset.c` may only do what is written here.

| question | rule |
|---|---|
| case | the artefact is lowercase, and the lookup folds ASCII `A`–`Z` before comparing, so a buffer reading `DON'T` matches `don't` |
| `'` and `-` | **part of the word.** Nothing strips them, nothing splits on them, and nothing normalises one to the other, so `dont` is not `don't` and `ice-cream` is not `ice cream` |
| shape | `^[a-z]+(?:['-][a-z]+)*$` — a leading or trailing `'`/`-`, a capital, a digit or any other character is outside the schema |
| length | no cap anywhere — not in this schema, not in the loader, not in the lookup |
| sort | strictly ascending UTF-8 byte order, unique. The lookup is a hash set, so order is not needed for correctness — it is needed for determinism and a readable diff |

**Loading is found, not named.** `hunmin_buffered_preedit_load_english_shipped(l)`
resolves `modes/en-US.json` through the same data-dir search the `hanja`/`krdict`/
`idioms` tables use (the context's dirs, else `HUNMIN_DATA_PATH`, else the build
default), so no frontend assembles a path itself;
`hunmin_buffered_preedit_load_english(l, path)` stays for a caller that already
has one. Both answer the same three states, and **every lookup fails closed**:

| return | meaning | `hunmin_buffered_preedit_english_valid` answers |
|---|---|---|
| `1` | loaded | membership |
| `-1` | no searched directory holds the file | `0` — absent data is B3's normal state: the feature is off, not "everything is English" |
| `0` | the file is found and does not parse | `0` — an installed-but-broken dictionary is an error, never an empty dictionary |

⚙️ `tools/doctor.py` `check_modes` is this section mechanically: word shape and
lowercase, strictly sorted and unique, `meta.json`'s required groups and nested
keys, `method.word_shape` equal to the pattern the check enforces (so the
declared policy and the checked one are one string), `counts.shipped` equal to
the array's length, and `LICENSE.md` present. ctest `schema-contract` runs it, so
a hand-edited artefact fails the commit gate rather than a lookup at runtime.

### A file that parses but holds words the schema forbids

**It is loaded, and the loader says so on stderr** — ruled by the maintainer
2026-11-01 on issue #173: *"중간 수준인 일단 해석은 하되 경고를 출력하는 방식으로
가자."* The reason is the choice of degradation: a damaged *installed* file
degrades to a smaller dictionary rather than to no feature, and the words it does
hold keep working. Silence is the failure this replaces — `["computer", 3]` used
to load as `1` with the `3` dropped and nothing said, which is a truncated list
that reads as complete.

| element | what the loader does | stderr |
|---|---|---|
| a string inside the shape | added | — |
| a string outside the shape (uppercase, a digit, a space, any other character) | **added** — the lookup already folds case, so `Computer` is found; a digit-bearing word is not, and that is what the warning is for | `warn: <path>: element <n> '<word>' is outside the schema's word shape -- loaded anyway` |
| not a string at all (number, object, `null`), or `""` | **skipped** | `warn: <path>: element <n> is not a word string -- skipped` |

The load still answers `1` and sets the ready flag. At most 8 bad elements are
named per load, then one line carries the total, so a wholly corrupt file is one
fact rather than 118k lines of stderr.

**The gate above is the refusing half of the same rule.** `check_modes` errors
on every shape in the table, so a file that ships never warns at runtime; this
half exists for the file that arrived by another road — a damaged download, a
hand-merged list, a repackaging distro. The tree stays strict, the runtime
degrades loudly, and `libhunmin/tests/wordset-test.cpp` asserts the runtime half:
a schema-violating file loads `1`, keeps its good words, and names the bad ones.

**The user dictionary is not in this directory.** It is a plain text file the
person owns — one word per line, `#` starts a comment, blank lines and a UTF-8
BOM tolerated — passed at runtime to
`hunmin_buffered_preedit_load_user_words`. The engine merges it with the shipped
list at lookup time and never writes back, so a frontend's own store remains the
persistence for words added through `hunmin_buffered_preedit_add_user_word`.

## Reference trace format (goldens / tier C)
JSONL: line 1 `{"meta":{...}}`; then one line per event
`{"in":{"ch":"q"}|{"key":"backspace"},"commit":"…","preedit":"…"}`
read after each engine step — exactly how frontends consume an IME. Generated from the
authoritative engine via `tools/gen-golden-libhangul.c` (`make goldens`, CI check
`goldens-reproducible`).

A shifted key has two spellings and both are legal: the resolved character (`"X"` for
Shift+x, `"!"` for Shift+1 — the convention every family uses) or the base character with
`"shift": true`, which is what `hunmin-sim --trace` records when a key is driven with an
explicit modifier and what tier A may carry without resolving it. `hunmin-sim --check` and
`tools/gen-conformance.py` must treat them as the same key: an event that ignores the
field replays the base key and measures the wrong row (ctest `engine-golden-shift-field`
is the fixture that fails when they do).

## Provenance / licensing
`data-orig/libhangul/` vendors libhangul 0.2.0's kbd/combination XML + `hangulkeyboard.h`
(LGPL-2.1+, © Choe Hwanjin et al.) as converter inputs. Keyboard **arrangements** are
functional data; each layout's `source` field records its normative origin for review.

`data-orig/hanja/` vendors libhangul 0.2.0's hanja dictionary — **BSD-3-Clause**, not
LGPL — together with the frequency and compatibility tables and the upstream `merge.py`
whose sorting algorithm `tools/hanja2data.py` reimplements. The shipped `hanja.txt` is
unsorted because upstream sorts at build time, so candidate order is reproduced here.
See `libhunmin/data/hanja/LICENSE.md`.
