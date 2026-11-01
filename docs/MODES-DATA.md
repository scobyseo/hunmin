# MODES-DATA — Word List (the "valid" criterion for hangul/latin judgment)

Requirements: `docs/SPEC-MODES.md` R8. This document holds what R8's "is valid"
is judged by, that is, **which word list is used**.

Status: **The source is settled, the list is not.** Together with R4 this is a
blocker for this feature.

**Added 2026-10-04 — §4-7.** The cap problem was measured. The conclusion is
that **the two lists are of a different nature**: English can be capped (§4-7a),
but **the hangul list must not be capped** (§4-7b). Without this distinction the
hangul side produces "un-embeddable" and "impossible".

---

# 1. Corpus candidates and licenses (measured 2026-10-04)

| Source | License | Verdict |
|---|---|---|
| National Institute of Korean Language 「Survey of Modern Korean Usage Frequency」 (2003) | Type 1 attribution (제1유형 출처표시) | Usable but **stale as a measurement**: KakaoTalk/messenger/blog/smartphone/cloud/login/download all absent; in-sa/ㅋㅋ/ㅎㅎ/ㄹㅇ absent. The frequencies come from volume 176 |
| ko.wikipedia dump | CC BY-SA 4.0, **commercial use explicitly permitted** | Best — KakaoTalk 477, in-sa 343, ㅋㅋ 2,251, blog 15,663, cloud 10,936 |
| namu.wiki | CC BY-**NC**-SA 2.0 KR | **Rejected.** "This work may not be used for commercial purposes." deb distribution (PPLAN.md P4 → PPA) is commercial use |
| Wiktionary `Korean_5800` | CC BY-SA | A derivative of the National Institute of Korean Language survey, the same stale content. No gain over the original |

**SA is not a blocker.** `debian/copyright` is copyleft as `LGPL-2.1-or-later`
and is on CC's list of compatible licenses, so derived data files ship as-is.
The only problem is that namu's **NC** is excluded.

Two reasons for choosing Wikipedia — the reason is the smaller license:
**freshness** (the 2003 corpus cannot show the vocabulary Koreans type today) and
**the population** (words that actually turn up mixed into Korean text. That
population is the one this feature acts on. Ordinary English like
`a`/`an`/`the` is irrelevant — a Korean typist does not
type it).

---

# 2. Distributable list (measured, 2026-10-04)

```
krdict       31,253 forms    3.5 MB   distributable (CC-BY-SA 2.0 KR)
wikipedia   654,336 forms   ~10 MB text / 1.06 GB RSS   distributable (CC BY-SA 4.0)
624       12,588,072 forms   235 MB text                 not distributable
```

**The Wikipedia figure is discarded because it was measured with the truncated
reader.** Full re-measurement
(including the markup removal of §4-6a):

```
ko.wikipedia full   791,929 pages   longest body 1,010,725 chars
  latin     815,822 forms   15,676,261 tokens    9 MB
  hangul  7,495,250 forms  191,498,857 tokens  135 MB
```

Before markup removal (not the truncated reader) latin was 1,163,426 forms /
37,685,948 tokens
was. Markup such as `ref` was eating 15.8% of latin tokens and that is what is
gone — **real English words decreased.**

RSS is not a problem even in the full case — the format is one line per entry
and the count shrank,
so embedding still needs a per-entry cap, and that cap is in §4-5.

```
National Institute of Korean Language 「현대 국어 사용 빈도 조사」 (2003)   54,095 forms   Type 1 attribution  — distributable, stale as a measurement
ko.wikipedia   654,336 forms   CC BY-SA 4.0     — distributable
71788 (archival)  ~? forms       study use only, redistribution without permission forbidden — not distributable, measurement only
624  (web data) ~? forms       same            — not distributable, measurement only
```

Both AIHub corpora are **measurement tools.** By policy they are for model
training and third-party (제3자) provision is
forbidden. Only krdict and the Wikipedia list can go into the deb.

`624` is the only source where English is actually typed — `sk`/`kt`/`lg` appear
17k–29k times in a 200-item sample, unlike 71788's `end`/`start` metadata which
are each ~10k. English in 624 alone
is skewed toward the telecom/IT family (`lg` `sk` `kt` `tv` `it` `ai` `sns` `kbs` `cj` `sbs`),
and this bias is stated explicitly — 624 is not general Korean web text
but a single vertical. 71788 is useless on the English side (metadata field names).

