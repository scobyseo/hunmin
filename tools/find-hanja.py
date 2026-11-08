#!/usr/bin/env python
"""Find Hanja in the docs, separating legitimate data from prose contamination.

The user's complaint: '네가 결과를 출력할 때, 문장에 한자어(정확히는 중국어)가
출력되는 경우가 많은데, 이걸 방지할 수 방법이 뭔지도 찾아보자.'

Two populations, and conflating them would fix the wrong thing:

  LEGITIMATE  the hanja-conversion feature. docs/SPEC-SCHEMA.md documents
              entries like 壽 "a long life"; pat.im/ist.xml is the normative
              P2 source and is full of them; libhunmin/data/hanja/ is the
              shipped table. Every one of these is the product's subject
              matter, and removing it would damage the documentation.

  CONTAMINATION  Hanja appearing in ENGLISH PROSE that is about something
              else -- a term the author happened to know in Chinese, or a
              translation artefact. This is what the user is objecting to,
              and it is invisible in review because each occurrence looks
              deliberate.

The earlier attempt to measure this passed the Hanja range through a shell
command line and got 1,582 hits in SPEC-MODES.md with no printable context,
which is a sign the pattern was mangled rather than that the file is full of
Hanja. So this file writes the ranges as escapes.
"""
import glob
import os
import re
import sys
import unicodedata
try:  # issue #272 phase 2: one ROOT derivation, with a file-relative fallback
    from _common import ROOT as _COMMON_ROOT
except ImportError:  # run from outside tools/: derive from this file
    _COMMON_ROOT = __import__("pathlib").Path(__file__).resolve().parents[1]

RANGES = [
    ("CJK Unified Ideographs", 0x4E00, 0x9FFF),
    ("CJK Ext A", 0x3400, 0x4DBF),
    ("CJK Compatibility Ideographs", 0xF900, 0xFAFF),
    ("CJK Compat Supplement", 0x2F800, 0x2FA1F),
]
# The regex is built from ORDINAL ranges, not from "\uXXXX" text. Putting the
# literal text \u4e00 inside a character class [...] makes the class contain the
# CHARACTERS backslash, u, 4, e, 0 — so it matched every line containing the
# letter "e". That is why the previous run reported 18,034 "Hanja" characters
# in TESTING.md and then printed lines that contained none.
PAT = re.compile("[" + "".join(chr(a) + "-" + chr(b)
                                for _, a, b in RANGES) + "]")

# Lines where Hanja IS the subject matter. Kept narrow on purpose: a loose
# filter would hide exactly the contamination we are looking for.
#
# The user's rule (2026-10-04): '이 도구에서 구현하는 한자 입력 기능을 설명하기
# 위해 한자가 나오거나 사용되는 건 괜찮아. 그와 상관없이 통상적인 문맥에서
# 중국어가 나오면 안돼.' So the test is whether the line is *about* the hanja
# conversion feature -- not whether it happens to sit in a file that mentions
# hanja elsewhere.
#
# Two ways a line qualifies, and the first version only had the first, which
# mislabelled the compose-table example in SPEC-P2.md as contamination:
#   1. the line itself names the feature (hanja, pat.im, ist.xml, the shipped
#      table, the lookup API);
#   2. the line shows a compose-table pair -- `(ㄷ,旦)→旦` -- which is the
#      feature's data, and appears in engine prose that never says "hanja".
LEGIT_LINE = re.compile(
    r"(hanja|漢字|한자|pat\.im|ist\.xml|symbols\.xml|yethangul|"
    r"data/hanja|hunmin_suggest|glossary|conventional reading|"
    r"한자 변환)", re.I)
