# The krdict fixture (issue #46)

`01.xml` is a **synthetic** extract from the 국립국어원 한국어기초사전 dump, small
enough to commit. It is NOT a sample of the dump: it is written by hand to
exercise every branch of `tools/krdict2data.py`, so the ctest test
`krdict-fixture` (tier-B) proves the converter's logic deterministically with no
368 MB dump present. `data-selfcheck` must be self-contained in a fresh
worktree; the dump-dependent full run is the maintainer-only
`tools/selfcheck-krdict.sh`, which is deliberately not a ctest test.

Branches covered, matching `data-orig/krdict/01.xml`'s shape:

- entry WITH origin (Hanja) → taken
- entry WITHOUT origin → skipped (not what a hanja mode shows)
- origin without any Hanja (pure romanised) → skipped
- multi-reading origin separated by `/` → `readings[0]` is `hanja`, the rest go
  to `hanja_alt` (`entries_with_alternate_hanja`)
- readings split by `/` where only some hold Hanja → the Hanja-free ones are
  dropped from the reading list (`gas料金 / bus 노선` shape)
- pos / vocabularyLevel / semanticCategory present → `pos`/`level`/`category`
- vocabularyLevel absent → no `level` key
- two Senses → only the first definition (the ATTR regexes search the whole
  entry body, so the FIRST match in document order wins)
- defn absent → `meanings: []`
- same `(ko, hanja)` twice → both kept, and `meta.distinct_word_hanja` counts
  the collapse
- sorted output: by `(ko, hanja)`, so the fixture order deliberately differs
  from the expected JSON order

`expected/krdict.json` is converter output, not hand-written. Regenerate it
deliberately when the converter changes:

```sh
python3 tools/krdict2data.py --orig tests/fixtures/krdict --out /tmp/kf \
  && cp /tmp/kf/krdict/krdict.json tests/fixtures/krdict/expected/krdict.json
```

Licence: the fixture is synthetic input to a CC-BY-SA-2.0-KR converter, not
derived data, so it carries no ShareAlike term — but it is kept under
`tests/fixtures/` with the rest of the test corpus rather than vendored anywhere.
