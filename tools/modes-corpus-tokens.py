#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Report whether a corpus has tokens the autoswitch judgement can act on.

For each whitespace token the script classifier from build-modes-sentences.py is
used (the same one the corpus filters use). Latin tokens are looked up through
the engine's own `hunmin_buffered_preedit_english_valid`, not a second word list:
the question is whether this corpus is dominated by words the shipped English
data already covers, or by technical/out-of-list forms that exercise E1.

Usage
  tools/modes-corpus-tokens.py --corpus CORPUS.jsonl [--out REPORT.json]
"""
import argparse
import collections
import ctypes
import datetime
import hashlib
import importlib.util
import json
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
DATA = HERE.parent / "libhunmin" / "data"
LIB = HERE.parent / "build" / "libhunmin.so"


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise SystemExit(f"cannot load {path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    ap = argparse.ArgumentParser(description=(__doc__ or "").splitlines()[0])
    ap.add_argument("--corpus", type=pathlib.Path, required=True)
    ap.add_argument("--lib", type=pathlib.Path, default=LIB)
    ap.add_argument("--data", type=pathlib.Path, default=DATA)
    ap.add_argument("--layout", default="sin3-p2")
    ap.add_argument("--latin", default="us-qwerty")
    ap.add_argument("--out", type=pathlib.Path)
    args = ap.parse_args()

    bms = load("build_modes_sentences", HERE / "build-modes-sentences.py")
    mm = load("modes_metrics", HERE / "modes-metrics.py")
    engine = mm.Engine(args.lib, args.data)
    engine.lib.hunmin_buffered_preedit_english_valid.argtypes = [
        ctypes.c_void_p, ctypes.c_char_p]
    engine.lib.hunmin_buffered_preedit_english_valid.restype = ctypes.c_int
    layer = engine.layer(args.layout, args.latin, 1)
    try:
        tokens = collections.Counter()
        for line in args.corpus.read_text(encoding="utf-8").splitlines():
            rec = json.loads(line)
            if "text" not in rec:
                continue
            for word in rec["text"].split():
                tokens[(word, bms.classify(word))] += 1
        latin = {w: c for (w, k), c in tokens.items() if k == "latin"}
        hit = {w: bool(engine.lib.hunmin_buffered_preedit_english_valid(
            layer.h, w.encode())) for w in latin}
    finally:
        layer.close()

    report = {
        "meta": {
            "tool": "tools/modes-corpus-tokens.py",
            "generated_at": datetime.datetime.now(datetime.timezone.utc)
                                    .isoformat(timespec="seconds"),
            "corpus": str(args.corpus),
            "corpus_sha256": hashlib.sha256(args.corpus.read_bytes()).hexdigest(),
            "latin_validity": "hunmin_buffered_preedit_english_valid on the shipped data",
            "script_classifier": "tools/build-modes-sentences.py classify",
        },
        "counts": {
            "tokens": sum(tokens.values()),
            "hangul_tokens": sum(c for (_, k), c in tokens.items() if k == "hangul"),
            "latin_tokens": sum(latin.values()),
            "other_tokens": sum(c for (_, k), c in tokens.items() if k == "other"),
            "latin_unique": len(latin),
            "latin_hapax": sum(1 for c in latin.values() if c == 1),
            "latin_dict_hit_unique": sum(hit.values()),
            "latin_dict_miss_unique": len(latin) - sum(hit.values()),
            "latin_dict_hit_tokens": sum(c for w, c in latin.items() if hit[w]),
            "latin_dict_miss_tokens": sum(c for w, c in latin.items() if not hit[w]),
        },
        "top_latin": [{"token": w, "count": c, "dict_hit": hit[w]}
                      for w, c in sorted(latin.items(), key=lambda kv: -kv[1])[:40]],
        "top_latin_misses": [{"token": w, "count": c}
                             for w, c in sorted(latin.items(), key=lambda kv: -kv[1])
                             if not hit[w]][:40],
    }
    text = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(text, encoding="utf-8")
        print(f"{args.out}: {report['counts']}")
    else:
        print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
