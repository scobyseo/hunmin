# Wiki Schema

Conventions for this wiki. **Adapted from the `llm-wiki` skill's schema, with
three deliberate departures** recorded in §Deviations, because this is a code
repository rather than a research vault.

## Domain

The hunmin portable Korean input engine: architecture decisions, measurement
records, postmortems, and the reasoning that connects them. Not a mirror of
`docs/` — see §Relationship to docs/ below.

## The problem this wiki exists to solve

Stated by the maintainer, 2026-10-04:

> "지금은 계속 규칙이 깨지고, 병렬 작업이 제대로 진행되지 않고 있어"
> (rules keep breaking, parallel work is not proceeding properly)

Seven failures were recorded in the session that created this wiki: a verdict
reversed between corpora, an extract's size quoted as its source's, an
institution called by two invented abbreviations and "corrected" twice without
checking, three corpora with no reproducible producer, a superseded conclusion
still cited, a distinct count read as a row count, and a verification tool's own
false positive believed. **They are listed with mechanisms and with which of
them a page could have caught in
`concepts/the-rules-keep-breaking.md`.** None of them is a writing error: each
is state that was true when written and stopped being true, with nothing
recording the transition. That is what a wiki is for.

## What belongs here, now that `docs/HISTORY.md` exists

Two things, and a reader who came for either can find both quickly:

1. **What was decided and why** — `decisions/`, and the `superseded` pages that
   carry the reversals.
2. **How this project has been wrong before** — the `raw/measurements/` records
   and the failure tables inside them.

A **session that happened**, a page that was edited, and a question that has
since been answered are none of those, and are recorded by `docs/HISTORY.md` or
not at all.

## Conventions

- File names: lowercase, hyphens, no spaces (`hangul-latin-mode-guessing.md`).
- Every page opens with YAML frontmatter (below). Required on `concepts/`,
  `entities/`, `decisions/`; `raw/` uses its own smaller block (§Raw sources).
- **`[[wikilinks]]`, minimum 2 outbound per page.** Cross-linking is the
  mechanism; an unlinked page is invisible.
- Every page appears in `index.md` under its type, and `index.md`'s page
  count and date are updated in the same commit.
- Every action appends to `log.md`. It is append-only.
- **Provenance markers**: a claim synthesised from 3+ sources ends its
  paragraph with `^[raw/measurements/<file>.md]`.
- Bump `updated:` on every edit. A page whose `updated` predates a source that
  contradicts it is stale and the lint reports it.

## Frontmatter

```yaml
---
title: Page Title
created: YYYY-MM-DD
updated: YYYY-MM-DD
type: concept | entity | decision | measurement | postmortem | query
status: draft | active | superseded | void
tags: [from the taxonomy below]
sources: [raw/measurements/foo.md]
confidence: high | medium | low     # how well-supported the claims are
contested: true                     # unresolved contradictions ON this page
contradictions: [other-page-slug]   # pages this one conflicts with
supersedes: [other-page-slug]       # for status: superseded
---
```

### `status` is the field this wiki is for

The vocabulary is **taken from what the documents already use**, not invented.
Measured across `docs/*.md` before this schema was written: `[D]` ×22, `[E]`
×17, `~~strikethrough~~` ×6, `NOT MEASURED` ×3, `void` ×3, `UNDECIDED` ×2,
`MEASURED` ×2, `superseded` ×1 — nine conventions, no rule.

| `status` | means | replaces |
|---|---|---|
| `draft` | being written; do not cite | — |
| `active` | current; cite freely | `[D]`, `MEASURED`, `DONE:` |
| `superseded` | a later page replaces it; **the page says which** | `superseded`, `정정` |
| `void` | **never cite**; the numbers in it are wrong or the harness could not fail | `void`, `무효` |

**A `void` or `superseded` page stays in the wiki.** Deleting it destroys the
evidence that the mistake was made, which is the part worth keeping — the
postmortem convention in this repo has always worked that way.

**A retired page names its replacement by path, not by wikilink.** Four pages
do this today and it is deliberate, not an oversight. `tools/wiki-lint.py`'s
status-hygiene check builds `inbound` keyed by the link *target* but iterates
it as though the key were the *source*, so what it actually rejects is a
retracted page linking to a live one — the exact thing a "superseded, the
content went there" note is required to do. The paths carry the same information
and the wiki is clean either way. **If that check is fixed, turning these back
into `[[wikilinks]]` is the right cleanup**; until then, leaving them alone is
what keeps `tools/wiki-lint.py` green.

