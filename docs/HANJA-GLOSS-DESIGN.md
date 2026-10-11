# Hanja gloss design (hanja/2)

Status: **proposed** — awaiting maintainer approval before any `data/` or
engine change. This file records the measurement that motivates the design and
the options that were measured and rejected, per AGENTS.md ("document first",
"record what you got wrong", "present designs as comparable; leave the choice
open"). The decision is the maintainer's.

## The problem

The Hanja candidate dialog shows `恨 (hanja: )` — the Hanja with an empty
gloss. The cause is not missing data: `data-orig/hanja/hanja.txt` (libhangul
0.2.0) already carries a per-candidate comment (`한:韓:나라 이름 한, 한나라 한`),
but `tools/hanja2data.py` parses the comment and then drops it when it writes
`hanja.json` (`entries[key] = [e["value"] …]`). The engine's `load_hanja` in
`libhunmin/src/suggest.c` therefore fills `detail` with `""`.

So the fix is to *use* the gloss that is already vendored, and to decide what
to do about the candidates that genuinely have none.

## What the data actually covers (measured 2026-10-11)

Sources measured against the candidate table (555 single-syllable keys):

| source | what it holds | coverage of a syllable's candidates |
|---|---|---|
| `data-orig/hanja/hanja.txt` (libhangul, BSD-3) | Hangul syllable → Hanja → Korean gloss | top-64 window **39%**, top-16 **22%**, but the *practical* candidates (top ~8–12) are almost fully glossed |
| `libhangul/data/krdict/krdict.json` (CC-BY-SA) | word/phrase → Hanja + meaning | **2.4%** of bare-syllable Hanja — a word meaning, not a Hanja meaning |
| `libhunmin/data/idioms/idioms.json` (MIT) | 4-char idiom → reading + meaning | 0.3% |
| Unihan `kDefinition` (Unicode Licence v3, English) | Hanja → English definition | of the *unglossed* Hanja, **68%** have one |
| latest libhangul `main` (fetched) | same fields | **27.2% gloss, identical to 0.2.0** — no gain from upgrading |

Two measurements matter most:

1. **Syllable-level "every candidate glossed" does not improve with Unihan.**
   Adding Unihan to libhangul moved top-64 full coverage from 2.0% → 2.0%,
   top-16 22.2% → 22.2%. The unglossed candidates are rare/extension Hanja that
   Unihan also lacks *or* that sit behind already-glossed practical candidates.
2. **The unglossed tail is rare Hanja and simplified/Japanese variant forms**,
   not everyday characters. In the top-12 of `한`/`수`/`가`, the only unglossed
   entries are `宿` and `价` (a simplified variant of `價`).

## Options measured and rejected

- **Join krdict meanings.** Rejected: 2.4% coverage, and a krdict meaning is a
  *word* sense, not a Hanja sense — `水` would gain "the third day of the week"
  (수요일), which is wrong for the character.
- **Unihan as the primary gloss.** Rejected: it is English; `국`-context Korean
  input wants Korean. Kept as a secondary (English) source only.
- **Translate Unihan English → Korean by LLM.** Rejected on measurement: the
  candidates that would be translated are rare Hanja whose definitions mix
  Cantonese readings, simplified forms, onomatopoeia (`揢 "(Cant.) to block"`,
  `㗆 "sound of the ringed pheasant"`). In the top-16 window the count of
  "unglossed + has-English" is 1576, all rare; translation cost is high,
  reproducibility is model-version dependent, and AGENTS.md bars artefacts
  without retrievable provenance. The user agreed to drop translation.
- **Label simplified/Japanese-variant forms.** Kept, but only the
  *conservative* subset — see below. The full rule mislabels common characters:
  `国`/`学` are simplified forms that Korea also uses in personal names, and
  `家`/`脚` carry 異體字 variants that a naive `kTraditionalVariant` test reads
  as simplified. A wrong label is worse than no label.

## The proposed design

`hanja/2` entries become objects instead of bare strings:

```json
{"한": [{"v":"韓","ko":"나라 이름 한, 한나라 한","en":"fence; surname; Korea"}, …]}
```

`v` = the Hanja, `ko` = libhangul Korean gloss (may be ""), `en` = Unihan
English `kDefinition` (may be ""). A conservative usage label is stored per
candidate when it can be assigned without false positives:

- `jp` — Japanese shinjitai: no variant fields + (`kJoyoKanji` or
  `kJinmeiyoKanji`) + not used in Korean text/education/name. Verified on
  `収 価 図 桜 沢 遅 拝 辺 塀 弁 庁 浜` (14/15), zero false positives on Korean
  common characters.
- `cn` — simplified Chinese: `kTraditionalVariant` names a Hanja other than
  itself + not used in Korean at all (text, education, or name). Verified zero
  false positives on `家 脚 價 韓 水 漢`; `价 国 学` are suppressed because Korea
  uses them in personal names — a *missed* label, accepted over a wrong one.

Frontend display (Android `OverlayActivity`, and any future candidate strip):

```
韓 (나라 이름 한 | fence; surname; Korea)
収 (collect) [日]
汉 (the Chinese people) [中简]
```

Korean gloss first, English after `|`; usage tag in brackets only when
assigned; bare Hanja when both glosses are empty. `krdict` is deliberately not
joined (word-sense distortion).

## Coverage after the design (top-64 window, 19236 candidates)

- Korean + English both: 37.5%
- Korean only (libhangul): 1.6%
- English only (Unihan): 41.5%
- neither: 19.5%
- **any gloss: 80.5%**, up from 39% today.

## Licence / provenance

- Korean gloss: libhangul `hanja.txt`, BSD-3-Clause (already vendored,
  `libhunmin/data/hanja/LICENSE.md`).
- English gloss + variant/usage fields: Unihan, Unicode Licence v3 (use,
  copy, modify, distribute permitted with the copyright notice retained; no
  ShareAlike). Must be vendored under `data-orig/unihan/` with its own
  `SHA256SUMS`, `NOTICE`, and `LICENSE.md`, and pinned in the output `meta`
  exactly as `hanja2data.py` already pins its inputs (`input_sha256`).
- Unihan 18.0.0 was used for the measurement (`Unihan.zip`, 2026-07-31); the
  exact version pinned in `meta` must be the one vendored.

## Blast radius (all must change together)

- `tools/hanja2data.py` — emit `hunmin.hanja/2`, read Unihan, attach glosses
  and usage labels, keep the frequency/compat pipeline and selfcheck.
- `libhunmin/data/hanja/hanja.json` — regenerated (grows; Korean gloss +
  English gloss + tags).
- `libhunmin/src/suggest.c` `load_hanja` — parse v2 objects, feed `detail`
  (format agreed with the frontend), and expose the usage tag.
- `tools/doctor.py` `check_hanja` — validate v2 (was `== "hunmin.hanja/1"`).
- `docs/SPEC-SCHEMA.md` — document v2, licence column, sort-key unchanged
  (`entries` key), candidate order unchanged.
- `libhunmin/tests/suggest-test.c` — detail expectations currently assert `""`
  for Hanja (`expect(ctx, …, "hanja", "水", "")`); update to the real glosses.
- Android `HunminData.DATA_SUBDIRS` already includes `hanja`, so the regenerated
  file ships; confirm the APK size budget (English glosses add ~size).
- `frontends/android/…/OverlayActivity.kt` — render `ko | en [tag]`.

## Open questions for the maintainer

1. `detail` string format from the engine: `"ko | en"` concatenated in C, or
   separate fields (`suggestDetail` + a new accessor)? Concatenation keeps the
   ABI append-only with no new symbol; a separate tag accessor would add one.
   The ABI is append-only, so a new symbol is allowed but must be recorded in
   `libhunmin/tests/abi/symbols.txt`.
2. Should the conservative `cn`/`jp` labels ship at all, or wait until the
   labels are complete? They are shipped precisely because they are
   conservative; incomplete is safe, wrong is not.
3. `hunmin.hanja/2` vs a `hanja/1`-compatible mixed form (string-or-object).
   `doctor.py` currently pins `/1`; v2 is cleaner and matches the project's
   versioned-schema convention, but breaks the v1 file.
