# MODES-METRICS — Metrics for Hangul/English Guessing

Requirements: `docs/SPEC-MODES.md` R12. This document contains the definition of
R12 (the criterion by which keystroke count measures success) and the measurement
results.

Status: **some metric definitions fixed, no pass line, most measurements invalid.**

**You must read this document in full before citing any of its numbers.** This
file's history contains mutually contradictory numbers, and which side is invalid
is stated below.

---

# 1. What Is Measured

## 1.1 ① Dictionary size — MEASURED, finalized

The cost actually paid on the keystroke path. That is, the size of what gets
embedded and the size it occupies in memory.

```
krdict       31,253 forms    3.5 MB   배포 가능 (CC BY-SA 2.0 KR)
wikipedia   654,336 forms   ~10 MB 텍스트 / 1.06 GB RSS   배포 가능 (CC BY-SA 4.0)
624       12,588,072 forms   235 MB 텍스트                 배포 불가
```

624 census: 51,830 members, 28m18s, streaming, additional disk 0.

```
624-ko.txt        12,588,072 distinct hangul runs   235 MB 텍스트
624-en.txt          284,201 distinct english
624-en-mixed.txt      259,590 english tokens appearing near hangul
토큰               1,016,338,031 hangul / 17,291,787 english
```

While the wikipedia list alone held 654k forms, RSS exceeded 1.06 GB. So ①
answers in the unit the user actually pays: **the only things embeddable are
krdict and a wiki list with a cap; AIHub is used only as a measuring instrument.**
Whatever ②③ turn out to be, this conclusion does not change — every corpus
measured in this session is at least one order of magnitude too large to embed,
and the two large ones are unusable by license alone.

## 1.2 ② Sentence replay — **NOT MEASURED**

Type real sentences without a single switch and see whether the same sentence
comes out. Assuming a human found the error and fixed it, count the **keystroke
delta**.

## 1.3 ③ Unrepresentability — **NOT MEASURED**

> Being unable to input some word or some expression at all is a defect.

This is a **grading target of the spec** (SPEC-MODES §4). But the current decision
line is one the user set as follows: reverting to 100% of previous behavior when
it is off is an invariant and must be verified, and the frequency of being unable
to input when it is on should be low, but **we do not currently judge that to be
a defect.**

**This metric is a property of the engine, not only of the guesser.** A layout
that cannot produce some jamo combination makes every sentence containing that
jamo unplayable regardless of mode logic. The values to compute are: among corpus
sentences, (a) how many are inputtable at all in the layout, and (b) how many of
those the rules pass through unchanged.

## 1.4 ③-bis Wrong-direction switch — **NOT MEASURED**

This is the maintainer's third efficiency metric, stated 2026-10-03 and defined
2026-10-05. It was **absent from this document** while §1.3 answered a different
question in its slot: an audit on 2026-10-05 found the metric named in the
transcript had no definition and no table row anywhere.

> "잘못된 모드 전환이라는 표현이 이상한데, 나는 의도적으로 한글을 쳤는데, 그게
> 영어로 변환되거나, 그 반대인 것을 말해. … 그래서 한글을 한글로 인식하지 못했는데,
> 우연히 그게 영어로도 표현되는 거라서 변경된 사례. 그리고 그 반대의 경우도
> 발생하지 않도록 하자는 거야."
>
> *"The expression 'wrong mode switch' is odd — I mean that I deliberately typed
> Hangul and it got converted to English, or the reverse. … So: a case where
> Hangul was not recognised as Hangul and, because it happened also to be
> expressible in English, it got switched. And that the reverse must not happen
> either."*

**Read carefully, this is not a separate property from §1.3 — it is an error
direction, not a rate.** The policy is already decided (SPEC-MODES R8, `[D]`):

- If the text is valid as **both** Hangul and English, or as **neither**, the
  current mode is kept. That is the specified behaviour, **not a failure.**
- Only when the two disagree does a switch happen.
- So the metric is not "how often does a switch occur" but **"how often does a
  switch occur in the direction the user did not intend"** — a false-positive
  rate on the judgement itself, symmetric across both directions.

**Why it is nonetheless a distinct measurement.** R8's third branch (keep the
current mode) is what *prevents* this, and that branch is correct as written. But
a rule being correct on paper and a rule never producing the error are different
claims, and only one of them is testable. `안녕ㅎ` — neither a Hangul word nor an
English word — is the maintainer's own worked example of a case that must be
**kept** in Hangul mode, and R8 says keeping it is correct. The metric asks how
often the implementation fails to keep it.

**The concrete failure both directions:**

| typed as | recognised as | consequence |
|---|---|---|
| Hangul | `hangul_valid` false, `english_valid` true | switched to English |
| English | `english_valid` false, `hangul_valid` true | switched to Hangul |

**How it would be measured** (not yet done, and it needs the same judgement
source R8 does — which is still open, see `docs/MODES-DATA.md` §4-7c): replay
corpus sentences typed in one script, and count the switches whose direction
contradicts the script actually typed. Reported as a **rate over both
directions**, never as a switch count, because a switch count rewards switching
seldom and this metric is not about that.

---

# 2. Pass Line — none [E]

R12 said "it is enough if the keystroke count goes down," and there is no **how
much**. Even when the implementation is finished, grading is impossible. This
corresponds to SPEC-MODES E4, and the figure is undetermined.

## Manual intervention count — the second metric [E]

R7-correction (2026-10-04) adds one more grading criterion.

> Since the user can deliberately input awkward words, if we can reduce the work
> of manually turning this feature off and on, that is the better direction.

