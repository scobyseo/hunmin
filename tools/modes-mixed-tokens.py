#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Count a Latin stem with a Korean particle attached — `IBM은`, `GPT는`, `API를`.

Docs: `docs/MODES-MEASUREMENT.md` §1.5 (the unit and the reading), `docs/SPEC-MODES.md`
§E1 (the symmetric rule this population sits outside of).

WHY THIS EXISTS. The judgement reads one eojeol as one language: the keys are
replayed on the other layout and the whole token is or is not a dictionary word.
`IBM은` therefore reads as `ibmdms` on the Latin layout — not a word — and as
gibberish on the Hangul one, so the word keeps the mode it was typed in and the
person gets Hangul where they wanted the English word plus a particle. That is a
population question before it is a design question, and the existing corpora
cannot answer it: `tools/build-modes-sentences.py` drops every sentence with a
mixed-script word (`mixed_word` in its `meta.json` drops), which is exactly this
population. This probe counts it from the same dump, before that filter.

WHAT IT COUNTS, and the three numbers that keep it honest:

  mixed       every word `classify()` calls `other` — the builder's own drop
              reason. Most of these are wiki furniture (`|`, `=`, `-`), which is
              why the number cannot be read as "particles" and is printed.
  stem+josa   a token that starts with a Latin stem and ends with a **listed**
              particle, longest match; `direct` is the strict subset where the
              particle sits on the stem itself (`IBM은`) rather than after a
              Hangul run the stem owns (`PC방은`). The
              list is the National Institute of Korean Language 2003 survey's
              조사 table (품사 == 조, Hangul only), so `PC방` (a compound noun) and
              `PC용` are NOT counted — only real particles are, which is the set
              a split would have to know.
  sentences   the share of sentences carrying at least one stem+josa token, which
              is the population the sentence-level corpus filter removes.

THE ENGINE IS NOT CONSULTED, deliberately: this counts text, not what the engine
would decide, and the engine has no mixed-token path to ask about. The corpus
reader, the wiki stripper and the word classifier are the builders' own
(`tools/build-modes-corpus.py`, `tools/build-modes-sentences.py`), imported rather
than copied, so this cannot drift from the filter whose blind spot it measures.

WHAT IT DOES NOT REPRESENT. Wikipedia prose, and the same caveat the sentence
corpus carries: people write `IBM은` in messages far more than in an article, so
this is a lower bound on the typed population. The page window is the dump's own
order, not a random sample. No frequency weighting beyond the raw counts.

The dump and the survey live outside the repository (AGENTS.md): both paths are
arguments. `build-modes-corpus.py` needs `mwparserfromhell`, so run this with an
interpreter that has it (`~/work/wikicorpus/.venv/bin/python` on this host).

USAGE
  tools/modes-mixed-tokens.py --dump ~/work/kowiki-dump/kowiki.xml --pages 300 \\
      --josa ~/work/nikl-2003-freq/조사_가나다색인.txt
  tools/modes-mixed-tokens.py --selfcheck      # the tokeniser's own guards