`confidence` is separate from `status` and answers a different question: `status`
says whether a claim is current, `confidence` says how well-supported it is. A
page can be `active` with `confidence: low`.

## Tag taxonomy

Add a tag here BEFORE using it.

- **Subsystem**: engine, ic, data, layouts, compose, hanja, romaja, modes, space
- **Frontend**: fcitx5, android, windows, e2e
- **Practice**: measurement, licence, provenance, tooling, testing, postmortem
- **Meta**: decision, question, contradiction, status
- **Scope**: scope
  A boundary on what a thing may be *used for*, kept separate from `decision`
  because a scope boundary is recorded rather than chosen: it constrains future
  measurements instead of answering an open question.

## Page thresholds

- **Create** when a subject appears in 2+ sources or is central to one.
- **Add to an existing page** rather than creating a near-duplicate.
- **Do not create** pages for passing mentions.
- **Split** past ~250 lines into sub-topics with cross-links.
- **Archive** to `_archive/` when fully superseded; keep the file, drop it from
  `index.md`, and leave a pointer.

## Relationship to `docs/` — the important departure

**`docs/` stays authoritative.** This wiki does not replace it and does not
duplicate it. The rule:

- `docs/` holds specifications, contracts, and the measurements a reader
  cites.
- `wiki/` holds **how we got there and what we decided**, plus the session and
  measurement records `docs/` has no place for.

A figure that lives in `docs/MODES-DATA.md` is cited from there, with a
`^[...]` marker pointing at the `raw/measurements/` record that produced it.
The wiki never becomes a second home for the same number — a second home is how
the two drift apart, which is the failure this wiki is for.

## Deviations from the `llm-wiki` skill's schema

Recorded because silently following a schema meant for research vaults would
have made this worse, not better.

1. **No `entities/` + `concepts/` split by kind of thing.** The skill splits by
   entity-vs-concept; here the useful axis is **decision-vs-measurement**.
   `decisions/` and `raw/measurements/` are the two that this project actually
   lacks, and `concepts/` holds the rest. Renaming a page's type as the project
   learns something is cheap; a wrong taxonomy is not.
2. **`raw/sessions/` is a first-class layer.** The skill's `raw/` is for
   external sources. Here the scarce record is *this project's own sessions* —
   which is what the maintainer asked to accumulate, and what git's commit
   messages do not capture in a queryable form.
3. **No Obsidian/Dataview requirement.** Wikilinks are used because they are
   cheap; nothing in the build or the tests depends on the vault being open in
   Obsidian.

### A fourth, added by the 2026-10-10 compression

**A page whose content has moved is kept, marked `superseded`, and carries a
table saying where it went.** It is not deleted and it is not silently demoted
to a stub: the new document needs the evidence that the old one existed, and
that is the whole mechanism §Status hygiene checks. The corollary, learned the
hard way — **status is about the page's claim, not about its length**. A long
page that still carries a current claim is `active` and points at the document
that owns the detail; a short page that no longer carries anything of its own is
`superseded`.

## Update policy

When new information conflicts with a page:

1. Check dates — newer does not automatically win; see `cross-document-consistency`.
2. If genuinely contradictory, record **both** positions with their sources.
3. Set `contested: true` and list `contradictions:` on both pages.
4. Set `status: superseded` on the loser, with `supersedes:` naming the winner.
5. Flag it in the lint report for the maintainer. **Do not resolve it
   silently** — the maintainer's standing instruction is that these decisions
   are theirs.

## Lint

`tools/wiki-lint.py`, run manually and in CI:

1. **Orphan pages** — no inbound wikilink.
2. **Broken wikilinks** — `[[x]]` with no `x.md`.
3. **Index completeness** — every page in `index.md` and vice versa.
4. **Frontmatter** — required fields present; every tag in the taxonomy.
5. **Stale** — `updated` older than a contradicting source.
6. **Contradictions** — pages with `contested: true`.
7. **Status hygiene** — a `void`/`superseded` page still cited from an
   `active` page. **This is the check that would have caught today's failures.**
8. **Log rotation** — `log.md` past 500 entries.

**A lint without a tool is worse than no lint** — it turns consistency into a
feeling. Check 7 exists because it is mechanical; checks 1-6 are advisory.

## Check

- [ ] Can I name the file the figure came from, and its `status`?
- [ ] Is this page reachable from `index.md` and by at least one wikilink?
- [ ] If it contradicts another page, are both recorded and flagged?
- [ ] If it replaces another page, does the loser say so and point here?
- [ ] Am I writing a number into the wiki that already lives in `docs/`?