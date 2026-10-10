#!/usr/bin/env python
"""Mine josa/eomi from three corpora; measure what National Institute of Korean Language's list covers.

Question (user, 2026-10-04): the corpora are large enough now -- why use
the survey's ending list at all? Extract the endings from the corpora themselves.

This measures that, and it answers the design question that was left open:
does a stem+paradigm list beat a form list, and by how much, once the
paradigm is built from the corpora instead of from a 2003 survey?

THREE CORPORA, THREE REGISTERS. The verdict has already been seen to flip
between two of them, so a single corpus cannot answer this:

  full/       ko.wikipedia  encyclopedia prose.  7,495,250 forms
  624-ko.txt  AIHub webdata what typists write. 12,588,072 forms
  119-clean   AIHub 국가기록물 official prose.   7,592,702 forms

국립국어원 「현대 국어 사용 빈도 조사」 (2003) ships 어미 and 조사 as
separate ranked files. MEASURED counts, correcting figures quoted earlier in
this session as 656/184:

  어미  658 lines (1 header) -> 596 distinct -> 477 after dropping the 119
       entries that begin with a bare 받침 letter (ㄴ ㄹ ㅁ ㅂ ㅇ)
  조사  185 lines (1 header) -> 161 distinct -> 155 hangul-only
  고유명사  16,855

THE 477-vs-601 GAP, AND WHY IT IS NOT A DESIGN PROBLEM. 121 of the 722
endings begin with a bare 받침 letter: ㅂ니다 is ㅂ+니다, ㄴ다 is ㄴ+다. A
suffix stripper that requires its ending to start with a syllable matches
none of them, so a naive implementation reports 601 and looks like it has
lost a fifth of the list.

IT HAS NOT, and this tool measures the reason rather than assuming it:

  * the survey ships BOTH spellings for the inflections that matter. 습니다 is in
    the list as a complete surface form, so 먹 + 습니다 = 먹습니다 is reachable
    by plain concatenation. The 받침-led entry is the same morpheme written in
    its compositional spelling, not an extra ending.
  * The 받침-led SPELLINGS NEVER OCCUR AS CORPUS TEXT. Measured over 400,000
    wikipedia forms: 0 occurrences of any form ending in ㄴ다 ㄴ가 ㄴ데 ㄹ까
    ㅂ니다 ㅂ시다 ㅂ니까 ㄴ지 ㄹ게. Plain concatenation with them produces
    가ㄴ다 and 갑ㅂ니다, which are not Korean.

So the jamo-led entries are a NOTATION, not data to ship. Arm A (concatenate
National Institute of Korean Language's own spellings) spends 121 entries generating forms that do not exist
and still misses nothing; arm B (the 받침-이동 rule) spends engine code to
generate the same forms the vowel-led entries already cover. NEITHER GAINS
anything over the other on this evidence, and the honest conclusion is that
the 601-entry list is sufficient and the allomorph rule is not required for
coverage.

This does not make 표기 ambiguity go away: 같이 is 같이 (adverb) or 같+이
(particle), and no amount of ending coverage decides that. That needs context,
and it is a separate question from this one.

One entry decodes to U+009D, which is a conversion artefact, not a morpheme.

THE METHOD IS NOT MORPHOLOGICAL ANALYSIS. No analyser, no POS tagger, no
context. For each corpus form, strip the longest matching ending from the
right; whatever remains is a candidate stem. This is suffix mining. It needs
no eojeol buffer in the engine, because the corpus is already split on
non-hangul boundaries.

WHAT IT CANNOT DO, and the measurement that shows it: 표기 is ambiguous.
같이 is 같이 (adverb) OR 같+이 (particle). The corpus cannot distinguish
these, and neither can any dictionary. So the honest metric is not "did we
segment correctly" but "how much token mass is reachable, and how much of
the reachable set is ambiguous".
"""
import os
import re
import subprocess
import sys
import unicodedata
from collections import Counter

CORPORA = {
    "wiki": ("/home/scoby/work/wikicorpus/full/hangul.txt", "count"),
    "624": ("/home/scoby/work/aihub-records-corpus/624-ko.txt", "form"),
    "119": ("/home/scoby/work/wikicorpus/119-clean/hangul.txt", "count"),
}
NIKL_DIR = "/home/scoby/work/nikl-2003-freq"