## The AIHub corpus is now used as a cross-validation tool [E]

The license blocking distribution is unchanged (§2). However there are **no limits
on measurement**, and on 2026-10-04 an actual mission was performed: the
"hangul with no space after a symbol" of `docs/SPEC-MODES.md` R4 was
**re-validated with a corpus of a different nature from Wikipedia**.
The result is recorded in SPEC-MODES, and the two corpora gave the same ratio (0.577% vs
0.522%). Without this cross-validation it would have been impossible to know
whether the Wikipedia sample distorted ordinary writing.

**Why this is needed — why Wikipedia alone is not enough:**

| | ko.wikipedia | AIHub 624 |
|---|---|---|
| Genre | Encyclopedia (technical terms of the kind `lte(롱텀에볼루션)`) | **News** (actual colloquial sentences) |
| News-sentence punctuation tendency | Ambiguous (written with `.` attached) | Separate (written `. `) — ratio is low as measured |
| License | CC BY-SA, **distributable** | **not distributable** |

The fact that two corpora of opposite tendencies give the same ratio is
**evidence that the conclusion is not sample
bias.** Considering only distributability 624 cannot be used, but
**for validation that very unfitness is what has value** — data that must not be
distributed corrects the bias of data that may be.

**How to use it (streaming the original zip, no 3.9 GB extraction):**

```sh
tools/modes-glued.py --both 2000                     # wiki vs aihub, ratio comparison
tools/modes-glued.py --aihub <zip> 2000              # aihub alone
tools/modes-glued.py --wiki 4000                     # wiki alone
```

The two things that had been split into two scripts were merged into the single
`tools/modes-glued.py` —
keeping the same regex and the same aggregation code in two places was in itself a source
of drift, and there was also the problem that, being outside the repository, verification scripts could not reach it.

**Schema (verified function, not assumed):**

```
{"SJML": {"header": {...}, "text": [ 99 SEGMENT DICTS ]}}
  each element = {"title": str, "subtitle": str, "content": str}
```

Iterating `text` as a list of strings gives **0 characters, rc=0** — a silent 0. This data
has three traps at once (reproduction of POSTMORTEM 10):
1. Guessing key names — errors of the `obj.get("data", [])` kind zero out everything to 0
2. Discarding short records — filters of the `len(obj) > 40` kind
3. **Assuming a dict inside a list is a string** — the one actually committed this time

**`title` must be included.** Headlines insert the most unspaced punctuation,
like `정부, 무선국 검사제도 손본다`.
Counting only `content` misses this phenomenon.

---

# 3. Data contract

The word list is **data**. A schema must be added to `docs/SPEC-SCHEMA.md`.
The inference logic is code in the core (SPEC-MODES R3), but **the list itself is a data file.**

This schema problem is shared with P2 hangul typesetting — the two lists solve the
same "find a hangul eojeol (어절, space-delimited word)" problem. So it goes into the
specification of `docs/SPEC-SCHEMA.md`,
and it is not an appendix document of `SPEC-MODES`.

Undecided:

- The wording of `debian/copyright` and the `LICENSE.md` next to the data for a CC BY-SA derived list.
- Whether the list stores only the form, or also stores part-of-speech/frequency/level.
  E1 (the symmetry rule) may need a josa (조사, particle)/eomi (어미, ending) distinction, so it connects here.
- Whether to attach a list of the eojeol's josa/eomi to Korean entries, or keep it as a separate list.

---

# 4. ~~Unresolved — the list cannot be made right now~~ — **Resolved (2026-10-04)**

**The strip rules deleted 75% of English and 37.6% of hangul.** In a 120,000-row
slice:

```
                english distinct   hangul distinct
no stripping           383               1,725
current rules           94               1,173
```

The 183 deleted English tokens are real dictionary words and include `computer`
`display` `article`
`basic` `archive` `algebra`. The deleted hangul is mostly **document titles**
(`마이크` `아파치` `워싱톤` `염색체` `표준`). Almost all of it is vocabulary this
feature needs.

## 4-1. The root cause was not the rules but the **reader**

Re-measured with `tools/build-modes-corpus.py`:

| | per 4,000 pages |
|---|---|
| Previous reader (1 line per page) | hangul forms **5,673** |
| Reader that reads the whole body | hangul forms **552,757** |

