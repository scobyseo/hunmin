# English word list — provenance and licence

## Source

`en-US.json` is generated from the **hunspell `en_US`** dictionary
(`en_US.dic` + `en_US.aff`), which belongs to the SCOWL family at
**SCOWL size 60** — the same vocabulary size as Debian's `wamerican` package,
which is the list `docs/MODES-DATA.md` §4-7d measured.

- Upstream package: <https://github.com/LibreOffice/dictionaries> (`en/`).
- Generator: `tools/en-us2data.py`; the measured cost of this choice, and of the
  alternatives, is `docs/OPTIONS-MEASURED.md` §EDW.
- `meta.json` next to this file is written by that tool on every run and records
  the input files' SHA-256, the declared version, the expansion method, the
  counts, and what the list does **not** represent. Do not edit it by hand: an
  edited provenance block has already drifted from its tool.

## Licence

The SCOWL collective work (© 2000–2018 Kevin Atkinson and the contributors named
in its copyright file) is redistributed under its own permission notice:

> Permission to use, copy, modify, distribute and sell these word lists, the
> associated scripts, the output created from the scripts, and its documentation
> for any purpose is hereby granted without fee, provided that the above
> copyright notice appears in all copies and that both that copyright notice and
> this permission notice appear in supporting documentation. Kevin Atkinson makes
> no representations about the suitability of this array for any purpose. It is
> provided "as is" without express or implied warranty.

Parts of SCOWL carry their own permissive terms, which the SCOWL copyright file
reproduces in full: the MWords package is public domain, the 12Dicts package and
supplement are public domain, and the WordNet-derived part is under the Princeton
WordNet licence. The `.aff` file carries its author's BSD licence.

The full upstream copyright file is reproduced by the Debian `wamerican`
package (`/usr/share/doc/wamerican/copyright`) and by SCOWL itself
(<http://wordlist.sourceforge.net>). **Attribution is required**: keep this file
and the notice above beside any redistribution of `en-US.json`.

## Why this is not in the same licence class as `krdict/`

`krdict/` is CC-BY-SA and lives in its own directory so ShareAlike does not reach
the rest of `libhunmin/data/`. This directory has no ShareAlike term to isolate —
SCOWL's notice permits redistribution and sale with attribution, which is
compatible with shipping it beside the LGPL-2.1+ engine. It is a separate
directory anyway, because the feature's data ships separately (B3) and because a
reader who wants to know where an English word list came from should find one
answer in one place.

## What is not here

A **user dictionary** is not part of this artefact. A person's own words live in
their own file (one word per line, `#` comments), passed to the engine through
`hunmin_buffered_preedit_load_user_words`; the engine merges it with this list at
lookup time and never writes back to this directory.
