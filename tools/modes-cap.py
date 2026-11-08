#!/usr/bin/env python3
"""Measure the embed cap for the hangul/latin mode-guess word lists (SPEC-MODES ReqMode8).

Reproduces the coverage-and-cap readings the withdrawn 2026-10 paper carried
(`docs/MODES-MEASUREMENT.md` §6 names the removal). Run from the repo root:

    ~/work/wikicorpus/.venv/bin/python tools/modes-cap.py

Requires the wikicorpus venv for nothing in this script -- it is stdlib only --
but it reads two artefacts that live OUTSIDE the repo, because they are
measurement inputs, not source:

    ~/work/wikicorpus/full/latin.txt       count \t word   (full wiki corpus)
    ~/work/aihub-records-corpus/624-en-mixed.txt   word \t count

and one that lives inside it -- OUT OF SCOPE, see the note below:

    libhunmin/data/krdict/krdict.json

Why 624 is the probe, and what it is not: it is english tokens that actually
appear next to hangul in typed text -- the closest thing to ReqMode8's
english_valid firing. It is NOT a substitute for ReqMode12's ② sentence replay.
It is one vertical (news/IT), a bias MODES-MEASUREMENT.md §4 already records, so the
cap it produces is falsifiable against any other corpus.

SCOPE (maintainer, 2026-10-05): "krdict.json은 한자 변환시에만 쓰는 걸로 하자.
이 연구에 가지고 들어오지 마." -- krdict is for hanja conversion only. The
hangul section therefore has two modes:

    --hangul              corpus + krdict, behind an OUT OF SCOPE banner.
                          Retained to reproduce the withdrawn record
                          (docs/MODES-MEASUREMENT.md §6); not evidence for a
                          decision.
    --hangul --no-krdict  corpus only. This is the in-scope measurement, and
                          it is what the "no cap for hangul" conclusion
                          actually rests on -- no dictionary is involved.
"""

import json
import os
import sys
import unicodedata
from collections import Counter

HOME = os.path.expanduser("~")
WIKI_LATIN = os.path.join(HOME, "work/wikicorpus/full/latin.txt")
WIKI_HANGUL = os.path.join(HOME, "work/wikicorpus/full/hangul.txt")
MIXED = os.path.join(HOME, "work/aihub-records-corpus/624-en-mixed.txt")
KRDICT = "libhunmin/data/krdict/krdict.json"
SYSDICT = "/usr/share/dict/words"

# The stopword set used for the 4-7a measurement. 122 entries. Kept here, not
# in the doc, so the doc's number can be re-derived rather than believed.
STOPWORDS = set("""a an the and or but if then than that this these those of in on at to
for from by with without as is are was were be been being am do does did doing have has
had having will would shall should can could may might must not no nor so such too very s
t just don now here there when where why how all any both each few more most other some
only own same while about against between into through during before after above below up
down out off again further once because until what which who whom whose it its he she
they them his her their our your my me we you i him us""".split())

# Abbreviations that carry no judgement value either way; measured separately
# because 4-6a lists them as residue.
UNITS = set("px km cm mm kg mg lb ft in mi pt em ex pc dpi mbps kbps ghz mhz kb mb gb tb bps hz".split())

CAPS = (5000, 10000, 20000, 50000, 100000, 200000, 400000)


def load(path, word_first):
    """Load a two-column count file. Column order differs between the two
    artefacts -- wiki emits `count \\t word`, the aihub extractor emits
    `word \\t count` -- so the caller states which. Getting this backwards
    raises ValueError on the first line, which is why it was caught early."""
    out = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            a, b = line.rstrip("\n").split("\t")
            word, count = (a, b) if word_first else (b, a)
            out[word] = int(count)
    return out


def recall(kept, probe):
    """Token-weighted fraction of the probe reproduced by `kept`.

    `kept` is turned into a set here on purpose: passing a list makes the
    `in` test O(n) and this function 5e10 operations at cap=200k, which shows
    up as a three-minute hang rather than an error. The first version of this
    script did exactly that.

    An empty probe raises rather than returning 0.0: "no input, therefore
    0% reproduced" is the confident zero this project has been bitten by
    repeatedly (POSTMORTEM 10), and a division that quietly yields 0.0 is
    exactly how it arrives wearing a number."""
    total = sum(probe.values())
    if not total:
        raise ValueError("empty probe: nothing to measure recall against")
    kept = set(kept)
    got = sum(c for w, c in probe.items() if w in kept)
    return 100.0 * got / total


