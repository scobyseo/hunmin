#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Price the candidate English dictionaries for SPEC-MODES ReqMode8's `english_valid`.

Docs: docs/SPEC-MODES.md §ReqMode8 (the ruling this tool priced and can
re-derive: "a lookup in the ENGLISH DICTIONARY"), docs/MODES-MEASUREMENT.md §4
(the corpus terms a new comparison must respect).

WHY A SEPARATE TOOL. modes-cap.py answers "how much does a dictionary filter
cost when the dictionary is /usr/share/dict/words"; the ruling that came back
asks which dictionary (debian wamerican is SCOWL size 60, one member of a
family), what it costs to ship, and what its licence allows. That is a
different question over the same corpora, so it is a second tool rather than a
flag on the first: modes-cap.py's numbers are cited in a merged document and
must stay reproducible as written.

TWO SUBCOMMANDS.

  fetch    Sample ko.wikipedia through the MediaWiki API with prop=extracts
           &explaintext, which is WHY it is not a dump reader: the 4-1 defect
           in MODES-MEASUREMENT.md was a hand-written markup stripper, and plain-text
           extracts have no markup to strip. Emits:
             latin-freq.txt   count<TAB>form, frequency descending
             ids.txt          the page ids, one per line (replay inputs)
             meta.json        provenance: source, timestamp, licence, the
                              sample size, and what the sample does NOT
                              represent.
  measure  For each --dict NAME=PATH, report entries, on-disk bytes, embed
           bytes at the same 8-byte/slot model modes-cap.py uses, and the
           token-weighted coverage of a frequency file. Any --dict may also be
           given --aff PATH to expand hunspell affixes; a stem list measured
           without its .aff understates coverage (dog/dogs), and the .aff is
           part of the licence-bearing artefact, not a convenience.

MEASUREMENT DISCIPLINE, inherited from the project's own postmortem list
(docs/MODES-POSTMORTEM.md 10):

  * an empty frequency file RAISES instead of reporting 0% -- "no input,
    therefore nothing reproduced" is the confident zero;
  * coverage is token-weighted, so a list that covers the frequent forms but
    not the rare ones is not mistaken for a good one;
  * both sides are lowercased by default, and the run says so in its output,
    because the dictionaries themselves are lowercased and a case-sensitive
    comparison would measure the dictionary's capitalisation convention rather
    than the English vocabulary;
  * the corpus list baseline (`--cap`) is measured on the SAME frequency file
    the baseline trivially covers itself at 100% and the comparison is a lie.

HARDCODED NOTHING. Every corpus and dictionary is an argument: bulk corpora
live outside this repository for licence and size reasons (AGENTS.md), and an
AIHub or Wikipedia extract must not be committed.

USAGE
  # two disjoint page sets: A builds the corpus-list baseline, B is the probe
  tools/modes-dict-probe.py fetch --pages 3000 --start 가 --out $HOME/work/edw/A
  tools/modes-dict-probe.py fetch --pages 3000 --start 히 --out $HOME/work/edw/B

  # price the candidates against B, with A's vocabulary as the corpus-list row
  tools/modes-dict-probe.py measure --freq $HOME/work/edw/B/latin-freq.txt \\
      --dict scowl60=$HOME/work/edw/words-sysdict.txt \\
      --dict en_us=en_US.dic --aff en_US.aff \\
      --dict words_alpha=words_alpha.txt \\
      --dict google_10k=google-10k.txt \\
      --corpus-list $HOME/work/edw/A/latin-freq.txt --cap 50000