**95x.** In a 6.7 GB dump the page body is **140–433 lines**, but the previous
reader read only the first line that has a `<text>` tag and cut at the first `>` with
`split('>', 1)[1]`.
A page starting with `{{대통령 정보` contributed only its header.

**Diagnosis:** the "lost words" list comes out sorted from the start of the
alphabet — `about/acid/advanced`, `가곡/가끔/가능`. It does not look like genuine loss.
It is a set difference of truncated inputs.

**Of the earlier numbers, still valid:** the 624/71788 magnitude figures (§2).
Those came
from a different pipeline (AIHub) and are not affected by this reader bug.

## 4-2. Neither Method 1 nor Method 2 is needed

The two options presented by `docs/MODES-DATA.md` (repairing the strip rules /
calling the MediaWiki API)
are **neither of them needed.** Instead of fixing the rules, the rules were
removed —
`mwparserfromhell` is structurally correct, and the remaining loss moved to being
a *decision rule* problem, namely
**the distinction between the Sino-Korean (hanja-derived) joining code and the
eojeol separator** (`docs/SPEC-MODES.md` E8).

## 4-3. Tools

```sh
### Corpus build (measurement sample or full)
tools/build-modes-corpus.py --dump ~/work/kowiki-dump/kowiki.xml \
    --pages 4000 --out ~/work/wikicorpus/out

### Prove the failure modes before believing the result
tools/build-modes-corpus.py --selfcheck --out ~/work/wikicorpus/out

### Cap measurement (every figure in §4-7)
tools/modes-cap.py            # english side, 1.2 seconds
tools/modes-cap.py --hangul   # includes the hangul side, reads a 135 MB file
```

**`modes-cap.py` is the only source of the §4-7 figures.** There have actually been
errors from copying figures into a document by hand (an embedding of 400,000 rows was
written as 3.51 MB but is actually 3.20 MB — increment/cumulative confusion). Make
the script first and copy from there.

The three measurement inputs are **outside the repository** (`~/work/wikicorpus/full/*.txt`,
`~/work/aihub-records-corpus/624-en-mixed.txt`) — they are measurement outputs, not sources,
so they are not committed. Only krdict is inside the repository. So this
script **does not run** in CI — for the same reason as §4-3b, and so that the
absence of data files is not reported as a failure.

**Required:** `mwparserfromhell` (in `~/work/wikicorpus/.venv`). The system
python3 does not have it — this project is PEP 668, so use a venv or uv.

## 4-3b. ctest registration — only partly, and why only partly

**The selfcheck is registered.** `modes-corpus-selfcheck` (CMakeLists.txt).

```sh
ctest --test-dir build -R modes-corpus-selfcheck --output-on-failure
```

It runs if the corpus is committed, and if not it is recorded as **77 = skip**. CI has no
dump, so that "data file missing" is not reported as a failure. **And it is never
recorded as passing** — that is the price this project has repeatedly paid
(POSTMOSTEM 10).

**The corpus build itself is not registered.** The reason is dependencies:

- The build needs `mwparserfromhell`, and CI installs only `python3` (apt), per
  the `linux` job's "Install build deps" step in `.github/workflows/ci.yml`. Hardcoding a venv absolute path here means
  **it passes locally and breaks in CI** — the same kind of thing as quietly
  losing 104 conformance tests.
- So the mwparser dependency of `--selfcheck` was removed via **lazy import**.
  That separated verifying from building, and this way selfcheck runs with CI's
  system python3.

**What was written at first was wrong.** It was written that "because of PEP 668 it
cannot be registered", but
it was already installed in `~/work/wikicorpus/.venv` (a venv made by uv,
mwparserfromhell 0.7.2). The real blocker was not the install but **that CI has no
such dependency**. It named a cause without checking for its absence — the same
kind of thing as POSTMORTEM 21.

**What the selfcheck catches:** (a) the truncated reader — 733 hangul tokens per
page vs the normal
1,033, floor 900, (b) a tokenizer that treats wiki boldquotes as tokens — 289
forms with `-` attached.

**The first criterion did not catch the failure** — measured by the number of
*distinct* forms per page, the truncated reader was actually higher (142.1 vs
138.2). The token count is the discriminator.

## 4-4. Why it moved to a decision problem rather than staying a rules problem

