#!/usr/bin/env python
"""Clean the 119 corpus: strip AIHub's entity tags and labels to SPACE.

Decision (user, 2026-10-04): option B -- strip both `<|...|>` and `#@...#`,
replacing with a space. Not A (tags only) and not C (tags only, leave hangul).

Two measured reasons this is the right shape, both from this file's earlier
runs:

1. 119's latin list is dominated by the tagging markup. `<|start|>` and
   `<|end|>` tokenise to the words `start` and `end`, 386,746 occurrences
   each -- 5.9% of every latin token in the corpus. That is why
   MODES-DATA.md 2 called 119 unfit for the english side, and it was
   right for a reason nobody had looked at. The corpus is not field names;
   it is a corpus with the field names' own markers in the text.

2. Replacing with a SPACE, not deleting. Deleting glues the neighbouring
   eojeol together: measured on 300 files, deletion created 팀장재난안전관리
   (17), 연락처주무관 (12), 청소년전화운영 (7). Those are new non-words that
   did not exist in the source. build-modes-corpus.py already does this
   (`_HTML_BLOCK_RE.sub(" ", ...)`) for the same reason.

B over A: the `#@...#` labels are AIHub's entity annotations, invisible to
a reader, and they are the top hangul form in the corpus (`이름`). Leaving
them means the corpus's own rank-1 word is partly annotation.

Streaming: 2.66 GB uncompressed across 28,628 zip members, so it is read
from the archive and never extracted.
"""
import argparse
import collections
import json
import os
import re
import sys
import unicodedata
import zipfile

# Source archive, fetched not vendored (AIHub terms forbid redistribution).
# Override with --zip. tools/fetch-aihub.sh documents where this comes from.
ZIP = os.environ.get("AIHUB_119_ZIP", os.path.expanduser(
    "~/work/aihub-records-corpus/119.국가기록물_대상_초거대AI_학습을_위한_말뭉치_데이터/"
    "3.개방데이터/1.데이터/Training/01.원천데이터/TS_1._말뭉치_데이터.zip"))

LATIN = re.compile(r"[A-Za-z]+(?:['’-][A-Za-z]+)*")
HANGUL = re.compile("[가-힣ᄀ-ᇿ㄰-㆏]+")
TAIL = re.compile(r"<\|[^|]*\|>")   # <|start|> <|end|>
LABEL = re.compile(r"#@[^#]*#")     # #@이름4# #@기타1#
PROSE_FIELDS = ("corpus", "title", "subtitle", "text", "content",
                "description", "abstract")


def clean(text):
    return LABEL.sub(" ", TAIL.sub(" ", text))


