#!/usr/bin/env python3
"""Build a jamo -> keystroke table for a 세벌식 layout, then type text with it.

WHY A TABLE, NOT A SEARCH
-------------------------
A keystroke for a jamo is a *lookup*, not a shortest path -- once the slot
(cho/jung/jong) and its physical side are known. BFS over the engine
automaton (tools/sin3p2-ksgen.py) explodes with depth and cannot even find a
4-syllable eojeol; probing each jamo in the right context is linear.

THE MEASURED STRUCTURE (sin3-p2)
--------------------------------
초성 and 종성 are on SEPARATE physical keys -- 초성 always on the right-hand
keys. This is the fact a naive single-jamo probe misses:

    ㄷ alone is typed by BOTH u and g, but u+v=도 (ㄷ+ㅗ) while g+v=ㅈ.
    g only *looks* like ㄷ in isolation; it is a 종성-side jamo. The 초성
    probe must therefore judge a key by whether key+vowel forms a real
    cho+jung syllable, not by what it renders on an empty buffer.

Measured (probe each key on an empty buffer, then key+중성):
    초성: ㄱ=k ㄴ=h ㄷ=u ㄹ=y ㅁ=i ㅂ=; ㅅ=n ㅇ=j ㅈ=l ㅊ=o ㅋ=/ ㅌ=' ㅍ=p ㅎ=m
    중성: ㅏ=f ㅓ=r ㅗ=v ㅜ=b ㅡ=g ㅣ=d ㅐ=e ㅔ=c ...  (compound ㅘ=/f, ㅟ=od)
    종성: (on ㄷ+ㅗ=도) ㅇ=a ㄱ=c ㄹ=w ㄴ=s ㅁ=z ㅂ=e ...   -- 왼쪽, 초성과 분리
    겹받침: 단일 종성 + 추가 키:  ㄶ=sd ㄳ=cq ㄺ=wc ...

typing is then cho -> jung -> jong in order (a person's order), and a jong
key lands in the jong slot precisely because cho+jung already filled the
earlier slots. 겹받침 is two jong keys; the engine composes them.

Fallback for a jamo with no key in its context is recorded as not_found, not
guessed.
"""
import json
import pathlib
import subprocess
import sys
import unicodedata

try:
    from _common import ROOT as REPO, DEFAULT_DATA as DATA, DEFAULT_SIM as _DEFAULT_SIM
except ImportError:
    REPO = pathlib.Path(__file__).resolve().parent.parent
    DATA = REPO / "libhunmin" / "data"
    _DEFAULT_SIM = REPO / "build" / "hunmin-sim"
SIM = _DEFAULT_SIM

# The sim binary `batch()` drives. Defaults to the in-tree build; a caller
# that builds elsewhere (build-wsl, debian's obj dir) sets it explicitly.
# `tools/modes-metrics.py` does, because the ctest lane passes its own
# --sim and the jamo probe is one of three sim consumers in that harness.
SIM_OVERRIDE = None

CHOSEONG = ['ㄱ','ㄲ','ㄴ','ㄷ','ㄸ','ㄹ','ㅁ','ㅂ','ㅃ','ㅅ','ㅆ','ㅇ','ㅈ','ㅉ','ㅊ','ㅋ','ㅌ','ㅍ','ㅎ']
JUNGSEONG = ['ㅏ','ㅐ','ㅑ','ㅒ','ㅓ','ㅔ','ㅕ','ㅖ','ㅗ','ㅘ','ㅙ','ㅚ','ㅛ','ㅜ','ㅝ','ㅞ','ㅟ','ㅠ','ㅡ','ㅢ','ㅣ']
JONGSEONG = ['','ㄱ','ㄲ','ㄳ','ㄴ','ㄵ','ㄶ','ㄷ','ㄹ','ㄺ','ㄻ','ㄼ','ㄽ','ㄾ','ㄿ','ㅀ','ㅁ','ㅂ','ㅄ','ㅅ','ㅆ','ㅇ','ㅈ','ㅊ','ㅋ','ㅌ','ㅍ','ㅎ']
EXT_CHO = [chr(c) for c in range(0x1100, 0x1160)]
EXT_JUNG = [chr(c) for c in range(0x1161, 0x11A8)]
EXT_JONG = [chr(c) for c in range(0x11A8, 0x1200)]

