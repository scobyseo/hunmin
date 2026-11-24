# MODES-GATES — G1/G2/G3 checker

`tools/autoswitch-gates.py` grades a `tools/modes-metrics.py` report. It does
not replay text itself and sets no numeric G2 line (E4/B2 owns those numbers).

## Gates in one line each

- **G1 — no net loss:** autoswitch must cost no more keys than pressing 한/영 by
  hand. `q = (keys_off - keys_on)/transitions >= 0`.
- **G2 — pool capture:** how many transitions are captured (`q`, K2) and how many
  still need a manual press (`u`, T3). A ruled pair implies a misfire budget
  `r <= (1-u-q)/(2k+1)` at the run's measured repair cost `k`. Wrong-direction
  switches are checked separately in each direction.
- **G3 — expectability:** mid-word display flips reduced at an EQUAL pool share
  (E6). Graded only when a baseline step is named: every other step must carry the
  same corpus sha256, the same `q_pool_share_measured` to the digit, strictly
  fewer `flips.total`, no more `words_with_a_flip`, and zero
  `flips_below_the_guard`. The unit is `docs/MODES-MEASUREMENT.md` §2.5: a change
  of the language a preedit renders, inside one eojeol, measured on the raw ON
  arm. Equal `q` is not a formality — a flip reduction bought by switching less
  is not the comparison G3 asks for, and that mutation turns the checker red.

A result belongs to its pinned corpus. K2/T3 are corpus-invariant shapes, but
G1's sign and the overall-keystroke reading are not: changing the population
needs a new pin and a new run, never a reused verdict.

The ids' wording is in `docs/MILESTONES.md` §0.4 Track 2; the units are in
`docs/MODES-MEASUREMENT.md` §2.

## Inputs

- **Pin:** `tools/autoswitch-gates.py pin --corpus … --out …`. Stores the corpus
  sha256 and the hash of its `meta.json`, not the corpus. Both are checked again
  against the run and, if present, the file on disk.
- **Report:** `tools/modes-metrics.py --corpus … --json-out …`. The checker uses
  the sibling `.meta.json` (engine/data hashes, cross-check, totals, and the
  `flips` block G3 reads). The display setting a run measured is recorded there as
  `flips.judge_hold` — `tools/modes-metrics.py --judge-hold N`.

Paths are arguments. Bulk corpora stay outside the repository (AGENTS.md).

## Checks

- Pin/provenance must match; harness cross-check must have zero mismatches.
- G1 uses `q_pool_share_measured`, not the model's q.
- G2 is report-only unless `--line Q U` supplies a maintainer-ruled pair; then
  `q >= Q` and `u <= U` are graded and that line's budget is printed.
- Wrong-direction guard: zero switches against the typed script for both
  `hangul->latin` and `latin->hangul`; the count is cross-checked against the
  harness's `switched_to_other`.

Failure still writes a report. It is a finding, not a decision to change the
judgement.

## Current finding

### G3 — measured 2026-11-24 (issue #295)

`tests/fixtures/modes-gates/g3/` holds the pair: one `wiki-2500` run at
`judge_hold` 0 (the shipped rendering) and one at 1, plus the graded report. 1,687
mid-word flips become 304 at the same `q_pool_share_measured` (−6.320790 in both),
and the words that flip back inside themselves (the 도깨비 shape) fall from 432 to
35. It is a checker smoke run on Wikipedia prose, like the G1/G2 fixture: not
evidence for the target population, and G1 still fails on this corpus — G3 grades
the pair, not the feature's overall worth.

The same comparison on the 772-sentence KLDP rating corpus (outside the
repository, unsuited to redistribution) reads 2,546 → 477 at an identical
−4.379396; the figures and their provenance are in `docs/SPEC-MODES.md` §E6.

The knob is an option, not the default: `judge_hold` ships at 0, so this gate
grades a maintainer-visible choice rather than a behaviour change.

`tests/fixtures/modes-gates/` holds the pin, the metrics meta, and the generated
gate report. This is a checker smoke run on broad Wikipedia prose, **not evidence
for the target population**: it shows the checker runs end-to-end, and that G1 is
not met on this corpus while both direction guards pass. Do not cite it as a
general G1 result. A target corpus needs its own pin and its own run.

### Candidate populations — licence probe, 2026-11-22

None of these was collected into a committed corpus. A robots.txt rule is an
access instruction, not a licence.

| source | access facts | licence facts found |
|---|---|---|
| KLDP `forum/6`, `/blog` | RSS with post bodies (`taxonomy/term/6/feed`, `/blog/feed`); `Crawl-delay: 10`; blog feed is not well-formed XML | no licence, terms or copyright notice on the home page, registration page, node pages or either feed; no `<copyright>` element |
| KLDPWiki | EUC-KR; `TitleIndex` lists pages, `?action=raw` returns page text, RecentChanges RSS is diffs only; anonymous fullsearch refused | a `GFDL` page exists ("GFDL: GNU Free Document License"), but FrontPage and page footers carry no site-wide licence notice |
| Wikidocs `book/1` | commercial e-book 점프 투 파이썬 | robots.txt: `ai-train=no, use=reference` |

