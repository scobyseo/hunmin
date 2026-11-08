# MODES-MEASUREMENT — Measuring the hangul/latin judgement

Requirements: `docs/SPEC-MODES.md` ReqMode12, ReqMode14 and ReqMode15. This
file is the home for **how the judgement is measured and what a fresh
measurement must preserve**. It is not a record of past figures.

Status: **metric definitions fixed; no pass line (E4, board B2); no current
measurement.** The 2026-10 and 2026-11 readings, the corpus scale tables and the
50,000-cap derivation were removed on 2026-11-08 together with
`docs/MODES-DATA.md` and `docs/MODES-METRICS.md`. Most of that material described
a corpus word list that ReqMode8 retired — `english_valid` is now an English
dictionary lookup and `hangul_valid` a syllable-structure test — and the figures
no longer describe anything this tree reproduces. Their history is in git and in
the wiki record; §6 states what the removal does not settle.

---

# 1. What is measured

## 1.1 ① Dictionary size

The cost actually paid on the keystroke path: the size of the data embedded for
the judgement and the memory it occupies while loaded. This is a property of the
shipped `libhunmin/data/modes/en-US.json` (`tools/en-us2data.py`), not of a
corpus kept outside the repository.

## 1.2 ② Sentence replay

Type real sentences without a single switch and see whether the same sentence
comes out. Assume a person found each wrong word and fixed it; count the
**keystroke delta** between the feature off and on.

The measured quantities, in the units a pass line can be stated in (§2.1):

- keys typed with the feature off and with it on;
- `d = t/n`, the corpus's transition density (script transitions per
  character);
- `k`, the mean cost of repairing one misfired eojeol;
- `u`, manual 한/영 interventions per transition (②b).

A pass line must name the corpus, `d` and `k`; otherwise it can be met by
changing the corpus rather than the implementation.

## 1.3 ③ Unrepresentability

> Being unable to input some word or some expression at all is a defect.

The metric is a property of the engine as well as the guesser: a layout that
cannot produce some jamo combination makes every sentence containing it
unplayable regardless of mode logic. Compute, among corpus sentences:

- (a) how many are inputtable at all in the layout (the replay must reproduce
  every sentence with the feature off);
- (b) of those, how many the rules pass through unchanged.

(b) is the feature's answer. The frequency of being unable to input when the
feature is **on** is measurable; whether it is a defect and what pass line it
gets is E4's, not this section's.

## 1.4 ③-bis Wrong-direction switch

> "잘못된 모드 전환이라는 표현이 이상한데, 나는 의도적으로 한글을 쳤는데, 그게
> 영어로 변환되거나, 그 반대인 것을 말해. … 그리고 그 반대의 경우도 발생하지
> 않도록 하자는 거야."

This is an error **direction**, not a separate switch rate. The policy is
already fixed (`docs/SPEC-MODES.md` ReqMode8): if a buffer is valid as both
scripts or as neither, keep the current mode — that is specified behaviour, not
a failure. A switch happens only when the two validities disagree. So the metric
is **how often a switch occurs in the direction the user did not intend**, as a
rate over switches in both directions — never as a raw switch count, which would
reward switching seldom.

At each script transition record whether a switch fired, whether it landed on
the script that was typed, and whether the text arrived correct. Report
`wrong_direction_rate` with both directions broken out.

---

# 2. Pass line — none [E]

ReqMode12 says success is "enough if the keystroke count goes down," and there is
no **how much**. This section fixes the units a pass line can be stated in and the
shapes it can take; it sets no number, and it carries no reading. Board row B2
holds the ruling.

## 2.1 The quantities

For one text: `n` characters, `t` script transitions (a point where the script
changes — exactly the number of 한/영 presses the same text needs by hand), and
`d = t/n`, the corpus's **transition density**. Per transition the feature has
three outcomes:

| outcome | share | keys the person spends |
|---|---|---|
| switches correctly | `a` | 0 |
| misfires — wrong word committed | `r` | `2k+1` (backspace `k`, 한/영 once, retype `k`) |
| does not switch | `s = 1-a-r` | 1 (한/영) |

`k` is the mean length of the word a misfire costs; `u = 1-a` is the
manual-toggle share of the pool (the ②b metric). Feature **off** is `s = 1`:
`n + t` keys. Feature **on** is `n + t·(s + r(2k+1))`. Hence

```
q  = pool share captured   = a - (2k+1)r = 1 - u - (2k+1)r
②  overall saving          = q · d/(1+d)
```

Two consequences that decide the shapes below:

- **A ruled pair `(q, u)` is a precision requirement on the judgement.**
  Inverting the line gives the misfire budget the ruling implies,
  `r ≤ (1-u-q)/(2k+1)` — the two numbers do not leave `r` free.
