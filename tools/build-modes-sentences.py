#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Build a mixed-script sentence corpus from a MediaWiki XML dump.

Docs: `docs/MODES-MEASUREMENT.md` §1.2 (the ② replay this feeds), §2 (the
pass-line shapes: the corpus's transition density `d` is what a shape's meaning
hangs on) and §3 (why a corpus built in-session is void).

WHY THIS EXISTS. ② and ③-bis replay *sentences*, and `tools/build-modes-corpus.py`
emits word-frequency lists, not sentences. A corpus assembled by hand in a
session is exactly what §3 voids, so the sentences have to come from a source
with retrievable provenance and the extraction has to be re-runnable. That is
what this tool is: one dump in, one JSONL corpus plus its `meta.json` out.

WHAT IT KEEPS. Only sentences that a person could actually have typed in one
sitting and that the ② metric can grade: at least one Hangul word and at least
one Latin word, a bounded length, and no character outside the script/space/
punctuation set below. Everything it drops is counted by reason in `meta.json`;
a filter whose effect is not recorded is how the earlier corpora lost 75% of
their English without anyone noticing (§3).

WHAT IT DOES NOT REPRESENT. Wikipedia is encyclopedia prose, and
`docs/MODES-MEASUREMENT.md` §4 records that this genre understates the thing the metric
is about: people mix scripts far more in typed messages than in an article. The
`d` this corpus produces is therefore a *lower* bound on the target population's
transition density, and a ② number computed on it is a lower bound too. The
window is also the dump's own page order, not a random sample — recorded as such
in `meta.json` rather than called representative.

USAGE
  tools/build-modes-sentences.py --dump ~/work/kowiki-dump/kowiki.xml \\
      --pages 3000 --limit 400 --out ~/work/hunmin-sentences/wiki-3000.jsonl

  # the guard: read back what it wrote and fail if it is not a corpus
  tools/build-modes-sentences.py --selfcheck ~/work/hunmin-sentences/wiki-3000.jsonl