# A compose-table pair: `(ㄷ,旦)→旦`. The pattern is anchored on the PAIR, not
# on a trailing arrow, because the canonical form has TWO of them -- one for
# the input and one for the result:
#
#     (ㄷ,旦)→旦
#     (ㄱ,甲)→甲        (ㄱ,丅)→丅
#
# An earlier version required a single arrow after the hanja and therefore
# rejected the real SPEC-P2.md line while accepting nothing legitimate; proven
# by the falsification probe in /tmp/hermes-verify-find-hanja.py, which feeds
# it a known-good compose line and a known-bad English sentence.
_JAMO = r"[ㄱ-ㅎㆆ-㆏가-힣]"
_HAN = r"[一-鿿㐀-䶿]"
COMPOSE_PAIR = re.compile(
    rf"[`(\[]\s*{_JAMO}\s*,\s*{_HAN}\s*\)\s*→\s*{_HAN}"      # (ㄷ,旦)→旦
    rf"|[`(\[]\s*{_JAMO}\s*,\s*{_HAN}\s*\)\s*\)?\s*→\s*{_JAMO}"
    rf"|{_HAN}\s*→\s*{_HAN}")                                  # a bare mapping

# A hanja shown as a DATA EXAMPLE rather than as prose: a hanja inside
# backticks or quotes with a reading or a gloss attached -- `壽` "a long
# life", `手` "a game of go". This is the shipped table being documented.
_DATA_EXAMPLE = re.compile(rf"`{_HAN}`|\"{_HAN}\"|'{_HAN}'")

# A hanja QUOTED to be corrected. Backticked or double-quoted, and the line says
# it is wrong -- "Broken here: `带받침`", "~~NIKE~~". Naming a mistake is not
# making the mistake, and flagging these trains people to strip the examples
# instead. The hanja still appears in the line, but it is a citation of an
# error rather than contamination, exactly as a compose-table pair is data.
_CITED_ERROR = re.compile(rf"(?:~~|`|\"){_HAN}")
_QUOTED_MISTAKE = re.compile(
    r"(broken here|for example|instead of|corrected|wrong|typo|misspell"
    r"|do not write|avoid)", re.I)


def scan(path):
    with open(path, encoding="utf-8") as fh:
        lines = fh.readlines()
    legit, prose = [], []
    for i, line in enumerate(lines, 1):
        if not PAT.search(line):
            continue
        chars = PAT.findall(line)
        # Two independent ways to be the subject matter: the line names the
        # feature, or it shows the feature's own data (a compose-table pair).
        # Either one makes the Hanja legitimate.
        ok = (bool(LEGIT_LINE.search(line))
              or bool(COMPOSE_PAIR.search(line))
              or bool(_DATA_EXAMPLE.search(line))
              or bool(_CITED_ERROR.search(line) and _QUOTED_MISTAKE.search(line)))
        (legit if ok else prose).append((i, chars, line.rstrip()))
    return legit, prose


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        str(_COMMON_ROOT), "docs")
    tot_l = tot_p = 0
    for f in sorted(glob.glob(os.path.join(root, "*.md"))) + \
             [os.path.join(os.path.dirname(root), "AGENTS.md")]:
        if not os.path.exists(f):
            continue
        legit, prose = scan(f)
        if not legit and not prose:
            continue
        tot_l += sum(len(c) for _, c, _ in legit)
        tot_p += sum(len(c) for _, c, _ in prose)
        print(f"\n{os.path.basename(f)}: "
              f"{len(legit)} legit lines / {sum(len(c) for _,c,_ in legit)} chars, "
              f"{len(prose)} prose lines / {sum(len(c) for _,c,_ in prose)} chars")
        for i, chars, line in prose[:6]:
            uniq = "".join(sorted(set(chars)))
            names = ", ".join(f"{c} U+{ord(c):04X}" for c in uniq[:6])
            print(f"  PROSE L{i}: {names}")
            print(f"         {line.strip()[:120]}")
    print(f"\nTOTAL legit {tot_l} chars, prose {tot_p} chars")
    return 0


if __name__ == "__main__":
    sys.exit(main())
