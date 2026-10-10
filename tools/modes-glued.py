#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Count hangul runs glued across a symbol, in ko.wikipedia or an aihub corpus.

Docs: docs/SPEC-MODES.md R4 (are symbols a valid composition separator?),
docs/MODES-DATA.md §2 (aihub is a measurement instrument, never shippable).

WHY THIS EXISTS AS A COMPARISON. The R4 question is whether hangul follows a
symbol with no space -- a question about ordinary writing. ko.wikipedia is
encyclopaedic prose and may under-represent informal punctuation, so a single
corpus cannot answer it. If a web corpus disagreed, the wikipedia result would
be in doubt. Two corpora that differ in genre agreeing is what makes the
answer trustworthy. Measured: 0.577% vs 0.522% -- they agree.

THE MIDDLE DOT IS NOT A SEPARATOR. `초·중` (초등·중등), `노무현·정몽준` join two
hanja inside ONE eojeol. Counting them as "a symbol glued to hangul" conflates
the exception with the rule, so they are tallied separately and excluded from
the punctuation figure (SPEC-MODES E8).

Sizes differ by an order of magnitude, so the comparison is a RATE against
hangul runs, never a raw count.

SCHEMA (verified, not assumed -- POSTMOSTEM 10). aihub records are:

    {"SJML": {"header": {...}, "text": [ 99 SEGMENT DICTS ]}}

and each segment is {"title", "subtitle", "content"} -- NOT a string.
Iterating `text` as if it held strings yields 0 characters and exit 0. The
title is counted deliberately: headlines are where unspaced punctuation
concentrates ("정부, 무선국 검사제도 손본다").

USAGE
  tools/modes-glued.py --wiki 4000
  tools/modes-glued.py --aihub ~/work/aihub-records-corpus/.../TS1.zip 2000
  tools/modes-glued.py --both ~/work/aihub-records-corpus/.../TS1.zip 2000