def main():
    doc_first = __doc__.splitlines()[0] if __doc__ else "Clean the 119 corpus"
    ap = argparse.ArgumentParser(description=doc_first)
    ap.add_argument("--zip", default=ZIP, help="AIHub 119 archive")
    ap.add_argument("--out", required=True,
                    help="output dir (outside the repo; the corpora are large)")
    a = ap.parse_args()

    latin = collections.Counter()
    hangul = collections.Counter()
    # The uncleaned counts, so the report can show what the strip removed
    # rather than asserting it worked.
    raw_latin = collections.Counter()
    raw_hangul = collections.Counter()
    records = rows_seen = 0
    stripped_tags = stripped_labels = 0

    with zipfile.ZipFile(a.zip) as z:
        names = [n for n in z.namelist() if n.endswith(".json")]
        for i, name in enumerate(names):
            try:
                obj = json.loads(z.read(name).decode("utf-8", errors="replace")
                                 .lstrip("﻿"))
            except ValueError as e:
                print(f"  skip {name}: {e}", file=sys.stderr)
                continue
            records += 1
            for row in (obj.get("data") if isinstance(obj, dict) else None) or []:
                if not isinstance(row, dict):
                    continue
                rows_seen += 1
                for f in PROSE_FIELDS:
                    v = row.get(f)
                    if not isinstance(v, str) or not v.strip():
                        continue
                    stripped_tags += len(TAIL.findall(v))
                    stripped_labels += len(LABEL.findall(v))
                    c = clean(v)
                    for m in LATIN.finditer(v):
                        raw_latin[m.group(0).lower()] += 1
                    for m in HANGUL.finditer(v):
                        raw_hangul[unicodedata.normalize("NFC", m.group(0))] += 1
                    for m in LATIN.finditer(c):
                        latin[m.group(0).lower()] += 1
                    for m in HANGUL.finditer(c):
                        hangul[unicodedata.normalize("NFC", m.group(0))] += 1
            if (i + 1) % 4000 == 0:
                print(f"  {i+1:,}/{len(names):,} files", file=sys.stderr, flush=True)

    if not hangul:
        sys.exit("no hangul after cleaning -- refusing to write an empty corpus")
    rb, ab = sum(raw_hangul.values()), sum(hangul.values())
    rl, al = sum(raw_latin.values()), sum(latin.values())
    print(f"records {records:,}  prose rows {rows_seen:,}")
    print(f"markup found: <|...|> {stripped_tags:,}   #@...# {stripped_labels:,}")
    print(f"hangul tokens {rb:,} -> {ab:,}  ({100.0*(rb-ab)/rb:.2f}% removed)  "
          f"forms {len(raw_hangul):,} -> {len(hangul):,}")
    print(f"latin  tokens {rl:,} -> {al:,}  ({100.0*(rl-al)/rl:.2f}% removed)  "
          f"forms {len(raw_latin):,} -> {len(latin):,}")
    newh = set(hangul) - set(raw_hangul)
    newl = set(latin) - set(raw_latin)
    print(f"new hangul forms created by the strip: {len(newh):,}")
    print(f"new latin  forms created by the strip: {len(newl):,}")

    os.makedirs(a.out, exist_ok=True)
    # count<TAB>form, matching wikicorpus/full (unlike 624-ko's form-first)
    for nm, ctr in (("latin", latin), ("hangul", hangul)):
        with open(f"{a.out}/{nm}.txt", "w", encoding="utf-8") as f:
            for w, c in sorted(ctr.items(), key=lambda kv: (-kv[1], kv[0])):
                f.write(f"{c}\t{w}\n")
    # Provenance travels WITH the artefact, emitted by the producer rather than
    # written beside it afterwards: a rebuild that writes only counts reproduces
    # the data and loses the record of where it came from, which is how a
    # measurement ends up quoted with nothing behind it.
    meta = {
        "_provenance": {
            "what_this_is": (
                "A frequency list PROJECTED from AIHub dataset 119 (국가기록물 "
                "초거대AI 학습을 위한 말뭉치). Not a corpus: the source text is "
                "not here, only (form, count) pairs. 'corpus' in the directory "
                "name means the same projection sense as wikicorpus/full."),
            "source": {
                "what": "AIHub 119. 국가기록물 대상 초거대AI 학습을 위한 말뭉치 데이터",
                "archive": os.path.basename(a.zip),
                "archive_path": a.zip,
                "licence": ("AIHub -- USE AND TRAINING ONLY, REDISTRIBUTION NOT "
                            "PERMITTED. A measurement instrument; may never be "
                            "vendored into the deb."),
            },
            "method": {
                "producer": "tools/build-modes-119.py",
                "command": f"tools/build-modes-119.py --zip {a.zip} --out {a.out}",
                "record_shape": ('{"data": [{..., "corpus": <prose>, "title": <prose>}]}'
                                 " read as UTF-8 with a BOM strip. NOT the 624 SJML "
                                 "shape -- a reader written for one returns zero for "
                                 "the other with no error."),
                "prose_fields": list(PROSE_FIELDS),
                "metadata_skipped": ("every other key (source_id, "
                                     "publisher_company, category_main, isbn, ...), "
                                     "skipped by construction and not by a filter -- "
                                     "which is what makes the latin side readable."),
                "streaming": "read from the zip; never extracted to disk",
            },
            "cleaning": {
                "decision": ("user, 2026-10-04: strip BOTH the entity tags and the "
                             "entity labels, replacing each with a SPACE."),
                "rules": {
                    "<|...|>": ("AIHub entity boundaries; they tokenise to the "
                                "english words 'start' and 'end'."),
                    "#@...#": "AIHub entity labels, e.g. #@이름4#.",
                    "replacement": ("a single space, NOT deletion. Deletion glues "
                                    "the neighbouring eojeol together -- measured on "
                                    "300 files it created 팀장재난안전관리 (17), "
                                    "연락처주무관 (12), 청소년전화운영 (7): new "
                                    "non-words absent from the source."),
                },
                "why_latin_forms_unchanged": (
                    "the pollution was in TOKENS, not FORMS: end/start plus "
                    "single-letter literals (m l a c mg g) and two-letter units "
                    "(kg mm ml cm db). Every >=3-char form survived."),
            },
            "what_it_does_not_represent": [
                "The population this feature acts on. 119 is official "
                "administrative prose -- 및/제/대한/위한/따른 are document "
                "connectives and honorifics, not typed eojeol.",
                "Typed latin: even cleaned, about 36% of latin tokens are 1-2 "
                "characters (units and letters).",
                "Any morphology: no josa/eomi segmentation in this build.",
            ],
            "reproducibility": (
                "VERIFIED byte-identical 2026-10-04: rebuilt from the archive with "
                "this script and cmp'd against the previously delivered tables -- "
                "hangul.txt and latin.txt both identical."),
        },
        "records": records,
        "prose_rows": rows_seen,
        "stripped_tags": stripped_tags,
        "stripped_labels": stripped_labels,
        "hangul_forms": len(hangul),
        "hangul_tokens": ab,
        "latin_forms": len(latin),
        "latin_tokens": al,
        "hangul_tokens_raw": rb,
        "latin_tokens_raw": rl,
        "new_hangul_forms": len(newh),
        "new_latin_forms": len(newl),
    }
    json.dump(meta, open(f"{a.out}/meta.json", "w"), ensure_ascii=False, indent=2)
    print("\ntop 25 hangul:")
    for w, c in hangul.most_common(25):
        print(f"  {c:>10,}  {w}")
    print("\ntop 25 latin:")
    for w, c in latin.most_common(25):
        print(f"  {c:>10,}  {w}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
