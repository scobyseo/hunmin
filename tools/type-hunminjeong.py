#!/usr/bin/env python3
"""Type the 훈민정음 언해본 preamble through sin3-p2-yet, by engine measurement.

Not a table-lookup: sin3-p2-yet's 옛한글 input has slot rules a static
jamo->key map cannot hold. MEASURED here against hunmin-sim:

  * 초성 key is judged with a vowel attached (k+f=가, not k alone): ㄷ is
    reachable from both u and g in isolation, but only u+v=도 -- the 초성
    key is paired with the vowel that follows it.
  * 중성 key is probed after a 초성 (ㄱ+키). 대부분 context-free, EXCEPT
    아래아 ᆞ: with no 종성 it is `p`, with a 종성 it is `z` -- the `z`
    form admits a final consonant, the `p` form absorbs the next key into
    a vowel combination (ip+z=ᄆᆢ, iz+z=ᄆᆞᆷ).
  * 종성 is a distinct code point (ㅇ jong=ᆼ U+11BC, not 초성 ᄋ), so it
    is mapped via _JONG_COMPAT, not NFKD (which collapses it to 초성).
  * compose-only 자모 (ᅙ=ㄱ+ㅎ, ᄠ=ㅂ+ㄷ, ᄫ=ㅂ+ㅇ, ᄯ=ㅅ+ㄷ) are the two
    constituent 초성 keys in sequence; the engine composes them.

Each 슬롯's key is probed once (batched), then syllables are assembled
초->중->종 and verified against the engine. Reports per-어절 match.
"""
import json
import subprocess
import sys
import unicodedata
import pathlib

REPO = pathlib.Path(__file__).resolve().parent.parent
SIM = REPO / "build" / "hunmin-sim"
DATA = REPO / "libhunmin" / "data"
LAYOUT = "sin3-p2-yet"
ALL = [chr(c) for c in range(0x20, 0x7f) if chr(c) not in ('"', '\\')]


def batch(kl):
    r = subprocess.run([str(SIM), "--data", str(DATA), "--layout", LAYOUT, "--batch"],
                       input="\n".join(kl), capture_output=True, text=True)
    return [json.loads(l) for l in r.stdout.splitlines() if l.strip()]


def canon(s):
    return ''.join(unicodedata.normalize("NFKD", c) for c in s)


# 종성 compat jamo -> conjoining jong (U+11A8). Explicit: NFKD collapses
# these to the 초성 code point, which never matches a probed 종성 key.
_JONG_COMPAT = {'ㄱ':'ᆨ','ㄲ':'ᆩ','ㄳ':'ᆪ','ㄴ':'ᆫ','ㄵ':'ᆬ','ㄶ':'ᆭ','ㄷ':'ᆮ',
    'ㄹ':'ᆯ','ㄺ':'ᆰ','ㄻ':'ᆱ','ㄼ':'ᆲ','ㄽ':'ᆳ','ㄾ':'ᆴ','ㄿ':'ᆵ','ㅀ':'ᆶ','ㅁ':'ᆷ',
    'ㅂ':'ᆸ','ㅄ':'ᆹ','ㅅ':'ᆺ','ㅆ':'ᆻ','ㅇ':'ᆼ','ㅈ':'ᆽ','ㅊ':'ᆾ','ㅋ':'ᆿ','ㅌ':'ᇀ',
    'ㅍ':'ᇁ','ㅎ':'ᇂ'}
_CO = ['ㄱ','ㄲ','ㄴ','ㄷ','ㄸ','ㄹ','ㅁ','ㅂ','ㅃ','ㅅ','ㅆ','ㅇ','ㅈ','ㅉ','ㅊ','ㅋ','ㅌ','ㅍ','ㅎ']
_JU = ['ㅏ','ㅐ','ㅑ','ㅒ','ㅓ','ㅔ','ㅕ','ㅖ','ㅗ','ㅘ','ㅙ','ㅚ','ㅛ','ㅜ','ㅝ','ㅞ','ㅟ','ㅠ','ㅡ','ㅢ','ㅣ']
_JO = ['','ㄱ','ㄲ','ㄳ','ㄴ','ㄵ','ㄶ','ㄷ','ㄹ','ㄺ','ㄻ','ㄼ','ㄽ','ㄾ','ㄿ','ㅀ','ㅁ','ㅂ','ㅄ','ㅅ','ㅆ','ㅇ','ㅈ','ㅊ','ㅋ','ㅌ','ㅍ','ㅎ']