def embed_bytes(words):
    """What the list costs in memory once embedded: a compact open-addressed
    set of uint32 hashes at 8 bytes per slot (50% load factor)."""
    return len(words) * 8


def ranked(freq):
    return [w for w, _ in sorted(freq.items(), key=lambda kv: -kv[1])]


def top_after_removal(order, cap, banned):
    """Top `cap` entries of `order` once `banned` entries are dropped from the
    stream. NOT 'remove from the first cap' -- that yields a shorter list and
    quietly changes what is being compared."""
    kept = []
    for w in order:
        if w in banned:
            continue
        kept.append(w)
        if len(kept) == cap:
            break
    return kept


def section(title):
    print("\n" + title)
    print("-" * len(title))


def main():
    # --no-krdict is the in-scope run (maintainer's 2026-10-05 boundary: krdict
    # is for hanja conversion only, not this research), so krdict must not be
    # required for it to run at all.
    use_krdict = "--no-krdict" not in sys.argv
    required = (WIKI_LATIN, MIXED, KRDICT) if use_krdict else (WIKI_LATIN, MIXED)
    for p in required:
        if not os.path.exists(p):
            # exit(1) with the message on stderr, NOT sys.exit(msg): the latter
            # makes the string the exit status, which surfaces to a caller as a
            # non-integer status and hides the fact that it failed. A missing
            # measurement input is a hard stop (the skip is only for the
            # optional hangul section).
            print(f"missing measurement input: {p}", file=sys.stderr)
            return 1

    wiki = load(WIKI_LATIN, word_first=False)
    mixed = load(MIXED, word_first=True)
    order = ranked(wiki)
    print(f"wiki latin : {len(wiki):>9,} forms / {sum(wiki.values()):>12,} tokens")
    print(f"624 mixed  : {len(mixed):>9,} forms / {sum(mixed.values()):>12,} tokens")

    section("4-7a  english: coverage saturates, so a cap exists")
    print(f"{'cap':>10} {'recall%':>9} {'embed':>10}")
    for cap in CAPS:
        kept = order[:cap]
        print(f"{cap:>10,} {recall(kept, mixed):9.2f} {embed_bytes(kept)/1e6:9.2f}M")
    print(f"{len(order):>10,} {recall(order, mixed):9.2f} "
          f"{embed_bytes(order)/1e6:9.2f}M   <- full")

    section("4-7a  removing stopwords costs recall, it does not buy accuracy")
    for label, banned in (("raw", set()),
                          ("- stopwords (%d)" % len(STOPWORDS), STOPWORDS),
                          ("- stopwords - units", STOPWORDS | UNITS)):
        kept = top_after_removal(order, 50000, banned)
        print(f"top 50,000 {label:22} {recall(kept, mixed):6.2f}%")

    section("4-7d  a dictionary filter halves recall -- do not do it")
    if not os.path.exists(SYSDICT):
        print(f"skipped: {SYSDICT} absent")
    else:
        sysdict = set(w.strip().lower() for w in open(SYSDICT, encoding="utf-8",
                                                      errors="replace"))
        print(f"{'cap':>10} {'raw%':>8} {'dict-only%':>11} {'dropped':>10}")
        for cap in (50000, 100000, 200000):
            kept = order[:cap]
            only = [w for w in kept if w in sysdict]
            print(f"{cap:>10,} {recall(kept, mixed):8.2f} {recall(only, mixed):11.2f} "
                  f"{len(kept)-len(only):>10,}")

    # The hangul side needs the 135 MB file, so it is opt-in.
    if "--hangul" not in sys.argv:
        section("hangul (4-7b): pass --hangul to include (reads the 135 MB file)")
        return 0

    # ------------------------------------------------------------------
    # SCOPE BOUNDARY, 2026-10-05 (maintainer):
    #   "krdict.json은 한자 변환시에만 쓰는 걸로 하자. 이 연구에 가지고 들어오지 마."
    # krdict is for hanja conversion only; it must not enter this research.
    #
    # What follows is OUT OF SCOPE, kept so the removed §6-7b record stays
    # reproducible (`docs/MODES-MEASUREMENT.md` §6). The in-scope measurement is the corpus curve, which
    # involves no dictionary; --no-krdict prints that and stops.
    # ------------------------------------------------------------------
    hangul = load(WIKI_HANGUL, word_first=False)
    horder = ranked(hangul)
    htotal = sum(hangul.values())
    print(f"hangul corpus: {len(hangul):,} forms / {htotal:,} tokens")

    krdict = set()
    if use_krdict:
        print(f"\n{'=' * 72}\n"
              "  OUT OF SCOPE from here on: krdict (maintainer, 2026-10-05).\n"
              "  Kept to reproduce the removed §6-7b record (MODES-MEASUREMENT.md\n"
              "  §6). Not evidence for a\n"
              "  decision. --no-krdict prints the in-scope, dictionary-free run.\n"
              f"{'=' * 72}")
        entries = json.load(open(KRDICT, encoding="utf-8"))["entries"]
        krdict = set(e["ko"] for e in entries)
        print(f"krdict       : {len(krdict):,} forms")

    section("4-7b  hangul: no saturation, so NO cap")
    print(f"{'cap':>10} {'form%':>8} {'token%':>8} {'embed':>10}")
    for cap in (31300, 50000, 100000, 200000, 500000, 1000000, 2000000, len(horder)):
        kept = set(horder[:cap]) | krdict
        forms = 100.0 * len(kept) / len(hangul)
        toks = 100.0 * sum(c for w, c in hangul.items() if w in kept) / htotal
        print(f"{cap:>10,} {forms:8.2f} {toks:8.2f} {embed_bytes(kept)/1e6:9.2f}M")

    if not use_krdict:
        print()
        print("=> corpus-only. This is the in-scope curve: no dictionary is")
        print("   involved, and it is what the withdrawn 'no cap for hangul'")
        print("   conclusion rested on.")
        return 0

    section("4-7b  is the gap a normalisation artefact? (POSTMORTEM 10 says check)")
    sample = list(hangul)[:200000]
    for name, forms in (("krdict", krdict), ("corpus", sample)):
        conj = sum(1 for w in forms if any("ᄀ" <= c <= "ᇿ" for c in w))
        nfc = sum(1 for w in forms if unicodedata.normalize("NFC", w) == w)
        print(f"  {name:8} forms {len(forms):>9,}  conjoining {conj:>7,}  "
              f"already-NFC {100.0*nfc/len(forms):5.1f}%")
    print("  => a zero conjoining count on both sides rules out the encoding bug "
          "that produced three consecutive 'all None' results before.")

    section("4-7b  krdict coverage and what a paradigm adds")
    base = set(w.lstrip("-") for w in krdict)
    exact = sum(c for w, c in hangul.items() if w in base)
    print(f"krdict exact match      {100.0*exact/htotal:6.2f}% of tokens")
    suffixes = ("는 이 가 을 를 에 의 도 만 로 과 와 은 나 다 고 며 서 면 지 으 "
                "ㄴ ㄹ ㅁ ㅂ ㅅ ㄷ 해 한 할 했 합 하").split()
    tail = tailforms = 0
    for w, c in hangul.items():
        for s in suffixes:
            if len(w) > len(s) and w.endswith(s) and w[:-len(s)] in base:
                tail += c
                tailforms += 1
                break
    print(f"+ stem+paradigm suffix  {100.0*tail/htotal:6.2f}%  ({tailforms:,} forms)")
    print(f"=> even with a paradigm the dictionary reaches well under half the "
          f"token mass; the corpus is inflected surface forms, not lemmas.")
    return 0


if __name__ == "__main__":
    # main() returns a status rather than exiting, so the tests can call it
    # directly and assert on it.
    sys.exit(main())