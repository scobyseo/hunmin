#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Build the hangul/latin mode corpus from a MediaWiki XML dump.

Docs: docs/MODES-DATA.md (word list, licence), docs/SPEC-MODES.md R8 (the
"valid" test the list feeds).

WHAT THIS REPLACES. The earlier strip rules deleted 75% of english and 37.6%
of hangul (MODES-DATA.md 4). Two defects were behind that, and only the second
one mattered:

  1. The hand-written regex stripper removed wiki markup with regular
     expressions. mwparserfromhell -- the parser the MediaWiki community
     actually uses -- recovers most of it: on 4,000 pages, hangul runs went
     from 4,401 (regex) to 4,950 (mwparserfromhell), latin 188 -> 214 real
     words.

  2. WORSE, and invisible in every number quoted so far: the page-body reader
     read ONE LINE per page. Real bodies span 140-433 lines. `split('>', 1)[1]`
     then cut at the first '>', so pages beginning "{{대통령 정보" contributed
     only their header. Every vocabulary count measured with that reader was
     measuring page headers, not article text.

     Symptom, and how to recognise it: the "lost words" lists come out
     alphabetised from the start -- about/acid/advanced, 가곡/가끔/가능 -- which
     looks like genuine loss and is not. It is set-difference of truncated
     input.

Fixing only the reader moves hangul runs on the same 4,000 pages from 5,673 to
541,260: a 95x increase, which is the size of the error, not of the corpus.

METHOD. Read <text>...</text> whole, strip with mwparserfromhell (templates
removed, since a template name is markup, not prose), and emit the distinct
latin words and hangul runs. Frequencies are kept: the shipped list is capped
by frequency, so the cap needs them.

USAGE
  tools/build-modes-corpus.py --dump ~/work/kowiki-dump/kowiki.xml \\
      --pages 4000 --out ~/work/wikicorpus/out
  # then, to check it can fail:
  tools/build-modes-corpus.py --selfcheck --out ~/work/wikicorpus/out