What `mwparserfromhell` leaves behind is wiki **furniture** (`분류:` chains, `15px`,
infobox), not dictionary vocabulary. But what remained turned out not to be a
dictionary problem but a
**decision problem about the eojeol separator** — cutting `노무현·정몽준` at `·`
splits one eojeol in two. This is not a problem of which words go into the list
but of **which character is a buffer boundary**, and so
it went to `docs/SPEC-MODES.md` E8.

---

## 4-5. Full re-measurement, and the two times the selfcheck was wrong

Running the full set (791,929 pages) changed the figures of §2 by a factor of 12
(see §2).
But **the selfcheck rejected the genuine corpus.**

```
FAIL  hangul tokens per page (271 tokens/page (floor 900; truncated reader 732))
```

**The floor 900 was a value fitted to the 4,000-page sample (1,033 tokens/page).**
Running the full set gave 271 for the genuine one, and the truncated reader 732 lay
**between the two genuine ones**:

```
truncated reader          732 tokens/page   <- should fail
full (genuine)            271 tokens/page   <- should pass
4000sample (genuine)      1033 tokens/page  <- should pass
```

**No single threshold can pass both while failing 732.** That is,
per-page statistics were not the discriminator. What discriminated was the
**reader**, so the statistical
check was dropped and replaced with a structural check — the length of the longest
body actually read.

```
longest body read length >= 5,000 chars
  genuine:  4,000sample 296,122 chars / full max 205,917 chars
  defective:  1,976 chars (one-line reader)
```

**And the planted mutation was not a genuine defect.** The earlier verification
planted it by cutting `cur.append(line[:60])`, but that **still reads the whole
page** — because it concatenates the first 60 characters of each line. Having
measured it, it
passed with hangul 282,550 forms and a longest of 89,234 chars. That is, that check was
checking a wrong defect. The genuine defect was the one-line reader itself
(not doing `cur.append` at all), and planting that gives hangul 5,054 and a longest of
1,976 chars, so it **fails.** Three mutations were separated and checked:

| What was planted | hangul forms | longest body | Verdict |
|---|---|---|---|
| none (genuine) | 552,757 | 296,122 | PASS |
| `line[:60]` cut | 282,550 | 89,234 | **PASS — not a defect** |
| one-line reader (genuine defect) | 5,054 | 1,976 | FAIL |

**What was realized:** the fact that "this check passed" is **not** evidence that
"this check is checking that". If a mutation is not the target of the check, passing
is the normal outcome. This is the same
kind of thing as POSTMORTEM 13 (re-deriving eleven times what the engine already
knows), and this time it only came to resetting the statistical threshold three
times.

## 4-6. Full selfcheck passes — and the remaining contamination

When the full rebuild (791,929 pages) finishes, the selfcheck passes:

```
PASS  longest body looks like a whole article (1,010,725 chars)
PASS  corpus is not tiny (1163426 latin forms)
PASS  hangul present (7986520 hangul forms)
```

The previous verdict (§4-5) rejected it at 271 tokens/page, and thanks to changing the
decision line to a structural check it works normally. A longest body of 1,010,725
chars is a 1MB document, and such
documents really do exist in the actual Wikipedia.

## 4-6a. Residual contamination — ~~16% of latin tokens~~ **Resolved**

### Cause: entity escaping + tags unknown to mwparserfromhell

Before the fix, the top items of the full corpus were not words but markup:

```
ref    5,955,310   15.8% of latin tokens
name   1,408,293    3.7%
br       935,554    2.5%
math     549,940    1.5%
분류    3,744,611    1.75% of hangul
```

**It took three steps to find it.** The guessed regex failed twice.

1. **`<[^>]*>` preprocessing — effect 0.** The variable was too smart and seemed to know
   it would delete the tags, but the input had **no** tags in it.
2. **Decisive observation:** `RAW has <ref>? False` yet grepping the output finds
   `<ref>`. **What was not in the input was created in the output.** This settled the
   nature of the bug.
3. **The real cause:** the dump stores HTML **entity-escaped** —
   the original has `&lt;ref&gt;`. `mwparserfromhell` does **not know** the HTML tags this
   wiki uses (`filter_tags()` only looks at `b/li/table/td/th/tr`), so
   `strip_code` cannot remove `<ref>` and lets it through as a string. On top of that,
   `normalize=True` **decodes** entities, **reviving** the tag. The escaping was
   in fact the cause: `&lt;ref&gt;` went in and `<ref>` came out.