def probe_slots():
    """Probe 초/중/종 key tables from the engine, once, batched."""
    cho = {}
    for rec in batch([k + "f" for k in ALL]):  # key+ㅏ -> 초성+ㅏ
        d = list(canon(rec["preedit"]))
        if len(d) == 2 and d[1] == "ᅡ" and 0x1100 <= ord(d[0]) <= 0x115F:
            cho.setdefault(d[0], rec["keys"][0])
    jung = {}
    for rec in batch(["k" + k for k in ALL]):  # ㄱ+key -> ㄱ+중성
        d = list(canon(rec["preedit"]))
        if len(d) == 2 and d[0] == "ᄀ" and 0x1161 <= ord(d[1]) <= 0x11A7:
            jung.setdefault(d[1], rec["keys"][1:])
    jong = {}
    for rec in batch(["kf" + k for k in ALL]):  # 가+key -> 종성
        d = list(canon(rec["preedit"]))
        if len(d) == 3 and d[2] not in jong:
            jong[d[2]] = rec["keys"][2:]
    return cho, jung, jong


def split_syllables(s):
    """Tokens: precomposed syllable | 초/중/종 jamo run | space | other."""
    toks, cur = [], []
    def flush():
        nonlocal cur
        if cur:
            toks.append(''.join(cur)); cur = []
    for c in s:
        cp = ord(c)
        if c == ' ':
            flush(); toks.append(' ')
        elif 0xAC00 <= cp <= 0xD7A3:
            flush(); cur = [c]
        elif 0x1100 <= cp <= 0x115F:
            flush(); cur = [c]
        elif 0x1161 <= cp <= 0x11A7 or 0x11A8 <= cp <= 0x11FF:
            cur.append(c)
        else:
            flush(); toks.append(c)
    flush()
    return toks


