#!/usr/bin/env python3
"""patim2golden.py — transcribe patim's P2 swift tests into total-text goldens.

Only (key, expected-total) facts are extracted; no code is copied (BSD-2
attribution in data-orig/patim/NOTICE). Cases touching 아래아 traits are skipped.
"""
import json
import re
import sys
from pathlib import Path

try:  # issue #272 phase 2: one ROOT derivation, with a file-relative fallback
    from _common import ROOT as _COMMON_ROOT
except ImportError:  # run from outside tools/: derive from this file
    _COMMON_ROOT = Path(__file__).resolve().parent.parent

SRC = _COMMON_ROOT / "data-orig/patim/Hangul3ShinP2Tests.swift"
OUT = _COMMON_ROOT / "tests/golden/patim-sin3-p2-basic.jsonl"

lines = SRC.read_text(encoding="utf-8").splitlines()
# patim implements an extra araea-chain composition mode (el-style djamo queue)
# that the normative ist files do not contain; cases whose expectations use it
# are recorded and skipped (facts, not code, are what we transcribe).
ARAEA_EXCLUDE = {"아래어ᄒᆟᆫ", "아래우ᄋᆠᆨ", "아래애ᄌᆡᆸ"}

cases, cur, name, skipped = [], [], "", []
in_test = False
for i, l in enumerate(lines):
    m = re.match(r'\s*@Test\("([^"]*)"\)', l)
    if m:
        if cur:
            cases.append((name, cur))
        name, cur, in_test = m.group(1), [], True
        continue
    if not in_test:
        continue
    if "traits" in l or name in ARAEA_EXCLUDE:
        skipped.append(name); cur = None; in_test = False
        continue
    if re.match(r"\s*struct ", l):
        break
    mk = re.search(r'rawChar = "(.)"', l)
    if mk and cur is not None:
        cur.append({"in": {"ch": mk.group(1)}})
        continue
    me = re.search(r'#expect\(c\d+ == "([^"]*)"\)', l)
    if me and cur:
        cur[-1]["total"] = me.group(1)
if cur:
    cases.append((name, cur))

out = []
kept = dropped = 0
for name, steps in cases:
    if not steps or not any("total" in s for s in steps):
        dropped += 1
        continue
    kept += 1
    out.append(json.dumps({"meta": {"case": name, "layout": "sin3-p2",
              "source": "patim Hangul3ShinP2Tests.swift (BSD-2, facts only)", "layout": "sin3-p2-basic"}},
              ensure_ascii=False))
    out.extend(json.dumps(s, ensure_ascii=False) for s in steps)
header = json.dumps({"meta": {"layout": "sin3-p2-basic", "mode": "total",
           "note": f"{kept} patim cases; {dropped} empty/none; traits-skipped: "
                   + ",".join(skipped)}}, ensure_ascii=False)
OUT.write_text(header + "\n" + "\n".join(out) + "\n", encoding="utf-8")
print(f"patim2golden: {kept} cases, {sum(len(c) for _, c in cases)} steps → {OUT.name}")
