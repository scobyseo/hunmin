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


def batch(kl, layout=None):
    """Run keystroke walks through one layout (`layout` defaults to LAYOUT).

    The parameter exists so tools/gen-coverage-golden.py can measure ANY layout
    with this probe instead of owning a second, slower one (measured: this probe
    does the 훈민정음 eight lines in 0.5 s; the BFS in tools/sin3p2-ksgen.py needs
    minutes per 세벌식 layout).
    """
    r = subprocess.run([str(SIM), "--data", str(DATA), "--layout", layout or LAYOUT,
                        "--batch"], input="\n".join(kl), capture_output=True, text=True)
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


_FILLER = ("\u115f", "\u1160")


def _strip_filler(s):
    """Drop the blank-filler jamo the engine pads an isolated slot with.

    MEASURED: `sin3-p2-yet` `af` renders U+115F U+1160 U+11B0. Without this the
    probe's length checks never see a two-jamo 초성, which is why 만든 compose-only
    letters (ᄡ, ᄫ, ᅙ, ᅇ) were missing from every table the tool built.
    """
    for f in _FILLER:
        s = s.replace(f, "")
    return s


def probe_slots(layout=None, g=None, a=None):
    """Probe 초/중/종 key tables from the engine, once, batched.

    A compose-only 초성 is TWO 초성 keys and renders as two jamo before the
    vowel (`nnf` -> ᄉ+ᄉ+ᅡ in sin3-p2-yet), so the probe keeps the pairs it
    sees instead of discarding anything that is not length 2.

    `g`/`a` are the context keys (초성 ㄱ, 중성 ㅏ). They are ARGUMENTS because
    the 세벌식 family puts 초성 and 종성 on separate physical sides and no single
    literal works everywhere: `k` is ㄱ on sin3-p2-yet and ㅏ on dubeolsik, so a
    hardcoded context measures a vowel as a consonant (measured: 10 of 67 slots
    on dubeolsik). The caller picks them by SCORE -- see
    tools/gen-coverage-golden.py -- and falls back to discovery here.
    """
    g_key, a_key = g, a
    if g_key is None or a_key is None:
        g_key, a_key = "k", "f"          # this probe's home layout
        rec = batch([g_key + a_key], layout)[0]
        d = list(_strip_filler(canon(rec["preedit"])))
        if not (len(d) == 2 and d[0] == "ᄀ" and d[1] == "ᅡ"):
            for x in ALL:
                for y in ALL:
                    rec = batch([x + y], layout)[0]
                    d = list(_strip_filler(canon(rec["preedit"])))
                    if len(d) == 2 and d[0] == "ᄀ" and d[1] == "ᅡ":
                        g_key, a_key = x, y
                        break
                else:
                    continue
                break
    cho, cho_pair = {}, {}
    for rec in batch([k + a_key for k in ALL], layout):  # key+ㅏ -> 초성+ㅏ
        d = list(_strip_filler(canon(rec["preedit"])))
        if not d or d[-1] != "ᅡ":
            continue
        pre = d[:-1]
        if len(pre) == 1 and 0x1100 <= ord(pre[0]) <= 0x115F:
            cho.setdefault(pre[0], rec["keys"][0])
        elif len(pre) == 2 and all(0x1100 <= ord(c) <= 0x115F for c in pre):
            cho_pair.setdefault("".join(pre), rec["keys"][:2])
    # A 된소리 is TWO 초성 keys in the 세벌식 family (its shift row is the 종성
    # side: measured on sebeolsik-final, `Rf` renders ㅏ, nothing composes). So the
    # second pass presses 초성 keys in pairs and keeps the composed letter.
    cho_compose = {}
    for c1 in sorted(set(cho.values())):
        for c2, rec in zip(ALL, batch([c1 + c2 + a_key for c2 in ALL], layout)):
            d = list(_strip_filler(canon(rec["preedit"])))
            if len(d) == 2 and d[-1] == "ᅡ" and 0x1100 <= ord(d[0]) <= 0x115F:
                if d[0] not in cho:
                    cho_compose.setdefault(d[0], c1 + c2)
    jung, jung_pair = {}, {}
    for rec in batch([g_key + k for k in ALL], layout):  # ㄱ+key -> ㄱ+중성
        d = list(_strip_filler(canon(rec["preedit"])))
        if not d or d[0] != "ᄀ":
            continue
        if len(d) == 2 and 0x1161 <= ord(d[1]) <= 0x11A7:
            jung.setdefault(d[1], rec["keys"][1:])
    # A compound 중성 is TWO 중성 keys in sequence (measured: `kb` 규 + `kd` 기
    # gives 귀, so ᅱ is b+d). Without this pass every compound vowel came out
    # as its first constituent -- `귁` typed as `게`.
    pairs = [(c1, c2) for c1 in ALL for c2 in ALL]
    for (c1, c2), rec in zip(pairs, batch([g_key + a + b for a, b in pairs], layout)):
        d = list(_strip_filler(canon(rec["preedit"])))
        if len(d) == 2 and d[0] == "ᄀ" and 0x1161 <= ord(d[1]) <= 0x11A7:
            jung_pair.setdefault(d[1], c1 + c2)
    jong, jong_pair = {}, {}
    for rec in batch([g_key + a_key + k for k in ALL], layout):  # 가+key -> 종성
        d = list(_strip_filler(canon(rec["preedit"])))
        if len(d) < 3 or d[0] != "ᄀ" or d[1] != "ᅡ":
            continue
        rest = d[2:]
        if len(rest) == 1 and rest[0] not in jong:
            jong[rest[0]] = rec["keys"][2:]
        elif len(rest) == 2 and rest[0] in jong and rest[1] in jong:
            jong_pair.setdefault("".join(rest), rec["keys"][2:])
    # A 종성 key depends on the 중성 before it: 'a' gives ᆼ after 가 and a 중성
    # after ᄍᆞ (measured). So the table is (jung, jong) -> key, probed per 중성.
    jong_ctx = {}
    for jamo, jkeys in list(jung.items()) + list(jung_pair.items()):
        # 아래아 has two keys and the 종성 decides which: `p` with no final
        # consonant, `z` with one (type-hunminjeong.py's own docstring). Probing
        # the `p` form would measure a state the text never uses.
        probe_jk = "z" if jamo == "ᆞ" else jkeys
        for k, rec in zip(ALL, batch([g_key + probe_jk + c for c in ALL], layout)):
            d = list(_strip_filler(canon(rec["preedit"])))
            if len(d) == 3 and d[0] == "ᄀ" and d[1] == jamo:
                jong_ctx.setdefault((jamo, d[2]), k)
    return cho, jung, jong, cho_pair, jung_pair, jong_pair, jong_ctx, cho_compose


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


