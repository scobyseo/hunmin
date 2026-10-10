---
title: The rules keep breaking — what this wiki is for
created: 2026-10-04
updated: 2026-10-10
type: postmortem
status: active
tags: [postmortem, status, tooling]
sources:
  - raw/sessions/2026-10-04-stemming-investigation.md
  - ../decisions/decisions-belong-to-the-maintainer.md
confidence: high
---

# The rules keep breaking, and the failure is state, not prose

The maintainer, 2026-10-04:

> "지금은 계속 규칙이 깨지고, 병렬 작업이 제대로 진행되지 않고 있어"
> "내가 너에게 전달한 내용이 계속 사라지고 있어"

Two skills were written that day
(`recording-what-you-built-and-checking-the-doc`,
`cross-document-consistency`) and both broke inside the same session. The
question this page answers is why writing down a rule did not help.

## The shape of every failure

Not one of these is a writing error. Each is **state that was true when written
and stopped being true**, with nothing recording the transition.

| what broke | mechanism | would a wiki page have caught it? |
|---|---|---|
| stemming verdict reversed | a measurement on a second corpus contradicted the first | **yes** — a second measurement is a second source |
| "31,253 forms" | an extract's size quoted as its source's | **no** — this needs a converter read |
| the institute called `NIKE` in tools and `NIKL` in docs | drifted across files, then "corrected" twice without checking | **yes** — `contradictions:` on two pages, and the glossary entry that ends it |
| 어미 656 / 조사 184 | a distinct-count read as a row-count | **no** — needs re-measurement |
| a `void` section still cited | retraction not propagated | **yes** — this is `wiki-lint`'s mechanical check |
| three corpora built from `/tmp` | provenance not attached to the artefact | **yes** — `raw/` + provenance in `meta.json` |
| `89,234.` read as a lost figure | the verifier was wrong and believed | **no** — this needs a falsifiable tool |

**Three of seven would have been caught by page state, and none by prose
quality.** That is the case for the wiki, stated honestly: it is a partial
instrument, and three of the failures need a different tool (a converter-read
discipline, a re-measurement, a checker that can be wrong).

Three of the seven have since been given one, and the fix is in `AGENTS.md` and
`docs/` rather than here:

- **`NIKE` → `NIKL` → back again, in one day.** Both spellings were already
  wrong: neither is the institute's name. The rule that ended it is *no invented
  abbreviations*, written into `AGENTS.md` and `docs/GLOSSARY.md`.
- **어미 656 / 조사 184** are 596 and 161. And 121 of the survey's 722 endings
  are written with a leading 받침, which a vowel-only stripper drops silently —
  including ㅂ니다.
- **Three corpora built by scripts in `/tmp`.** Two producers had to be
  recovered from `/tmp` the following day, and one of them had a bug that wrote
  nothing after six minutes of correct work. Provenance now travels with the
  artefact; see [[the-three-corpora-are-three-registers]].

## The recurring one: a test that was green while measuring nothing

Written down on 2026-10-04 as a table of unrelated slips, it turned out six days
later to be the project's single most repeated failure. Five separate
instruments reported success while measuring something else, and **in every case
the count stayed right and nothing errored**:

| what | what it measured instead | record |
|---|---|---|
| `engine-d3-x11` — "Passed" in 5.01 s | 1 of its 8 cases; the XIM harness drained the runner's case list off stdin | [[2026-10-10-d3-case-coverage]] |
| `android-mutations` — "all 9 caught", exit 0, 0.31 s | the runner's skip exit, read as a catch; nothing was built | [[2026-10-10-mutation-tiers-measuring-nothing]] |
| the GTK tier — every case green | `XMODIFIERS=@im=fcitx` had routed GTK's XIM module to the same fcitx5, so the GTK path was never exercised | [[2026-10-09-gtk-forward-path]] |
| `cases.tsv` — "no pass-through keys, by design" | a harness gap: no `forward_event` callback, so the tier could not see the one key in dispute | [[2026-10-09-where-space-is-emitted]] |
| the sandbox copy filter — "0 MB, 100% saved" | it was **empty**: `shutil`'s `ignore` callable must return the names to exclude, and the version written returned the ones to keep | `tools/frontend-mutations.py:164` |

Two defences exist and both are mechanical: **a tier must assert the number of
cases it ran** (the Android tiers do; the D3 tiers did not, which is how finding
one exposed that the one case that ran was the one that could not inherit state
from a previous one), and **a filtered run must print what it omitted**
(`tools/ctest-select.sh`), because a green selected run is otherwise
indistinguishable from a green full one.

The rule behind all five: **a check that cannot fail must be shown capable of
failing before its output is believed.** A check passing and what that check
checks are separate things. The last row is the purest form of it — the
replacement filter was built three times before it was right, and only a **file
count** caught the version that excluded everything, so `_check_copy` now
asserts a control path (`libhunmin/src`) survives: a filter that excludes
everything must not be able to pass by being vacuously correct.

## Why the two skills failed the same day

Both skills said the right thing. Neither was **load-bearing**:

- `recording-...` said to write provenance. The corpora's provenance went into
  `/tmp` scripts, then a chat reply, and only later into `meta.json`.
- `cross-document-consistency` said to diff numbers. The first diff tool
  reported **three files as having lost figures when none had** — the checker
  was wrong in both directions at once, and the fix was to exclude sentence
  punctuation from the number regex.

A rule that is not enforced at a point where it would have to fire is a
preference. The rules that survived this session are the ones attached to a
command: `tools/wiki-lint.py`, `docnums.py --from-git`, `--compare`,
`build-modes-corpus.py --selfcheck`. The rest are in prose and were ignored.

## The parallel-work failure

> "병렬 작업이 제대로 진행되지 않고 있어"

Measured, not guessed:

- A delegated research batch (2 subagents) returned **no output and no
  running process**; the delegation id exists only in conversation context, so
  a failed dispatch is indistinguishable from a slow one from the outside.
- Delegated translations were verified by hand afterwards and **two had lost a
  figure each** while still running.
- A snapshot taken from a working tree **mid-edit** was corrupt as a baseline,
  because another agent was writing the files it was measuring.

The common cause is that nothing about a dispatch was **recorded where the next
step could see it**. A `raw/sessions/` entry per dispatch, written by the
dispatcher, is the mechanism the skill lacked. See
[[decisions-belong-to-the-maintainer]] for the standing instruction that this
session also broke.