So no reuse permission was found in what was checked; the rights question stays
open rather than being settled by this probe. A scratch probe of 10 forum items,
10 blog items and three wiki pages yielded 74 candidates under the existing
sentence filter (blog 60, forum 9, wiki 5). Its JSONL, pin, run and gate report
live outside the repository. They were run through the gates for rating only:
G1 fails there too and both direction guards pass. No redistribution or
target-population claim follows from that run.

### Rating collection — repeatable and extensible

`tools/collect-kldp-modes-corpus.py` is the procedure. It writes the corpus and
its `meta.json` outside the repository; every row carries `text` and
`source: {kind, title, url}`, and the meta records each fetched URL with a
sha256, the delay, the counts and the rights note. The sentence filter is
imported from `tools/build-modes-sentences.py`, and rows are deduplicated by
text.

| source | how it is read | note |
|---|---|---|
| KLDP forum | `forum/6?page=N` → `/node/<id>` bodies; `taxonomy/term/6/feed` was the first seed | 10 s delay from robots.txt |
| KLDP blog | `/blog?page=N` → node ids and author ids; `/blog/<uid>/feed` and `/node/<id>` bodies; `/blog/feed` was the first seed | 10 s delay; blog feed is not well-formed XML |
| KLDPWiki | `TitleIndex` → `?action=raw` | EUC-KR; tool spaces requests by 0.2 s |

To add another source, write rows shaped like `{text, source}` to a JSONL and
pass it as `--seed-jsonl`, or add a fetcher beside `feed_sentences`,
`wiki_sentences` and `node_sentences`. No source path is built into the gate
checker.

The rating corpus stops at **772 sentences** (blog 457, forum 9, wiki 306) from
90 blog nodes, 7 author feeds and 101 wiki pages, kept as `kldp-772.jsonl` beside
its meta outside the repository. The maintainer chose to stop there rather than
spend roughly 15 more minutes of `Crawl-delay: 10` waiting for the remaining
topic pages. No process remains; the tool never parallelised those requests.

### What the 772-sentence rating showed

`modes-corpus-tokens.py` reports the token makeup; the pin, metrics run and gate
report for the 772 corpus live beside it outside the repository. Of 6,690 tokens,
5,442 are Hangul and 1,248 Latin. The Latin side has 736 unique forms, 569 of
them hapax, and the engine's shipped English list accepts 601 of the 1,248 tokens
(48%) — 323 unique hits against 413 unique misses. The misses are technical forms
(`UART`, `NAND`, `318Na`, `apt-get`, `proc()`, `SDK`, `GPIO`), so this is **not**
a corpus where every Latin word is ordinary dictionary vocabulary. It also is
not a clean autoswitch test: those are exactly the words a user dictionary could
absorb, so the run prices dictionary coverage as much as judgement.

The replay itself graded 772 sentences with zero cross-check mismatches. G1 is
not met and both direction guards pass; the 973 wrong deliveries are dominated
by transitions where no switch fired, not by switches in the wrong direction.
On the token mix the maintainer said, verbatim, *"뭐 나름 나쁘지 않은 것 같아.
저 비율이면."* That accepts the 772 sentences as a rating source; it does not
set a G1/G2 pass line or close the issue.

## Reproduce

```sh
# collect the rating corpus; add sources with --seed-jsonl or a new fetcher
tools/collect-kldp-modes-corpus.py --seed-jsonl <seed>.jsonl \
    --out <corpus>.jsonl --limit 1000 \
    --blog-pages 9 --blog-nodes 150 --forum-nodes 150 --kldp-delay 10

tools/autoswitch-gates.py pin --corpus <corpus>.jsonl --out <pin>.json --name <name>
tools/modes-metrics.py --corpus <corpus>.jsonl --lib build/libhunmin.so \
    --sim build/hunmin-sim --keytable build/sin3-p2.keytable.json \
    --json-out <run>.jsonl
tools/autoswitch-gates.py check --step <name> <run>.jsonl.meta.json \
    --pin <pin>.json [--line Q U]... [--flip-baseline <step>] \
    [--report-out <gates>.meta.json]
```

`ctest modes-gates-selfcheck` proves the guards bite: a negative pool share, a
wrong-direction switch in either direction alone, a stale pin, broken harness
arithmetic, an unmet G2 line, and five wrong ways to buy a G3 flip drop (no drop,
a drop at a different pool share, more flickering words, a flip below the guard,
a report with no `flips` block) each turn the checker red. It needs no corpus and
no build.
