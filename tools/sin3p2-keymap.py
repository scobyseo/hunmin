#!/usr/bin/env python3
"""Probe the real sin3-p2 key -> jamo map by running hunmin-sim.

sin3-p2's `keys` entries are nested case trees (a value may be a dict with
`case`/`tier`/`noop` inside), so parsing the JSON is the wrong way to learn what
a key produces: the ground truth is what the engine commits. That is what this
does. Nothing here is transcribed by hand.

Vowels only appear under shift, so both cases are probed: `x` (plain) and `X`
(shift, which is how hunmin-sim's --keys string expresses a shifted key).

Usage:
    python3 tools/sin3p2-keymap.py [--layout sin3-p2] [--json out.json]
"""
import argparse
import json
import pathlib
import re
import string
import subprocess
import sys
try:  # issue #272 phase 2: one ROOT derivation, with a file-relative fallback
    from _common import ROOT as _COMMON_ROOT
except ImportError:  # run from outside tools/: derive from this file
    _COMMON_ROOT = __import__("pathlib").Path(__file__).resolve().parent.parent

REPO = _COMMON_ROOT
SIM = REPO / "build/hunmin-sim"
DATA = REPO / "libhunmin/data"

# Compatibility jamo, the blocks the engine's preedit string is written in.
CHO = set("ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ")
JUNG = set("ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ")
JONG = set("ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ")

PROBE_CHARS = (
    string.ascii_letters
    + string.digits
    + "".join(c for c in ";,/.-=[]`\\' " if c.strip())
)


def run(keys, layout):
    r = subprocess.run(
        [str(SIM), "--data", str(DATA), "--layout", layout, "--keys", keys],
        capture_output=True,
        text=True,
        timeout=30,
    )
    if r.returncode != 0:
        raise RuntimeError(f"hunmin-sim failed on {keys!r}: {r.stderr}")
    com = "".join(re.findall(r"commit='([^']*)'", r.stdout))
    pre = "".join(re.findall(r"preedit='([^']*)'", r.stdout))
    return com, pre


def kind(s):
    if not s:
        return ""
    c = s[0]
    if c in CHO:
        return "CHO"
    if c in JUNG:
        return "JUNG"
    if c in JONG:
        return "JONG"
    return ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--layout", default="sin3-p2")
    ap.add_argument("--json", type=pathlib.Path)
    a = ap.parse_args()

    out = {}
    for c in PROBE_CHARS:
        for case, key in (("plain", c), ("shift", c.upper())):
            com, pre = run(key, a.layout)
            k = kind(com) or kind(pre)
            if k:
                out.setdefault(k, {}).setdefault(case, {})[key] = com or pre

    for k in ("CHO", "JUNG", "JONG"):
            got = out.get(k, {})
            print(
                f"{k}: {len(got.get('plain', {}))} plain, "
                f"{len(got.get('shift', {}))} shift"
            )
            for case in ("plain", "shift"):
                m = got.get(case, {})
                print(f"   {case:5}: {' '.join(f'{key}={v}' for key, v in sorted(m.items()))}")

    if a.json:
        a.json.write_text(
            json.dumps({"layout": a.layout, "map": out}, ensure_ascii=False, indent=1),
            encoding="utf-8",
        )
        print(f"wrote {a.json}")
    return 0


if __name__ == "__main__":
    sys.exit(main())