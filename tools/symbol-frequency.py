#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Count symbol usage frequency in a text corpus (issue #298, KP3).

Symbol frequency is register-dependent: encyclopedia/records prose, chat and
passwords use different symbols at different rates. This tool measures ONE
corpus and records what population it represents; it does not claim to represent
typed input in general. Word-frequency lists (word<TAB>count) are poor inputs
because word forms rarely contain symbols — use raw text or a wiki dump.

A character is a symbol when its Unicode general category is P* (punctuation) or
S* (symbols); this covers the whole ASCII punctuation set including ` ^ ~ |.
Letters, digits, Hangul and whitespace are counted separately for context.

Usage:
  python3 tools/symbol-frequency.py --corpus PATH [--corpus PATH ...] [--top 30]
  python3 tools/symbol-frequency.py --corpus PATH --jsonl text   # a JSONL field
  python3 tools/symbol-frequency.py --wiki-dump PATH --pages 3000  # stripped prose
  python3 tools/symbol-frequency.py ... --out DIR   # writes artefact + meta.json

The corpus path is an argument, never hardcoded (AGENTS: bulk corpora live
outside the repo). --wiki-dump reuses tools/build-modes-corpus.py read_pages +
strip_wiki (needs mwparserfromhell, e.g. the wikicorpus venv); residual table/
link markup can inflate ~ | =, recorded as a caveat. With --out the tool writes
symbol-frequency.json and meta.json beside it, recording source, method, counts
and what the artefact does NOT represent.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import sys
import unicodedata
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def iter_text(path: Path, jsonl_field: str | None):
    if jsonl_field:
        with path.open(encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    obj = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if isinstance(obj, dict) and isinstance(obj.get(jsonl_field), str):
                    yield obj[jsonl_field]
    else:
        with path.open(encoding="utf-8", errors="replace") as f:
            for line in f:
                yield line


def count_texts(texts) -> dict:
    sym: Counter = Counter()
    total = letters = digits = hangul = whitespace = symbols = 0
    for text in texts:
        for ch in text:
            total += 1
            cat = unicodedata.category(ch)
            if cat[0] in ("P", "S"):
                sym[ch] += 1
                symbols += 1
            elif ch.isspace() or cat[0] == "Z":
                whitespace += 1
            elif cat[0] == "N":
                digits += 1
            elif 0xAC00 <= ord(ch) <= 0xD7A3 or 0x1100 <= ord(ch) <= 0x11FF \
                    or 0x3130 <= ord(ch) <= 0x318F or 0xA960 <= ord(ch) <= 0xA97F \
                    or 0xD7B0 <= ord(ch) <= 0xD7FF:
                hangul += 1
            elif cat[0] == "L":
                letters += 1
    per_million = (lambda n: round(n * 1_000_000 / total, 1)) if total else (lambda n: 0.0)
    ranked = [
        {"char": ch, "codepoint": f"U+{ord(ch):04X}", "name": unicodedata.name(ch, "?"),
         "count": n, "per_million": per_million(n)}
        for ch, n in sym.most_common()
    ]
    return {
        "total_chars": total,
        "classes": {"symbols": symbols, "letters": letters, "digits": digits,
                    "hangul": hangul, "whitespace": whitespace},
        "symbol_share_per_million": per_million(symbols),
        "distinct_symbols": len(sym),
        "ranked": ranked,
    }


def wiki_texts(dump: Path, pages: int):
    spec = importlib.util.spec_from_file_location("bmc", ROOT / "tools/build-modes-corpus.py")
    assert spec is not None and spec.loader is not None, "cannot load build-modes-corpus.py"
    bmc = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(bmc)
    bodies, _ = bmc.read_pages(str(dump), pages)
    return (bmc.strip_wiki(b) for b in bodies), len(bodies)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--corpus", type=Path, action="append", default=[])
    ap.add_argument("--jsonl", metavar="FIELD", default=None)
    ap.add_argument("--wiki-dump", type=Path, default=None)
    ap.add_argument("--pages", type=int, default=3000)
    ap.add_argument("--top", type=int, default=30)
    ap.add_argument("--out", type=Path, default=None)
    args = ap.parse_args()
    if not args.corpus and not args.wiki_dump:
        raise SystemExit("need --corpus PATH or --wiki-dump PATH")

    caveats = [
        "typed input in general — symbol frequency is register-dependent; this is "
        "the register of the named corpus (e.g. encyclopedia prose), not chat or "
        "passwords",
        "password symbol demand (digit+symbol) — prose understates it; KP1's "
        "number-row pairing is justified by that use case, not by prose frequency",
        "a random sample unless the corpus itself is one",
    ]
    if args.wiki_dump:
        dump = args.wiki_dump.resolve()
        texts, npages = wiki_texts(dump, args.pages)
        result = count_texts(texts)
        sources = [{"path": str(dump), "sha256": sha256(dump), "pages_read": npages}]
        method = ("count Unicode P*/S* codepoints per character; normalize per million; "
                  "wiki bodies read by tools/build-modes-corpus.py read_pages and stripped "
                  "by strip_wiki (templates/markup removed)")
        caveats.append("residual table/link/signature markup can inflate ~ | = ; the clean "
                       "prose punctuation is . , ( ) - : \" ' and the CJK brackets · 《》 〈〉")
        caveats.append(f"first {npages} ns0 pages in dump order, not a random sample")
        input_form = f"wiki dump, {npages} pages stripped"
    else:
        paths = [p.resolve() for p in args.corpus]
        for p in paths:
            if not p.is_file():
                raise SystemExit(f"missing corpus: {p}")
        result = count_texts(t for p in paths for t in iter_text(p, args.jsonl))
        sources = [{"path": str(p), "sha256": sha256(p)} for p in paths]
        method = ("count Unicode P*/S* codepoints per character; normalize per million; "
                  "letters/digits/Hangul/whitespace counted separately for context")
        input_form = f"JSONL field {args.jsonl!r}" if args.jsonl else "raw text lines"

    meta = {
        "tool": "tools/symbol-frequency.py",
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "issue": "https://github.com/scobyseo/hunmin/issues/298",
        "sources": sources,
        "method": method,
        "input_form": input_form,
        "does_not_represent": caveats,
    }
    if args.out:
        out = args.out.resolve()
        out.mkdir(parents=True, exist_ok=True)
        (out / "symbol-frequency.json").write_text(
            json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        (out / "meta.json").write_text(
            json.dumps(meta, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"wrote {out / 'symbol-frequency.json'} and {out / 'meta.json'}")
    print(f"total chars: {result['total_chars']:,}  symbols: {result['classes']['symbols']:,} "
          f"({result['symbol_share_per_million']} per million)  distinct: {result['distinct_symbols']}")
    print(f"{'char':<6}{'codepoint':<10}{'count':>12}{'per_million':>14}  name")
    for r in result["ranked"][:args.top]:
        print(f"{r['char']!r:<6}{r['codepoint']:<10}{r['count']:>12,}{r['per_million']:>14}  {r['name']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
