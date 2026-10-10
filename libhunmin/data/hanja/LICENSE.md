# Hanja dictionary — provenance and licence

## Source

`data-orig/hanja/hanja.txt` is fetched from the Debian package
`libhangul-data` (version 0.2.0-1build1), which packages
`data/hanja/hanja.txt` from https://github.com/libhangul/libhangul.

## Licence

BSD-3-Clause. This is **not** LGPL-2.1 — the Debian copyright file in
`libhangul-data` marks `data/hanja/hanja.txt` (and `mssymbol.txt`) as
BSD-3-Clause, copyright 2005-2006 Choe Hwanjin. The LGPL-2.1 in that file
covers the rest of libhangul, not the dictionary.

BSD-3 is compatible with hunmin's LGPL-2.1+ and preferable to it here: a
data file carries none of LGPL's linking conditions.

## Format

One entry per line, `key:candidate:comment`, UTF-8, `#` comments at the top.
A key repeats once per candidate, and **file order is candidate priority**.
libhangul parses this with `strtok_r(buf, ":", &save)` and reads candidates
forward — see `hangul/hanja.c` in the libhangul tree.

Measured: 303,494 entries, 222,709 distinct keys, up to 352 candidates for
one key. Keys are syllables, and also some jamo/partial readings
(`ㄱㄴ순`) — a hanja search is not syllable-only.

## Candidate order

libhangul's own frequency pipeline, reimplemented in
`tools/hanja2data.py` from `data-orig/hanja/merge.py`: each candidate's
frequency comes from `freq-hanja.txt` + `freq-hanyaeo.txt` (max of the two),
candidates are sorted by it descending, stably, so ties keep source order;
duplicates are merged rather than repeated; `compat-table.txt` unifies
compatibility characters. Upstream runs this at build time, so the shipped
`hanja.txt` is *unsorted* — that is why the ordering has to be redone here.

## Vendoring

`data-orig/hanja/` holds the raw dictionary plus the three frequency/compat
tables and `merge.py`, sha256-pinned, same convention as every other
converter input. No build step needs the network.
