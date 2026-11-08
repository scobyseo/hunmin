#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Build the AIHub 624 hangul frequency list.

Docs: docs/MODES-DATA.md (corpus survey and licences), docs/SPEC-MODES.md ReqMode8
(the "valid" test this list feeds).

WHY THIS EXISTS, and why its own provenance records a gap. The 624 frequency
list was built in a session whose extraction script was never committed, so
the artefact arrived without a reproducible producer. That is recorded in
`~/work/aihub-records-corpus/624-ko.provenance.json` as a known gap, and this
file is the answer to it. It is written from the SCHEMA and from the shape of
the delivered file, not from the original extraction -- so one thing it
cannot promise is byte-identity with what is already on disk. It is verified
against that file instead (see --compare).

WHAT 624 IS. AIHub dataset 030, 텍스트 웹데이터 말뭉치 구축, dataset number
624. Two facts from MODES-DATA.md 2 that this file exists to preserve:

  * It is the ONLY AIHub corpus in which english is actually TYPED rather
    than being a schema field name. 71788/119's `end`/`start` are metadata;
    624's head is `lte` `iot` `sk` `kt` `tv` `kbs`. That is what makes it the
    closest available proxy for the population this feature acts on.
  * It is one vertical (communications/IT), and that bias is stated rather
    than smoothed over.

LICENCE. AIHub terms are use-and-training only; redistribution is not
permitted. The output belongs OUTSIDE the repo and is a measurement
instrument. A re-sorted or re-filtered table is still the dataset reshaped,
so it may not be vendored into the deb either.

SCHEMA. `624` is `{"SJML": {"header": {...}, "text": [{"title", "subtitle",
"content"}]}}` -- NOT the `{"data": [...]}` shape that AIHub 71788/119 use.
A reader written for one returns zero records for the other with no error,
which is the same silent-zero shape as a `.get()` default. This file asserts
the shape rather than trusting it.

OUTPUT FORMAT is form<TAB>count, frequency-descending -- the OPPOSITE of
`tools/build-modes-corpus.py`, which writes count<TAB>form. Every consumer
takes a flag or a proof for this; `modes-cap.py`'s load() has a word_first
parameter for exactly this reason.

USAGE
  tools/build-modes-624.py --zip ~/work/aihub-records-corpus/030.../TS1.zip \\
      --out ~/work/aihub-records-corpus
  # check a delivered file without rebuilding it:
  tools/build-modes-624.py --compare ~/work/aihub-records-corpus/624-ko.txt