"""
import argparse
import collections
import json
import re
import sys
import zipfile

HANGUL = "[가-힣]"
RUNS = re.compile(HANGUL + "+")
GLUED = re.compile(HANGUL + "+[,.·;:?!]" + HANGUL + "+")
DOT = re.compile(HANGUL + "+·" + HANGUL + "+")
REAL = re.compile(HANGUL + "+[,.;:?!]" + HANGUL + "+")

DEFAULT_AIHUB = ("/home/scoby/work/aihub-records-corpus/"
                 "030.웹데이터_기반_한국어_말뭉치_데이터/01.데이터/1.Training/"
                 "원천데이터/TS1.zip")
DEFAULT_WIKI = "/home/scoby/work/kowiki-dump/kowiki.xml"


def wiki_texts(path, limit):
    """Yield rendered page bodies. Whole bodies, never the first line only --
    that single-line reader is what made every earlier corpus number wrong
    (docs/MODES-DATA.md §4-1)."""
    import mwparserfromhell as mw  # type: ignore[import]
    in_text = in_ns0 = False
    cur = []
    kept = 0
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if "<page>" in line:
                in_ns0 = in_text = False
                cur = []
                continue
            if "<ns>" in line and "<text" not in line:
                in_ns0 = "<ns>0</ns>" in line
                continue
            if "<text" in line and "<ns>" not in line:
                in_text = True
                cur = [line.split(">", 1)[1] if ">" in line else ""]
                continue
            if in_text:
                cur.append(line)
                if "</text>" in line:
                    in_text = False
                    body = "".join(cur).rsplit("</text>", 1)[0]
                    cur = []
                    if in_ns0 and body.strip():
                        kept += 1
                        c = mw.parse(body)
                        for t in c.filter_templates(recursive=True):
                            try:
                                c.remove(t)
                            except ValueError:
                                pass
                        yield re.sub(r"\s+", " ",
                                     c.strip_code(normalize=True, collapse=True))
                        if limit and kept >= limit:
                            return


def aihub_records(path, limit=None):
    """Yield (member, segments). Skips are counted and explained, never silent."""
    kept = skipped = 0
    reasons = collections.Counter()
    try:
        with zipfile.ZipFile(path) as z:
            for n in [x for x in z.namelist() if not x.endswith("/")]:
                if limit and kept >= limit:
                    break
                try:
                    obj = json.loads(z.read(n))
                except (ValueError, zipfile.BadZipFile, OSError) as e:
                    skipped += 1
                    reasons[type(e).__name__] += 1
                    continue
                segs = obj["SJML"]["text"] if isinstance(obj, dict) and \
                    "SJML" in obj else None
                if not isinstance(segs, list):
                    skipped += 1
                    reasons["no SJML" if segs is None else "text not a list"] += 1
                    continue
                kept += 1
                yield n, segs
    finally:
        # finally, not after the loop: a scan that is interrupted must still
        # say what it dropped.
        print(f"  aihub kept {kept:,}  skipped {skipped:,}  {dict(reasons)}",
              file=sys.stderr)


def aihub_texts(path, limit):
    for _, segs in aihub_records(path, limit):
        for seg in segs:
            if isinstance(seg, dict):
                parts = (seg.get("title"), seg.get("subtitle"), seg.get("content"))
            else:
                parts = (seg,)
            for s in parts:
                if isinstance(s, str) and s.strip():
                    yield s


def tally(texts, label):
    """Count runs and glued forms. ALL == DOT + REAL, and the two are disjoint."""
    runs = all_ = dot = real = 0
    for t in texts:
        runs += len(RUNS.findall(t))
        all_ += len(GLUED.findall(t))
        dot += len(DOT.findall(t))
        real += len(REAL.findall(t))
    return {"corpus": label, "hangul_runs": runs, "glued_all": all_,
            "glued_middot": dot, "glued_real_punct": real,
            "pct_all": round(100 * all_ / runs, 4) if runs else 0.0,
            "pct_real": round(100 * real / runs, 4) if runs else 0.0}


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--wiki", type=int, metavar="PAGES", nargs="?")
    ap.add_argument("--wiki-dump", default=DEFAULT_WIKI)
    ap.add_argument("--aihub", type=int, metavar="RECORDS", nargs="?")
    ap.add_argument("--aihub-zip", default=DEFAULT_AIHUB)
    ap.add_argument("--both", type=int, metavar="RECORDS", nargs="?",
                    help="aihub records, compared against 4000 wiki pages")
    a = ap.parse_args()
    if a.both is not None:
        a.aihub = a.both
        a.wiki = a.wiki or 4000
    if a.aihub is None and a.wiki is None:
        ap.error("give --wiki N, --aihub N, or --both N")

    rows = []
    if a.wiki:
        rows.append(tally(wiki_texts(a.wiki_dump, a.wiki),
                          f"ko.wikipedia ({a.wiki:,} pages)"))
    if a.aihub:
        print(f"scanning {a.aihub_zip}", file=sys.stderr)
        rows.append(tally(aihub_texts(a.aihub_zip, a.aihub),
                          f"aihub web ({a.aihub:,} records)"))

    w = max(len(r["corpus"]) for r in rows)
    print(f"{'corpus':<{w}}  {'hangul runs':>12}  {'glued all':>10}  "
          f"{'· only':>8}  {'real punct':>11}  {'all %':>7}  {'punct %':>8}")
    for r in rows:
        print(f"{r['corpus']:<{w}}  {r['hangul_runs']:>12,}  "
              f"{r['glued_all']:>10,}  {r['glued_middot']:>8,}  "
              f"{r['glued_real_punct']:>11,}  {r['pct_all']:>6}%  "
              f"{r['pct_real']:>7}%")
    if len(rows) == 2:
        a_, b_ = rows
        print(f"\n  punct-rate ratio: {b_['pct_real']}/{a_['pct_real']} = "
              f"{b_['pct_real']/a_['pct_real']:.2f}x  "
              f"({'agree' if 0.5 < b_['pct_real']/a_['pct_real'] < 2 else 'DISAGREE'})")
    print("\n· is excluded from the separator set (SPEC-MODES E8: it joins hanja "
          "inside one eojeol, e.g. 초·중 = 초등·중등).")
    print("'real punct' = , . ; : ? ! glued between two hangul runs.")


if __name__ == "__main__":
    main()