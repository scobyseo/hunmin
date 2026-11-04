#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""One golden per layout: every key, and every jamo IN THE SLOT THAT ADMITS IT.

The corpus used to spread a layout's evidence over several files (`ref-<layout>`
from libhangul's random walk, `words-<layout>`, `cultural-<text>-<layout>`), and
none of them asked the question this one asks: a jamo is not merely "typeable",
it is typeable AS A CHOSUNG, AS A JUNGSEONG or AS A JONGSEONG, and a layout may
admit it in one slot and refuse it in another. 세벌식 puts the three slots on
different keys; 신세벌식 P2 makes the same key id mean a vowel or a consonant
depending on what is already filled (tools/sin3p2-ksgen.py, its WHY section).
So the unit of coverage here is (jamo, slot):

  case keys    every key id the layout declares, plain and shifted, one sweep
  case cho/<j> the jamo as a CHOSUNG   (target: <j> + ㅏ)
  case jung/<j> the jamo as a JUNGSEONG (target: ㅇ + <j>)
  case jong/<j> the jamo as a JONGSEONG (target: ㅇ + ㅏ + <j>)

What a case asserts is the composed SYLLABLE, not the character: `jong/ㄳ` is
`앍`, which only exists if ㄳ is admitted in the final slot. That is the
difference between this file and a jamo list.

Keystrokes are SEARCHED (BFS over the layout's automaton, hunmin-sim --batch as
the oracle) or read from tools/jamo-keytable.py's probe of a table layout; no
expectation is typed by hand -- each case's events are hunmin-sim --trace.

A jamo the search cannot reach in a slot is NOT a failure: a layout need not
admit every jamo everywhere (현대 두벌식 has no 옛한글 jong at all). It is
recorded in the meta as `unreachable` with the slot, because "the layout cannot
type it" and "the test forgot it" must not look the same.

    python3 tools/gen-coverage-golden.py --layout dubeolsik \\
        --out tests/golden/coverage-dubeolsik.jsonl