"""
import argparse
import collections
import importlib.util
import json
import pathlib
import re
import subprocess
import sys
import unicodedata

WORD = re.compile(r"\S+")
STEM = re.compile(r"^([A-Za-z][A-Za-z0-9._+#/-]*)([\uac00-\ud7a3]+)$")


def load(name):
    """Import tools/<name>.py, whose hyphens make a plain import impossible."""
    path = pathlib.Path(__file__).resolve().parent / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name.replace("-", "_"), path)
    if spec is None or spec.loader is None:
        raise SystemExit(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def read_josa(path, max_len):
    """The survey's 조사 table as a set of Hangul suffixes, longest match wins.

    The file is EUC-KR with a `품사` (part of speech) column; only `조` rows are
    particles, and a row whose entry is not pure Hangul (`ㄴ`, a symbol) is not a
    suffix a Latin stem can carry — both exclusions are counted and reported, so
    a shrinking list cannot look like a shrinking population.
    """
    raw = subprocess.run(["iconv", "-f", "EUC-KR", "-t", "UTF-8", str(path)],
                         capture_output=True, text=True, check=False)
    if raw.returncode != 0:
        raise SystemExit(f"{path}: not EUC-KR? {raw.stderr.strip()[:200]}")
    rows = [line.split("\t") for line in raw.stdout.splitlines()[1:]]
    kept, not_particle, not_hangul, too_long = set(), 0, 0, 0
    for cols in rows:
        if len(cols) < 3 or not cols[1].strip():
            continue
        entry = unicodedata.normalize("NFC", cols[1].strip())
        if cols[2].strip() != "조":
            not_particle += 1
            continue
        if not all("\uac00" <= c <= "\ud7a3" for c in entry):
            not_hangul += 1
            continue
        if len(entry) > max_len:
            too_long += 1
            continue
        kept.add(entry)
    if not kept:
        raise SystemExit(f"{path}: no 조사 rows — this would count zero and look "
                         f"like a finding")
    return kept, {"rows": len(rows), "kept": len(kept),
                  "not_particle": not_particle, "not_hangul": not_hangul,
                  "too_long": too_long, "max_len": max_len}


def split_token(word, josa):
    """(stem, josa) for `IBM은`, else None. Longest listed suffix wins."""
    m = STEM.match(word)
    if not m:
        return None
    rest = m.group(2)
    for take in range(min(len(rest), max((len(j) for j in josa), default=0)), 0, -1):
        if rest[-take:] in josa:
            return m.group(1), rest[-take:]
    return None


def probe(dump, pages, josa_path, max_len, top):
    corpus = load("build-modes-corpus")
    sentences = load("build-modes-sentences")
    josa, josa_meta = read_josa(josa_path, max_len)
    if not hasattr(corpus, "read_pages"):
        raise SystemExit("build-modes-corpus.py has no read_pages -- it is the page "
                         "reader this tool must not re-implement")
    bodies, source = corpus.read_pages(dump, pages)
    if not bodies:
        raise SystemExit("no page bodies read -- the dump is not a MediaWiki XML export")
    stem_josa: collections.Counter = collections.Counter()
    direct = 0
    suffixes: collections.Counter = collections.Counter()
    mixed: collections.Counter = collections.Counter()
    n_sentences = n_with = n_words = 0
    for body in bodies:
        for line in body.split("\n"):
            if len(line) < 40:  # the sentence builder's own floor, same reason
                continue
            for sentence in sentences.split_sentences(corpus.strip_wiki(line)):
                n_sentences += 1
                hit = False
                for word in WORD.findall(sentence):
                    n_words += 1
                    if sentences.classify(word) != "other":
                        continue
                    mixed[word] += 1
                    parts = split_token(word, josa)
                    if parts:
                        stem_josa[word] += 1
                        suffixes[parts[1]] += 1
                        whole = STEM.match(word)
                        if whole is not None and len(parts[1]) == len(whole.group(2)):
                            direct += 1
                        hit = True
                if hit:
                    n_with += 1
    tokens = sum(stem_josa.values())
    return {
        "provenance": {
            "tool": "tools/modes-mixed-tokens.py",
            "dump": {"path": str(dump), "bytes": pathlib.Path(dump).stat().st_size,
                     "pages_read": len(bodies)},
            "source": source if isinstance(source, dict) else None,
            "josa": {"path": str(josa_path), **josa_meta},
            "note": "the dump's sha256 is recorded by the corpus built from it "
                    "(tools/build-modes-sentences.py meta.json); a probe does not "
                    "hash 6 GB to repeat it",
        },
        "counts": {
            "sentences": n_sentences, "words": n_words,
            "mixed_script_tokens": sum(mixed.values()),
            "mixed_script_unique": len(mixed),
            "stem_josa_tokens": tokens,
            "stem_josa_direct": direct,
            "stem_josa_unique": len(stem_josa),
            "sentences_with_stem_josa": n_with,
        },
        "shares": {
            "stem_josa_of_words": tokens / n_words if n_words else None,
            "sentences_with_stem_josa": n_with / n_sentences if n_sentences else None,
        },
        "top_tokens": stem_josa.most_common(top),
        "top_josa": suffixes.most_common(top),
        "top_other_mixed": mixed.most_common(top),
    }


def selfcheck():
    """A count that cannot fail loudly is the class §3 records; prove it can."""
    josa = {"은", "는", "를", "가", "이", "의"}
    problems = []
    if split_token("IBM은", josa) != ("IBM", "은"):
        problems.append("· IBM은 did not split into IBM + 은")
    if split_token("PC방", josa) is not None:
        problems.append("· PC방 (a compound noun) was counted as a particle")
    if split_token("|", josa) is not None:
        problems.append("· a wiki-furniture token was counted")
    if split_token("은", josa) is not None:
        problems.append("· a bare particle with no Latin stem was counted")
    if split_token("café는", josa) is not None:
        problems.append("· a non-ASCII stem was counted")
    if problems:
        print("SELFCHECK FAIL", file=sys.stderr)
        for p in problems:
            print(f"  - {p}", file=sys.stderr)
        return 1
    print(f"OK  stem+josa splits and the three exclusions hold ({len(josa)} suffixes)")
    return 0


def main():
    ap = argparse.ArgumentParser(description=(__doc__ or "").splitlines()[0])
    ap.add_argument("--dump", type=pathlib.Path)
    ap.add_argument("--pages", type=int, default=300)
    ap.add_argument("--josa", type=pathlib.Path,
                    help="the 2003 survey's 조사_가나다색인.txt (EUC-KR)")
    ap.add_argument("--max-josa-len", type=int, default=3)
    ap.add_argument("--top", type=int, default=20)
    ap.add_argument("--json", action="store_true", help="the whole record as JSON")
    ap.add_argument("--selfcheck", action="store_true")
    args = ap.parse_args()
    if args.selfcheck:
        return selfcheck()
    if not args.dump or not args.josa:
        ap.error("need --dump and --josa (both live outside the repository)")
    record = probe(args.dump, args.pages, args.josa, args.max_josa_len, args.top)
    if args.json:
        print(json.dumps(record, ensure_ascii=False, indent=1))
        return 0
    c, s = record["counts"], record["shares"]
    print(f"pages {record['provenance']['dump']['pages_read']}  "
          f"sentences {c['sentences']}  words {c['words']}")
    print(f"mixed-script tokens {c['mixed_script_tokens']} "
          f"(unique {c['mixed_script_unique']}) -- mostly wiki furniture")
    print(f"stem+josa tokens {c['stem_josa_tokens']} (unique {c['stem_josa_unique']}), "
          f"{100 * (s['stem_josa_of_words'] or 0):.2f}% of words; "
          f"{c['stem_josa_direct']} put the particle straight on the stem")
    print(f"sentences with one {c['sentences_with_stem_josa']} "
          f"({100 * (s['sentences_with_stem_josa'] or 0):.1f}%) -- the sentences the "
          f"sentence corpus filter drops")
    print(f"top josa {record['top_josa']}")
    print(f"top tokens {record['top_tokens']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())