#!/usr/bin/env python3
"""Measure which 옛한글 jamo sin3-p2-yet admits per slot, and by which key.

The per-slot probe in tools/type-hunminjeong.py covers the 현대 jamo; the
훈민정음 언해 서문 needs ᄡ, ᅇ, ᇙ, ᄫ, ᅙ and the rest, and those are exactly the
slots it reports MISS on. This script asks the ENGINE instead of a table: for
every key, press it in the three contexts that define a slot and record what
comes out.

    cho   context: <key> + ㅏ          (a consonant becomes a 초성 only when a
                                        vowel follows it -- the same judgement
                                        type-hunminjeong.py's docstring makes)
    jung  context: ㄱ + <key>          (after a 초성)
    jong  context: ㄱ + ㅏ + <key>     (when D and E are already filled)

한글 is emitted as conjoining jamo here, so a result is compared codepoint by
codepoint, not by precomposed syllable.
"""
import json
import pathlib
import subprocess
import sys
try:  # issue #272 phase 2: one ROOT derivation, with a file-relative fallback
    from _common import ROOT as _COMMON_ROOT
except ImportError:  # run from outside tools/: derive from this file
    _COMMON_ROOT = __import__("pathlib").Path(__file__).resolve().parent.parent

REPO = _COMMON_ROOT
SIM = REPO / "build" / "hunmin-sim"
DATA = REPO / "libhunmin" / "data"
LAYOUT = sys.argv[1] if len(sys.argv) > 1 else "sin3-p2-yet"

CHO = list("ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ")
JUNG = list("ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ")
JONG = list("ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ")
CONJ = {}
for i, c in enumerate(CHO):
    CONJ[c] = chr(0x1100 + i)
for i, c in enumerate(JUNG):
    CONJ[c] = chr(0x1161 + i)
for i, c in enumerate(JONG):
    CONJ[c] = chr(0x11A8 + i)


def batch(keys):
    r = subprocess.run([str(SIM), "--data", str(DATA), "--layout", LAYOUT, "--batch"],
                       input="\n".join(keys) + "\n", capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(r.stderr[:400])
    return [json.loads(l) for l in r.stdout.splitlines() if l.strip()]


def render(rec):
    return rec["commit"] + rec["preedit"]


def main():
    layout = json.loads((DATA / "layouts" / f"{LAYOUT}.json").read_text(encoding="utf-8"))
    keys = []
    for k in layout["keys"]:
        if k == "\\":
            # `\` is hunmin-sim's own escape lead, so it cannot appear in a batch
            # line: `\s\` reads as an unknown escape (measured). Its behaviour is
            # covered by the key case of the golden, not by a slot probe.
            continue
        keys.append(k)
        if len(k) == 1:
            keys.append("\\s" + k)
    # The two context keys come from tools/jamo-keytable.py's measured table: a
    # bare key press cannot identify ㅏ here, because sin3-p2-yet emits a vowel
    # only when a 초성이 already filled (measured: no single key renders "ㅏ").
    jk = json.loads(pathlib.Path(sys.argv[2]).read_text(encoding="utf-8"))["table"]
    a_key = jk["jung"]["ㅏ"]
    g_key = jk["cho"]["ㅇ"]
    print(f"context keys: ㅏ={a_key!r} ㅇ={g_key!r} (jamo-keytable)")

    cases = []
    for k in keys:
        cases.append(("cho", k, k + a_key))
    for k in keys:
        cases.append(("jung", k, g_key + k))
    for k in keys:
        cases.append(("jong", k, g_key + a_key + k))
    out = batch([c[2] for c in cases])

    found = {"cho": {}, "jung": {}, "jong": {}}
    for (slot, k, _), rec in zip(cases, out):
        text = render(rec)
        # The engine pads an isolated 종성 with the filler jamo U+115F/U+1160
        # (measured: sin3-p2-yet `af` renders U+115F U+1160 U+11B0), so the
        # comparison has to drop them before the jamo is read.
        text = text.replace("\u115f", "").replace("\u1160", "")
        if not text:
            continue
        if slot == "cho":
            if 0xAC00 <= ord(text[0]) <= 0xD7A3:
                jamo = CONJ[CHO[(ord(text[0]) - 0xAC00) // 588]]
            elif 0x1100 <= ord(text[0]) <= 0x115F:
                jamo = text[0]
            else:
                continue
        else:
            jamo = [ch for ch in text if 0x1100 <= ord(ch) <= 0x11FF]
            if not jamo:
                continue
            jamo = jamo[-1]
        found[slot].setdefault(jamo, k)

    for slot in ("cho", "jung", "jong"):
        print(f"{slot}: {len(found[slot])} jamo reachable")
    missing = {
        "cho": [CONJ[c] for c in CHO if CONJ[c] not in found["cho"]],
        "jung": [CONJ[c] for c in JUNG if CONJ[c] not in found["jung"]],
        "jong": [CONJ[c] for c in JONG if CONJ[c] not in found["jong"]],
    }
    for slot, jamo in missing.items():
        if jamo:
            print(f"  modern {slot} still unreachable: {''.join(jamo)}")
    out_path = pathlib.Path("/tmp/oldhangul-slots.json")
    out_path.write_text(json.dumps(found, ensure_ascii=False, indent=1), encoding="utf-8")
    print(f"-> {out_path}")
    # 옛한글: anything found that is not one of the modern 19/21/27
    modern = {"cho": {CONJ[c] for c in CHO}, "jung": {CONJ[c] for c in JUNG},
              "jong": {CONJ[c] for c in JONG}}
    for slot in ("cho", "jung", "jong"):
        extra = sorted(set(found[slot]) - modern[slot])
        print(f"{slot}: {len(extra)} 옛한글 jamo reachable"
              + (f" -- {''.join(extra[:14])}" if extra else ""))


if __name__ == "__main__":
    main()