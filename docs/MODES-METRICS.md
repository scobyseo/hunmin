# MODES-METRICS — Metrics for Hangul/English Guessing

Requirements: `docs/SPEC-MODES.md` ReqMode12. This document contains the definition of
ReqMode12 (the criterion by which keystroke count measures success) and the measurement
results.

Status: **definitions fixed and measurable — the harness exists and has run
(2026-11-04); no pass line (E4, board B2); one corpus measured, and what it does
not represent is stated with it.**

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

## 1.2 ② Sentence replay — **MEASURED 2026-11-04**

Type real sentences without a single switch and see whether the same sentence
comes out. Assuming a human found the error and fixed it, count the **keystroke
delta**.

**The harness is `tools/modes-metrics.py`.** Its corpus is an argument, its run
is re-runnable, and the model that prices a run is inside the output rather than
in a reader's head:

```sh
# 1. a corpus, with provenance written beside it (the dump stays outside the repo)
tools/build-modes-sentences.py --dump <ko.wikipedia XML> --pages 2500 \
    --limit 1000 --out <corpus>.jsonl
# 2. the measurement, one JSON line per sentence plus totals
tools/modes-metrics.py --corpus <corpus>.jsonl --json-out <run>.jsonl
```

**The corpus behind the figures below** is `ko.wikipedia` (CC BY-SA 4.0): the
first 2,500 namespace-0 pages in dump order, 1,000 sentences that each switch
script at least once, 40,241 characters. It is a *lower bound on the target
population*, not a sample of it — `docs/MODES-DATA.md` §4 records that
encyclopedia prose mixes scripts far less than typed messages do, and this corpus
is unusually dense anyway (one transition per 19 characters). What it is not is
a corpus built in a session: every drop is counted in its `meta.json`, and the
source's sha256 is in it.

**The model**, printed into every run's JSON so a number cannot travel without it:

- a eojeol is typed with the Hangul layout's own keys (`tools/jamo-keytable.py`,
  which probes the engine per jamo — §3's failure mode was a hand-derived table),
  and the punctuation attached to it with `probe_literals()`, which probes the
  same way for the characters a jamo table cannot answer for; a Latin eojeol is
  its own characters; one space after each;
- off: `autoswitch 0`, 한/영 pressed by the replayer at each script transition;
- on: `autoswitch 1`, no presses, the judgement read after every key and the
  switch performed exactly as `tools/sim.c`'s `judge_step()` does;
- a eojeol that arrives in the wrong script is a **misfire** and costs, per
  `docs/OPTIONS-MEASURED.md` §PAS-1, backspaces (one per delivered character),
  one 한/영 press to reach the script that was typed, and the eojeol's own keys
  to retype it. Repairs are counted, not replayed: the mode is corrected so later
  judgements start where a person would be, and the retyped text is the OFF arm's
  proof that the eojeol is typeable in its own script.

**What it measured** (run 2026-11-04, `hunmin-sim` and `libhunmin.so` at the
session's HEAD; every 10th sentence cross-checked against `hunmin-sim --judge`,
99 compared, 0 mismatches):

| quantity | value |
|---|---|
| sentences graded / not inputtable | 1,000 / 0 of 1,000 |
| characters / transitions / `d` | 40,241 / 2,126 / 0.0528 |
| ② keys typed with the feature off | 76,956 |
| ② keys typed with it on | 86,868 |
| ② delta | **−9,912 keys, −12.88% of off** |
| sentences that needed no repair at all | 208 of 1,000 (§1.3(b)) |
| misfires and their cost | 1,090; `k` = 4.71 backspaces each |
| ②b `u` (manual interventions per transition) | 51.27% |
| `q` off the replay, `q` from §PAS-1's formula | −466.23%, −485.61% |
| ③-bis | 0 of 1,036 switches (0.00%), both directions |

**On this corpus the feature as shipped costs keystrokes rather than saving
them**, and the mechanisms are visible in the per-sentence lines rather than
inferred:

- at a transition the current mode is by definition the previous eojeol's, so a
  word that is not switched arrives in the wrong script — there is no third
  outcome: of 2,126 transitions, 1,036 switched and 1,090 did not, and every one
  of the 1,090 arrived wrong;
- 282 of those 1,090 are below ReqMode9's three-key guard (268 Hangul→Latin, 14
  Latin→Hangul) and are never judged at all — `M.`, `TV`, `가`;
