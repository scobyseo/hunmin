# How to add a layout

Adding a layout is **a data change**. No C, no Kotlin, no CMake. If you find
yourself editing code to add one, you are doing it the wrong way — see AGENTS.md
rule #1, "Layouts are data, never code".

This is the procedure, with the parts that are easy to get wrong called out.

---

## 1. Decide which engine kind

Three exist, and the choice is not free — it determines what you may write in
`keys`:

| `engine` | Use for | Values you can write |
|---|---|---|
| `"std"` | ordinary two-set layouts (두벌식, 세벌식 3-2/3-90, 옛한글) | jamo strings, `=literals`, `amb`, `j/compose:false` |
| `"h3"` | anything whose behaviour depends on what is **already in the buffer** | everything `std` allows, plus `{"case": ...}` expressions |
| `"romaja"` | latin-to-Hangul syllabification | `{"seq": ...}` |

**Choosing `h3` is how you express a conditional layout without touching the
engine.** If your rule is of the form "this key means X unless the vowel slot is
occupied", it is a `case` expression, not a new primitive. Adding a primitive
requires a justification in `docs/SPEC-SCHEMA.md`; adding a `case` does not.

Note `libhangulType` is separate from `engine` and is only about provenance: which
libhangul algorithm your layout was ported from (`jamo`, `jamo-yet`, `jaso`,
`jaso-yet`). `std` + `jamo` is 두벌식. Leave it out only if `jaso` is right.

---

## 2. Write `libhunmin/data/layouts/<id>.json`

`docs/SPEC-SCHEMA.md` §"Layout file" is the contract; this is the shape:

```json
{
  "schemaVersion": 1,
  "id": "my-layout",
  "name": { "en": "My Layout", "ko": "마레이아웃" },
  "engine": "std",
  "compose": ["modern"],
  "keys": {
    "k": { "plain": "ᄀ", "shift": "ᄏ" },
    "o": { "plain": "ᅩ" },
    " ": { "plain": " " }
  }
}
```

Four rules that are not obvious:

**`id` must equal the filename stem**, and must match `[a-z0-9.-]+`. Not numeric
— layout identity is a string everywhere, including the ABI.

**`name` needs both `en` and `ko`.** The `ko` name is the label the Android
layout picker shows, so a missing one produces a blank row in the UI, and
`hunmin-doctor` warns about it.

**Key ids are US-QWERTY positions named by the unshifted character.** `Q` is
`q` with `"shift"`, never a key called `"Q"`. This is what lets one set of tier-A
goldens compare across X11, Android and Windows. If your layout wants a non-US
physical key, that is a *frontend* change (and a documented one) — the layout
data cannot express it.

**`compose` lists tables in precedence order, and "first file wins" is
documented behaviour.** Two tables may legitimately disagree about the same
`(a,b)` pair; the first one listed is the one that applies. Do not try to make
them agree.

### Value forms

`docs/SPEC-SCHEMA.md` §"Values" is authoritative. The two that cause the most
damage:

- **Unmapped is `null` or absent** — flushes the composition, does not consume
  the key, and the frontend passes it to the application. `null` and `""` are
  different things.
- **Any non-jamo character must be written `"=c"`** — `"="`, `"“"`, `"✳"`.
  A bare jamo string is a jamo; anything else needs the `=` prefix or the loader
  will reject it.

---

## 3. Validate before you test

```sh
python3 tools/doctor.py libhunmin/data
```

This is the cheapest gate in the repo and it catches most mistakes: a missing
`ko` name, a bad `schemaVersion`, an unmapped key that should be a literal, a
`compose` id that does not exist, a filler jamo (`U+115F`/`U+1160`, which are
illegal in data).

**`data PRs must pass `hunmin-doctor`.** It is a ctest too
(`schema-contract`), so CI runs it, but running it first is faster than
waiting for CI.

---

## 4. Add a golden

```sh
./build/hunmin-sim --data libhunmin/data --layout my-layout --keys dkkssud --trace
```

Turn the trace into a corpus in `tests/golden/`, one file per case, in the
`.jsonl` format (`docs/SPEC-SCHEMA.md` §"Reference trace format"). Rules:

- **Cases are independent.** Each `{"meta": {"case": ...}}` line opens a case
  that starts from a **fresh** `hunmin_ic`. A trailing preedit does not carry
  into the next case, and a golden that concatenates cases is wrong — tier E1
  replayed exactly that way once and compared nonsense.
- **Cover the parts that are specific to your layout.** For a `h3` layout that
  means each `case` expression, both branches.
- **Write down where the expectations came from.** `meta.source` and
  `meta.spec` are not decoration; a golden nobody can trace back to an
  authority is an assertion with no provenance.

Then register it (`CMakeLists.txt` has one `engine-golden-*` per layout) and run
it:

```sh
cmake -B build && cmake --build build
ctest --test-dir build -R engine-golden-my-layout
```

---

## 5. Cross-platform check

A layout is not done when the host golden passes. Every frontend must agree:

```sh
python3 tools/keymap-fixture.py          # every key you used has a platform encoding
ctest --test-dir build -R "conformance|android"   # tier C replay + the Android tiers
./tools/android-golden-check.sh          # the same goldens on a real device
```

`keymap-fixture.py` is the one that bites: if your layout uses a key that has no
AOSP keycode, Android cannot reach it and no host tier will tell you. The table
it checks was wrong for all 47 keys before that was measured.

---

## 6. Checklist

- [ ] `tools/doctor.py libhunmin/data` — 0 errors
- [ ] `id` == filename stem, matches `[a-z0-9.-]+`
- [ ] `name.en` and `name.ko` both present
- [ ] key ids use the unshifted US-QWERTY character; shifted forms via `"shift"`
- [ ] non-jamo characters written `"=c"`
- [ ] golden added in `tests/golden/`, one independent case per line group
- [ ] registered in `CMakeLists.txt`, `ctest -R engine-golden-<id>` passes
- [ ] `keymap-fixture.py` passes (every key reachable on every platform)
- [ ] `hunmin-sim --trace` output matches what you intended, key by key
- [ ] if `engine: h3`: every `case` expression has a golden for **both** branches

---

## When you do need to change code

Rarely, and never for "this layout has a rule the engine cannot express". The
legitimate cases:

- **A genuinely new composition primitive.** Justify it in
  `docs/SPEC-SCHEMA.md` first; the default answer is to express it with the
  existing `Literal | SlotJamo | Ambiguous | Sequence` primitives plus rule
  tables plus per-layout flags.
- **A new platform key encoding.** Then it is a frontend table, and
  `tools/keymap-fixture.py` is where it is proved rather than assumed.
- **A data format change.** Bump `schemaVersion`; loaders reject anything newer
  than they support.

Note what is *not* on that list: per-layout fields in core structs, per-layout
`switch` statements, or numeric layout ids in the ABI. Those are the three
things the architecture exists to prevent.