def slots_of(tok):
    """(cho, jung, jong) conjoining jamo. 완성형 decomposed; jamo run grouped."""
    if 0xAC00 <= ord(tok[0]) <= 0xD7A3:
        x = ord(tok[0]) - 0xAC00
        cho = unicodedata.normalize("NFKD", _CO[x // 588])
        jung = unicodedata.normalize("NFKD", _JU[(x % 588) // 28])
        jc = _JO[x % 28]
        jong = _JONG_COMPAT.get(jc, '') if jc else ''
        return cho, jung, jong
    cho = jung = jong = ''
    for ch in tok:
        cp = ord(ch)
        if cp <= 0x115F: cho += ch
        elif cp <= 0x11A7: jung += ch
        else: jong += ch
    return cho, jung, jong


def build(cho, jung, jong, C, J, G, COMPOSE, JG):
    """Assemble keys. 아래아: `z` when a 종성 follows, else `p`.

    A compose-only 자모 (no single 초성/종성 key -- pat.im 옛한글) is two
    constituent keys in sequence; the engine composes them. 초성-only 자모
    go through COMPOSE; 종성-only 옛 자모 through JG (e.g. ᇰ = a+f, where a
    is the ㅇ jong and f the ᇁ jong -- two 종성 keys, the engine composes
    the 겹받침).
    """
    if cho in COMPOSE:
        return COMPOSE[cho] + (J.get(jung, '') if jung else '') + \
               (JG.get(jong, G.get(jong, '')) if jong else '')
    ck = C.get(cho, '')
    if jung == 'ᆞ':
        jk = 'z' if jong else 'p'
    else:
        jk = J.get(jung, '')
    gk = (JG.get(jong, G.get(jong, '')) if jong else '')
    return ck + jk + gk


# compose-only 자모 -> two constituent 초성 keys (pat.im/1136 §2-4 옛한글).
# Built lazily in main() once the 초성 table is probed.
def build_compose(C, G):
    c = lambda j: C.get(j, '')
    comp = {
        'ᄊ': c('ᄉ') + c('ᄉ'),   # ㅅ+ㅅ (쌍시옷)
        'ᄍ': c('ᄌ') + c('ᄉ'),   # ㅈ+ㅅ (쌍지읒)
        'ᄠ': c('ᄇ') + c('ᄃ'),   # ㅂ+ㄷ
        'ᄫ': c('ᄇ') + c('ᄋ'),   # ㅂ+ㅇ
        'ᄯ': c('ᄉ') + c('ᄃ'),   # ㅅ+ㄷ
        'ᅙ': c('ᄀ') + c('ᄒ'),   # ㄱ+ㅎ
        'ᅇ': c('ᄆ') + c('ᄒ'),   # ㅁ+ㅎ
    }
    return {k: v for k, v in comp.items() if v and '?' not in v and v}


# 종성 겹받침 -> two 종성 keys (engine composes). MEASURED on 듀(ug)+키키,
# each verified to land d[2] on the 현대 code point (not an 옛 합성). These
# pairs are recorded verbatim because the constituent-jamo lookup (g('ㄴ')+
# g('ㅎ')) gives the WRONG pair in context -- e.g. ㄶ is sd, not hm; the
# h+키 form composes onto a stray 채움. Key ORDER matters (w+e=현대 ㄼ,
# q+b=옛 ퟰ). Verified: ug+<pair> renders 드+<겹받침> byte-exact.
_JONG_GEUP = {
    'ᆩ': 'cc', 'ᆪ': 'cq', 'ᆬ': 'sv', 'ᆭ': 'sd', 'ᆰ': 'wc', 'ᆱ': 'wz',
    'ᆲ': 'we', 'ᆳ': 'wq', 'ᆴ': 'wr', 'ᆵ': 'wf', 'ᆶ': 'wd', 'ᆹ': 'eq',
    # 옛 final: ᆼ(ㅇ jong = a) + ᇁ(ㄹ jong = f) -> ᇰ (U+11F0), 듀ᇰ = uaaf
    'ᇰ': 'af',
}


def build_jong_compose(G):
    return dict(_JONG_GEUP)


def trace_events(keys):
    """Trace an 어절's keystrokes (with a trailing space to flush the last
    syllable), yielding the golden event dicts."""
    r = subprocess.run([str(SIM), "--data", str(DATA), "--layout", LAYOUT,
                        "--keys", keys + " ", "--trace"], capture_output=True, text=True)
    events = []
    for line in r.stdout.splitlines():
        if line.strip():
            o = json.loads(line)
            events.append({"in": o["in"], "commit": o.get("commit", ""),
                           "preedit": o.get("preedit", "")})
    return events


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--text-file", type=pathlib.Path, required=True)
    ap.add_argument("--slots-out", type=pathlib.Path)
    ap.add_argument("--golden-out", type=pathlib.Path,
                    help="write a golden JSONL of the 어절 that type byte-exact, "
                         "with full provenance in the meta line")
    args = ap.parse_args()

    C, J, G = probe_slots()
    COMPOSE = build_compose(C, G)
    JG = build_jong_compose(G)
    if args.slots_out:
        args.slots_out.write_text(json.dumps({"cho": C, "jung": J, "jong": G},
                                              ensure_ascii=False, indent=1))
    text = args.text_file.read_text(encoding="utf-8").strip()
    ok = tot = 0
    golden_events = []
    for w in text.split(" "):
        keys = ''.join(build(*slots_of(t), C, J, G, COMPOSE, JG)
                       for t in split_syllables(w) if t != ' ')
        rec = batch([keys + " "])[0]
        out = canon(rec["commit"] + rec["preedit"])
        tot += 1
        if out == canon(w):
            ok += 1
            if args.golden_out:
                # trace the matching 어절 so the golden holds every event
                for ev in trace_events(keys):
                    golden_events.append(ev)
        else:
            print(f"MISS {w}\n  keys={keys}\n  got ={out}\n  want={canon(w)}",
                  file=sys.stderr)
    print(f"어절 {ok}/{tot}")
    if args.golden_out and golden_events:
        meta = {
            "source": "훈민정음 언해본 세종 서문 (월인석보 1459)",
            "source_url": "https://m.blog.naver.com/pso164/222527368887",
            "layout": LAYOUT,
            "tool": "tools/type-hunminjeong.py",
            "method": "engine-measured per-slot probe (초성+vowel judge, "
                      "아래아 p/z gate, distinct 종성 code point, 현대 겹받침 "
                      "key pairs); 어절 that type byte-exact are traced to events",
            "coverage": f"{ok}/{tot} 어절 byte-exact; the rest need the compose "
                        "chain across a syllable boundary (see the tool's TODO)",
            "reproduce": "python3 tools/type-hunminjeong.py --text-file <원문> --golden-out <this file>",
        }
        args.golden_out.write_text(
            json.dumps({"meta": meta}, ensure_ascii=False) + "\n" +
            "\n".join(json.dumps(e, ensure_ascii=False) for e in golden_events) + "\n")
        print(f"golden -> {args.golden_out} ({len(golden_events)} events)", file=sys.stderr)
    return 0 if ok == tot else 1


if __name__ == "__main__":
    sys.exit(main())
