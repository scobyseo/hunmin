#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Merge krdict.json rows that share the same (ko, hanja) pair (issue #67).

The user-visible defect: a word with several senses whose Hanja string is
identical (안녕 → 安寧 twice; 가가호호 → 家家戶戶 in both its krdict senses)
showed the SAME candidate row repeatedly. The entries were distinct only in
their sense text, not in what selecting them produced.

Rule (the maintainer's C choice, ship-minimal): inside ONE table,
deduplicate by (ko, hanja); the surviving row keeps the first occurrence's
pos/level/category in encounter order and its `meanings` list carries every
distinct sense text joined in encounter order. Cross-word duplicates (the
same hanja behind two different ko words, e.g. -가량/가량) are deliberately
NOT merged here: they are different input words, and merging them would need
a ko-list schema change (issue #67 records them as the 251-case tail).

Deterministic: same input -> byte-identical output; the transform runs over
the committed krdict.json (34,142 rows, measured 2026-10-18: 951 duplicate
(ko,hanja) pairs, 958 rows dropped) and records itself into the table's
meta.json-adjacent provenance, so the committed artefact stays traceable
without a НИК dump — the 368 MB dump lives outside the repo
(tools/fetch-krdict.sh) and is not on this host; add the same rule inside
tools/krdict2data.py is a separate task there, since its full run also
rebases the table onto the dump.

Usage: python3 tools/krdict-dedupe.py [--in PATH] [--out PATH]
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

SCHEMA = "hunmin.krdict/1"


def dedupe(entries: list) -> tuple[list, int]:
    out: list = []
    index: dict[tuple[str, str], int] = {}
    dropped = 0
    for e in entries:
        if not e.get("hanja"):
            out.append(e)  # entries without hanja origin keep their own row
            continue
        key = (e["ko"], e["hanja"])
        i = index.get(key)
        if i is None:
            index[key] = len(out)
            out.append(e)
        else:
            dropped += 1
            for m in e.get("meanings") or []:
                if m not in out[i].get("meanings"):
                    out[i].setdefault("meanings", []).append(m)
    return out, dropped


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--in", dest="src", default=None,
                    help="path to the shipped krdict.json (default: the repo's)")
    ap.add_argument("--out", dest="dst", default=None,
                    help="where to write the deduped table (default: same file)")
    args = ap.parse_args()
    root = Path(__file__).resolve().parent.parent
    src = Path(args.src) if args.src else root / "libhunmin/data/krdict/krdict.json"
    dst = Path(args.dst) if args.dst else src
    doc = json.loads(src.read_text(encoding="utf-8"))
    if doc.get("schema") != SCHEMA:
        print(f"krdict-dedupe: schema {doc.get('schema')!r}, want {SCHEMA!r}", file=sys.stderr)
        return 2
    entries = doc["entries"]
    merged, dropped = dedupe(entries)
    doc["entries"] = merged
    doc["meta"] = {
        **(doc.get("meta") or {}),
        "dedupe": {
            "tool": "tools/krdict-dedupe.py",
            "rule": "merge (ko,hanja) duplicates; meanings joined unique, "
                    "encounter order; cross-word pairs NOT merged (251 tail, "
                    "see tools/krdict-dedupe.py docstring)",
            "source": "the committed krdict.json table",
            "rows_before": len(entries),
            "rows_after": len(merged),
            "rows_dropped": dropped,
        },
    }
    dst.write_text(json.dumps(doc, ensure_ascii=False, separators=(",", ":")),
                   encoding="utf-8")
    print(f"krdict-dedupe: {len(entries)} -> {len(merged)} rows "
          f"({dropped} merged) -> {dst}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