ALL_KEYS = [chr(c) for c in range(0x20, 0x7f) if chr(c) not in ('"', '\\')]

# Hangul Compatibility Jamo (U+3131-318E) -> conjoining jamo (U+1100 block).
# Explicit, not a formula: the vowel block does not start where the consonant
# block does, so ord(c)-0x3131+0x1100 is wrong for vowels.
_COMPAT = {
    'ㄱ':'ᄀ','ㄲ':'ᄁ','ㄴ':'ᄂ','ㄷ':'ᄃ','ㄸ':'ᄄ','ㄹ':'ᄅ','ㅁ':'ᄆ',
    'ㅂ':'ᄇ','ㅃ':'ᄈ','ㅅ':'ᄉ','ㅆ':'ᄊ','ㅇ':'ᄋ','ㅈ':'ᄌ','ㅉ':'ᄍ',
    'ㅊ':'ᄎ','ㅋ':'ᄏ','ㅌ':'ᄐ','ㅍ':'ᄑ','ㅎ':'ᄒ',
    'ㅏ':'ᅡ','ㅐ':'ᅢ','ㅑ':'ᅣ','ㅒ':'ᅤ','ㅓ':'ᅥ','ㅔ':'ᅦ','ㅕ':'ᅧ',
    'ㅖ':'ᅨ','ㅗ':'ᅩ','ㅘ':'ᅪ','ㅙ':'ᅫ','ㅚ':'ᅬ','ㅛ':'ᅭ','ㅜ':'ᅮ',
    'ㅝ':'ᅯ','ㅞ':'ᅰ','ㅟ':'ᅱ','ㅠ':'ᅲ','ㅡ':'ᅳ','ㅢ':'ᅴ','ㅣ':'ᅵ',
    'ㄳ':'ᆪ','ㄵ':'ᆬ','ㄶ':'ᆭ','ㄺ':'ᆰ','ㄻ':'ᆱ','ㄼ':'ᆲ','ㄽ':'ᆳ',
    'ㄾ':'ᆴ','ㄿ':'ᆵ','ㅀ':'ᆶ','ㅄ':'ᆹ',
}
# 종성 (final consonant) compat jamo -> conjoining jong (U+11A8 block). Distinct
# from _COMPAT: the same compat letter (ㄱ, ㄴ, ㅇ ...) is a DIFFERENT code point
# as a final (ㄱ cho=ᄀ U+1100 vs jong=ᆨ U+11A8). probe_jong needs the jong form.
_JONG_COMPAT = {
    'ㄱ':'ᆨ','ㄲ':'ᆩ','ㄳ':'ᆪ','ㄴ':'ᆫ','ㄵ':'ᆬ','ㄶ':'ᆭ','ㄷ':'ᆮ',
    'ㄹ':'ᆯ','ㄺ':'ᆰ','ㄻ':'ᆱ','ㄼ':'ᆲ','ㄽ':'ᆳ','ㄾ':'ᆴ','ㄿ':'ᆵ',
    'ㅀ':'ᆶ','ㅁ':'ᆷ','ㅂ':'ᆸ','ㅄ':'ᆹ','ㅅ':'ᆺ','ㅆ':'ᆻ','ㅇ':'ᆼ',
    'ㅈ':'ᆽ','ㅊ':'ᆾ','ㅋ':'ᆿ','ㅌ':'ᇀ','ㅍ':'ᇁ','ㅎ':'ᇂ',
}


def batch(keys_list, layout):
    if not keys_list:
        return []
    sim = SIM_OVERRIDE or SIM
    r = subprocess.run([str(sim), "--data", str(DATA), "--layout", layout, "--batch"],
                       input="\n".join(keys_list), capture_output=True, text=True,
                       encoding="utf-8", errors="replace", timeout=600)
    if r.returncode != 0:
        raise RuntimeError(f"sim --batch failed: {r.stderr.strip()[:300]}")
    return [json.loads(l) for l in r.stdout.splitlines() if l.strip()]


