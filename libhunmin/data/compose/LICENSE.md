# Composition tables — provenance and licence

## Source

The eight composition JSON files in this directory come from four inputs:

1. **`modern.json`** and **`modern-full.json`** — converted by
   `tools/kbd2data.py` from libhangul 0.2.0's
   `data/keyboards/hangul-combination-default.xml` and
   `hangul-combination-full.xml`. The raw XML is vendored under
   `data-orig/libhangul/`.
2. **`ahn.json`** and **`romaja.json`** — converted from the composition
   tables in libhangul 0.2.0's `hangul/hangulkeyboard.h`; libhangul never
   shipped these as XML.
3. **`sin3-p2-basic-mix.json`**, **`sin3-p2-mix.json`** and
   **`sin3-p2-yet-mix.json`** — lifted by `tools/ist2data.py` from the
   `UnitMixTable` of the three pat.im/1136 날개셋 ist XML files under
   `data-orig/naraeseot/`.
4. **`sin3-p2-yet-ohi.json`** — the OHI literal composition table,
   transcribed from the facts in pat.im/1136 sections 2-4 with the
   2022-12-24 deletions applied; see `data-orig/ohi/` and `docs/SPEC-P2.md`.

Every file carries its own `source` string, so the provenance travels with the
data. Layout files that use these tables name them in their `compose` field.

## Licence

The tables are derived works:

- libhangul 0.2.0's composition data is **LGPL-2.1-or-later**, copyright
  2009-2016 Choe Hwanjin and libhangul contributors. Only the composition
  facts are lifted, never libhangul's code.
- pat.im/1136's ist XML and OHI tables are published by 팥알 as freely
  reusable layout facts; see `data-orig/naraeseot/NOTICE` and `docs/SPEC-P2.md`.
- The conversion code is **LGPL-2.1-or-later**, copyright 2026 Seo Bon Keun.

This matches the tree's default terms for `libhunmin/data/*` in
`debian/copyright`. The full text is there.

## What this directory does not contain

- No keyboard layout tables: those live in `../layouts/`.
- No engine code: the composition rules here are data the engine interprets.
