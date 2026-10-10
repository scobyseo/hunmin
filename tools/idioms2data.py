#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Convert the vendored four-character idiom list into hunmin's phrase table.

Source: data-orig/idioms/idioms.json (LiF-Lee/idioms, MIT, (c) 2021 EliF).
Two fields per entry: `Q` = "<hanja> (<hangul reading>)" and `A` = the
meaning. Measured over all 2,318 entries: every `Q` parses, every head is
exactly four Hanja characters, and the reading always has the same syllable
count as the head -- so the pairing needs no heuristics.

Why this table exists alongside the other two:

  krdict/  word- and phrase-level Hanja with a definition, but only where the
           National Institute dictionary happens to carry the entry
  hanja/   one syllable at a time, frequency-ordered
  idioms/  the common four-character idioms, which neither of the above covers

Search-wise it fills the gap in the other direction: it is keyed by Hangul, so
`家家戶戶` is reachable from 가가호호, and it is keyed by Hanja too, so a user
who knows the characters can look the meaning up.

Note on coverage, measured not assumed: this source is entirely four
characters. Longer fixed expressions — 천상천하유아독존 and similar — are not
here. Those come from krdict/ when it has them.

One entry is defective in the source: 皮闌會獄 is written 피갈회옥 in Hangul
in the `Q` field, so it carries no Hanja at all. It is kept as-is and
reported by the `hanja_in_head` count rather than corrected — guessing the
intended characters would put an unverifiable claim in a data file.

Deterministic: same inputs -> byte-identical output (tools/selfcheck.py).
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

SCHEMA = "hunmin.idioms/1"
HAN = re.compile("[\u4e00-\u9fff\u3400-\u4dbf\uf900-\ufaff]")
Q = re.compile(r"^(.+?)\s*\((.+?)\)$", re.S)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--orig", type=Path, required=True,
                    help="data-orig/idioms (idioms.json)")
    ap.add_argument("--out", type=Path, required=True, help="libhunmin/data")
    ap.add_argument("--name", default="idioms")
    args = ap.parse_args()

    warnings: list[str] = []
    def warn(m: str) -> None:
        warnings.append(m)

    src = args.orig / "idioms.json"
    if not src.is_file():
        print(f"idioms2data: missing {src}", file=sys.stderr)
        return 2

    raw = json.loads(src.read_text(encoding="utf-8"))
    import hashlib
    pin = hashlib.sha256(src.read_bytes()).hexdigest()
    rows = raw["idioms"] if isinstance(raw, dict) else raw

    entries = []
    unparsed = 0
    for row in rows:
        q, a = row.get("Q", ""), row.get("A", "")
        m = Q.match(q)
        if not m:
            unparsed += 1
            warn(f"unparsed Q: {q!r}")
            continue
        entries.append({
            "hanja": m.group(1).strip(),
            "reading": m.group(2).strip(),
            "meaning": a.strip(),
        })
    if unparsed:
        warn(f"skipped {unparsed} unparsable entr(ies)")
    if not entries:
        print("idioms2data: nothing extracted", file=sys.stderr)
        return 2

    # Sorted by the Hangul reading, not by the Hanja: the engine looks a
    # candidate up by its Hangul and binary-searches, so the sort key must be
    # the field it searches. Sorting by Hanja (as this did at first) leaves
    # the reading order unsorted and every idiom lookup silently misses.
    # UTF-8 byte order, matching what C strcmp compares.
    entries.sort(key=lambda e: (e["reading"].encode(), e["hanja"].encode()))

    by_reading: dict[str, list[str]] = {}
    for e in entries:
        by_reading.setdefault(e["reading"], []).append(e["hanja"])

    table = {
        "version": 1,
        "schema": SCHEMA,
        "entries": entries,
        "meta": {
            "sources": {
                "idioms": "LiF-Lee/idioms idioms.json",
                "licence": "MIT",
                "sha256": pin,
            },
            "entries": len(entries),
            "distinct_hanja": len({e["hanja"] for e in entries}),
            "distinct_readings": len(by_reading),
            "readings_with_several_hanja": sum(1 for v in by_reading.values() if len(v) > 1),
            # Counts characters, so the one Hangul-spelled head still lands
            # under 4 -- entries_without_hanja_in_head is what flags it.
            "head_length_distribution": {
                str(n): sum(1 for e in entries if len(e["hanja"]) == n)
                for n in sorted({len(e["hanja"]) for e in entries})
            },
            "entries_with_meaning": sum(1 for e in entries if e["meaning"]),
            "indexed_by": ["hanja", "reading"],
            "entries_without_hanja_in_head": sum(
                1 for e in entries if not HAN.search(e["hanja"])),
        },
    }

    for w in warnings:
        print(f"idioms2data: {w}", file=sys.stderr)

    out_dir = args.out / "idioms"
    out_dir.mkdir(parents=True, exist_ok=True)
    dst = out_dir / f"{args.name}.json"
    dst.write_text(json.dumps(table, ensure_ascii=False,
                              separators=(",", ":")) + "\n", encoding="utf-8")

    m_ = table["meta"]
    print(f"idioms2data: {m_['entries']} entries "
          f"({m_['distinct_readings']} readings) -> {dst}")
    return 0


if __name__ == "__main__":
    sys.exit(main())