def jamo(s):
    """NFD-decompose syllables and map compat jamo to conjoining jamo."""
    s = unicodedata.normalize("NFD", s)
    return ''.join(_COMPAT[c] if c in _COMPAT else c for c in s)


# 중성 decisions where more than one physical key renders the vowel. Filled by
# _pick_jung and written into the table JSON, so the artefact carries the choice
# and its reason (see the twin-key rule there).
TWINS = {}


# A representative 초성 key (ㄱ = k) used to prime the buffer so a vowel key
# is emitted as a 중성, and a cho+jung (가 = kf) to prime for 종성. The 초성
# table is built first, so the prime uses whatever ㄱ maps to.
def _render_all(seqs, layout):
    """Render every candidate in ONE subprocess; return {seq: jamo-tuple}."""
    recs = batch(seqs, layout)
    return {rec["keys"]: tuple(jamo(rec["preedit"])) for rec in recs}


def _cho_probe_keys(layout):
    """Keys that form a real cho+jung syllable when followed by a vowel.

    A key qualifies as a 초성 only if key+vowel yields a 2-jamo syllable
    (cho in U+1100-115F). This rejects 종성-side keys that merely *render* a
    consonant on an empty buffer (g looks like ㄷ but g+v=ㅈ).
    """
    vowels = ["f", "r", "v"]  # ㅏ ㅓ ㅗ
    cands = [k + vk for k in ALL_KEYS for vk in vowels]
    out = {}
    for keys, d in _render_all(cands, layout).items():
        if len(d) == 2 and 0x1100 <= ord(d[0]) <= 0x115F and 0x1161 <= ord(d[1]) <= 0x11A7:
            out.setdefault(d[0], set()).add(keys[0])
    return out


def probe_cho(jamo_target, layout, _cache={}):
    """초성 key(s) for jamo_target.

    One key for a simple 초성. 된소리 (ㄲ,ㄸ,ㅃ,ㅆ,ㅉ) have no single key --
    they are the base 초성 key pressed twice (까 = kkf, ㄲ = k+k), so this
    also tries two keys and returns the pair. A key qualifies only if
    key(+key)+vowel forms a real cho+jung syllable, which rejects 종성-side
    keys that merely render a consonant in isolation.
    """
    if layout not in _cache:
        _cache[layout] = _cho_probe_keys(layout)
    t = jamo(jamo_target)
    one = _cache[layout].get(t, set())
    if one:
        return min(one)
    # 2-key: base 초성 (any key that forms a syllable) pressed twice, then a
    # vowel, must yield the target 된소리.
    vowels = ["f", "r", "v"]
    bases = set().union(*_cache[layout].values()) if _cache[layout] else set()
    cands = [b + b for b in bases]
    out = {}
    for keys, d in _render_all([c + vk for c in cands for vk in vowels], layout).items():
        if len(d) == 2 and d[0] == t and 0x1161 <= ord(d[1]) <= 0x11A7:
            out.setdefault(d[0], set()).add(keys[:-1])  # drop the vowel
    pair = out.get(t, set())
    return min(pair) if pair else None


