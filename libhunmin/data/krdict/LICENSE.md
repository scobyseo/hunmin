# 한국어기초사전 extract — provenance and licence

## Source

`국립국어원 한국어기초사전` (Korean Basic Dictionary), as redistributed in the
NIKE open-data dump (`spellcheck-ko/korean-dict-nikl`, `krdict/*.xml`). Not
vendored: 368 MB. Fetch with `tools/fetch-krdict.sh`; only this extracted table
is committed.

## Licence

**CC-BY-SA 2.0 KR** — the terms the National Institute of Korean Language
publishes its dictionaries under since 2019-03-11.

This is deliberately *not* the same licence as the rest of `libhunmin/data`
(`hanja/` is BSD-3 from libhangul, layouts and compose tables are ours under
LGPL-2.1+). CC-BY-SA's ShareAlike term applies to modifications of **this**
data, so it lives in its own directory rather than being merged into the
syllable table or the layout files. Keeping them separate means the LGPL parts
of the project are not dragged under ShareAlike by proximity.

Attribution required: "국립국어원 한국어기초사전".

## What was extracted

Entries whose `origin` contains Hanja — 34,142 of 56,555, since the rest are
Korean-origin words a hanja mode cannot use. Per entry: the Hangul form, the
Hanja (first reading, with alternates kept separately because `origin` allows
`/`-separated forms such as 無斷家出/無端家出), the definition, part of speech,
vocabulary level and semantic category.

Hanja length runs from 1 to 9 characters, averaging about 2.4. The long tail is
the point: `hanja/` covers a syllable at a time, this covers words and fixed
expressions (`金始初聞` = 금시초문, 8 characters).

## Upstream exclusions

The source README warns that example sentences quoted from publications, and
all media (audio, images) referenced by URL, are **not** open content. None of
that is extracted here: definitions are taken, examples and media URLs are not.

`origin` is stored verbatim, as the dictionary writes it — so it includes
Korean inflections (`加減하다`), a `▽` marker, and romanised parts where
Korean uses them (`gas料金` = 가스 요금). Only entries containing at least one
Hanja character are kept; the Hanja class is CJK Unified, Extension A and
Compatibility Ideographs, which excludes the Hangul block the source also uses
for inflected forms.
