# Keyboard layouts — provenance and licence

## Source

The 15 layout JSON files in this directory are generated or hand-authored from
three kinds of input:

1. **Nine layouts converted from libhangul 0.2.0's keyboard XML** by
   `tools/kbd2data.py`: `dubeolsik` (2), `dubeolsik-yet` (2y), `sebeolsik-3-2`
   (32), `sebeolsik-3-90` (39), `sebeolsik-final` (3f), `sebeolsik-noshift`
   (3s), `sebeolsik-yet` (3y), `romaja` (ro) and `ahnmatmae` (ahn). The raw
   inputs are vendored verbatim under `data-orig/libhangul/`; each JSON file's
   own `source` field names the exact XML.
2. **Three 신세벌식 P2 layouts lifted from pat.im/1136's 날개셋 ist XML** by
   `tools/ist2data.py`: `sin3-p2-basic`, `sin3-p2` (symbols) and
   `sin3-p2-yet`. The three normative XML files are vendored under
   `data-orig/naraeseot/` with a `NOTICE` and sha256 pins;
   `docs/SPEC-P2.md` records the extraction rules and the cross-checks.
3. **Three layouts authored here** — `us` and the Colemak/Dvorak physical-key
   tables — whose key ids follow this project's own convention
   (`docs/SPEC-SCHEMA.md` "Key ids"). They are not copied from an implementation.

Each generated file carries its `source` string, so provenance travels with the
data even when the directory is shipped without this file's neighbours.

## Licence

The generated tables are derived works:

- libhangul 0.2.0's keyboard data is **LGPL-2.1-or-later**, copyright
  2009-2016 Choe Hwanjin and libhangul contributors. No libhangul code is
  copied — only the key-to-jamo facts are lifted and rewritten as JSON.
- pat.im/1136's ist XML is published by its author 팥알 as freely reusable
  layout arrangements; see `data-orig/naraeseot/NOTICE`. The layout facts are
  normative for 신세벌식 P2 (`docs/SPEC-P2.md`), not the XML's expression syntax.
- The conversion code and the hand-authored tables are **LGPL-2.1-or-later**,
  copyright 2026 Seo Bon Keun.

This is the same licence as the tree's default for `libhunmin/data/*` in
`debian/copyright`; the per-directory statements exist so the terms travel with
the data into any other consumer, not just the Debian package. The full text is
in `debian/copyright`.

## What this directory does not contain

- No keycap artwork, no OS keymap binaries, and no layout id numbers in the
  ABI: layouts are selected by string id (`docs/SPEC-SCHEMA.md`).
- No generated composition rules: those live in `../compose/` and are named by
  each layout's `compose` field.