### The fix

`strip_wiki` removes HTML **before parsing** (`_strip_html`):

- Unescape entities first (`&lt;` → `<`) to make tags, and then
- Block tags (`ref` `math` `gallery` `code` …) are removed **together with their content** — citation
  URLs and formulas are not prose
- The remaining tags have only the tag removed and the text in between is kept

250-page measurement:

| | Markup share | latin tokens |
|---|---|---|
| Before the fix | **30.32%** | 89,342 |
| After the fix | **2.09%** | 17,796 |

`ref` 5,955,310 → **2 cases**.

**Tokens dropped by a factor of 5, and it was checked whether this is excessive removal** —
the prose is intact and only
citations are gone:
```
before: ...아래 하위분야이다.<ref>조남현, 고등학교 문학(상), 중앙교육진흥연구소, 2003, 12~15쪽.</ref> 간단하게…
after:  ...아래 하위분야이다. 간단하게…
```
The decrease is correct because most of it was markup.

**Recurrence prevention:** a guard was added to the selfcheck —
`latin is not mostly markup` (5% ceiling). As measured, 2.09%. This guard **rejects the old
corpus** (26.16% → FAIL), so a silent regression is impossible.

**Remaining residue:** `분류` remains at the top of hangul (1.75%). It is the label of
`[[분류:...]]`, and because it is a wiki link rather than a template, `filter_templates` does not
catch it. **It has no effect on the English decision** (it is on the hangul side), and in
the hangul decision it is a token not in the dictionary, so it is treated as "not a
word" and is safe. It was not removed.


### Full re-measurement result — the top items are now real words

```
PASS  latin is not mostly markup (1.62% markup)
PASS  hangul present (7495250 hangul forms)
PASS  longest body looks like a whole article (1,010,725 chars)
```

| | Before markup removal | After removal |
|---|---|---|
| latin forms | 1,163,426 | **815,822** |
| latin tokens | 37,685,948 | **15,676,261** |
| hangul forms | 7,986,520 | **7,495,250** |
| Markup share | 26.16% | **1.62%** |
| `ref` | 5,955,310 | **약 1,100** |

**"The number of words decreased" is in fact the correct result.** What decreased
was markup, and
the remaining top items are actual language:

```
the  291,293     px  237,070     m  232,296     a  231,417
 of  206,582   kbs  144,682     b  134,506    tv  132,732
  x  130,440     c  128,967   km  111,256   and  110,633
```

`kbs` `tv` `mbc` `sbs` `fc` `fifa` are real vocabulary of the Korean Wikipedia —
broadcasters'
abbreviations and unambiguous Latin tokens such as soccer/FIFA.

**The identity of the 1.62% residual markup:** `px` `km` `cm` (infobox image sizes),
`align` `center` `style` (infobox HTML attributes). Most of it comes from infoboxes.
It is harmless for the English decision — `px` or `align` are not words a Korean typist
types, and even if they are in the
dictionary, an eojeol like `안녕px` does not hold.

**And English stopwords are not the target of the decision.** `the` is 1st (291,293
times) but
a Korean typist does not type `the`. It is only in the corpus because it is used in
Wikipedia sentences, and is not the population of this feature. **The list for the
English decision must not just use the corpus top N as-is and must remove stopwords** —
this is the input to the next
task.

**Measured directly:** 33 English stopwords are 8.72% of latin tokens, unit
abbreviations are 3.22%,
combined **11.94%**. That is, even keeping only the top 10만 entries, 12% of them are
words
that are not used.

---

# 4-7. Cap measurement (2026-10-04) — the two lists are different problems

The question §4-6a left = "cap it **after** removing stopwords." This was measured.
**Conclusion: that question is right for English and wrong for hangul.**

## 4-7a. English — a cap is possible (coverage saturation)

Reproduction command (a dedicated venv is required, the system python3 has no
`mwparserfromhell`):

```sh
~/work/wikicorpus/.venv/bin/python <the script above>
```

The measurement target (`624-en-mixed.txt`, 259,590 forms / 15,322,442 tokens) is
**English that is actually typed mixed with hangul** — this is where R8's
`english_valid` is directly reproduced. It is not a substitute for ② (sentence
replay) of R12 but a substitute for the **decision
oracle**, and its limit is stated explicitly.

**Reproduction rate per cap (judged with the list above):**