"""
import argparse
import importlib.util
import json
import pathlib
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_SIM = REPO / "build" / "hunmin-sim"
DEFAULT_DATA = REPO / "libhunmin" / "data"

# 현대 자모. 옛한글 letters reach these cases through the layout's own compose
# tables (see `extra_jamo`), not through a hardcoded list: which archaic letters
# a layout admits is data, and this file must not become a second copy of it.
CHO = list("ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ")
JUNG = list("ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ")
JONG = list("ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ")

# compat -> conjoining, because the compose tables speak U+11xx/U+116x and the
# keys emit compatibility jamo. Without this every table side looks untypeable
# (measured: dubeolsik reported 36 of 36 compose sides "unreachable").
# conjoining -> compatibility. The direction matters: the compose tables write
# U+11xx/U+116x and the keys emit U+313x, so the map has to answer
# compat(ᄀ) == ㄱ. Built the other way round it answered nothing, every modern
# jamo came back as its conjoining self, and 31 syllables reported as
# "unreachable" on a layout that types all of them (measured on dubeolsik).
COMPAT = {}
for _i, _c in enumerate(CHO):
    COMPAT[chr(0x1100 + _i)] = _c
for _i, _c in enumerate(JUNG):
    COMPAT[chr(0x1161 + _i)] = _c
for _i, _c in enumerate("ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ"):
    COMPAT[chr(0x11A8 + _i)] = "ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ"[_i]


def compat(s: str) -> str:
    return "".join(COMPAT.get(c, c) for c in s)


def load_probe():
    """tools/type-hunminjeong.py as a module (its filename has a hyphen)."""
    spec = importlib.util.spec_from_file_location(
        "type_hunminjeong", REPO / "tools" / "type-hunminjeong.py")
    assert spec is not None and spec.loader is not None
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


# One map PER SLOT: the same letter is a different codepoint in two of them (ㄱ is
# ᄀ as a 초성 and ᆨ as a 종성), so a single map would silently pick the wrong one.
CONJ = {c: chr(0x1100 + i) for i, c in enumerate(CHO)}
CONJ_JUNG = {c: chr(0x1161 + i) for i, c in enumerate(JUNG)}
CONJ_JONG = {c: chr(0x11A8 + i) for i, c in enumerate(JONG)}


def syllable(cho: str, jung: str, jong: str = "") -> str:
    """Precomposed syllable. The 종성 index is JONG.index(jong) + 1: in the
    U+AC00 block jong 0 means NO final consonant, so using the list index
    directly names the previous syllable (measured: `ㅇ+ㅏ+ㄱ` produced 아 and
    every 종성 probe compared against its neighbour, 0 of 27 passing)."""
    i = CHO.index(cho) * 588 + JUNG.index(jung) * 28 + (JONG.index(jong) + 1 if jong else 0)
    return chr(0xAC00 + i)


def sim_batch(a, keys_list):
    """Rendered text for many keystroke strings, one engine process."""
    if not keys_list:
        return []
    r = subprocess.run([str(a.sim), "--data", str(a.data), "--layout", a.layout, "--batch"],
                       input="\n".join(keys_list) + "\n", capture_output=True,
                       encoding="utf-8", errors="replace", timeout=900)
    if r.returncode != 0:
        raise SystemExit(f"hunmin-sim --batch failed: {r.stderr.strip()[:300]}")
    out = []
    for line in r.stdout.splitlines():
        if not line.startswith("{"):
            continue
        rec = json.loads(line)
        out.append(rec["commit"] + rec["preedit"])
    return out


def sim_trace(a, keys):
    r = subprocess.run([str(a.sim), "--data", str(a.data), "--layout", a.layout,
                        "--keys", keys, "--trace"], capture_output=True,
                       encoding="utf-8", errors="replace", timeout=900)
    if r.returncode != 0:
        raise SystemExit(f"hunmin-sim --trace failed: {r.stderr.strip()[:300]}")
    return [json.loads(l) for l in r.stdout.splitlines() if l.startswith("{")]


def search(a, targets):
    """Keystrokes for each target string: BFS through the layout's automaton."""
    tmp = pathlib.Path(a.work) / "targets.txt"
    tmp.write_text("\n".join(targets), encoding="utf-8")
    out = pathlib.Path(a.work) / "ksgen.json"
    r = subprocess.run([sys.executable, str(REPO / "tools/sin3p2-ksgen.py"),
                        "--layout", a.layout, "--text-file", str(tmp),
                        "--out", str(out), "--max-len", str(a.max_len)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"sin3p2-ksgen failed: {r.stderr.strip()[:400]}")
    print("  " + r.stderr.strip().replace("\n", "\n  ")[:400], file=sys.stderr)
    return json.loads(out.read_text(encoding="utf-8"))["keystrokes"]


def declared_jamo(a):
    """Every jamo the layout's own compose tables mention, by slot.

    The tables are `{a, b, r}` pairs in conjoining jamo. `r` is what the pair
    composes to, so it says which slot the result belongs in: a CHO addition is
    a 초성 jamo (U+1100 block), a JUNG one a 중성, a JONG one a 종성. Anything the
    modern lists already cover is dropped here; what remains is the 옛한글 and
    the compounds the layout adds.
    """
    layout = json.loads((pathlib.Path(a.data) / "layouts" / f"{a.layout}.json")
                        .read_text(encoding="utf-8"))
    names = layout.get("compose") or []
    if isinstance(names, str):
        names = [names]
    extra = {"cho": set(), "jung": set(), "jong": set()}
    for name in names:
        p = pathlib.Path(a.data) / "compose" / f"{name}.json"
        if not p.is_file():
            continue
        for it in json.loads(p.read_text(encoding="utf-8")).get("items", []):
            for side in ("a", "b", "r"):
                # Classify the CONJOINING codepoint the table writes (U+1100
                # block): compat() would map ᄀ to ㄱ and lose the slot, and an
                # 옛한글 letter has no compatibility form at all.
                for ch in it.get(side, ""):
                    o = ord(ch)
                    # Classified by the conjoining codepoint (the slot), stored
                    # in its compatibility form so 현대 jamo dedupe against
                    # CHO/JUNG/JONG; an 옛한글 letter has no compatibility form,
                    # keeps its conjoining codepoint, and is its own target.
                    if 0x1100 <= o <= 0x115F:
                        extra["cho"].add(compat(ch))
                    elif 0x1160 <= o <= 0x11A7:
                        extra["jung"].add(compat(ch))
                    elif 0x11A8 <= o <= 0x11FF:
                        extra["jong"].add(compat(ch))
    return layout, extra


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--layout", required=True)
    ap.add_argument("--out", type=pathlib.Path, required=True)
    ap.add_argument("--sim", type=pathlib.Path, default=DEFAULT_SIM)
    ap.add_argument("--data", type=pathlib.Path, default=DEFAULT_DATA)
    ap.add_argument("--work", type=pathlib.Path, default=None)
    ap.add_argument("--max-len", type=int, default=4,
                    help="BFS depth cap for one probe (a jong needs 3 strokes "
                         "on 신세벌식 P2, so 2 is too small)")
    ap.add_argument("--keys-case", action="store_true",
                    help="also sweep every key id plain and shifted")
    ap.add_argument("--probe", action="store_true",
                    help="assemble the slot keys with tools/type-hunminjeong.py's "
                         "measured probe instead of searching: the 세벌식 family "
                         "has 초성 on the right-hand keys, 중성 in the middle and "
                         "종성 on the left (measured 67/67 for sebeolsik-final "
                         "and sin3-p2, where the BFS needed minutes)")
    ap.add_argument("--require-modern", type=int, default=0,
                    help="refuse to write the golden unless at least N of the 67 "
                         "modern slots compose (the gate that catches a probe "
                         "which silently measures the wrong side of the board)")
    ap.add_argument("--jamo-table", type=pathlib.Path, default=None,
                    help="tools/jamo-keytable.py JSON: cho/jung/jong sit on "
                         "separate keys, so no search is needed")
    a = ap.parse_args()
    if a.work is None:
        import tempfile
        a.work = pathlib.Path(tempfile.mkdtemp(prefix="hunmin-coverage-"))
    else:
        a.work.mkdir(parents=True, exist_ok=True)

    layout, extra = declared_jamo(a)
    table = None
    if a.jamo_table:
        table = json.loads(a.jamo_table.read_text(encoding="utf-8"))["table"]

    # (label, target, slot, jamo). A 현대 jamo's target is a PRECOMPOSED syllable
    # (까, 와, 앍): the engine folds it into one codepoint, so "did this slot
    # accept the jamo" is an equality test a reader can check by eye. An 옛한글
    # jamo has no precomposed syllable -- the engine renders it as the conjoining
    # sequence -- so its target is that sequence (ᄞ + ᅡ). Same assertion, one
    # codepoint per jamo.
    probes = []
    for j in CHO:
        probes.append((f"cho/{j}", syllable(j, "ㅏ"), "cho", j))
    for j in JUNG:
        probes.append((f"jung/{j}", syllable("ㅇ", j), "jung", j))
    for j in JONG:
        probes.append((f"jong/{j}", syllable("ㅇ", "ㅏ", j), "jong", j))
    for j in sorted(extra["cho"] - set(CHO)):
        probes.append((f"cho/{j}", j + "\u1161", "cho", j))
    for j in sorted(extra["jung"] - set(JUNG)):
        probes.append((f"jung/{j}", "\u110b" + j, "jung", j))
    for j in sorted(extra["jong"] - set(JONG)):
        probes.append((f"jong/{j}", "\u110b\u1161" + j, "jong", j))

    if a.probe:
        th = load_probe()
        # The context keys are chosen by SCORE, not by a literal: `k` is ㄱ on
        # sin3-p2-yet and ㅏ on dubeolsik, and a hardcoded pair measures a vowel
        # as a consonant (measured: 10 of 67 slots on dubeolsik with a literal
        # context, 0 without the (jung, jong) 종성 table).
        def modern_score(g, ak):
            C, J, G, C2, J2, G2, JC, CH = th.probe_slots(a.layout, g, ak)
            COMPOSE = th.build_compose(C, G)
            JG = dict(th.build_jong_compose(G))
            for jamo, kk in (G2 or {}).items():
                JG[jamo] = kk
            ks = [th.build(CONJ[c], CONJ_JUNG["ㅏ"], "", C, J, G, COMPOSE, JG, J2, JC, CH)
                  for c in CHO]
            outs = sim_batch(a, [k or "" for k in ks])
            return sum(1 for c, o in zip(CHO, outs) if o == syllable(c, "ㅏ"))

        best = None
        # ... and when NO literal scores, the probe discovers the pair itself (one
        # batched scan). ahnmatmae and romaja put jamo on shifted keys and their
        # unshifted keys commit literal ASCII, so no literal in this list stands
        # on 가 -- measured: both read as 1 of 19 초성 until discovery ran.
        for g, ak in ((None, None), ("k", "f"), ("r", "k"), ("r", "K"), ("j", "f")):
            try:
                s = modern_score(g, ak)
            except SystemExit:
                continue
            if best is None or s > best[2]:
                best = (g, ak, s)
        if best is None:
            raise SystemExit(f"{a.layout}: no context pair renders 가, so the slot "
                             "probe has nothing to stand on")
        g, ak, s = best
        C, J, G, C2, J2, G2, JC, CH = th.probe_slots(a.layout, g, ak)
        COMPOSE = th.build_compose(C, G)
        JG = dict(th.build_jong_compose(G))
        for jamo, kk in (G2 or {}).items():
            JG[jamo] = kk

        def keys_for(slot, jamo):
            if slot == "cho":
                cho, jung, jong = CONJ.get(jamo, jamo), CONJ_JUNG["ㅏ"], ""
            elif slot == "jung":
                cho, jung, jong = CONJ["ㅇ"], CONJ_JUNG.get(jamo, jamo), ""
            else:
                cho, jung, jong = CONJ["ㅇ"], CONJ_JUNG["ㅏ"], CONJ_JONG.get(jamo, jamo)
            return th.build(cho, jung, jong, C, J, G, COMPOSE, JG, J2, JC, CH)

        keys = {t: keys_for(slot, j) for _, t, slot, j in probes}
        print(f"probe context {g}+{ak}: cho {len(C)}, jung {len(J)}, jong {len(G)}, "
              f"compose {len(CH)} -> {s}/19 초성", file=sys.stderr)
    elif table:
        # A jamo-table entry is the keystroke for that jamo IN ITS SLOT (ㄱ -> 'k'
        # renders ㄱ, not 가), so a probe's keys are the slot keystrokes
        # concatenated: cho+ㅏ, ㅇ+jung, ㅇ+ㅏ+jong. Read as a whole syllable the
        # table looks empty -- measured: sebeolsik-final reported 0 of 67 slots
        # until the sides were assembled here.
        def slot_key(slot, jamo):
            return table.get(slot, {}).get(jamo)

        keys = {}
        for label, target, slot, jamo in probes:
            if slot == "cho":
                parts = [slot_key("cho", jamo), slot_key("jung", "ㅏ")]
            elif slot == "jung":
                parts = [slot_key("cho", "ㅇ"), slot_key("jung", jamo)]
            else:
                parts = [slot_key("cho", "ㅇ"), slot_key("jung", "ㅏ"),
                         slot_key("jong", jamo)]
            if all(parts):
                keys[target] = "".join(parts)
    else:
        keys = search(a, [t for _, t, _, _ in probes])

    # A probe is admitted only if the ENGINE composes exactly the target: the
    # search finds keys that RENDER something, and a layout may render a
    # different syllable than the one asked for.
    # Verified ONE probe at a time. A batched check put the wrong line's answer
    # against a probe often enough to report 25 종성 failures that `--trace`
    # showed composing fine (measured on sebeolsik-3-90: `jfs` renders 안 alone and
    # in a two-line batch, yet the 67-line batch disagreed), and a golden built on
    # a misaligned answer is worse than a slow generator: 67 traces cost ~2 s.
    renders = []
    for _label, target, _slot, _jamo in probes:
        k = keys.get(target, "")
        if not k:
            renders.append("")
            continue
        ev = sim_trace(a, k)
        got = "".join(e.get("commit", "") for e in ev) + \
              (ev[-1].get("preedit", "") if ev else "")
        renders.append(got)
    cases, unreachable = [], []
    for (label, target, slot, jamo), r in zip(probes, renders):
        if target in keys and r == target:
            cases.append((label, target, keys[target]))
        else:
            unreachable.append({"case": label, "target": target,
                                "because": "no keystroke composes it in this slot"
                                           if target in keys
                                           else f"keys render {r!r}"})

    if a.require_modern:
        modern_labels = {f"cho/{c}" for c in CHO} | {f"jung/{j}" for j in JUNG} \
            | {f"jong/{j}" for j in JONG}
        hit = sum(1 for label, _t, _k in cases if label in modern_labels)
        if hit < a.require_modern:
            raise SystemExit(
                f"{a.layout}: {hit} modern slots compose, below --require-modern "
                f"{a.require_modern} -- refusing to write a golden that would look "
                f"like coverage (see docs/TESTING.md, the instrument decides the "
                f"number)")
        print(f"gate: {hit} modern slots compose (>= {a.require_modern})",
              file=sys.stderr)

    out = [json.dumps({"meta": {
        "layout": a.layout,
        "tool": "tools/gen-coverage-golden.py",
        "unit": "one case per (jamo, slot): the composed SYLLABLE is asserted, "
                "so a jamo admitted in one slot and refused in another is visible",
        "keys_from": ("tools/jamo-keytable.py probe (cho/jung/jong sit on separate "
                      "keys)" if table else
                      "tools/sin3p2-ksgen.py BFS over the layout's own automaton, "
                      "hunmin-sim --batch as oracle"),
        "expectations": "hunmin-sim --trace over those keys -- never typed by hand",
        # Named "not_reached", not "unreachable": this is what the GENERATOR's
        # search did not find, and a probe gap and a layout that cannot type a
        # jamo look the same from here (measured: the jamo-keytable path reaches
        # 38 of 67 slots on sebeolsik-3-90 while the BFS reaches all of them,
        # so the difference is the instrument, not the layout).
        "not_reached": unreachable,
        "cases": f"{len(cases)} of {len(probes)} probes admitted by the engine",
    }}, ensure_ascii=False)]

    if a.keys_case:
        # One case per key, not one long sweep: `\s` is hunmin-sim's shift
        # escape, so a layout whose key set contains `\` cannot be swept as one
        # string (measured: sebeolsik-3-2 died on "unknown key escape \]"), and
        # one case per key is also what a reader can check -- `key/R` says
        # "Shift+r on this layout renders ㄲ", which a 94-event blob does not.
        key_cases = []
        for k in layout["keys"]:
            key_cases.append((f"key/{k}", k if k != "\\" else "\\\\"))
            if len(k) == 1:
                key_cases.append((f"key/shift-{k}", "\\s" + (k if k != "\\" else "\\\\")))
        for label, k in key_cases:
            events = sim_trace(a, k)
            out.append(json.dumps({"meta": {"case": label, "keys": k}}, ensure_ascii=False))
            for rec in events:
                out.append(json.dumps({"in": rec["in"], "commit": rec.get("commit", ""),
                                       "preedit": rec.get("preedit", "")},
                                      ensure_ascii=False))

    for label, target, k in cases:
        out.append(json.dumps({"meta": {"case": label, "target": target, "keys": k}},
                              ensure_ascii=False))
        for rec in sim_trace(a, k):
            out.append(json.dumps({"in": rec["in"], "commit": rec.get("commit", ""),
                                   "preedit": rec.get("preedit", "")},
                                  ensure_ascii=False))

    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text("\n".join(out) + "\n", encoding="utf-8")
    events = sum(1 for l in out if '"in"' in l)
    print(f"{a.out}: {len(cases)}/{len(probes)} slots, {len(unreachable)} unreachable, "
          f"{events} events", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())