- of the 808 at or above the guard, **every** abstention is a branch of ReqMode8 rather
  than a skipped evaluation, and reading which branch is how the text gets lost:
  a Hangul eojeol whose key sequence spells something valid as neither script
  (`2.5cm`, `Bosna`) or as both (`내쭈`), a Latin eojeol the shipped list does not
  carry, and a Hangul eojeol carrying punctuation, which `hangul_valid` rejects
  outright (`절제되었다.`), so the Latin→Hangul line cannot fire on it. The one
  that would matter most and cannot be seen from this corpus: an English word
  missing from the list abstains as "not English" and reads exactly like a word
  that is genuinely both.

Because `d` is a property of the corpus and the repair dominates, the same
implementation's overall saving moves by two orders of magnitude between this
corpus and a broad one (§PAS-1: `d ≈ 2.6·10⁻⁴` on the 624 corpus, where the
ceiling for a *perfect* implementation is 0.03%). **A pass line stated as a
percentage of all keystrokes therefore still cannot be ruled from this run** —
what this run supplies is the shape the §PAS-2 row needs: `u = 51.27%`,
`d = 0.0528`, `k = 4.71`, and a `q` that is negative on this corpus.

## 1.3 ③ Unrepresentability — **PARTLY MEASURED 2026-11-04**

> Being unable to input some word or some expression at all is a defect.

This is a **grading target of the spec**, and the spec states what it grades as
two requirements of different kinds (`docs/SPEC-MODES.md` §ReqMode14 and §ReqMode15): the off
side and the fallback are checked, while **the frequency of being unable to input
when it is on is not judged a defect** and has no pass line in E4. What this
section measures is that frequency — ③ is the only handle on it.

**This metric is a property of the engine, not only of the guesser.** A layout
that cannot produce some jamo combination makes every sentence containing that
jamo unplayable regardless of mode logic. The values to compute are: among corpus
sentences, (a) how many are inputtable at all in the layout, and (b) how many of
those the rules pass through unchanged.

**Measured 2026-11-04, on the §1.2 corpus and the same run:**

| quantity | value |
|---|---|
| (a) sentences the layout can type as written | 1,000 of 1,000 |
| (b) of those, sentences the rules pass through unchanged | 208 of 1,000 |

**(a) was 969 of 1,000 when this harness first ran, and that number was the
tool's error, not the layout's.** The replay typed a punctuation character as
its own ASCII key, and sin3-p2 does not put `; " '` there — it types them as
Shift+P / Shift+K / Shift+J, while the `; " '` keys themselves emit jamo. Thirty
sentences were therefore counted "untypeable" that are typeable. The fix is
`probe_literals()` in `tools/modes-metrics.py`: the same probe the jamo table
uses, over the printable keys, accepting a key only when it delivers the
character *and* leaves nothing composing (`;` commits `'` but leaves `ㅂ`
composing, so it is not the literal). All 1,000 sentences now round-trip through
the OFF arm, and the lesson is the one §3 has been teaching all along: a mapping
derived from the alphabet instead of from the engine is a mapping that is wrong.

(b) is the feature's answer, and it is the number that matters for ③: 792 of
1,000 sentences changed the text the person got, and §1.2 counts what repairing
each change cost. **The frequency of being unable to input when the feature is
on is therefore measurable now**; whether it is a defect and what pass line it
gets is still E4's, not this section's.

## 1.4 ③-bis Wrong-direction switch — **MEASURED 2026-11-04**

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
direction, not a rate.** The policy is already decided (SPEC-MODES ReqMode8, `[D]`):

- If the text is valid as **both** Hangul and English, or as **neither**, the
  current mode is kept. That is the specified behaviour, **not a failure.**
- Only when the two disagree does a switch happen.
- So the metric is not "how often does a switch occur" but **"how often does a
  switch occur in the direction the user did not intend"** — a false-positive
  rate on the judgement itself, symmetric across both directions.

**Why it is nonetheless a distinct measurement.** ReqMode8's third branch (keep the
current mode) is what *prevents* this, and that branch is correct as written. But
a rule being correct on paper and a rule never producing the error are different
claims, and only one of them is testable. `안녕ㅎ` — neither a Hangul word nor an
English word — is the maintainer's own worked example of a case that must be
**kept** in Hangul mode, and ReqMode8 says keeping it is correct. The metric asks how
often the implementation fails to keep it.

**The concrete failure both directions:**

| typed as | recognised as | consequence |
|---|---|---|
| Hangul | `hangul_valid` false, `english_valid` true | switched to English |
| English | `english_valid` false, `hangul_valid` true | switched to Hangul |

**How it is measured.** Replay corpus sentences and count the switches whose
direction contradicts the script actually typed, as a **rate over switches in
both directions**, never as a switch count — a switch count rewards switching
seldom, and this metric is not about that. `tools/modes-metrics.py` does exactly
that: at each script transition it records whether a switch fired, whether it
landed on the script that was typed, and whether the text arrived correct, and
reports `wrong_direction_rate` with both directions broken out.