def _pick_jung(cands, layout, cho_key, target, jamo_target):
    """Among keys that render the vowel, prefer the STANDALONE one.

    THE RULE (maintainer, 2026-11-04): a 중성 can have two keys and they are not
    interchangeable. EXACTLY THREE pairs exist on sin3-p2 — measured over all 47
    keys and their shifts, so this is not "the vowels I happened to check":

        ㅗ  `/` connecting (와)      |  `v` standalone
        ㅜ  `o` connecting (워)      |  `b` standalone
        ㅡ  `i` connecting (의 ㅢ)   |  `g` standalone

    (`p`/`z` for 아래아 is the separate 종성-admission gate, and every other
    duplicate is one key's own Shift.) The left-hand member builds a compound
    중성 with a following vowel; the right-hand one is what a running syllable
    takes. So 오 takes `v`, 우 takes `b`, 으 takes `g`.

    THE BUG THIS FIXES (measured, issue #227). Both members render the vowel
    from a ㄱ-primed buffer, so the "shortest, plainest" rule took `/` — and
    `/`+`f` is ㅏ, because a connecting ㅗ keeps composing vowels (ㅗ+ㅏ=ㅘ). The
    jong probe had found 종성 ㅍ on `f` using a ㄱ+ㅏ prime, so the two halves of
    the table disagreed about which ㅗ they meant: 노+`f` came out 놔 and 높 was
    reported untypeable, when 높 is ㄴ+`v`+`f`. Measured: `v`+`f` → 높,
    `/`+`f` → 놔; `b`+`r` → 웉 (jong), `o`+`r` → 워 (compound).

    Measured, not read off the table: a candidate is CONNECTING if some single
    key after cho+candidate yields cho + a DIFFERENT 중성 (a compound vowel) —
    that is what `/` does with `f` and `o` does with `r`. The candidate with
    fewest such extensions wins; ties fall back to plain, then short, then
    lexicographic.
    """
    probes, owner = [], []
    for s in cands:
        for x in ALL_KEYS:
            probes.append(cho_key + s + x)
            owner.append(s)
    rendered = _render_all(probes, layout)
    extensions = {s: 0 for s in cands}
    for seq, s in zip(probes, owner):
        d = rendered.get(seq)
        if d and len(d) == 2 and 0x1161 <= ord(d[1]) <= 0x11A7 and d[1] != target:
            extensions[s] += 1

    def rank(s):
        return (extensions[s], sum(c.isupper() for c in s), len(s), s)

    chosen = min(cands, key=rank)
    # Record the decision beside it when a 중성 has more than one physical key:
    # the twin pairs are the 세벌식-family fact this rule exists for (sin3-p2:
    # ㅗ /|v, ㅜ o|b, ㅡ i|g — and NOTHING else, measured 2026-11-04 over all 47
    # keys and their shifts; every other duplicate is one key's own shift, and
    # 아래아's p|z is the separate 종성-admission gate). A table written from this
    # probe must say which twin it took and why, or the next layout silently
    # takes the connecting form again.
    def base(c):      # '\\sv' and 'V' are the same physical key as 'v'
        return (c[2:] if c.startswith("\\s") else c).lower()
    bases = {base(c) for c in cands}
    if len(bases) > 1:
        TWINS[jamo_target] = {"candidates": {c: extensions[c] for c in sorted(cands)},
                              "chosen": chosen,
                              "rule": "fewest compound-vowel extensions (standalone over connecting)"}
    return chosen


def probe_jung(jamo_target, layout, cho_key):
    """중성 key: on a buffer primed with cho_key, the key that yields the vowel.

    One key, or two for a compound vowel (base vowel + second key: sin3-p2
    ㅘ=/f, ㅟ=od). All candidates render in one batch, so this is a few
    subprocess calls, not thousands.
    """
    target = jamo(jamo_target)
    for nkeys in (1, 2):
        seqs = ALL_KEYS if nkeys == 1 else [a + b for a in ALL_KEYS for b in ALL_KEYS]
        cands = []
        for keys, d in _render_all([cho_key + s for s in seqs], layout).items():
            s = keys[len(cho_key):]
            if len(d) == 2 and d[1] == target and 0x1100 <= ord(d[0]) <= 0x115F:
                cands.append(s)
        if cands:
            return _pick_jung(cands, layout, cho_key, target, jamo_target)
    return None