The dump is an *instrument*, not a dependency: it belongs outside the repository
(AGENTS.md), so the path is an argument and nothing here hardcodes one.
"""
import argparse
import collections
import hashlib
import json
import os
import pathlib
import re
import sys
import unicodedata

# tools/build-modes-corpus.py is the page reader and the wiki stripper. Its page
# reader exists because the one-line reader it replaced measured page headers
# (MODES-MEASUREMENT.md §5 rule 2); re-implementing it here would re-open that bug, so it is
# imported rather than copied. The hyphens in the name are why this is not a
# plain `import`.
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import importlib.util  # noqa: E402


def _load_module(name):
    path = pathlib.Path(__file__).resolve().parent / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name.replace("-", "_"), path)
    if spec is None or spec.loader is None:
        raise SystemExit(f"cannot load {path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


# Characters a sentence may contain, besides Hangul syllables: space, ASCII
# letters, digits, and the punctuation the Hangul layouts actually carry a
# literal for. A character outside this set is a reason to drop the sentence,
# not something to transliterate: a sentence the layout cannot produce is
# §1.3's question, and this corpus must not pre-answer it.
ALLOWED_PUNCT = set(".,?!'\"()[]:;-")

SENT_END = re.compile(r"(?<=[.!?])\s+")
HANGUL_SYL = re.compile(r"[\uac00-\ud7a3]")
LATIN_WORD = re.compile(r"[A-Za-z]{2,}")


def norm_space(text):
    return re.sub(r"\s+", " ", text).strip()


def words(sentence):
    return sentence.split(" ")


def classify(word):
    """hangul / latin / other — by what the word is made of, not by intent.

    A word that mixes the two scripts (`LTE급`, `USB-C`) is `other`: the modes
    judgement runs per eojeol, so a token that changes script inside itself is
    not something ② can grade, and dropping it is recorded rather than guessed.
    """
    has_hangul = bool(HANGUL_SYL.search(word))
    has_latin = bool(re.search(r"[A-Za-z]", word))
    if has_hangul and has_latin:
        return "other"
    if has_hangul:
        return "hangul"
    if has_latin:
        return "latin"
    return "other"


def split_sentences(body):
    text = norm_space(body)
    for piece in SENT_END.split(text):
        piece = piece.strip()
        if piece:
            yield piece


def assess(sentence, max_chars, max_words):
    """(keep, reason). Reasons are counted in meta.json, one per drop."""
    if not (12 <= len(sentence) <= max_chars):
        return False, "length"
    if len(words(sentence)) > max_words:
        return False, "too_many_words"
    for ch in sentence:
        if ch in " " or ch.isascii() and ch.isalnum():
            continue
        if 0xAC00 <= ord(ch) <= 0xD7A3:
            continue
        if ch in ALLOWED_PUNCT:
            continue
        return False, "charset"
    scripts = [classify(w) for w in words(sentence)]
    if "other" in scripts:
        return False, "mixed_word"
    if "hangul" not in scripts:
        return False, "no_hangul"
    if "latin" not in scripts:
        return False, "no_latin"
    # A sentence has to be typeable without an empty word or a double space.
    if "" in words(sentence):
        return False, "spacing"
    return True, "keep"


def build(dump, pages, limit, max_chars, max_words):
    corpus = _load_module("build-modes-corpus")
    bodies, _ = corpus.read_pages(dump, pages)
    if not bodies:
        raise SystemExit("no page bodies read — the dump is not a MediaWiki XML export")
    drops = collections.Counter()
    seen = set()
    out = []
    for body in bodies:
        # Per LINE, not per body. A wiki body's lines are paragraphs, headings
        # and citation crumbs; stripping the whole body at once glues a section
        # heading onto the paragraph after it (`참고 문헌 Oxtoby, D.`), which then
        # reads as a sentence with a script change that nobody typed. Headings
        # are short, so a floor on the raw line drops them before they can be
        # counted — and the floor is recorded, not silently applied.
        for line in body.split("\n"):
            if len(line) < 40:
                drops["line_short"] += 1
                continue
            try:
                text = corpus.strip_wiki(line)
            except Exception:  # a line the parser cannot read is a dropped line
                drops["strip_error"] += 1
                continue
            for sentence in split_sentences(text):
                keep, why = assess(sentence, max_chars, max_words)
                if not keep:
                    drops[why] += 1
                    continue
                if sentence in seen:
                    drops["duplicate"] += 1
                    continue
                seen.add(sentence)
                out.append(sentence)
                if limit and len(out) >= limit:
                    break
            if limit and len(out) >= limit:
                break
        if limit and len(out) >= limit:
            break
    return out, drops, len(bodies)


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def write(out_path, sentences, meta):
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(json.dumps({"meta": meta}, ensure_ascii=False) + "\n")
        for i, s in enumerate(sentences):
            scripts = [classify(w) for w in words(s)]
            f.write(json.dumps({"id": f"s{i:05d}", "text": s, "scripts": scripts},
                               ensure_ascii=False) + "\n")
    meta_path = out_path.with_suffix(out_path.suffix + ".meta.json")
    meta_path.write_text(json.dumps(meta, ensure_ascii=False, indent=1) + "\n",
                         encoding="utf-8")
    return meta_path


def selfcheck(path):
    """Read the artefact back and assert it is the corpus it claims to be.

    The failure this guards is the one §3 lists first: a tool that returns a
    confident zero. An empty or all-one-script corpus would make ② read 0 with
    no error, so it refuses instead.
    """
    lines = pathlib.Path(path).read_text(encoding="utf-8").splitlines()
    if not lines:
        raise SystemExit(f"{path}: empty")
    head = json.loads(lines[0])
    if "meta" not in head:
        raise SystemExit(f"{path}: first line is not the meta record")
    meta = head["meta"]
    recs = [json.loads(l) for l in lines[1:] if l.strip()]
    if len(recs) != meta.get("sentences_kept"):
        raise SystemExit(f"{path}: {len(recs)} records, meta says "
                         f"{meta.get('sentences_kept')}")
    both = 0
    transitions = 0
    chars = 0
    for r in recs:
        if not r["text"] or not r["scripts"]:
            raise SystemExit(f"{path}: {r.get('id')} has no text or scripts")
        if "hangul" in r["scripts"] and "latin" in r["scripts"]:
            both += 1
        # The same definition the harness uses (tools/modes-metrics.py): the
        # initial mode is Hangul, so a sentence that opens with a Latin word
        # costs one 한/영 press and is one transition. Two definitions of `t` in
        # two tools is how the same corpus reports two different densities.
        prev = "hangul"
        for s in r["scripts"]:
            if s != prev:
                transitions += 1
                prev = s
        chars += len(r["text"])
    if both != len(recs):
        raise SystemExit(f"{path}: {len(recs) - both} records have no script change — "
                         f"② cannot grade them")
    if not chars:
        raise SystemExit(f"{path}: records carry no text")
    # `d` is the harness's to report (it excludes sentences the layout cannot
    # type); the builder states the corpus's own shape.
    print(f"OK  {path}: {len(recs)} sentences, {both} mixed, "
          f"{transitions} script transitions (initial mode hangul), "
          f"{chars} characters")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dump", type=pathlib.Path)
    ap.add_argument("--pages", type=int, default=3000)
    ap.add_argument("--limit", type=int, default=400)
    ap.add_argument("--max-chars", type=int, default=160)
    ap.add_argument("--max-words", type=int, default=14)
    ap.add_argument("--out", type=pathlib.Path)
    ap.add_argument("--selfcheck", type=pathlib.Path)
    args = ap.parse_args()

    if args.selfcheck:
        return selfcheck(args.selfcheck)
    if not args.dump or not args.out:
        ap.error("--dump and --out are required (or pass --selfcheck PATH)")

    sentences, drops, n_pages = build(args.dump, args.pages, args.limit,
                                      args.max_chars, args.max_words)
    if not sentences:
        raise SystemExit("no sentences passed the filters — see the drop counts "
                         "in the meta record before trusting a zero")
    meta = {
        "tool": "tools/build-modes-sentences.py",
        "invocation": " ".join(sys.argv),
        "source": {"path": str(args.dump), "sha256": sha256_of(args.dump),
                   "pages_read": n_pages, "reader": "tools/build-modes-corpus.py:read_pages",
                   "stripper": "tools/build-modes-corpus.py:strip_wiki"},
        "licence": "CC BY-SA 4.0 (ko.wikipedia dump); this corpus is derived text",
        "method": {
            "sentences": "split on [.!?] followed by whitespace, after collapsing "
                         "whitespace and stripping wiki markup",
            "filters": {"min_chars": 12, "max_chars": args.max_chars,
                        "max_words": args.max_words,
                        "allowed": "Hangul syllables, ASCII letters/digits, space, "
                                   + "".join(sorted(ALLOWED_PUNCT)),
                        "needs": ">=1 hangul word and >=1 latin word, no word "
                                 "mixing the two scripts in itself"},
            "drops": dict(drops),
            "window": f"first {n_pages} namespace-0 pages in dump order, not a "
                      "random sample",
        },
        "sentences_kept": len(sentences),
        "does_not_represent": [
            "encyclopedia prose, not typed messages — MODES-MEASUREMENT.md §6 records this "
            "genre understates how often people switch scripts, so the transition "
            "density here is a lower bound on the target population",
            "the dump's page order is not a random sample of Wikipedia",
            "sentences the layout cannot serialise are kept here and counted as "
            "not-inputtable by tools/modes-metrics.py, not dropped",
        ],
        "generated_at": None,
    }
    import datetime
    meta["generated_at"] = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")
    meta_path = write(args.out, sentences, meta)
    print(f"{args.out}: {len(sentences)} sentences from {n_pages} pages "
          f"(drops: {dict(drops)})")
    print(f"{meta_path}: provenance")
    return selfcheck(args.out)


if __name__ == "__main__":
    sys.exit(main())