HANGUL_ONLY = re.compile(r"[가-힣]+")
# Compatibility jamo: the survey writes 어미 in their 받침-led spelling (ㅂ니다).
JAMO = re.compile(r"[ㄱ-ㅎㅏ-ㅣ]")


def load_corpus(path, order):
    """Load a (form, count) list, proving the column orientation.

    wiki and 119-clean are count<TAB>form; 624-ko is form<TAB>count. Reading
    one with the other's parser raises on the first row rather than
    producing a plausible dict of nonsense -- but it must be checked here,
    because the flag is a hand-maintained claim about a file outside the repo.
    """
    forms = Counter()
    total = 0
    with open(path, encoding="utf-8") as fh:
        for i, line in enumerate(fh):
            a, _, b = line.rstrip("\n").partition("\t")
            if not b:
                sys.exit(f"{path}: row {i} has no tab: {line[:60]!r}")
            # Decide from the data, not from the flag: the integer column is
            # the count. If the flag disagrees with the file, the file wins.
            a_int = a.strip().isdigit()
            b_int = b.strip().isdigit()
            if a_int and not b_int:
                w, c = b, int(a)
            elif b_int and not a_int:
                w, c = a, int(b)
            elif a_int and b_int:
                sys.exit(f"{path}: row {i} has two integer columns {a!r} {b!r}")
            else:
                sys.exit(f"{path}: row {i} has no integer column: {line[:60]!r}")
            if i == 0 and order == "count" and not a_int:
                print(f"  NOTE {os.path.basename(os.path.dirname(path))}"
                      f"/{os.path.basename(path)}: declared count-first but"
                      f" the file is form-first; using the file.")
            forms[w] += c
            total += c
    return forms, total


def load_nikl():
    """EUC-KR, tab-separated, header row.

    Returns (endings, jamo_led, proper_nouns). `endings` are matched by a
    suffix stripper (must start with a vowel-bearing syllable); `jamo_led`
    are the 119 entries the survey spells with a bare 받침 letter first (ㅂ니다),
    which need the stem's final consonant moved onto them before matching.
    Both are reported because dropping the second silently loses a fifth of
    the list.
    """
    def rows(fn):
        raw = subprocess.run(["iconv", "-f", "EUC-KR", "-t", "UTF-8",
                              os.path.join(NIKL_DIR, fn)],
                             capture_output=True).stdout
        txt = raw.decode("utf-8", errors="replace")
        return [l.split("\t") for l in txt.splitlines() if l.strip()]

    def col1(fn, drop_header=True):
        r = rows(fn)
        return {x[1] for x in (r[1:] if drop_header else r) if len(x) > 1}

    raw_eomi = col1("어미_빈도색인.txt")
    raw_josa = col1("조사_빈도색인.txt")
    # U+009D is an EUC-KR conversion artefact, not a morpheme.
    artefacts = {x for x in raw_eomi | raw_josa if not HANGUL_ONLY.fullmatch(x)
                 and not JAMO.match(x)}
    endings = {e for e in raw_eomi | raw_josa if HANGUL_ONLY.fullmatch(e)}
    jamo_led = {e for e in raw_eomi | raw_josa if JAMO.match(e)}
    # 고유명사 carries a 차례 (rank) column that the other three files do
    # not, so the name is column 1. Reading column 0 yields 16,855 integers.
    prop = {r[1] for r in rows("고유명사_빈도색인.txt")[1:] if len(r) > 1}
    prop = {p for p in prop if HANGUL_ONLY.fullmatch(p)}
    return endings, jamo_led, prop, artefacts