| Cap | Reproduction rate | Embedding (×8B) |
|---|---|---|
| 5,000 | 68.33% | 0.03 MB |
| 10,000 | 75.44% | 0.06 MB |
| 20,000 | 81.94% | 0.13 MB |
| 50,000 | **86.35%** | **0.40 MB** |
| 100,000 | 88.44% | 0.76 MB |
| 200,000 | 89.99% | 1.60 MB |
| 400,000 | 90.91% | 3.20 MB |
| 815,822 (full) | 91.96% | 6.53 MB |

**It saturates.** It is 86.35% at 50,000, and 16 times larger is only +5.61pp.
100k→200k is
+1.55pp, **the smallest gain** (0.87 MB given up for 1.55pp). The cap is set to
**50,000** — the point where the slope of the coverage-embedding curve changes.

**Removing stopwords lowers the reproduction rate.** Measured (cap 50,000, after
removing from the ranking, fill
50,000 back up):

```
top 50,000 raw                  86.35%
top 50,000 - stopwords(122)     82.43%   -3.92pp
top 50,000 - stopwords - unit abbrevs     78.66%   -7.69pp
```

(On the basis of the full list, raw 91.96% / -stop 88.04% / -stop-units 84.27%.)

**§4-6a's "remove stopwords first" is a conflicting piece of evidence.** Stopwords
actually shave 3.92pp off the reproduction rate. The gain from removing stopwords is
**disappearing from the target of the decision**, and the cost is **the reproduction
rate.** The more it grows beyond 33 to 122, the more disadvantageous this is.
**Stopword removal is not an embedding-size problem but a problem of decision
semantics, and is not adopted for now** — reasons:

- The purpose of this feature (R12) is a **reduction in the number of key strokes**, not decision accuracy.
  Putting `the` in the dictionary does not create a person who types `the`, and misfires do not
  increase either (if ambiguous, keep the current mode, the third line of R8).
- The price of subtracting 122 stopwords is 3.92pp of reproduction rate, and this is the correction of §4-7c.
- **It is reversible** — the removal can be undone at any time, but recovering a
  deleted word requires
  re-running the corpus (some hours).

**However, residual markup is a separate matter.** The `px` `km` `cm` `align`
`center` `style` that §4-6a pointed out **are present** in the English dictionary (verified in
`/usr/share/dict/words`: `px` IN, `km` IN, `cm` IN, `align` IN, `center` IN, `style` IN). That is,
these characters are judged to be *valid* English words of the dictionary, and **the validity
oracle is contaminated.** This is in exactly the
opposite direction from stopwords: stopwords are not scoring targets, but these
**are judged to be true precisely because they are in the dictionary.** This is treated
in §4-7c.

## 4-7b. Hangul — a cap must not be applied (there is no saturation)

The same curve was drawn for hangul. The result is completely different.

| Cap | Form coverage | Token coverage | ×8B |
|---|---|---|---|
| 31,300 (krdict) | 0.76% | 71.96% | 0.46 MB |
| 50,000 | 0.98% | 75.94% | 0.59 MB |
| 100,000 | 1.61% | 81.38% | 0.97 MB |
| 200,000 | 2.90% | 86.15% | 1.74 MB |
| 500,000 | 6.85% | 91.22% | 4.10 MB |
| 1,000,000 | 13.48% | 94.14% | 8.08 MB |
| 2,000,000 | 26.80% | 96.42% | 16.07 MB |
| 7,495,250 (full) | 100% | 100% | **60.01 MB** |

(The ×8B column is the **union** (합집합) of the corpus top N and krdict — krdict
is already
in the file, so it is always included in what is paid. That is why the 31,300 row is
0.46 MB rather than 0.25 MB: krdict 31,253 is added.)

> **[OUT OF SCOPE — krdict, 2026-10-05]** The 31,300 row and this ×8B column are
> krdict-inclusive and are therefore out of scope under §5-1. **The conclusion
> this table was drawn to support — that a cap must not be applied to hangul —
> does not use them.** It uses the same curve measured **corpus-only**
> (`tools/modes-cap.py --hangul --no-krdict`), which needs no dictionary:
> **2.67%** of forms at 200,000, **13.34%** at 1,000,000, **26.68%** at
> 2,000,000. The corpus-only numbers are slightly *lower* than the krdict-inclusive
> ones above (krdict adds forms), and the point is unchanged: no cap is
> affordable.