"""
import argparse
import collections
import json
import os
import re
import sys
import unicodedata
import zipfile

LATIN = re.compile(r"[A-Za-z]+(?:['’-][A-Za-z]+)*")
HANGUL = re.compile("[가-힣ᄀ-ᇿ㄰-㆏]+")
PROSE_FIELDS = ("content", "title", "subtitle")


def norm_hangul(w):
    return unicodedata.normalize("NFC", w)


def records(zf, name):
    """Yield (source_file, row) for one archive member.

    Asserts the SJML shape. The alternative -- obj.get("data") -- returns
    nothing at all for this corpus and looks like an empty corpus rather than
    a wrong reader, which is the failure this whole file is guarding against.
    """
    obj = json.loads(zf.read(name).decode("utf-8", errors="replace").lstrip("﻿"))
    if not isinstance(obj, dict) or "SJML" not in obj:
        raise ValueError(
            f"{name}: expected an SJML record with a 'SJML' key, got "
            f"{sorted(obj)[:6] if isinstance(obj, dict) else type(obj).__name__}. "
            f"This corpus is NOT the {{'data': [...]}} shape.")
    for row in (obj["SJML"].get("text") or []):
        if isinstance(row, dict):
            yield name, row


def main():
    ap = argparse.ArgumentParser(
        description="Build (or check) the AIHub 624 hangul frequency list.",
        epilog="See the module docstring for the schema, the licence and why "
               "this corpus is the closest proxy for the feature's population.")
    ap.add_argument("--zip", help="AIHub 030 / dataset 624 archive (TS1.zip)")
    ap.add_argument("--out", help="output dir; 624-ko.txt is written here")
    ap.add_argument("--compare", metavar="FILE",
                    help="do not rebuild: check this delivered 624-ko.txt "
                         "against the checks that matter and exit")
    a = ap.parse_args()

    if a.compare:
        return compare(a.compare)

    if not a.zip or not a.out:
        sys.exit("--zip and --out are required unless --compare is given")

    latin = collections.Counter()
    hangul = collections.Counter()
    files = rows_seen = 0
    longest = 0

    with zipfile.ZipFile(a.zip) as z:
        names = [n for n in z.namelist() if n.endswith(".json")]
        print(f"{len(names):,} json files in archive", file=sys.stderr)
        for i, name in enumerate(names):
            try:
                pairs = list(records(z, name))
            except ValueError as e:
                sys.exit(f"schema mismatch: {e}")
            files += 1
            for _, row in pairs:
                rows_seen += 1
                buf = [row[f] for f in PROSE_FIELDS
                       if isinstance(row.get(f), str) and row[f].strip()]
                if not buf:
                    continue
                text = "\n".join(buf)
                longest = max(longest, len(text))
                for m in LATIN.finditer(text):
                    latin[m.group(0).lower()] += 1
                for m in HANGUL.finditer(text):
                    hangul[norm_hangul(m.group(0))] += 1
            if (i + 1) % 500 == 0:
                print(f"  {i+1:,}/{len(names):,} files  "
                      f"hangul {len(hangul):,}", file=sys.stderr, flush=True)

    if not hangul:
        sys.exit("no hangul found -- the SJML text path or the prose-field "
                 "list does not match this archive. Refusing to write an "
                 "empty corpus.")

    os.makedirs(a.out, exist_ok=True)
    dest = os.path.join(a.out, "624-ko.txt")
    with open(dest, "w", encoding="utf-8") as f:
        for w, c in sorted(hangul.items(), key=lambda kv: (-kv[1], kv[0])):
            f.write(f"{w}\t{c}\n")           # form first: 624's own orientation
    meta = {
        "files": files, "rows": rows_seen, "longest_prose": longest,
        "hangul_forms": len(hangul), "hangul_tokens": sum(hangul.values()),
        "latin_forms": len(latin), "latin_tokens": sum(latin.values()),
        "orientation": "form<TAB>count (opposite of wikicorpus)",
    }
    json.dump(meta, open(os.path.join(a.out, "624-ko.meta.json"), "w"),
              ensure_ascii=False, indent=2)
    print(json.dumps(meta, ensure_ascii=False, indent=2))
    print(f"\ntop 30 hangul:")
    for w, c in hangul.most_common(30):
        print(f"  {c:>10,}  {w}")
    return 0


def compare(path):
    """Check a delivered 624-ko.txt without rebuilding it.

    The delivered file's extraction is not reproducible (that is why this
    script exists), so this does not claim to reproduce it. It asserts the
    properties a consumer actually depends on, each of which has bitten this
    project: the column orientation, a headerless first row, monotone
    frequency, and a head that is vocabulary rather than schema.
    """
    ok = True
    rows = toks = badint = 0
    prev = None
    desc = True
    first = None
    top = []
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            w, _, c = line.rstrip("\n").partition("\t")
            if first is None:
                first = (w, c)
            try:
                v = int(c)
            except ValueError:
                badint += 1
                continue
            rows += 1
            toks += v
            if rows <= 30:
                top.append((v, w))
            if prev is not None and v > prev:
                desc = False
            prev = v

    checks = [
        ("form<TAB>count, not the reverse", first and not first[0].strip().isdigit()
         and first[1].strip().isdigit(), f"first row {first}"),
        ("no header row", first and first[0] != "항목",
         f"first form {first[0] if first else None!r}"),
        ("every row parses as form<TAB>int", badint == 0, f"{badint} unparsable"),
        ("frequency-descending", desc, ""),
        ("corpus is not tiny", rows > 1_000_000, f"{rows:,} rows"),
        ("head is vocabulary, not schema",
         all(not w.isupper() and w not in ("ref", "start", "end")
             for _, w in top[:10]),
         " ".join(w for _, w in top[:10])),
    ]
    print(f"compare {path}")
    for name, passed, detail in checks:
        print(f"  {'PASS' if passed else 'FAIL'}  {name}"
              + (f"  ({detail})" if detail else ""))
        ok &= passed
    print(f"\n  rows {rows:,}  tokens {toks:,}")
    if not ok:
        sys.exit("compare FAILED")
    print("compare ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