def build(cho, jung, jong, C, J, G, COMPOSE, JG, JJ=None, JC=None, CH=None):
    """Assemble keys. 아래아: `z` when a 종성 follows, else `p`.

    A compose-only 자모 (no single 초성/종성 key -- pat.im 옛한글) is two
    constituent keys in sequence; the engine composes them. 초성-only 자모
    go through COMPOSE; 종성-only 옛 자모 through JG (e.g. ᇰ = a+f, where a
    is the ㅇ jong and f the ᇁ jong -- two 종성 keys, the engine composes
    the 겹받침).
    """
    # The compose-only 초성 path must run through the SAME 중성/종성 logic below:
    # it used to concatenate J/JG directly, which dropped the 아래아 p/z choice
    # and the (jung, jong) 종성 table (measured: `ᄍᆞᆼ` typed as `ᄍᆞᅲ` although
    # `llz`+`a` renders ᄍᆞᆼ).
    ck = COMPOSE.get(cho) or C.get(cho) or (CH or {}).get(cho, '')
    if jung == 'ᆞ':
        jk = 'z' if jong else 'p'
    else:
        # A compound 중성 has no single key: it is two 중성 keys in sequence
        # (measured: ᅱ = b+d on sin3-p2-yet).
        jk = J.get(jung) or (JJ or {}).get(jung, '')
    # A single probed 종성 key wins over the two-key pair. This does NOT by
    # itself make `ᄍᆞᆼ` compose: the same key 'a' gives ᆼ after 가 and a 중성
    # after ᄍᆞ (measured), so a 종성 key is state-dependent on the 중성 that
    # precedes it and one slot table cannot hold it. That is the next gap.
    gk = (((JC or {}).get((jung, jong)) or G.get(jong) or JG.get(jong, ''))
          if jong else '')
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
    # MEASURED over the layout's own keys (2026-11-04): the constituent-pair
    # guesses above hold for the letters they were written for, but the engine's
    # pairs for these are different, and a wrong pair renders a different letter
    # (`ᄍ` came out `ᄼ`, `ᄡ` came out `ᄎ`). These are the shortest key pairs that
    # actually compose each letter in 초성 position, recorded verbatim.
    measured = {'ᄍ': 'll', 'ᄫ': ';j', 'ᅇ': 'jj', 'ᅙ': 'km', 'ᄡ': 'nn'}
    comp.update({k: v for k, v in measured.items() if all(ch in ALL for ch in v)})
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
    ap.add_argument("--accept-engine", action="store_true",
                    help="record the ENGINE's rendering as the expectation even "
                         "where it differs from the text (issue #239: an 옛 "
                         "spelling comes back precomposed, so byte-exact cannot "
                         "pass; the divergence is listed in the meta instead)")
    args = ap.parse_args()

    C, J, G, C2, J2, G2, JC, CH = probe_slots()
    COMPOSE = build_compose(C, G)
    JG = dict(build_jong_compose(G))
    # A compose-only 종성 the probe saw as two 종성 jamo (ᇰ = ㅇ+ᇁ) carries its
    # own pair, measured; the hardcoded table is the fallback for the ones the
    # probe reaches as a single code point.
    for pair, keys in G2.items():
        JG.setdefault(pair[-1], keys)
    if args.slots_out:
        args.slots_out.write_text(json.dumps({"cho": C, "jung": J, "jong": G},
                                              ensure_ascii=False, indent=1))
    raw = args.text_file.read_text(encoding="utf-8")
    # '#' lines are provenance, not input: without this the header was typed too
    # (measured: the first run's divergences were "#", "언해본", "(월인석보,").
    text = " ".join(line for line in raw.splitlines()
                    if line.strip() and not line.lstrip().startswith("#"))
    ok = tot = 0
    golden_events, diverged, literals = [], [], set()
    for w in text.split(" "):
        # Only Hangul has a keystroke. A 한자 (中, 國), the 언해's '·'/'：' and any
        # other character are delivered by the CLIENT, not the engine, so they
        # must not enter the key stream: typing them produced no keys and every
        # later syllable attached to the previous one (measured: the 13
        # "divergences" were all letters lost this way, not an encoding
        # difference -- `百ᄇᆡᆨ姓셩이` typed as 베셩이).
        def hangul(ch):
            return 0xAC00 <= ord(ch) <= 0xD7A3 or 0x1100 <= ord(ch) <= 0x11FF

        target = "".join(ch for ch in w if hangul(ch))
        literals.update(ch for ch in w if not hangul(ch))
        if not target:
            tot += 1
            continue
        w = target
        keys = ''.join(build(*slots_of(t), C, J, G, COMPOSE, JG, J2, JC, CH)
                       for t in split_syllables(w) if t != ' ')
        rec = batch([keys + " "])[0]
        out = canon(rec["commit"] + rec["preedit"])
        tot += 1
        same = out == canon(w)
        if same:
            ok += 1
        elif args.accept_engine:
            # Option A (issue #239): the engine renders an 옛 spelling precomposed,
            # so the golden's expectation is what the engine emits and the
            # original orthography rides along in the meta. The event stream still
            # asserts every key, which is the half a key-event golden can assert.
            diverged.append({"text": w, "engine": out, "keys": keys})
        else:
            print(f"MISS {w}\n  keys={keys}\n  got ={out}\n  want={canon(w)}",
                  file=sys.stderr)
            continue
        if args.golden_out:
            for ev in trace_events(keys):
                golden_events.append(ev)
    print(f"어절 {ok}/{tot}" + (f" ({len(diverged)} 엔진 렌더로 기록)" if diverged else ""))
    if args.golden_out and golden_events:
        meta = {
            "source": "훈민정음 언해본 세종 서문 (월인석보 1459)",
            "source_url": "https://m.blog.naver.com/pso164/222527368887",
            "layout": LAYOUT,
            "tool": "tools/type-hunminjeong.py",
            "method": "engine-measured per-slot probe (초성+vowel judge, "
                      "아래아 p/z gate, distinct 종성 code point, 현대 겹받침 "
                      "key pairs); 어절 that type byte-exact are traced to events",
            "coverage": f"{tot} 어절 typed, {ok} byte-exact"
                        + (f"; {len(diverged)} recorded as the ENGINE's rendering "
                           "because an 옛 spelling comes back precomposed "
                           "(issue #239)" if diverged else ""),
            "client_delivered": "".join(sorted(literals)) or None,
            **({"divergences": diverged} if diverged else {}),
            "reproduce": "python3 tools/type-hunminjeong.py --text-file <원문> --golden-out <this file>",
        }
        args.golden_out.write_text(
            json.dumps({"meta": meta}, ensure_ascii=False) + "\n" +
            "\n".join(json.dumps(e, ensure_ascii=False) for e in golden_events) + "\n")
        print(f"golden -> {args.golden_out} ({len(golden_events)} events)", file=sys.stderr)
    return 0 if ok == tot else 1


if __name__ == "__main__":
    sys.exit(main())
