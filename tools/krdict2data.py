#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Extract 한자어 (Sino-Korean) entries from the NIKE 한국어기초사전 dump.

Source: data-orig/krdict/*.xml (국립국어원, CC-BY-SA 2.0 KR — see
libhunmin/data/krdict/LICENSE.md). Not vendored: 368 MB, so it is fetched by
tools/fetch-krdict.sh and the extracted table is what we commit. Everything
this file emits is therefore CC-BY-SA 2.0 KR, unlike the rest of libhunmin/data,
which is BSD-3 or LGPL. The two live in separate directories on purpose.

What we take: entries whose `origin` contains Hanja, because those are the
ones a hanja conversion mode can use. Of 56,555 entries, 34,142 have a Hanja
origin, averaging about 2.4 characters — mostly 2, with a real tail out to 9
(金始初聞/金時初聞 = 금시초문, 8 characters). That tail is the reason this
exists: tools/hanja2data.py covers one syllable at a time, and this covers
words and fixed expressions.

`origin` is the dictionary's own field, kept verbatim: it carries Korean
inflections (加減하다 = 가감하다), a ▽ marker, and romanised parts where the
language uses them (gas料金 = 가스 요금, bus 노선). Those are the source's
notation, not corruption, so this converter does not normalise them.

`origin` may hold several readings separated by `/` (無斷家出/無端家出); all
are kept. Definitions come from the first `Sense`.

Deterministic: same inputs -> byte-identical output (tools/selfcheck.py).
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

from _hostcompat import write_lf

HANJA = re.compile(
    "[\u4e00-\u9fff"        # CJK Unified Ideographs
    "\u3400-\u4dbf"        # Extension A
    "\uf900-\ufaff]"       # Compatibility Ideographs
)
SCHEMA = "hunmin.krdict/1"

# `origin` is the only place Hanja appears, and only for some entries. Entries
# without it are Korean-origin words and are not what a hanja mode shows.
ENTRY = re.compile(r"<LexicalEntry\b.*?</LexicalEntry>", re.S)
ATTR = {
    "writtenForm": re.compile(r'att="writtenForm" val="([^"]*)"'),
    "origin": re.compile(r'att="origin" val="([^"]*)"'),
    "definition": re.compile(r'att="definition" val="([^"]*)"'),
    "pos": re.compile(r'att="partOfSpeech" val="([^"]*)"'),
    "level": re.compile(r'att="vocabularyLevel" val="([^"]*)"'),
    "category": re.compile(r'att="semanticCategory" val="([^"]*)"'),
}


def unescape(s: str) -> str:
    return (s.replace("&lt;", "<").replace("&gt;", ">")
             .replace("&quot;", '"').replace("&apos;", "'")
             .replace("&amp;", "&"))


def extract(path: Path, warn) -> list[dict]:
    text = path.read_text(encoding="utf-8", errors="replace")
    out = []
    for m in ENTRY.finditer(text):
        body = m.group(0)
        origin = ATTR["origin"].search(body)
        form = ATTR["writtenForm"].search(body)
        if not origin or not form:
            continue
        raw = unescape(origin.group(1))
        if not HANJA.search(raw):
            continue
        readings = [unescape(r) for r in raw.split("/") if HANJA.search(r)]
        defn = ATTR["definition"].search(body)
        entry = {
            "ko": unescape(form.group(1)),
            "hanja": readings[0],
            "meanings": [unescape(defn.group(1))] if defn else [],
        }
        if len(readings) > 1:
            entry["hanja_alt"] = readings[1:]
        for key, pat in (("pos", ATTR["pos"]), ("level", ATTR["level"]),
                         ("category", ATTR["category"])):
            v = pat.search(body)
            if v:
                entry[key] = unescape(v.group(1))
        out.append(entry)
    if not out:
        warn(f"{path.name}: no Hanja-origin entries found")
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--orig", type=Path, required=True, help="data-orig/krdict")
    ap.add_argument("--out", type=Path, required=True, help="libhunmin/data")
    ap.add_argument("--name", default="krdict")
    args = ap.parse_args()

    warnings: list[str] = []
    def warn(m: str) -> None:
        warnings.append(m)

    files = sorted(args.orig.glob("*.xml"))
    if not files:
        print(f"krdict2data: no *.xml under {args.orig}", file=sys.stderr)
        return 2

    entries: list[dict] = []
    for f in files:
        entries.extend(extract(f, warn))

    # Sort by the Hangul form so the file diffs cleanly and lookup order is
    # independent of how the source happened to be sharded.
    entries.sort(key=lambda e: (e["ko"], e["hanja"]))

    # The same word appears once per sense in some cases; keep them all, but
    # record how many entries collapsed so it is visible if it ever moves.
    seen: dict[tuple[str, str], int] = {}
    for e in entries:
        seen[(e["ko"], e["hanja"])] = seen.get((e["ko"], e["hanja"]), 0) + 1

    hanja_chars = [len(HANJA.findall(e["hanja"])) for e in entries]
    from collections import Counter
    dist = Counter(hanja_chars)

    table = {
        "version": 1,
        "schema": SCHEMA,
        "entries": entries,
        "meta": {
            "source": "국립국어원 한국어기초사전 (NIKE krdict dump)",
            "licence": "CC-BY-SA 2.0 KR",
            "entries": len(entries),
            "distinct_word_hanja": len(seen),
            "entries_with_alternate_hanja": sum(1 for e in entries if "hanja_alt" in e),
            "entries_with_definition": sum(1 for e in entries if e["meanings"]),
            "hanja_length_distribution": {str(k): dist[k] for k in sorted(dist)},
            "max_hanja_length": max(hanja_chars) if hanja_chars else 0,
        },
    }

    for w in warnings:
        print(f"krdict2data: {w}", file=sys.stderr)

    out_dir = args.out / "krdict"
    out_dir.mkdir(parents=True, exist_ok=True)
    dst = out_dir / f"{args.name}.json"
    write_lf(dst, json.dumps(table, ensure_ascii=False,
                             separators=(",", ":")) + "\n")

    m = table["meta"]
    print(f"krdict2data: {m['entries']} entries from {len(files)} file(s), "
          f"max {m['max_hanja_length']} hanja chars -> {dst}")
    return 0


if __name__ == "__main__":
    sys.exit(main())