**Even with 200,000 forms in, it is only 2.90% of the whole, and with 2 million in,
26.80%.**
Then instead of paying 16 MB you **misjudge 73% of hangul forms as "not a word".**
`hangul_valid` becomes false at 73% in R8's decision expression, and
following the third line of R8 (if ambiguous, the current mode) **the hangul mode is
simply kept** —
which is exactly as if this feature did not exist.

**An interesting inversion:** English covers 86.35% with 50,000 entries, so a cap is
possible. That
is possible because Wikipedia is text of **limited vocabulary** (encyclopedia,
technical terms), while
the hangul corpus produces 500만 distinct spellings as **long tangled
forms**.
English saturates and hangul does not. **The same cap policy cannot be applied to
both lists at the same time.**

**krdict cannot close this gap.** Measured:

> **[OUT OF SCOPE — krdict, 2026-10-05]** The measurement below uses krdict,
> which the maintainer's scope boundary puts outside this research (§5-1).
> **Retained as a record, not as support for a decision**: these numbers were
> taken and are not withdrawn, but the conclusion they supported — "krdict alone
> will not do" — was about a source that may not be used here, so it cannot
> decide anything on its own. The *positive* finding that a corpus list cannot
> work is unaffected, because §4-7b derives it from corpus figures with no
> dictionary involved.

```
krdict (31,253 forms) covers hangul corpus tokens:  29.86%
NFC convention: krdict 100%, corpus 100%, conjoining(U+1100) 0  <- not a normalization mismatch
stem(word root)+suffix(가/는/이/을/를/에/의/도/만/로/과/와/은/나/다...):
                                              +15.56%
total                                                45.42%
```

**The decisive josa (조사, particle) is this one: it is not a normalization mismatch.**
Just as POSTMORTEM 10 ("all None — the data is normal") failed three times, the
observation "the dictionary cannot cover the corpus"
can **come out of an encoding difference**, and it actually had that origin once
(the conjoining vs compatibility bug in `measure.py`). So it was excluded first:
both krdict and the corpus **are NFC and have 0 conjoining jamo**.
**The remaining 70% is a difference of the vocabulary itself** — the corpus is not
dictionary stems but **inflected forms** (활용형), and the top items are `년` `분류`
`월` `일` `있다` `이`
`는` `의`. The 15.56% of inflected forms can be filled by inflection rules, but the
remaining 55% **is not reachable with a vocabulary list.**
This conclusion is the **evidence that a cap must not be applied to the hangul
list.**

**Since krdict alone will not do, three options remain:** (a) make a list of the
corpus top N — §4-7b refutes this. (b) **If lexical weight is not needed, revert
the hangul decision to a dictionary lookup** — R8's `hangul_valid` can
be "not any hangul vowel/final combination" rather than
"is it in the dictionary" (§4-7c). (c) Revert to a combination list.

## 4-7c. Conclusion — the cap exists only for English

| | Decision criterion | Cap | Adopted |
|---|---|---|---|
| English | Corpus ranking | **50,000** (86.35%) | adopted (§4-7a) |
| Hangul | krdict lookup | **none** | no cap (option b) |

> **[OUT OF SCOPE — krdict, 2026-10-05]** The "Decision criterion" cell above is
> wrong under the maintainer's scope boundary (*krdict is only for hanja
> conversion; do not bring it into this research* — §5-1). The hangul row cannot
> rest a decision on krdict. **"no cap (option b)" is unaffected** — option (b)
> is *dictionary lookup or combination judgement*, and §4-7b's finding that a
> list cannot work stands on corpus figures alone: measured corpus-only,
> **2.67%** of forms at 200,000 and **26.68%** at 2 million
> (`tools/modes-cap.py --hangul --no-krdict`), no dictionary involved. What is
> void is only the label "krdict lookup", which was the
> dictionary the option happened to name. **The decision below this table is
> still open** and must be re-derived from a source inside the boundary.

**Falsification condition for the English cap of 50,000:** if the reproduction rate
comes out lower than 86.35% on a corpus other than
624, this number is carried over. 624 is a single vertical (news/IT) and
§2 has already stated that bias.

**§4-6a wrote "cap after stopword removal" but the measurement says the order is the
reverse: the cap comes first (saturation), and whether stopword removal is done
afterwards is only
reviewed.** For a list of the same size, 50,000 entries with stopwords subtracted
(82.43%) and
50,000 entries as-is (86.35%) have different reproduction rates. **Removal cannot
be a means of raising the reproduction rate on the same list.**