**Measured 2026-11-04** (the §1.2 corpus and run): **0 of 1,036 switches, 0.00%,
in both directions** — 0 of 311 Hangul→Latin switches and 0 of 725 Latin→Hangul
switches landed on the wrong script. The metric is a rate over switches, and the
denominator is what makes this reading honest: 1,036 switches fired and every one
went the way the person typed. The failure this metric exists to catch did not
occur on this corpus — which is a measurement, not a clearance, because the
corpus is one genre and 282 of its 1,090 unswitched eojeol were never judged at
all (ReqMode9's guard, §1.2), so they could not have produced a wrong-direction switch
either way.

---

# 2. Pass Line — none [E]

The candidate shapes are `docs/OPTIONS-MEASURED.md` §PAS-2 (②) and §PAS-3
(②b); the units they are stated in are §PAS-1's, with `d = t/n` the corpus's
**transition density**.

ReqMode12 said "it is enough if the keystroke count goes down," and there is no **how
much**. Even when the implementation is finished, grading is impossible. This
corresponds to SPEC-MODES E4, and the figure is undetermined.

## Manual intervention count — the second metric [E]

ReqMode7-correction (2026-10-04) adds one more grading criterion.

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
|English→Hangul mistyping are counted as the same number (ReqMode7-correction). The
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

**③-bis is measured now** (§1.4): the judgement source it waited on retired with
B1 on 2026-11-01 and the judgement itself landed 2026-11-03, so what was missing
was only the harness (#181), which exists as of 2026-11-04. The paragraph this
replaces said the metric had "no definition and no row at all" until an audit on
2026-10-05 found it behind a §1.3 that answered a different question in its slot;
that finding stands, and the row now has a number and still no pass line.

## What one run supplies for each candidate shape

The pass line is still the maintainer's (§PAS-4 lists the six sub-decisions a
bare number leaves open). What the §1.2 run supplies is the reading each candidate
shape would grade, so a ruling can be stated against a measured quantity instead
of a formula:

| shape (§PAS-2/§PAS-3) | this run's reading |
|---|---|
| K1, percentage of all keystrokes | **−12.88%** (a loss, not a saving) |
| K2, share of the pool `q` | **−466.23%** off the replay; −485.61% from §PAS-1's formula |
| K3, absolute per 1,000 characters | **−246.3 keys** per 1,000 characters |
| K4, no net loss (`q ≥ 0`) | **not met** on this corpus |
| T2, manual presses per 10 sentences | **10.90** |
| T3, `u ≤ X%` | **51.27%** |

`d = 0.0528` and `k = 4.71` are the two corpus/word parameters these readings
depend on; §PAS-4 item 1 (which corpus) and item 3 (what `k` is) are therefore
still open, and a pass line ruled without them can be met by changing the corpus
rather than the implementation.

The rows above are the §1.2 run restated in the units each shape grades — §1.2 is
the home, and a new run replaces both tables in the same change.

---

# 3. Numbers Declared Invalid

**The numbers in this section must not be cited.**

**②③ are not measurements.** All numbers cited earlier in this file (88.6% /
95.7% conversion, 0/150 COST) came from a corpus I constructed myself, and after
being contradicted four times in the body text they were recreated in prose.
**void.** 

**The loss of the ④ strip rules also contaminates the follow-on numbers** —
MODES-DATA.md §4. The EFFECT/COST figures inherit that loss.

## 3. Harness design (moved, #272 D4)

The harness lessons, keystroke-inversion notes, and "what was built instead"
design rationale were folded into
[`DESIGN-MODES.md` §4 — Measurement and harness design](DESIGN-MODES.md#5-measurement-and-harness-design-folded-from-modes-metrics-272-d4)
per issue #272 (D4). Measured figures and metric readings remain in this
document's §1–§2.


# 4. References

- Requirements: `docs/SPEC-MODES.md` ReqMode12, ReqMode15
- Corpus/license: `docs/MODES-DATA.md`
- tier structure: `docs/TESTING.md`
- The harness: `tools/modes-metrics.py` (ctest `modes-metrics-selfcheck` guards
  it); the corpus builder: `tools/build-modes-sentences.py`; the jamo key table it
  types with: `tools/jamo-keytable.py`
- The keystroke arithmetic a pass line is stated in: `docs/OPTIONS-MEASURED.md`
  §PAS-1, and the candidate shapes in §PAS-2/§PAS-3