- **② has a ceiling that is a property of the text, not of the feature.** Even
  with `r = 0` and `u = 0`, the most any implementation can save is `t` keys
  against `n + t`, i.e. `d/(1+d)`:

| `d` (transitions per character) | 0.01 | 0.02 | 0.05 | 0.10 | 0.20 | 0.50 |
|---|---|---|---|---|---|---|
| ceiling on ②, `r=u=0` | 1.0% | 2.0% | 4.8% | 9.1% | 16.7% | 33.3% |

`d` is therefore not a free parameter the ruling may ignore: **the corpus is
unwritten**, and a number stated as a percentage of all keystrokes cannot be ruled
before the corpus is chosen. A number stated as a share of the pool can. Whatever
corpus a run uses, the run prints its own `d`, `t`, `k` and `u`; a figure that
travels without them is not comparable to anything.

## 2.2 ② — the shapes a keystroke pass line can take

| | shape | reads | what it costs, and where it fails |
|---|---|---|---|
| K1 | **percentage of all keystrokes**, e.g. "≥5% fewer" | `q·d/(1+d)` | Requires `d ≥ 0.053` at `q=1` — one transition per 19 characters. On a sparse corpus it is unsatisfiable by any implementation; on a dense one it is passed by a mediocre one. The number moves by an order of magnitude when the corpus changes |
| K2 | **share of the pool `q`**, e.g. "≥50% of the switching removed" | `a - (2k+1)r` | Corpus-invariant: the same number grades a dense corpus and a sparse one. Loses the "how big is the win overall" reading, which is `q·d/(1+d)` and can be printed beside it |
| K3 | **absolute**, e.g. "≥25 keystrokes per 1,000 characters" | `1000·d·q` | Comparable only across runs on the *same* corpus; means nothing as a quality statement until `d` is fixed |
| K4 | **no net loss** (`q ≥ 0`, i.e. `r ≤ (1-u)/(2k+1)`) | sign | Satisfiable by an implementation that is merely careful; grades the absence of harm, not the presence of the feature. Its value is as a *guard* beside a K2 line, not as the line |

Every shape carries the risk ReqMode11 states plainly — a person may fix a
misfire with backspace — so **the repair cost has to be inside the metric**, not
excluded from it. K1 and K3 put it nowhere near the number unless `d` is fixed; K2
does, because `q` is defined net of repair.

## 2.3 ②b — the shapes a manual-toggle cap can take

ReqMode7's words are about "turning the feature off and on"
(`docs/SPEC-MODES.md` §ReqMode7), so the strict reading counts **mode presses
only** — backspace repairs are ②'s business. That is a choice with a price: read
solely as mode presses, the metric reads well for an implementation that lets the
person repair constantly rather than toggle.

| | shape | reads | what it costs, and where it fails |
|---|---|---|---|
| T1 | **zero toggles per sentence** | `u = 0` | Binary, and it forbids the behaviour ReqMode7 explicitly permits (a person may deliberately type an awkward word). A single deliberate word for one sentence fails it |
| T2 | **rate cap**, e.g. "≤1 per 10 sentences" | `u·t/sentences` | Attainable and readable, but it hides `t`: one toggle per 10 sentences is excellent on dense text and impossible to beat on sparse text if `t` itself is small |
| T3 | **ratio to the manual baseline**, `u ≤ X%` | `u` | The same quantity as T2 divided by the corpus's own `t`; corpus-invariant for the same reason K2 is. Must be stated with the `k` it assumes, or ② and ②b can be ruled into a negative `r` |

T2 and T3 are the same quantity scaled by the corpus's own `t`, so they agree only
when `t` is fixed. The ruling has to say which is *binding*.

**The misfire budget a pair implies** — `r ≤ (1-u-q)/(2k+1)`:

| `q` \ `u` | `k=2` | `k=3` | `k=4` | `k=5` |
|---|---|---|---|---|
| `q=0`, `u=25%` | 15.0% | 10.7% | 8.3% | 6.8% |
| `q=25%`, `u=25%` | 10.0% | 7.1% | 5.6% | 4.5% |
| `q=40%`, `u=25%` | 7.0% | 5.0% | 3.9% | 3.2% |
| `q=50%`, `u=25%` | 5.0% | 3.6% | 2.8% | 2.3% |
| `q=50%`, `u=10%` | 8.0% | 5.7% | 4.4% | 3.6% |
| `q=75%`, `u=5%` | 4.0% | 2.9% | 2.2% | 1.8% |

Read across a row: the same saving at a longer repaired word needs more precision.
Read down a column: 25 percentage points of extra saving costs a few points of
misfire budget. Every cell is a requirement on the judgement and on the short-word
threshold, not on the harness.

## 2.4 What a ruling has to include

