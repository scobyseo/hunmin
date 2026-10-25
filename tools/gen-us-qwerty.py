#!/usr/bin/env python3
"""Generate the key table for data/layouts/us-qwerty.json.

QWERTY is a physical keyboard, so the table is mechanical -- but it is
generated rather than typed so that it cannot drift from the key-id convention
in docs/SPEC-SCHEMA.md, and so a reviewer can check the rule instead of 94
lines of JSON.

Rules, both taken from code/docs already in this repository:

  * key ids are the physical US positions named by their UNSHIFTED character:
    26 letters + 10 digits + 21 punctuation (SPEC-SCHEMA.md, "Key ids")
  * a letter's shifted form is its case-swapped self, which is what
    libhunmin/src/schema_loader.c:shifted_of() does for a-z

Usage:  python3 tools/gen-us-qwerty.py [--out FILE] [--check]
        --check  verify the committed table matches, without writing
"""
import argparse
import json
import pathlib
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_OUT = REPO / "libhunmin/data/layouts/us-qwerty.json"

# The 21 punctuation key ids and their shifted forms, in SPEC-SCHEMA.md order.
PUNCT = [
    ("`", "~"), ("1", "!"), ("2", "@"), ("3", "#"), ("4", "$"), ("5", "%"),
    ("6", "^"), ("7", "&"), ("8", "*"), ("9", "("), ("0", ")"), ("-", "_"),
    ("=", "+"), ("[", "{"), ("]", "}"), ("\\", "|"), (";", ":"), ("'", '"'),
    (",", "<"), (".", ">"), ("/", "?"),
]
LETTERS = "abcdefghijklmnopqrstuvwxyz"
DIGITS = "0123456789"


HEADER = {
    "schemaVersion": 1,
    "id": "us-qwerty",
    "name": {
        "en": "US QWERTY (Latin, no composition)",
        "ko": "QWERTY (영어)",
    },
    "aliases": ["us", "qwerty"],
    "engine": "std",
    # The one Latin-outputting layout in the tree, and the whole reason the
    # Android settings window has two array lists (docs/SPEC-SCHEMA.md,
    # maintainer 2026-10-11). Every other layout composes Hangul, romaja
    # included, so nothing else may say "latin".
    "mode": "latin",
    "libhangulType": "jamo",
    "compose": ["modern"],
    "source": "US QWERTY as the physical key names in docs/SPEC-SCHEMA.md (Key ids); each key id is named by its unshifted character and each modifier emits that character directly. This is not copied from any implementation -- the mapping IS the US keyboard, and the key-id convention is this project's own (SPEC-SCHEMA.md 'Key ids').",
    "x-notes": {
        "why": "SPEC-MODES R2 requires an English mode that selects qwerty/dvorak, and R6's OFF+English behaviour requires 'English mode has to be a Latin layout rather than a Hangul layout -- since a Latin key produces a Latin letter there is nothing to compose'. No such layout existed in libhunmin/data/layouts/, so R2's English arm was unmeasurable. This adds it.",
        "composition": "There is none, by design. A literal value is not a jamo, so process_jamo() takes the ic.c:170 branch and commits it directly, leaving the buffer empty. That is what makes R6's 'commit per character' fall out rather than be special-cased.",
        "not_a_layout_choice": "This is a data addition required to measure R8, not a decision to ship qwerty as the English mode. The maintainer decides which Latin layouts English mode offers.",
    },
}


def build():
    """The whole layout file: a hand-written header plus the derived keys.

    The header lives here rather than being read back from the output file so
    the generator works into an empty tree, which is what tools/selfcheck.py
    needs when it regenerates into a scratch directory to diff against the
    committed data.
    """
    keys = {}
    for c in LETTERS:
        keys[c] = {"plain": "=" + c, "shift": "=" + c.swapcase()}
    for c in DIGITS:
        # digits have no shifted letter form; the shifted column is punctuation
        keys[c] = {"plain": "=" + c}
    for base, shifted in PUNCT:
        keys[base] = {"plain": "=" + base, "shift": "=" + shifted}
    return {**HEADER, "keys": keys}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=pathlib.Path, default=DEFAULT_OUT,
                    help="write here; used by selfcheck.py to regenerate into "
                         "a scratch tree")
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()

    want = build()

    if a.check:
        cur = json.loads(a.out.read_text(encoding="utf-8"))
        if cur != want:
            print("us-qwerty.json does not match tools/gen-us-qwerty.py")
            return 1
        print(f"us-qwerty.json matches the generator ({len(want['keys'])} key ids)")
        return 0

    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps(want, ensure_ascii=False, indent=2) + "\n",
                     encoding="utf-8")
    print(f"wrote {len(want['keys'])} key ids to {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())