If the keystroke count measures "how advantageous it is when the feature is on,"
then the **manual on/off count** measures "whether this feature is actually
usable." If the hand that comes later gets tired of turning it on and off and
soon turns it off, then the feature is a failure even if the keystroke metric
looks good.

So grading is two values:

| Metric | Measures | Pass line |
|---|---|---|
| ② Sentence replay | Keystroke delta of feature ON vs OFF | undetermined |
| **②b Manual intervention** | Number of times a human pressed on/off during replay | undetermined |

**Neither distinguishes direction** — Hangul→English mistyping and
|English→Hangul mistyping are counted as the same number (R7-correction). The
direction distinction was a rule that existed only before 2026-10-04.

**That is true of ② and ②b, and NOT of ③-bis.** The maintainer defined the third
efficiency metric on 2026-10-05 as an error *direction*
(`docs/MODES-METRICS.md` §1.4): "I deliberately typed Hangul and it got converted
to English, **or the reverse** … and the reverse must not happen either." So the
grading table has three rows, not two-plus-a-different-third:

| Metric | Measures | Pass line |
|---|---|---|
| ② Sentence replay | Keystroke delta of feature ON vs OFF | undetermined |
| **②b Manual intervention** | Number of times a human pressed on/off during replay | undetermined |
| **③-bis Wrong-direction switch** | Switches whose direction contradicts the script typed, as a **rate over both directions** | undetermined |

**③-bis is not measurable until the judgement source is** — the same open
question as `docs/MODES-DATA.md` §4-7c, since "which Hangul is valid" has no
settled answer inside the krdict scope boundary. Until then it is a defined
metric with no number, which is different from the metric being undefined: an
audit on 2026-10-05 found it had **no definition and no row at all**, sitting
behind a §1.3 that answered a different question in its slot.

---

# 3. Numbers Declared Invalid

**The numbers in this section must not be cited.**

**②③ are not measurements.** All numbers cited earlier in this file (88.6% /
95.7% conversion, 0/150 COST) came from a corpus I constructed myself, and after
being contradicted four times in the body text they were recreated in prose.
**void.** 

**The loss of the ④ strip rules also contaminates the follow-on numbers** —
MODES-DATA.md §4. The EFFECT/COST figures inherit that loss.

### Harnesses that were unfit (all returned "confident zeros")

| Harness | Defect |looked like |
|---|---|---|
| `count-corpus.py` | `len(obj) > 40` discards all short JSON bodies | All JSON counted as 0 |
| `count-corpus.py` | `obj.get("data", [])` discards other schemas | Records disappear |
| `metric-2-3.py` | Passes Hangul to `hunmin-sim --keys` (which requires keyboard keys) | Empty commit |
| `measure.py` | conjoining (U+1100) vs compatibility (U+3131) jamo | All None |
| `measure.py` | jungseong range ends at `JUNG+9` (actually `+20`) | Missing ㅜㅛㅠㅡㅣ |
| debug probe | `hunmin_ic_commit` is a **destructive** read; read per key | 사람 → 람 |

**Every failure in this file has the same shape: a plausible-looking number
produced by a tool that cannot fail loudly.**

### Filter table (unusable — because of §3)

```
filter          runs     EFFECT    COST
raw 7.85M    7,850,834    88.7%   84.5%
len>=2       7,839,312    88.7%   84.5%   ← 2자 필터는 비용 없다
len>=3       7,653,438   100.0%   50.7%   ← 3자 필터가 COST를 반으로
len>=4       6,268,792   100.0%   21.8%
len>=2 top200k  200,000  100.0%   80.0%   ← 실용적 선택지
```

The `len>=2` cap of 200k is written as 2.5 MB, EFFECT 100%, COST 80% — but this
figure is **a re-measurement target and has no basis, because the procedure that
produced the filter table below is invalid.** It must be re-measured with the
conclusion that `len>=2 top200k` is the "practical choice" at its center.

`len>=4` dropping to COST 21.8% is the only power-saving step in the table that
points in the right direction: **length filters are the direction of the work**
(SPEC-MODES E3).

### Keystroke inversion rules — obtained by brute force

`hunmin_ic_process` takes US-QWERTY characters (header: "case encodes shift,
0x08 is backspace") — therefore a Hangul word must be turned back into a
keystroke sequence. Those facts were obtained by brute-forcing the engine (not
by reading a table):

```
가→rk   나→sk   사→tk                      초성/중성: 예상대로
사+t→살   사+t+T→ㄳㅆ   사+tt→삿             쌍받침은 키를 두 번
rnR→굮   rnrR→국ㄲ                        시프트는 자음 쪽
```

The inversion in `measure.py` now produces the correct keys (`사람` → `tkfka`,
verified with `hunmin-sim`), **but with ctypes the same keys return `람` while
the CLI returns `사람`.** One unexplained difference remains in the receive
path. That `hunmin_ic_commit` is a destructive read, so reading per key, was
part of that difference, and reporting "the receive path is broken" without
comparing the two paths with the same keys was mistake number 14.

**Recommendation:** do not build more new harnesses; use the **already existing
and verified X11 e2e path** (`tests/e2e/`, the dkit client) — real key input
works there and there is no inversion. If that cannot be done, add a test-only
entry point to libhunmin that receives finished text. Neither re-derives the
combination rules the engine has already implemented.

---

# 4. References

- Requirements: `docs/SPEC-MODES.md` R12, §4
- Corpus/license: `docs/MODES-DATA.md`
- tier structure: `docs/TESTING.md`
