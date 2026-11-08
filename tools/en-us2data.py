#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Generate libhunmin/data/modes/en-US.json from the hunspell en_US dictionary.

Ruling (maintainer, 2026-11-01): `english_valid` looks up hunspell `en_US`, and
people may extend it with a user dictionary. The measurement behind that choice
is docs/OPTIONS-MEASURED.md §EDW; the requirement is docs/SPEC-MODES.md ReqMode8.

WHY A GENERATED FILE AND NOT THE .dic/.aff PAIR. The engine needs a word SET,
not a speller: one membership test per finished buffer, no affix rules at
runtime. Expanding once here, at build-authoring time, also removes the only
reason the engine would need hunspell's affix semantics — and it buys real
coverage: on the §EDW probe the same stems score 60.75% unexpanded against
65.14% expanded (≥2 characters, token-weighted).

THE EXPANSION IS NOT REIMPLEMENTED HERE. `load_hunspell`, `load_aff` and
`expand` come from tools/modes-dict-probe.py, so the number measured in §EDW and
the file shipped to users are produced by one implementation. A second copy
would be free to drift, and drift would be invisible: the file would still load.

WHAT THIS DOES NOT DO, recorded in meta.json rather than left to be discovered:
continuation classes are not followed (one level only), compound rules are not
applied, and the `.dic`'s capitalised forms are lowercased — so this set can
answer "is this an english word", and cannot answer "is this capitalisation
right".

USAGE
  tools/en-us2data.py --dic en_US.dic --aff en_US.aff \\
      --out libhunmin/data/modes --readme README_en_US.txt
"""

import argparse
import hashlib
import importlib.util
import json
import os
import re
import sys

# A word this set can carry: ascii letters, plus an internal apostrophe or
# hyphen. Everything else (digits, ordinals like `1st`, `0th`, unicode forms) is
# excluded and COUNTED -- an exclusion nobody can see is a silent loss.
WORD = re.compile(r"^[a-z]+(?:['-][a-z]+)*$")
# Below this the generator has been pointed at the wrong file. The measured
# count for SCOWL size 60 expanded is ~118k; a hundred thousand is the floor
# that separates "the dictionary" from "a stray word list".
MIN_WORDS = 50000


def load_probe(repo_root):
    path = os.path.join(repo_root, "tools", "modes-dict-probe.py")
    spec = importlib.util.spec_from_file_location("modes_dict_probe", path)
    if spec is None:
        sys.exit(f"cannot load {path}")
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=(__doc__ or "").split("\n")[0])
    ap.add_argument("--dic", required=True, help="hunspell en_US .dic")
    ap.add_argument("--aff", required=True, help="the matching .aff")
    ap.add_argument("--out", required=True, help="directory to write into (libhunmin/data/modes)")
    ap.add_argument("--readme", help="README_en_US.txt, for the SCOWL version line")
    ap.add_argument("--repo-root", default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    args = ap.parse_args()

    probe = load_probe(args.repo_root)
    stems = probe.load_hunspell(args.dic)
    rules = probe.load_aff(args.aff)
    expanded = probe.expand(stems, rules)
    if not expanded:
        sys.exit("no forms: the .dic/.aff pair produced an empty expansion")

    kept, dropped = [], {"not_word_shaped": 0}
    for raw in expanded:
        w = raw.strip().lower()
        if WORD.match(w):
            kept.append(w)
        else:
            dropped["not_word_shaped"] += 1
    words = sorted(set(kept))
    if len(words) < MIN_WORDS:
        sys.exit(f"only {len(words):,} word-shaped forms (< {MIN_WORDS:,}); "
                 f"refusing to ship a list that is probably not the dictionary")

    os.makedirs(args.out, exist_ok=True)
    data_path = os.path.join(args.out, "en-US.json")
    with open(data_path, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(words, fh, ensure_ascii=False, indent=0)
        fh.write("\n")

    scowl = None
    if args.readme and os.path.exists(args.readme):
        with open(args.readme, encoding="utf-8", errors="replace") as fh:
            for line in fh:
                if line.startswith("Version"):
                    scowl = line.strip()
                    break

    meta = {
        "what": "the english word set behind SPEC-MODES ReqMode8's `english_valid`",
        "source": {
            "name": "hunspell en_US (SCOWL size 60 family)",
            "dic": os.path.basename(args.dic),
            "dic_sha256": sha256(args.dic),
            "aff": os.path.basename(args.aff),
            "aff_sha256": sha256(args.aff),
            "declared_version": scowl,
            "upstream": "https://github.com/LibreOffice/dictionaries (en/)",
        },
        "method": {
            "expansion": "one level of hunspell SFX/PFX via tools/modes-dict-probe.py "
                         "expand(); continuation classes and COMPOUNDRULE not applied",
            "lowercased": True,
            "word_shape": WORD.pattern,
        },
        "counts": {
            "stems": len(stems),
            "expanded_forms": len(expanded),
            "shipped": len(words),
            "dropped": dropped,
        },
        "licence": {
            "how": "SCOWL collective work: permission to use, copy, modify, "
                   "distribute and sell, provided the copyright notice and this "
                   "permission notice appear in copies and supporting "
                   "documentation. The .aff is BSD. Full text: "
                   "libhunmin/data/modes/LICENSE.md",
            "redistributable": True,
        },
        "does_not_represent": [
            "capitalisation (every form is lowercased)",
            "inflections outside one affix level (continuation classes skipped)",
            "compound words (COMPOUNDRULE not applied)",
            "korean-typed latin strings such as kbs/mbc/fifa -- absent from every "
            "english dictionary (docs/MODES-DATA.md 4-7d); the user dictionary is "
            "the ruled answer to that gap",
        ],
        "generator": "tools/en-us2data.py",
    }
    with open(os.path.join(args.out, "meta.json"), "w", encoding="utf-8", newline="\n") as fh:
        json.dump(meta, fh, ensure_ascii=False, indent=2)
        fh.write("\n")

    size = os.path.getsize(data_path)
    print(f"wrote {data_path}: {len(words):,} words, {size:,} bytes "
          f"({len(stems):,} stems -> {len(expanded):,} expanded, "
          f"{dropped['not_word_shaped']:,} dropped)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