def probe_jong(jamo_target, layout, cho_key, jung_key):
    """종성 key(s): on cho+jung, one key for a simple jong, two for 겹받침.

    Returns the shortest keystroke that puts jamo_target in the jong slot
    (3-jamo syllable), plain preferred. 겹받침 (ㄶ, ㄳ, ...) need two keys and
    the engine composes them. All candidates render in one batch.
    """
    prime = cho_key + jung_key
    # 종성 target: same compat letter, different code point (ㅇ jong = ᆼ, not ᄋ)
    target = _JONG_COMPAT.get(jamo_target, jamo(jamo_target))
    for nkeys in (1, 2):
        seqs = ALL_KEYS if nkeys == 1 else [a + b for a in ALL_KEYS for b in ALL_KEYS]
        best = None
        for keys, d in _render_all([prime + s for s in seqs], layout).items():
            s = keys[len(prime):]
            if len(d) == 3 and d[2] == target and 0x11A8 <= ord(d[2]) <= 0x11FF:
                cand = (sum(c.isupper() for c in s), len(s), s)
                if best is None or cand < best:
                    best = cand
        if best:
            return best[2]
    return None


def build_table(layout):
    table = {"cho": {}, "jung": {}, "jong": {}}
    not_found = {"cho": [], "jung": [], "jong": []}
    for j in CHOSEONG + EXT_CHO:
        if j in table["cho"]:
            continue
        key = probe_cho(j, layout)
        if key:
            table["cho"][j] = key
        else:
            not_found["cho"].append(j)
    # prime with the ㄱ 초성 key for jung/jong probes
    cho_key = table["cho"].get("ㄱ") or (next(iter(table["cho"].values())) if table["cho"] else "k")
    for j in JUNGSEONG + EXT_JUNG:
        if j in table["jung"]:
            continue
        key = probe_jung(j, layout, cho_key)
        if key:
            table["jung"][j] = key
        else:
            not_found["jung"].append(j)
    # prime jong probes with a real cho+jung (ㄱ + ㅏ)
    jung_key = table["jung"].get("ㅏ") or "f"
    for j in JONGSEONG[1:] + EXT_JONG:
        if j in table["jong"]:
            continue
        key = probe_jong(j, layout, cho_key, jung_key)
        if key:
            table["jong"][j] = key
        else:
            not_found["jong"].append(j)
    return table, not_found


def type_text(text, table):
    """Type text cho -> jung -> jong per syllable, space for word breaks."""
    out = []
    for syl in text:
        if syl == " ":
            out.append(" ")
            continue
        cp = ord(syl)
        if 0xAC00 <= cp <= 0xD7A3:
            x = cp - 0xAC00
            cho = CHOSEONG[x // 588]
            jung = JUNGSEONG[(x % 588) // 28]
            jong = JONGSEONG[x % 28]
        elif 0x1100 <= cp <= 0x11FF:
            if cp <= 0x115F: cho, jung, jong = syl, "", ""
            elif cp <= 0x11A7: cho, jung, jong = "", syl, ""
            else: cho, jung, jong = "", "", syl
        else:
            out.append("?")
            continue
        for slot, j in [("cho", cho), ("jung", jung), ("jong", jong)]:
            if j and j in table[slot]:
                out.append(table[slot][j])
            elif j:
                out.append(f"?{j}?")
    return "".join(out)


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--layout", required=True)
    ap.add_argument("--text", default=None)
    ap.add_argument("--text-file", type=pathlib.Path)
    ap.add_argument("--table-out", type=pathlib.Path)
    args = ap.parse_args()

    table, not_found = build_table(args.layout)
    n = sum(len(v) for v in table.values())
    print(f"{args.layout}: table {n} jamo -> key "
          f"(not_found: cho={len(not_found['cho'])} jung={len(not_found['jung'])} "
          f"jong={len(not_found['jong'])})", file=sys.stderr)

    if args.table_out:
        args.table_out.write_text(json.dumps(
            {"layout": args.layout, "table": table, "not_found": not_found,
             "jung_twins": TWINS},
            ensure_ascii=False, indent=1), encoding="utf-8")

    if args.text_file:
        text = args.text_file.read_text(encoding="utf-8").strip()
    elif args.text:
        text = args.text
    else:
        return

    keys = type_text(text, table)
    print(f"keys: {keys}", file=sys.stderr)
    if "?" in keys:
        print("WARNING: some jamo had no key (see not_found)", file=sys.stderr)
    print(keys)


if __name__ == "__main__":
    sys.exit(main())