Each of these is a sub-decision a bare number leaves open, and each changes the
measured value:

1. **The corpus**, and whether it is the target population (typed messages where
   people mix scripts) or a broad one. `d` differs by orders of magnitude between
   them, and with it the meaning of a K1 line.
2. **Aggregation**: per sentence (mean of ratios) or corpus-wide (ratio of sums) —
   different numbers; the per-sentence mean is dominated by the many sentences
   with `t=0` or `t=1`.
3. **`k`**, the mean length of a repaired word: every budget above is per `k`.
4. **Whether a repair counts as a manual intervention** (②b) or only a mode press
   does.
5. **Which metric set the numbers cover.** B2 says three, E4 says two; ③-bis is the
   one that needs its own line.
6. **What happens on a corpus where the feature cannot fire** (no `en-US.json`
   present — B3's data gate). A metric that can be met by not switching needs the
   OFF-by-default state stated as a precondition, not silently averaged in.

## 2.5 Status

| Metric | Measures | Pass line |
|---|---|---|
| ② Sentence replay | keystroke delta of feature ON vs OFF | undetermined |
| **②b Manual intervention** | number of times a person pressed on/off during replay | undetermined |
| **③-bis Wrong-direction switch** | switches whose direction contradicts the script typed, as a rate over both directions | undetermined |

All three are the maintainer's to set (board B2). Neither ② nor ②b distinguishes
direction; ③-bis does, which is why it has its own row.

*The paper that priced these shapes against a corpus was withdrawn on 2026-11-08
(§6; the ruling's record is
`wiki/decisions/2026-11-08-the-retired-corpus-measurement-is-deleted.md`). The
algebra above is a property of the metric, not of that corpus.*

# 3. Reproduction

The corpus and the run are arguments; neither is hardcoded. A run writes its
provenance (source sha256, drops, the pricing model) beside its output, so a
figure cannot travel without the conditions that produced it.

```sh
# 1. a sentence corpus from a Korean Wikipedia dump (the dump stays outside the repo)
tools/build-modes-sentences.py --dump <ko.wikipedia XML> --pages 2500 \
    --limit 1000 --out <corpus>.jsonl

# 2. the measurement: one JSON line per sentence plus totals
tools/modes-metrics.py --corpus <corpus>.jsonl --json-out <run>.jsonl

# 3. the harness's own guards; also ctest modes-metrics-selfcheck
tools/modes-metrics.py --selfcheck
```

Corpus builders and probes kept from the earlier investigation. They are
reproduction tools, not evidence, and their output is not a current figure:

| tool | what it builds or measures |
|---|---|
| `tools/build-modes-sentences.py` | sentence corpus from a Wikipedia XML dump |
| `tools/build-modes-corpus.py` | word/run census from a dump |
| `tools/build-modes-624.py`, `tools/build-modes-119.py` | census of the AIHub corpora |
| `tools/modes-glued.py` | symbol-glued Hangul ratio, across corpora |
| `tools/modes-cap.py` | coverage by cap size |
| `tools/modes-dict-probe.py` | dictionary-vs-corpus coverage |
| `tools/modes-metrics.py` | the ② / ②b / ③-bis harness |
| `tools/jamo-keytable.py` | engine-probed jamo → key table the harness types with |

---

# 4. Corpus sources and licences

Bulk corpora live outside the repository. Never hardcode their paths into a
committed tool; take the path as an argument or derive it from the script's own
location. Each artefact carries the `meta.json` written by the tool that
produced it.

| source | licence | distributable | use |
|---|---|---|---|
| ko.wikipedia dump | CC BY-SA 4.0, commercial use permitted | yes | the sentence corpus; the only source that is both fresh and distributable |
| National Institute of Korean Language, 2003 usage-frequency survey | Type 1 attribution (제1유형 출처표시) | yes | distributable, but stale as a measurement of what people type today |
| namu.wiki | CC BY-NC-SA 2.0 KR | **no** (non-commercial) | not used: the deb distribution is commercial use |
| AIHub 624 (web data) | use and training permitted, redistribution not | **no** | measurement only; the one corpus where English is actually typed mixed with Hangul, so it is a cross-validation instrument |
| AIHub 119 (national records) | same terms | **no** | measurement only; its English is mostly metadata field names |
| hunspell `en_US` (SCOWL size 60) | SCOWL collective permission, `.aff` BSD | yes | the shipped `en-US.json` |
| krdict | CC BY-SA 2.0 KR | yes | not part of the modes measurement |

SA is not a blocker: `debian/copyright` is `LGPL-2.1-or-later`, which is on CC's
compatibility list, so derived data files ship as-is. Only an **NC** term
excludes a source from the deb.

Two properties matter when choosing a corpus:

- **freshness** — the 2003 survey cannot show the vocabulary Koreans type today;
- **population** — the feature acts on words that turn up mixed into Korean
  text. Ordinary English (`a`/`an`/`the`) is irrelevant because a Korean typist
  does not type it.

One corpus is one genre, not a sample of the target population. Encyclopedia
prose mixes scripts less than typed messages do; a reading from it is a lower
bound, and the run must say which corpus produced it.

---

# 5. Harness rules

These are the failure modes the earlier investigation actually committed. A new
harness keeps them, or it will produce the same confident zeros.

1. **Derive keys from the engine, never from the alphabet.** A hand-derived
   jamo → key map is the recorded failure. `tools/jamo-keytable.py` probes the
   engine per jamo in its slot; the twin 중성 (ㅗ/ㅜ/ㅡ) record which member the
   table took. Punctuation is not ASCII identity either — `probe_literals()` in
   `tools/modes-metrics.py` probes printable keys and accepts one only when it
   delivers the character **and** leaves nothing composing.
2. **Read the source whole.** A line-by-line reader measures page headers
   instead of bodies; a naive strip removes markup the reader never saw and
   reports the loss as real words. Use a real parser and count every drop.
3. **A filter or cap is a measurement decision, not a cleanup.** Count what it
   removes, by token and by type. A frequency or length filter can look like a
   coverage win while deleting the words the judgement exists for.
4. **The corpus is an argument with provenance.** Take the path as a parameter;
   write `meta.json` next to the output with source, method, exclusions and what
   the artefact does not represent.
5. **A harness that cannot fail loudly is not a guard.** The OFF arm must
   reproduce every input as written; a receive-path disagreement must fail the
   run; `--selfcheck` must assert that the comparator notices a changed
   sentence and that the transition count matches a hand count.
6. **Cross-validate against a corpus of a different nature.** Two corpora with
   opposite tendencies giving the same ratio is evidence the conclusion is not
   sample bias. A corpus that may not be distributed is a valid instrument
   precisely because it can correct the bias of one that may.

---

# 6. Removed records and what they do not settle

`docs/MODES-DATA.md` and `docs/MODES-METRICS.md` were deleted on 2026-11-08.
Their narrative, tables and dated readings are in git history and in the
2026-11 wiki records. Nothing in them is a current definition:

- **The corpus word list is retired.** ReqMode8's current ruling defines
  `english_valid` as an English dictionary lookup and `hangul_valid` as a
  syllable-structure test. The 50,000-entry cap, the stopword policy and the
  corpus list described nothing that is still used; the shipped artefact is
  `libhunmin/data/modes/en-US.json`, specified in `docs/SPEC-SCHEMA.md`
  §English word list.
- **The cap derivation is not a current measurement target.** It measured
  coverage saturation for a list the tree no longer ships. The number survives
  in git history only; do not cite it as a current property.
- **The last ② / ②b / ③-bis readings are history, not a baseline.** They were
  taken on one Wikipedia corpus and a retired validity definition. Board B2's
  pass line is still unset, so there is nothing for a new run to be compared
  against except the definitions above.

What the removal does **not** settle, and what stays open:

- E4's pass line for all three metrics (board B2);
- ReqMode9's threshold, now that `hangul_valid` is structural (board B1's
  residue);