"""

import argparse
import json
import os
import re
import sys
import time
import urllib.parse
import urllib.request
from collections import Counter

API = "https://ko.wikipedia.org/w/api.php"
USER_AGENT = "hunmin-modes-dict-probe/1.0 (https://github.com/scobyseo/hunmin)"

# Latin-script token: an ASCII letter run with internal apostrophes. Digits are
# excluded because a number is not an english word in any dictionary, and
# including them would give every candidate the same free coverage.
TOKEN = re.compile(r"[A-Za-z][A-Za-z']*[A-Za-z]")


def die(msg):
    print(msg, file=sys.stderr)
    return 1


# ---------------------------------------------------------------- fetch ----


def _corpus_builder():
    """The corpus builder's own stripper and token regex, imported rather than
    re-typed.

    `tools/build-modes-corpus.py` spent a session fixing exactly two defects in
    this step (a regex markup stripper, and a one-line-per-page reader --
    MODES-MEASUREMENT.md §5 rule 2), so a second copy of the rules here would be a second home
    for them, free to drift back to the broken shape. Imported lazily for the
    same reason that file gives: a machine without mwparserfromhell must be
    able to run `measure`, which needs no parser.
    """
    import importlib.util
    here = os.path.dirname(os.path.abspath(__file__))
    spec = importlib.util.spec_from_file_location(
        "build_modes_corpus", os.path.join(here, "build-modes-corpus.py"))
    if spec is None:
        raise ImportError("tools/build-modes-corpus.py is not next to this file")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)  # type: ignore[union-attr]
    return mod


def api_get(params, attempts=5):
    """One MediaWiki query, retried on the transient failures.

    A 3000-page walk is ~60 requests and the network in this session dropped one
    with WinError 10060, which killed the run at the last batch and wrote NO
    artefact (the empty-output guard held). Retrying is the difference between a
    flaky network and an unrepeatable measurement.
    """
    url = API + "?" + urllib.parse.urlencode(params)
    for attempt in range(1, attempts + 1):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(req, timeout=60) as fh:
                return json.loads(fh.read().decode("utf-8"))
        except Exception as exc:
            if attempt == attempts:
                raise
            wait = 2 ** attempt
            print(f"  api error ({exc}); retry {attempt} in {wait}s", file=sys.stderr)
            time.sleep(wait)
    raise AssertionError("unreachable")


def walk_titles(pages, start):
    """Namespace-0 titles in allpages order from `start`.

    ORDER IS THE REPRODUCIBILITY. `generator=random` cannot be seeded, so a
    random sample is one artefact nobody can replay; an allpages walk from a
    fixed title is a rule anyone can repeat, and `start` plus `pages` is the
    whole of the input. It also walks rather than samples, so the two page
    sets this tool is meant to be given can be made disjoint by starting the
    second at the first one's last title.
    """
    titles, cont = [], None
    while len(titles) < pages:
        params = {"action": "query", "format": "json", "list": "allpages",
                  "apnamespace": "0", "aplimit": "500", "apfrom": start}
        if cont:
            params["apcontinue"] = cont
        data = api_get(params)
        batch = data.get("query", {}).get("allpages", [])
        titles.extend(p["title"] for p in batch)
        cont = data.get("continue", {}).get("apcontinue")
        if not cont or not batch:
            break
        time.sleep(0.3)
    return titles[:pages]


def fetch(pages, outdir, start="가", batch=50, pause=0.3):
    """Read `pages` namespace-0 articles from `start`, plain text, and count
    the latin-script tokens in their prose."""
    # expanduser HERE, not in the shell: a `~/...` argument reaching this tool
    # through a Windows python creates a literal C:/c/Users/... tree, and the
    # run then looks like it never started.
    outdir = os.path.expanduser(outdir)
    os.makedirs(outdir, exist_ok=True)
    corpus = _corpus_builder()
    titles = walk_titles(pages, start)
    if not titles:
        return die(f"no namespace-0 pages from {start!r}")

    counts = Counter()
    ids, chars = [], 0
    for i in range(0, len(titles), batch):
        chunk = titles[i:i + batch]
        data = api_get({
            "action": "query", "format": "json", "prop": "revisions",
            "rvprop": "ids|content", "rvslots": "main", "titles": "|".join(chunk),
        })
        for pid, page in data.get("query", {}).get("pages", {}).items():
            revs = page.get("revisions") or []
            if not revs:
                continue
            wikitext = revs[0].get("slots", {}).get("main", {}).get("*", "")
            text = corpus.strip_wiki(wikitext)
            if not text.strip():
                continue
            ids.append(pid)
            chars += len(text)
            counts.update(t.lower() for t in corpus.LATIN.findall(text))
        print(f"  {len(ids)}/{len(titles)} pages, {sum(counts.values())} tokens",
              file=sys.stderr)
        time.sleep(pause)

    if not counts:
        return die(f"fetch produced no tokens in {outdir}; refusing to write an empty artefact")

    freq = os.path.join(outdir, "latin-freq.txt")
    with open(freq, "w", encoding="utf-8", newline="\n") as fh:
        for word, n in counts.most_common():
            fh.write(f"{n}\t{word}\n")
    with open(os.path.join(outdir, "ids.txt"), "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(ids) + "\n")

    meta = {
        "source": API + " list=allpages + prop=revisions (rvslots=main), which is "
                  "the same reader tools/build-modes-corpus.py uses on a dump",
        "walk_start_title": start,
        "requested_pages": pages,
        "retrieved_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "pages": len(ids),
        "distinct_tokens": len(counts),
        "tokens": sum(counts.values()),
        "characters_of_text": chars,
        "token_rule": "tools/build-modes-corpus.py LATIN, lowercased",
        "stripper": "tools/build-modes-corpus.py strip_wiki (mwparserfromhell)",
        "licence": "text is CC BY-SA 4.0 (ko.wikipedia); this artefact is a "
                   "measurement input and is not redistributed with the engine",
        "represents": "latin-script tokens in ko.wikipedia article prose, as "
                      "the closest public stand-in for the corpus MODES-MEASUREMENT.md §4 "
                      "harvested (it was built from a kowiki dump too)",
        "does_not_represent": [
            "typed text: wikipedia prose is written and edited, not typed in a "
            "hurry, so it over-weights encyclopedic vocabulary",
            "the AIHub 624 evaluation set (one news/IT vertical, unredistributable)",
            "sentence context: ReqMode12's metric is a sentence replay, not token recall",
            "a frequency-ordered corpus of its own: this sample is ordered by "
            "article, so its counts are a sample of a snapshot, not the kowiki "
            "corpus MODES-MEASUREMENT.md §6 was measured on",
        ],
        "replay": f"tools/modes-dict-probe.py fetch --pages {pages} --start {start} --out <dir>",
        "generator": "tools/modes-dict-probe.py fetch",
    }
    with open(os.path.join(outdir, "meta.json"), "w", encoding="utf-8", newline="\n") as fh:
        json.dump(meta, fh, ensure_ascii=False, indent=2)
        fh.write("\n")
    print(f"wrote {freq}: {len(ids)} pages, {len(counts)} forms, "
          f"{sum(counts.values())} tokens")
    return 0


# --------------------------------------------------------------- parsing ----


def load_freq(path, word_first=False):
    freq = {}
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            a, _, b = line.partition("\t")
            if not b:
                raise ValueError(f"{path}: not a two-column count file: {line!r}")
            word, count = (a, b) if word_first else (b, a)
            freq[word.lower()] = freq.get(word.lower(), 0) + int(count)
    if not freq:
        raise ValueError(f"{path}: empty frequency file")
    return freq


def load_list(path):
    out = set()
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            w = line.strip().lower()
            if w:
                out.add(w)
    return out


def load_hunspell(path):
    """`word/flags`, first line a count. Escaped slashes are `\\/`."""
    out = {}
    with open(path, encoding="utf-8", errors="replace") as fh:
        lines = fh.read().split("\n")
    if lines and lines[0].strip().isdigit():
        lines = lines[1:]
    for line in lines:
        line = line.strip()
        if not line:
            continue
        line = line.replace("\\/", "\x00")
        if "/" in line:
            word, flags = line.split("/", 1)
        else:
            word, flags = line, ""
        out[word.replace("\x00", "/").lower()] = flags.replace("\x00", "/")
    return out


def load_aff(path):
    """Only the SFX/PFX rules, keyed by flag. Enough for en_US; not a speller."""
    rules = {"SFX": {}, "PFX": {}}
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            parts = line.split()
            # A rule line is `KIND flag strip add condition`, five fields. The
            # header is `KIND flag Y|N count` -- four, where the third field is
            # the cross-product switch, which this expands nothing for either
            # way. Matching the header by `parts[3] == "Y"` alone missed every
            # `N` header and raised on the first one.
            if len(parts) < 5 or parts[0] not in ("SFX", "PFX"):
                continue
            kind, flag, strip, add, cond = parts[:5]
            add = "" if add == "0" else add.split("/")[0]
            strip = "" if strip == "0" else strip
            rules[kind].setdefault(flag, []).append((strip, add, cond))
    return rules


def _cond_ok(word, cond, kind):
    """Hunspell conditions are regex-shaped: `.` is any character, `[abc]` and
    `[^abc]` are classes, and the pattern is matched against the end (SFX) or
    the start (PFX) of the word.

    The characters that carry hunspell meaning are passed through UNESCAPED --
    escaping `[`/`]`/`^` the way one would escape arbitrary prose turned every
    class into a literal and silently under-generated (`dog/SM` produced no
    `dogs`), which reads as "the dictionary lacks plurals".
    """
    if cond == ".":
        return True
    pattern = ""
    for ch in cond:
        if ch.isalnum() or ch in ".^[]-":
            pattern += ch
        else:
            pattern += re.escape(ch)
    if kind == "PFX":
        return re.match(pattern, word) is not None
    return re.search(pattern + r"$", word) is not None


def expand(words, rules):
    """One level of affix expansion -- the level en_US needs for plurals and
    -ed/-ing. Continuation classes are not followed, so this UNDER-counts a
    little; the doc says so where the number is used."""
    out = set(words)
    for word, flags in words.items():
        for flag in flags:
            for kind in ("SFX", "PFX"):
                for strip, add, cond in rules[kind].get(flag, ()):
                    if strip:
                        if kind == "SFX" and not word.endswith(strip):
                            continue
                        if kind == "PFX" and not word.startswith(strip):
                            continue
                    if not _cond_ok(word, cond, kind):
                        continue
                    base = word[: -len(strip)] if (strip and kind == "SFX") else (
                        word[len(strip):] if strip else word)
                    out.add((base + add) if kind == "SFX" else (add + base))
    return out


def embed_bytes(n):
    return n * 8


def coverage(kept, probe):
    total = sum(probe.values())
    if not total:
        raise ValueError("empty probe: nothing to measure coverage against")
    kept = set(kept)
    return 100.0 * sum(c for w, c in probe.items() if w in kept) / total


# -------------------------------------------------------------- measuring ----


def measure(args):
    probe = load_freq(args.freq, args.word_first)
    if args.corpus_list and os.path.abspath(args.corpus_list) == os.path.abspath(args.freq):
        return die("--corpus-list and --freq are the same file; a list built from "
                   "the probe covers it 100% by construction and measures nothing")

    named = [t.lower() for t in args.tokens.split()] if args.tokens else []

    print(f"probe      : {args.freq}  {len(probe):,} forms / {sum(probe.values()):,} tokens")
    print(f"case       : {'lowercased both sides' if not args.case_sensitive else 'case sensitive'}")
    print()

    rows = []
    for spec in args.dicts:
        name, _, path = spec.partition("=")
        if not path:
            return die(f"--dict wants NAME=PATH, got {spec!r}")
        size_disk = os.path.getsize(path)
        kind = args.kind
        if kind == "auto":
            kind = "hunspell" if path.endswith(".dic") else "list"
        if kind == "hunspell":
            stems = load_hunspell(path)
            words = expand(stems, load_aff(args.aff)) if args.aff else stems
            note = f"{len(stems):,} stems" + (f", {len(words):,} expanded" if args.aff else ", NOT expanded")
            if args.aff:
                # the .aff is part of what ships and part of the licence file,
                # so the disk column is the pair, not the .dic alone
                size_disk += os.path.getsize(args.aff)
        else:
            words = load_list(path)
            note = f"{len(words):,} forms"
        rows.append({
            "name": name,
            "note": note,
            "entries": len(words),
            "disk": size_disk,
            "embed": embed_bytes(len(words)),
            "cov": coverage(words, probe),
            "named": [t for t in named if t in words],
        })

    base = None
    if args.corpus_list:
        listed = load_freq(args.corpus_list, args.word_first)
        order = [w for w, _ in sorted(listed.items(), key=lambda kv: -kv[1])]
        kept = order[: args.cap] if args.cap else order
        overlap = len(set(kept) & set(probe))
        base = {
            "name": "corpus list",
            "entries": len(kept),
            "cov": coverage(kept, probe),
            "overlap": f"{overlap:,} of the probe's {len(probe):,} forms are in "
                       f"{os.path.basename(args.corpus_list)}'s top "
                       f"{f'{len(kept):,}' if kept else 'all'}",
        }

    print(f"{'candidate':26} {'entries':>10} {'disk':>9} {'embed':>8} {'cover%':>8}")
    print("-" * 64)
    for r in rows:
        print(f"{r['name']:26} {r['entries']:>10,} {r['disk']/1e6:>8.2f}M "
              f"{r['embed']/1e6:>7.2f}M {r['cov']:>8.2f}")
    if base:
        print(f"{base['name']:26} {base['entries']:>10,} {'-':>9} "
              f"{embed_bytes(base['entries'])/1e6:>7.2f}M {base['cov']:>8.2f}"
              f"   <- the pre-정정 definition")
        print(f"\n  {base['overlap']}")
    print()
    for r in rows:
        print(f"{r['name']:28} {r['note']}")
    if named:
        print()
        print(f"named tokens ({len(named)}): the Korean-typed latin strings MODES-MEASUREMENT.md §6 names")
        for r in rows:
            miss = [t for t in named if t not in r["named"]]
            print(f"{r['name']:28} has {len(r['named']):>2}/{len(named)}"
                  + (f"  missing: {' '.join(miss)}" if miss else "  (all present)"))
    return 0


def main():
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)

    f = sub.add_parser("fetch", help="sample ko.wikipedia plain-text extracts")
    f.add_argument("--pages", type=int, default=1000)
    f.add_argument("--start", default="가",
                   help="first allpages title; the walk is deterministic so the "
                        "sample is replayable and two sets can be made disjoint")
    f.add_argument("--out", required=True)

    m = sub.add_parser("measure", help="price dictionaries against a frequency file")
    m.add_argument("--freq", required=True, help="count<TAB>word frequency file (the probe)")
    m.add_argument("--word-first", action="store_true", help="the file is word<TAB>count")
    m.add_argument("--dict", dest="dicts", action="append", default=[],
                   metavar="NAME=PATH")
    m.add_argument("--aff", help="hunspell .aff for the .dic being measured")
    m.add_argument("--kind", choices=("auto", "list", "hunspell"), default="auto")
    m.add_argument("--corpus-list", help="frequency file the corpus-list baseline is built from")
    m.add_argument("--cap", type=int, default=50000)
    m.add_argument("--tokens", default="kbs mbc sbs fc fifa uefa sk kt nhk cj")
    m.add_argument("--case-sensitive", action="store_true")

    args = ap.parse_args()
    if args.cmd == "fetch":
        return fetch(args.pages, args.out, start=args.start)
    try:
        return measure(args)
    except ValueError as exc:
        return die(f"error: {exc}")
    except OSError as exc:
        return die(f"error: {exc}")


if __name__ == "__main__":
    sys.exit(main())