## 4-7d. A dictionary filter halves the reproduction rate (the strongest refutation)

The residue of §4-6a is `px` `km` `cm` `align` `center` `style` and mostly
infobox HTML attributes, so **couldn't it be removed with a dictionary filter** is
the natural
next question. It was measured. **It cannot:**

| Cap | Original | Only what is in the dictionary | Loss |
|---|---|---|---|
| 50,000 | **86.35%** | 43.87% | **-42.48pp** |
| 100,000 | 88.44% | 44.08% | -44.36pp |
| 200,000 | 89.99% | 44.23% | -45.76pp |

**No matter how much the cap is raised it does not recover** (flat at 44.23%) — even
allowing 200,000 dictionary
entries it falls short of half of what 624 reproduces. If the cap is
200,000 then 80.3% of top-200k is outside the dictionary, and most of it is
vocabulary that Wikipedia
actually **uses**:

```
kbs  144,682   mbc 107,611   fc 101,075   sbs  79,713   fifa  46,934
uefa  32,458  sk   20,344   kt  13,182   nhk  10,670   cj   7,458
```

None of these are in `/usr/share/dict/words` — they are not English dictionary words
but
**the real vocabulary of the Korean Wikipedia.** `colspan` `rowspan` `bgcolor` are both
in the dictionary and occur **0 times** in 624 (the true residue of markup). That is,
the dictionary **cannot distinguish the two kinds**,
and excludes together the broadcaster abbreviations that Wikipedia actually uses
(`kbs`) and the markup that is genuinely unused (`colspan`). And yet **49.2% of the
tokens this list covers on 624 are outside the dictionary.**

**Conclusion:** a dictionary filter cannot make this list. This list is not a "list of
English words" but **a "list of Latin strings that Koreans type in Hangul".** The
moment `english_valid` is
defined as a dictionary English word, this feature loses half of its reproduction.
**R8's `english_valid` is a list lookup, not a dictionary lookup, and the raw material
of the list is not the dictionary but the corpus.**

**This reverses §4-6a's "harmless":** it is true that `px` `align` are residue,
and the means of removing them (the dictionary) is **worse.** The residue is
**permitted.** What
must not be permitted is not the residue but the hangul side of §4-7b.

# 5. References

- Requirement: `docs/SPEC-MODES.md` R8
- Schema: `docs/SPEC-SCHEMA.md`
- krdict license: `libhunmin/data/krdict/LICENSE.md`
- krdict extraction tools: `tools/fetch-krdict.sh`, `tools/krdict2data.py`

## 5-1. krdict is out of scope for this document — standing note

The maintainer, 2026-10-05:

> krdict.json은 한자 변환시에만 쓰는 걸로 하자. 이 연구에 가지고 들어오지 마.
> *krdict.json is only for hanja conversion. Do not bring it into this research.*

**This is a scope boundary, not a data-quality finding.** Nothing here claims
krdict is a poor dictionary, and nothing in this document's conclusions depends
on it — see the audit below. What the boundary forbids is *using* it as evidence
in the hangul/latin mode research, which is what §4-7's hangul numbers were.

**Two things follow, and they are different in kind.**

1. **The measurements that already cite it are retained, marked as out of
   scope.** They were taken; deleting them would destroy the record and would
   read as if the finding had been reversed. Every such figure carries a
   provenance marker at the point of use.
2. **No *new* hangul measurement may cite krdict.** Where a conclusion was
   *reached* using krdict, the conclusion must be re-derived without it or held
   open. §4-7c is the one place where that had not happened.

**What is unaffected — the hanja path.** `libhunmin/src/candidate_tables.c`,
`tools/krdict2data.py`, `tools/fetch-krdict.sh`, `docs/SPEC-SCHEMA.md`'s Hanja
files section and the deb packaging of `libhunmin/data/krdict/` are the hanja
conversion feature and are **outside this boundary**. They are not "uses of
krdict in the modes research" and this note does not ask for them to change.
The Android build excluding `krdict/**` from assets is likewise a
size decision about the hanja feature, not a modes one.

**The one place this bites.** `tools/modes-cap.py` reads krdict, and the
hangul half of its §4-7 output is therefore out of scope. See the marker on
that function.
