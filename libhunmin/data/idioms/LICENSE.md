# Four-character idioms — provenance and licence

## Source

`data-orig/idioms/idioms.json`, from https://github.com/LiF-Lee/idioms
(MIT, © 2021 EliF). 2,318 entries, vendored verbatim with a sha256 pin, like
every other converter input.

## Licence

**MIT** — the friendliest licence of anything vendored here, and deliberately
so: `hanja/` is BSD-3 and `krdict/` is CC-BY-SA 2.0 KR, so this directory is
the one part of the Hanja data with no attribution or ShareAlike obligation
beyond naming the source.

## What was extracted

Each entry's `Q` is `"<hanja> (<reading>)"` and `A` is the meaning. Measured
over all 2,318: every `Q` parses, every head is exactly four Hanja characters,
and the reading always has the same syllable count as the head — so the
pairing needs no heuristics and the converter has no fallback path.

One entry is defective in the source: 皮闌會獄 (피란회옥) is written
`피갈회옥` in Hangul in its `Q` field, so that head carries no Hanja. It is
kept verbatim and reported by `entries_without_hanja_in_head`; correcting it
would put an unverifiable claim into a data file. Note that
`head_length_distribution` counts characters, so this one still appears under
4 — the explicit counter is what flags it.

Two readings are shared by more than one Hanja form (如足如手/與足與手 =
여족여수, 虎死留皮/豹死留皮 = 호사유피), and nine Hanja forms appear under two
readings, mostly where a laryngeal ㄱ shifts to ㄷ or ㅂ (乾木水生 =
간목수생/건목수생). Both are kept as separate entries; neither is merged,
because the source treats them as distinct idioms.

Note the readings are the Korean readings of the *idiom*, which need not be a
literal reading of the characters — 一石二鳥 is 일이 아니라 일석이조,
一衣帶水 is 의가 아니라 의대수. This is standard Sino-Korean lexicon, not a
parsing problem, so nothing is normalised.

## Coverage limit

This source is entirely four characters. Longer fixed expressions —
천상천하유아독존 and similar — are absent; those are the province of
`krdict/`, and of a data source not yet found.