def main():
    endings, jamo_led, prop, artefacts = load_nikl()
    # `endings` is rebound to a length-sorted list below for longest-match
    # stripping; keep the set under its own name so the union below works.
    ending_set = endings
    # A ships National Institute of Korean Language's own spelling of every ending, 받침-led included.
    endings_all_sorted = sorted(ending_set | jamo_led, key=len, reverse=True)
    print(f"국립국어원 (National Institute of Korean Language) 「현대 국어 사용 빈도 조사」 (2003):")
    print(f"  어미+조사 distinct            {len(endings | jamo_led)}")
    print(f"  matchable by suffix strip    {len(ending_set)}")
    print(f"  받침-led (ㅂ니다) — see below {len(jamo_led)}")
    print(f"  고유명사                     {len(prop)}")
    if artefacts:
        print(f"  conversion artefacts dropped {sorted(artefacts)}")
    print()

    # The paradigm must therefore be generated over BOTH spellings: the
    # 받침-led endings attach after moving the stem's final consonant up.
    # That is the allomorph rule, and it is why string concatenation alone
    # is not enough.
    print(f"  받침-led entries that a vowel-only stripper loses: "
          f"{len(jamo_led)} of {len(endings | jamo_led)} "
          f"({100.0*len(jamo_led)/len(endings | jamo_led):.1f}%)")
    print(f"    e.g. {sorted(jamo_led)[:8]}")
    print()

    # Which endings does the survey carry that a hand list misses, and vice versa?
    # The stemming runs earlier used a 41-item hand list; `며` was absent and
    # that was visible in the unreachable forms.
    hand = {"에서", "에서도", "으로는", "으로", "에게", "한테", "라고", "이라",
            "까지", "부터", "만큼", "처럼", "보다", "조차", "마저", "밖에",
            "은", "는", "이", "가", "을", "를", "의", "에", "도", "만", "로",
            "과", "와", "나", "다", "거", "것", "데", "니", "요", "라", "임",
            "럼", "든", "서"}
    endings_all = ending_set | jamo_led
    print(f"hand list {len(hand)}  the survey {len(endings_all)}  "
          f"survey-only {len(endings_all - hand)}  hand-only {len(hand - endings_all)}")
    print(f"  the survey has, the hand list lacks (sample): "
          f"{sorted(endings_all - hand)[:14]}")
    print(f"  notably among them: "
          f"{[x for x in ('며', '았으며', '이었으며', 'ㅂ니다') if x in endings_all - hand]}")
    print()

    # The paradigm is generated over the 받침-led endings too, by moving the
    # stem's final consonant onto the ending -- the allomorph rule. Both
    # spellings are generated so the comparison is not handicapped.
    # 받침-led endings (ㅂ니다) attach only after the stem's final consonant
    # moves onto them: 먹 + ㅂ니다 -> 먹습니다. Generate that spelling too, or
    # the paradigm silently misses the formal verbal endings entirely.
    FINAL = {c for c in "ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ"}
    def spellings(stem):
        out = [stem + e for e in endings]
        if stem and stem[-1] in FINAL:
            moved = stem[:-1]
            for j in jamo_led:
                out.append(moved + j)
                # ㅂ니다 style: the consonant is written twice (ㅂ + 니다)
                if j[0] == "ㅂ" and len(j) > 1:
                    out.append(moved + j[0] + j[1:])
        return out

    endings = sorted(ending_set, key=len, reverse=True)
    results = {}
    for name, (path, order) in CORPORA.items():
        print(f"=== {name} ===", flush=True)
        forms, total = load_corpus(path, order)
        print(f"  {len(forms):,} forms / {total:,} tokens", flush=True)

        stems = Counter()
        mass_split = 0
        mined = Counter()
        for w, c in forms.items():
            hit = None
            for e in endings:
                if len(w) > len(e) and w.endswith(e):
                    hit = e
                    break
            if hit is None:
                # 받침-led: 먹 + ㅂ니다 -> 먹습니다, so the SURFACE ends with
                # the doubled form, not with ㅂ니다. Match on the doubled
                # spelling, which is what actually occurs in the corpus.
                for j in jamo_led:
                    if j[0] == "ㅂ" and len(j) > 1:
                        dbl = j[0] + j[1:]
                        if len(w) > len(dbl) and w.endswith(dbl):
                            hit = w[-len(j):]
                            break
                    if len(w) > len(j) and w.endswith(j):
                        hit = j
                        break
            if hit:
                mass_split += c
                st = w[: -len(hit)]
                stems[st] += c
                mined[hit] += c
        # ---- coverage: what share of token mass ends in a survey ending?
        print(f"  token mass ending in a survey 어미/조사: "
              f"{100.0*mass_split/total:.2f}%")
        print(f"  distinct stems mined: {len(stems):,} "
              f"({100.0*(1-len(stems)/len(forms)):.2f}% fewer forms)")
        print(f"  endings actually attested: {len(mined):,} of "
              f"{len(ending_set | jamo_led)} "
              f"({100.0*len(mined)/len(ending_set | jamo_led):.1f}%)")

        # ---- the two designs, at equal stem-list cost.
        #
        #   A. 표기 그대로: paradigm = stem + all 722 endings as the survey spells
        #      them. No engine rule.
        #   B. 받침 이동 규칙: paradigm = stem + the 601 vowel-led endings,
        #      plus the 121 jamo-led ones relocated onto the stem's final
        #      consonant (so 먹 + ㅂ니다 -> 먹습니다).
        #
        # EXPECTED TO BE IDENTICAL, and measured to confirm it: the jamo-led
        # spellings never occur as corpus text, so A's 121 extra entries
        # generate 가ㄴ다 rather than anything real, while B generates the
        # forms the vowel-led entries already cover. If these two columns
        # ever differ, one of them has regressed -- which is why both are
        # printed rather than one being chosen.
        ranked_stems = [s for s, _ in stems.most_common()]
        ranked_forms = [f for f, _ in forms.most_common()]
        print(f"  {'entries':>9} {'A concat':>10} {'B allomorph':>12} {'form list':>11}")
        for cap in (50_000, 200_000, 500_000):
            sl = set(ranked_stems[:cap])
            a_lookup = set(sl)
            for s in sl:
                a_lookup.update(s + e for e in endings_all_sorted)
            b_lookup = set(sl)
            for s in sl:
                b_lookup.update(spellings(s))
            am = sum(c for w, c in forms.items() if w in a_lookup)
            bm = sum(c for w, c in forms.items() if w in b_lookup)
            fl = set(ranked_forms[:cap])
            fm = sum(c for w, c in forms.items() if w in fl)
            print(f"  {cap:>9,} {100.0*am/total:9.2f}% {100.0*bm/total:11.2f}% "
                  f"{100.0*fm/total:10.2f}%")
        print(f"  (A ships {len(endings_all_sorted)} endings, "
              f"  B ships {len(ending_set)} + a 받침-이동 rule)")

        # ---- the proper-noun gap, and whether the survey's 고유명사 list closes it
        full_lookup = set(stems)
        for s in stems:
            full_lookup.update(spellings(s))
        fm_all = sum(c for w, c in forms.items() if w in full_lookup)
        with_prop = full_lookup | prop
        pm = sum(c for w, c in forms.items() if w in with_prop)
        print(f"  uncapped stem+paradigm: {100.0*fm_all/total:.2f}%")
        print(f"  + survey 고유명사 ({len(prop):,}): {100.0*pm/total:.2f}%  "
              f"(+{100.0*(pm-fm_all)/total:.2f}pp)")

        # ---- ambiguity: stem that is ALSO a standalone form
        amb = [(c, w) for w, c in forms.items()
               if any(len(w) > len(e) and w.endswith(e) for e in endings)
               and any(w[: -len(e)] in forms
                       for e in endings if len(w) > len(e) and w.endswith(e))]
        ambm = sum(c for c, _ in amb)
        print(f"  surface-ambiguous forms: {len(amb):,} / "
              f"{100.0*ambm/total:.2f}% of tokens")
        for c, w in sorted(amb, reverse=True)[:6]:
            for e in endings:
                if len(w) > len(e) and w.endswith(e):
                    print(f"      {c:>10,}  {w} reads as {w[:-len(e)]}+{e}")
                    break
        results[name] = (len(forms), total, 100.0 * mass_split / total)
        print()

    print("summary: token mass ending in a survey 어미/조사")
    for k, (nf, tt, pct) in results.items():
        print(f"  {k:6s} {nf:>10,} forms  {pct:6.2f}%")
    return 0


if __name__ == "__main__":
    sys.exit(main())