**Settled 2026-11-08 — there is no run, and none is planned.** The maintainer's
words, verbatim:

> "no measurement until i mention. current rule - using english dict only for
> auto-switch is enough for me."

Two things follow, and both are rules rather than observations:

- **Nothing here is measured until the maintainer says so.** Not on the
  ko.wikipedia dump, not on AIHub 624, not on a corpus found later. The §4 table
  and §3's commands are what a run would need to respect; they are not a queue
  of work, and no agent starts one because a budget was declared or a shape was
  priced.
- **For 0.3, the shipped English dictionary is the whole of the judgement's
  data.** Clarified the same day — *"i mean enough for 0.3"* — so the scope is
  the 0.3 lane and not "for ever": `english_valid` as an `en_US` lookup plus a
  user dictionary carries the feature through 0.3, and for that lane the corpus
  list, its cap, a curated supplement and a typed-text corpus search are all
  *not* wanted. Whether 0.4 or 1.0 needs more English than a dictionary is a
  later ruling, and nothing in this document asks for it.

---

# 7. References

- Requirements: `docs/SPEC-MODES.md` ReqMode8, ReqMode12, ReqMode14, ReqMode15
- Pass-line shapes, units and the open sub-decisions: §2
- Engine rules and the judgement: `docs/DESIGN-MODES.md`, `docs/DESIGN-CORE.md`
- Shipped English list and its contract: `docs/SPEC-SCHEMA.md` §English word list,
  `libhunmin/data/modes/meta.json`
- Tier structure: `docs/TESTING.md`