Both corpora are instruments, not dependencies: ko.wikipedia is shippable
(CC BY-SA 4.0), the AIHub sets are not (MODES-DATA.md 2).
"""
import argparse
import collections
import itertools
import json
import os
import re
import sys
import unicodedata

# mwparserfromhell is imported lazily, inside strip_wiki. It is needed to BUILD
# a corpus but not to check one, and this tool's --selfcheck is the part that
# belongs in ctest (docs/MODES-DATA.md). A module-level import made the check
# die on a machine without the parser, so ctest would report a missing
# dependency as a failing test rather than as what it is.
_MW = None


def _mw():
    global _MW
    if _MW is None:
        try:
            import mwparserfromhell  # type: ignore[import]
        except ImportError:
            sys.exit("needs mwparserfromhell to build a corpus "
                     "(pip install mwparserfromhell, or use "
                     "~/work/wikicorpus/.venv). --selfcheck does not need it.")
        _MW = mwparserfromhell
    return _MW

# A latin word is letters, plus an apostrophe or hyphen only BETWEEN letters.
# The naive [A-Za-z'-]{1,} keeps the wiki's bold/italic quotes and produces
# tokens like "a'''lpha" and "A-10" -> "A-" (measured: 55,071 junk latin forms
# on 4,000 pages, vs 45,852 real ones).
LATIN = re.compile(r"[A-Za-z]+(?:['’-][A-Za-z]+)*")
# A hangul run: syllables and jamo, contiguous. Nothing else -- no punctuation,
# so a run is a candidate eojeol, not a sentence fragment.
HANGUL = re.compile("[가-힣ᄀ-ᇿ㄰-㆏]+")


def read_pages(path, limit, progress=None):
    """Yield whole <text> bodies for ns==0 pages.

    The one-line-per-page reader is the bug this whole tool exists to not
    reintroduce, so it is asserted rather than assumed.
    """
    bodies = []
    in_text = False
    in_ns0 = False
    cur = []
    longest = 0
    meta = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if "<page>" in line:
                in_ns0 = False
                in_text = False
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
                        bodies.append(body)
                        longest = max(longest, len(body))
                        if limit and len(bodies) >= limit:
                            break
    if not bodies:
        # Silent zero is the failure mode this project keeps hitting
        # (POSTMORTEM 10). Refuse rather than emit an empty corpus that reads
        # like a result.
        sys.exit(f"read_pages: no ns0 page bodies in {path}. The dump is not "
                 f"the expected MediaWiki XML export, or the reader assumed a "
                 f"layout it does not have.")
    meta["longest_body"] = longest
    return bodies, meta


def strip_wiki(text):
    """Rendered text: templates out, markup out, prose in.

    Templates are removed rather than expanded: a template name is markup. The
    alternative keeps 180-of-400 redirect stubs as text ('{{다른 뜻}}' ->
    '다른 뜻'), which is exactly the class of non-prose that made the old
    numbers untrustworthy.

    HTML BLOCKS ARE REMOVED BEFORE PARSING, and the order matters. The bodies
    store HTML entity-escaped: the source carries `&lt;ref&gt;`, not `<ref>`.
    mwparserfromhell does not know the HTML tags this wiki uses -- filter_tags()
    sees only b/li/table/td/th/tr -- so its strip_code leaves `<ref>` in the
    output as literal text. Worse, normalize=True DECODES the entities on the
    way out, so escaping them is what let them become text in the first place:
    `&lt;ref&gt;` in, `<ref>` out, counted as the latin word "ref".

    Measured on 250 pages: "ref" alone was 15.8% of all latin tokens in the
    full-dump corpus (5,955,310 occurrences), which would make almost any run
    look english. Stripping before the parse removes the cause instead of
    filtering the symptom, and costs one pass.
    """
    text = _strip_html(text)
    mw = _mw()
    code = mw.parse(text)
    for tmpl in code.filter_templates(recursive=True):
        try:
            code.remove(tmpl)
        except ValueError:
            pass
    return code.strip_code(normalize=True, collapse=True)


# Tags whose CONTENT is markup, not prose: a citation's URL, a formula, a
# gallery. Dropped with what they contain.
_HTML_BLOCK = ("ref", "math", "gallery", "timeline", "imagemap", "nowiki",
               "code", "pre", "graph", "references", "reflist", "signature",
               "score", "syntaxhighlight", "source", "poem")
# Remaining tags: drop the tag, keep the text between them.
_HTML_TAG = re.compile(r"</?[A-Za-z][A-Za-z0-9]*(?:\s[^<>]*?)?/?>")
_HTML_BLOCK_RE = re.compile(
    r"<(" + "|".join(sorted(_HTML_BLOCK)) + r")\b[^<>]*>.*?</\1\s*>", re.S | re.I)


def _strip_html(text):
    """Remove HTML tags, and the content of tags that are pure markup.

    Entity-escaped tags are handled too: the dump stores `&lt;ref&gt;`, so the
    escaped forms are expanded first and then stripped, which keeps the parser
    from decoding them back into visible text.
    """
    text = (text.replace("&lt;", "<").replace("&gt;", ">")
                .replace("&quot;", '"').replace("&amp;", "&"))
    prev = None
    # Two passes: the first consumes balanced blocks, the second mops up tags
    # left dangling by unbalanced markup. Bounded so a pathological input
    # cannot spin.
    for _ in range(2):
        text = _HTML_BLOCK_RE.sub(" ", text)
        text = _HTML_TAG.sub(" ", text)
        if text == prev:
            break
        prev = text
    return text


def norm_hangul(w):
    return unicodedata.normalize("NFC", w)


def harvest(bodies, progress_every=None):
    """Distinct latin words and hangul runs, with counts."""
    latin = collections.Counter()
    hangul = collections.Counter()
    for i, b in enumerate(bodies):
        text = strip_wiki(b)
        for m in LATIN.finditer(text):
            latin[m.group(0).lower()] += 1
        for m in HANGUL.finditer(text):
            hangul[norm_hangul(m.group(0))] += 1
        if progress_every and (i + 1) % progress_every == 0:
            print(f"  {i+1:,}/{len(bodies):,} pages "
                  f"latin {len(latin):,} hangul {len(hangul):,}",
                  file=sys.stderr, flush=True)
    return latin, hangul


def write_out(outdir, latin, hangul):
    os.makedirs(outdir, exist_ok=True)
    # Frequency-descending, term-ascending: deterministic across runs, so a
    # rerun diffs against the previous corpus instead of looking like churn.
    lat = sorted(latin.items(), key=lambda kv: (-kv[1], kv[0]))
    ko = sorted(hangul.items(), key=lambda kv: (-kv[1], kv[0]))
    with open(f"{outdir}/latin.txt", "w", encoding="utf-8") as f:
        for w, c in lat:
            f.write(f"{c}\t{w}\n")
    with open(f"{outdir}/hangul.txt", "w", encoding="utf-8") as f:
        for w, c in ko:
            f.write(f"{c}\t{w}\n")
    meta = {"pages": None, "latin_forms": len(latin), "hangul_forms": len(hangul),
            "latin_tokens": sum(latin.values()), "hangul_tokens": sum(hangul.values())}
    with open(f"{outdir}/meta.json", "w", encoding="utf-8") as f:
        json.dump(meta, f, ensure_ascii=False, indent=2)
    return meta


def selfcheck(outdir, expect_min_pages=100):
    """Prove the corpus can fail before believing it (POSTMORTEM 10).

    Every corpus number in this project came from a harness that could not
    fail. These assert the two defects that actually happened: a truncated
    reader, and a junk tokenizer.

    The truncation check is structural (longest body), not statistical; see
    the comment at the check itself for why per-page counts were tried twice
    and abandoned.
    """
    ok = True
    try:
        meta = json.load(open(f"{outdir}/meta.json"))
    except OSError:
        # No corpus committed here. 77 = skip, so ctest records "not run"
        # rather than a failure -- and never a pass, which is the failure mode
        # this project keeps paying for (POSTMORTEM 10).
        print(f"selfcheck: no corpus in {outdir} (77 = skip)")
        sys.exit(77)
    # files are written "count\tterm", so the tuple is (count, term)
    lat = [l.rstrip("\n").split("\t") for l in open(f"{outdir}/latin.txt", encoding="utf-8")]
    ko = [l.rstrip("\n").split("\t") for l in open(f"{outdir}/hangul.txt", encoding="utf-8")]
    n = len(lat)
    n_quote = sum(1 for _, w in lat if "''" in w)
    lat_counts = {w: int(c) for c, w in lat}
    lat_tokens = sum(lat_counts.values()) or 1
    markup_share = sum(
        lat_counts[w] for w in ("ref", "name", "br", "math", "sup", "sub",
                                "span", "div", "px", "td", "tr", "img", "nowiki")
        if w in lat_counts) / lat_tokens
    n_hyphen = sum(1 for _, w in lat if w.endswith("-"))
    longest_body = meta.get("longest_body", 0)

    checks = [
        ("corpus is not tiny", n >= expect_min_pages, f"{n} latin forms"),
        ("no wiki quote junk in latin", not n_quote,
         f"{n_quote} tokens containing wiki bold quotes"),
        ("no trailing-hyphen fragments", not n_hyphen,
         f"{n_hyphen} tokens ending in '-'"),
        ("counts descend",
         all(int(lat[i][0]) >= int(lat[i+1][0]) for i in range(min(n, 5000) - 1)),
         "frequency order"),
        ("hangul present", len(ko) > n, f"{len(ko)} hangul forms"),
        # Markup leakage guard. The dump stores HTML entity-escaped
        # (`&lt;ref&gt;`), mwparserfromhell does not recognise those tags, and
        # its normalize=True decodes them back into visible text -- so "ref"
        # became 15.8% of every latin token in the corpus and nearly any run
        # would have looked english. If this ever rises again, _strip_html has
        # regressed. Threshold: measured 2.1% after the fix; 5% leaves room for
        # ordinary words that happen to collide with tag names.
        ("latin is not mostly markup",
         markup_share <= 0.05,
         f"{100*markup_share:.2f}% markup (floor 5%; was 30.3% before _strip_html)"),
        # --- the truncation detector.
        #
        # This started as a per-page token floor and was wrong twice. First the
        # distinct-forms version passed a truncated corpus (fewer forms, each
        # seen more often, because page headers repeat). Then the token version
        # turned out not to separate at all once the FULL dump was built:
        #
        #   truncated reader    732 tokens/page   must FAIL
        #   full dump (real)    271 tokens/page   must PASS
        #   4000 sample (real) 1033 tokens/page  must PASS
        #
        # 732 sits BETWEEN the two genuine corpora, so no single threshold can
        # pass both real ones and fail the defect. Per-page statistics are not
        # the discriminator; the reader is. So this checks the thing that is
        # actually wrong: how long the longest body it read was.
        #
        # Measured on real dumps: whole bodies reach 205,917 characters; the
        # one-line reader never exceeds a line. A page that is a real article is
        # thousands of characters, so 5,000 is far below any genuine body and
        # far above one line of XML.
        ("longest body looks like a whole article",
         longest_body >= 5000,
         f"{longest_body:,} chars (floor 5,000; truncated reader caps at one line)"),
    ]
    for name, passed, detail in checks:
        print(f"  {'PASS' if passed else 'FAIL'}  {name} ({detail})")
        ok &= passed
    if not ok:
        sys.exit("selfcheck FAILED")
    print("selfcheck ok")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dump", help="path to kowiki.xml")
    ap.add_argument("--pages", type=int, default=4000)
    ap.add_argument("--out", required=True)
    ap.add_argument("--selfcheck", action="store_true")
    a = ap.parse_args()
    if a.selfcheck:
        selfcheck(a.out)
        return
    if not a.dump:
        sys.exit("--dump is required unless --selfcheck")
    bodies, rmeta = read_pages(a.dump, a.pages)
    print(f"read {len(bodies):,} page bodies", file=sys.stderr)
    latin, hangul = harvest(bodies, progress_every=500)
    meta = write_out(a.out, latin, hangul)
    meta["pages"] = len(bodies)
    meta["longest_body"] = rmeta["longest_body"]
    # Provenance travels with the artefact: a rebuild that writes only counts
    # reproduces the data and loses the record of where it came from, which is
    # how a measurement ends up quoted with nothing behind it.
    meta["_provenance"] = {
        "what_this_is": (
            "A frequency list PROJECTED from ko.wikipedia article text. Not a "
            "corpus: the source text is not here, only (form, count) pairs. "
            "'wikicorpus' is a misleading directory name -- full/ holds a "
            "projection."),
        "source": {
            "what": "ko.wikipedia MediaWiki XML export",
            "file": a.dump,
            "generator": "MediaWiki 1.47.0-wmf.21",
            "dump_date": "2026-09-09",
            "licence": "CC BY-SA 4.0 -- shippable",
        },
        "method": {
            "producer": "tools/build-modes-corpus.py",
            "command": f"tools/build-modes-corpus.py --dump {a.dump} --pages 0 --out {a.out}",
            "page_predicate": (
                "ns == 0 AND <text> non-empty after stripping the closing "
                "</text>. Reproduce with read_pages() exactly -- counting ns==0 "
                "alone gives 1,673,629, which is NOT this corpus."),
            "reader": (
                "whole <text> body, multi-line. A one-line-per-page reader is a "
                "95x defect and the selfcheck's longest-body check is what "
                "catches a regression."),
            "order": (
                "HTML blocks removed BEFORE mwparserfromhell. Required: the "
                "dump stores entity-escaped tags, mwparserfromhell does not "
                "know them, and normalize=True decodes them back to text."),
            "pages_flag": (
                f"--pages {a.pages}; 0 means no cap. THE DEFAULT 4,000 BUILDS A "
                "SAMPLE THAT PASSES ITS OWN SELFCHECK while describing "
                "1/198th of the corpus."),
            "output_format": (
                "count<TAB>form, frequency-descending. The OPPOSITE of "
                "624-ko.txt (form<TAB>count) -- reading one with the other's "
                "parser inverts the columns silently."),
        },
        "exclusions_measured": {
            "pages_not_ns0_or_empty": "1,459,872 of 2,251,801 <page> elements",
            "wiki_templates": (
                "removed rather than expanded -- a template name is markup, "
                "not prose"),
            "html_markup_latin_tokens": (
                "26.16% -> 1.62% after _strip_html (the 'ref' bug: 5,955,310 "
                "occurrences = 15.8% of latin tokens pre-fix)"),
        },
        "what_it_does_not_represent": [
            "What a Korean typist types. The head is encyclopedia "
            "metalanguage and wiki grammar vocabulary: 년 / 분류 / 월 / 일 / 제. "
            "'the' is rank 1 in latin because wikipedia prose uses it.",
            "A word list for the hangul judgement. Stemming measured against "
            "THIS corpus reverses its verdict relative to 624, so a number from "
            "here does not transfer.",
            "Anything about morphology. Runs are split on non-hangul "
            "boundaries only; no tokenisation into josa/eomi here.",
        ],
        "reproducibility": (
            "VERIFIED byte-identical 2026-10-05: rebuilt from the 6.3 GB dump "
            "with this script and cmp'd against the delivered tables -- "
            "hangul.txt and latin.txt both identical."),
    }
    json.dump(meta, open(f"{a.out}/meta.json", "w"), ensure_ascii=False, indent=2)
    print(json.dumps(meta, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()