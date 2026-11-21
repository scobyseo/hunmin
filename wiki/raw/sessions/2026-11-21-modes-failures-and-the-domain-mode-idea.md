---
title: Modes failures, the judgement principle, and the domain-mode idea
created: 2026-11-21
updated: 2026-11-21
type: session
status: active
tags: [session, modes, qas, judgement, shell, corpus, fcitx5, tsf]
sources:
  - ../../../docs/SPEC-MODES.md
  - ../../../docs/MODES-MEASUREMENT.md
  - ../../../docs/DESIGN-CORE.md
  - ../../../docs/DESIGN-FRONTEND.md
  - ../../../docs/architecture.md
  - ../../../libhunmin/src/buffered_preedit.c
  - ../../../libhunmin/src/wordset.c
  - ../../../libhunmin/data/modes/en-US.json
  - ../../../libhunmin/data/modes/meta.json
  - ../../../tools/sin3p2-eval.py
  - ../../../tests/golden/judgement-r8.jsonl
  - ../../../wiki/decisions/2026-10-05-closed-questions.md
confidence: medium
---

# Modes failures, the judgement principle, and the domain-mode idea

One session with two halves. First the maintainer restated what the auto-switch
feature is *for*, and four concrete failures were traced to code. Then a larger
idea — explicit domain modes — was priced and kept as a finding, not a ruling.

## 0. G0 retired

The maintainer ruled that the Track 2 G0 "reproduce the recorded baseline" gate is
meaningless. There is nothing to reproduce: the figures and corpus were removed on
2026-11-08 with `docs/MODES-METRICS.md`, and the judgement definition changed. The
gate is removed from `docs/MILESTONES.md` Track 2, and the problems felt in use —
the four failures in §2 and the observation in §7 — are placed there as input to the
next ruling instead. Issue #294 (the G0 issue) is left for the maintainer to close or
redefine; this session does not dispose of it.

## 1. The restated intent (distilled, not quoted)

The maintainer asked that their spoken sentences not be reproduced verbatim; the
intent, in refined form, is:

- The ideal for a mixed-script sentence is to produce the same text **without a
  single manual 한/영 press**. Pressing at every transition is the worst-case
  baseline, not the target.
- A wrong judgement is costly because a person must delete and retype a word, so
  wrong judgements must be absent — not merely reduced to an acceptable rate.
- The description of behaviour with the feature OFF was written as an aid to
  understanding the mechanism, not as an additional goal.
- Perfect script discrimination is the aim; a word a person deliberately types
  awkwardly is accepted as an unavoidable exception.

The earlier "100% success is not necessary" wording belongs to the keystroke
*pricing* discussion, not to this accuracy aim. Both statements stay recorded;
neither is retracted here.

## 2. Four observed failures

Each was reported by the maintainer from live use. What is measured below is the
code path and the data, not a reproduction run — no built simulator was available
in this worktree, so token-level composition claims are reasoning from recorded
key/jamo facts, not measurements.

| symptom | mechanism | recorded evidence |
|---|---|---|
| `자동변환` typed on sin3-p2 while in Latin mode is not recognised as Hangul | keys that are jamo on the Hangul layout are punctuation on the Latin layout; `cp_is_symbol` classifies the produced character, ends the word there, and the symbol key is never pushed into the slot log, so the Hangul reading never sees the whole word | `libhunmin/src/buffered_preedit.c` `cp_is_symbol`, symbol split, `keys_push`; `docs/SPEC-MODES.md` §ReqMode4 |
| `opencode` is not recognised as English | lookup is exact whole-token membership; the shipped list is hunspell en_US with compounds not expanded; `open` and `code` are each present, the concatenation is not | `libhunmin/src/wordset.c` `hm_wordset_has`; `libhunmin/data/modes/meta.json`; `docs/SPEC-MODES.md` §ReqMode8 |
| a shell command typed in Latin mode sometimes flips to Hangul | the E1 line switches when the token is absent from the dictionary **and** the same keys replay to well-formed Hangul; absence is read as positive evidence for the other script, and a jamo-only layout makes the structural Hangul test easy to satisfy | `libhunmin/src/buffered_preedit.c` `judge_boundary`; `libhunmin/src/hangul_valid.c`; `tools/sin3p2-eval.py` (no Latin output path on sin3-p2) |
| a `/update` slash command is unreliable | `/` is a mapped key; with the switch on the engine consumes it into the preedit, so the host never receives the raw key event that would open the command palette | `frontends/fcitx5/src/hunminadapter.cpp`; `frontends/tsf/src/keydispatch.cpp`; `docs/DESIGN-FRONTEND.md`; [[2026-11-01-modes-ownership-retires-the-detector]] records the related TSF punctuation class |

### Dictionary coverage probe (2026-11-21)

A direct membership query against `libhunmin/data/modes/en-US.json` (exact
lowercase token, no user dictionary):

- present: `git ls cd grep docker cargo python make echo cat apt brew ssh curl node java update`
- absent: `pwd sudo cmake mkdir chmod kubectl npm wget gradle adb ffmpeg opencode`

The pattern is arbitrary, not principled. The conclusion is **not** "add these
words": the defect is that dictionary absence is used as evidence for Hangul.

## 3. Why these happen — the common structure

Four layers of the same problem, each owning one failure:

1. **Span integrity** — the word judged must be the same key span in both
   readings. A delimiter in one layout must not truncate the other reading.
   (`자동변환`)
2. **Symmetric evidence** — "valid" must mean the same kind of evidence in both
   directions. Absence from a closed list is not positive evidence for the other
   language; a structural test and a lexical test cannot be paired as equals.
   (`opencode`, shell)
3. **Key ownership** — a key the host treats as a control key must reach the
   host even when the switch is on; "mapped" is not "consumable". (`/update`)
4. **Explicit exceptions** — a gap is filled by declared data (the user
   dictionary) or context, never by loosening the validity predicate.

These are candidate principles for a future requirement, **not a ruling**. They
are written here so the next design does not rediscover them.

## 4. Quality-attribute scenarios

Common to all scenarios: **Environment** = auto-switch enabled; **Artifact** =
the auto-switch module; **Response** = the sentence the user intended is
produced exactly.

| | Source | Stimulus | Measure |
|---|---|---|---|
| QAS-1 no manual switching | a person typing a mixed-script sentence | typing the sentence without pressing 한/영 | manual 한/영 presses per sentence = 0 (ideal); toggle share `u=1-a` alongside |
| QAS-2 no wrong judgement | a developer who expects accurate language discrimination | a judgement at an eojeol boundary | wrong judgements on judgeable eojeol = 0, per direction; repair cost `2k+1` stated as the reason |
| QAS-3 deliberate anomalies | a person intentionally typing an awkward word | a boundary where both or neither validity holds | that word excluded from the wrong-judgement count and the coverage denominator; absorbed only by declared user data |
| QAS-4 shell/commands | a developer at a shell prompt | a command token, flag, or path | unintended switches per command = 0; swallowed control keys = 0 |
| QAS-5 adaptivity | a developer moving between shell, code, and prose | a change of working context | wrong judgements after a context change = 0 |

QAS-1's zero is an ideal, QAS-2's zero is a hard predicate, QAS-3 is a scope
boundary, and QAS-4/QAS-5 are incomplete until a ruling defines their evidence.

## 5. The domain-mode idea (recorded, not decided)

The maintainer proposed imagining explicit working modes — shell command, C++
development, Python development, document writing — selectable by a shortcut,
informed by the words being typed, with recommendations drawn from earlier input.
They judged this "not a small thing" and asked for a separate analysis issue,
so **no design is adopted here**.

What the tree already says about it:

- A third mode value collides with the binary `hangul|latin` invariant in
  `docs/SPEC-MODES.md` §ReqMode2, `docs/architecture.md` §3 and the layout schema
  check in `tools/doctor.py`.
- A second judgement rule collides with ReqMode3's single engine-owned rule.
- A per-app or transient suppress state collides with the one-state, no-per-frontend-store
  rule in `docs/architecture.md` §3 and ReqMode13.
- The engine receives only `(base, shift)` and cannot see line starts, Enter, or
  the active application; neither fcitx5 nor TSF has a per-app hook.
- Input history has no home: the user dictionary is the only persisted word data,
  the engine never writes it, and each frontend keeps its own settings store.

Options compared (all left open for the maintainer):

| option | form | cost | what it preserves |
|---|---|---|---|
| A | domain mode as a real state, plus new ABI symbols and UI on three frontends | schema, ABI, UI and three hard requirements change | none of the above |
| B | a recommendation layer fed by input history, judgement rule left single | a new history store and a per-frontend data path | ReqMode3 in form only |
| C | domain evidence injected into the existing single rule (command-shaped tokens and declared user data) | evidence model and data source only | ReqMode2/3/13 and architecture §3 |
| D | record and defer | analysis issue only | everything |

The analysis recommended C on the grounds that ReqMode1's problem is not knowing
which mode one is in, not recognising the application. Evidence that would move
it to A or B: a measurement showing command tokens cannot be separated by shape
or declared data at an acceptable false-positive rate.

## 6. Re-opening a closed question

The maintainer asked to re-review the coding-corpus question that was closed on
2026-10-05 with "this was just curiosity, exclude it"
([[2026-10-05-closed-questions]] §2). Per the standing rule that a ruling is not
closure, this page records the request to re-review; it does **not** reverse the
earlier exclusion or record a new verdict. The separate analysis issue is where
that re-review is carried out.

## 7. An unverified observation: Alt+Enter at the end of a word

The maintainer reported, tentatively, that plain Enter now behaves correctly but
pressing **Alt+Enter** at the end of an in-progress word appears to move the text
being edited to the next line. This is recorded as an **observation, not a
finding** — nothing was reproduced or measured, and which frontend was in use
was not stated. The repository does not accept "probably" as coverage
([[2026-11-04-shared-edit-key-catalogue-was-executed]] §EK7).

What the code says would happen, per frontend:

- fcitx5: `Alt` sets `HUNMIN_MOD_ALT`, a chord bit; `classify()` returns
  `HUNMIN_KEY_BOUNDARY_PASS` before the named-key check, so the PASS branch
  flushes and returns without consuming (`hunminadapter.cpp`, `buffered_preedit.c`).
  The word commits and the host receives Enter as a newline.
- TSF: in the test phase the Alt-chord branch precedes `IsKeyOfPassage`, so an
  Alt-chorded Enter can be passed without the `DrainAndPass` flush that a bare
  Enter gets (`keydispatch.cpp`); a default-sink host may skip `OnKeyDown`
  entirely. The commit is therefore less certain here than on fcitx5.
- Android: a chord classifies as PASS, `commitWord()` runs, and the method
  returns false, so the client produces the newline (`HunminAdapter.kt`).

Commit-then-newline is the **ruled chord behaviour**
([[2026-11-01-syllable-commit-turns-autoswitch-off]]; `wiki/index.md` §chord), so
the open question is not whether the word commits but whether the newline
arrives before the commit and moves uncommitted text. The existing measured
Enter defect was the inverse — `안녕`+Enter+`d` produced `안녕ㅇ\r\n` instead of
`안녕\r\nㅇ` (`docs/TSF-INTERIM-INPUT-SPIKE.md`, issue #175's
punctuation+Enter cases in `docs/REMAINING-WORK.md`). Those were fixed; this
Alt-chord case was never tested — the engine's chord test and the Enter test
exist separately and are never combined (`libhunmin/tests/key-rule-test.cpp`).

Reproduction, frontend identification and a before/after trace are needed before
this can become a finding or an issue.

## 8. Personal typing diagnostics (design recorded, not shipped)

The maintainer confirmed the model for a typing log: it exists only in a personal
evaluation build, not in an official distribution; data is accumulated locally
in the maintainer's own environment and handed to the agent for analysis; there
is no path for collecting another person's typing. The privacy and consent
procedures that this implies are necessary. The existing opt-in sinks already
fit — fcitx5's build-time `HUNMIN_TRACE` compiled out by default, and Android's
broadcast-gated `HunminTraceRecorder` writing `filesDir/e3.trace` — so no new
shipped feature is needed.

The maintainer supplied the detection rule for "rework": a person deletes
characters with Backspace (or similar) and then types a **similar** input again.
The trace format already records the ingredients — BackSpace is a named key and
`commit`/`preedit`/`consumed` are per event (`docs/trace-spec.md` §`in`/§`out`) —
so "commit → BackSpace → similar commit" can be detected by analysis without
changing the trace schema. Judgement metadata itself is absent from the format
and is deliberately not added: an inference from the commit sequence is a
finding with lower confidence than a recorded judgement, and that limit must be
stated with any result.

A personal analysis tool reads such a trace and reports suspected rework
episodes. It is a diagnostic for one's own data, not a product feature, and it
carries its own provenance (source file, method, threshold, exclusions).

The maintainer's focus is faithful capture of *which key was pressed and what the
frontend did with it*, at the frontend level — not an engine-internal log, and not
environment metadata. The key path must lose as few key events as possible.

Two episodes are the representative cases the maintainer named explicitly:

- **the autoswitch-off correction** — a wrong switch is followed by turning the
  feature off (the Ctrl+한/영 chord) and retyping the word;
- **the manual 한/영 correction** — a wrong switch is followed by a 한/영 press
  that undoes it, then a retype.

In trace terms the chord and the 한/영 key are themselves keys with known
identities, so both cases are "a commit, then a boundary/mode key, then a similar
commit" episode. The tool must tag them separately from a plain BackSpace
retype, because they are the two paths ReqMode7 is about (manual intervention),
whereas a plain retype prices ReqMode11/12's repair.

A passed-through key is recorded too, as an event with `consumed: false` followed
by a `flush` line when the boundary delivered a word. Recording only engine-consumed
keys would lose the boundary cases — Enter, Alt+Enter, chords — that are exactly
where "behaved differently than expected" is reported. The maintainer chose
completeness over tidiness: every key reaching the frontend is recorded, including
Shift, F9 and candidate-window navigation, even where that needs a synthetic
`hunmin_key_event`. A missed key is where a bug hides, so the shared dialect covers
all three frontends (fcitx5, Android, TSF) with the same fields.

### The maintainer's governing principle for this divergence

The maintainer stated that frontends behaving differently on the same error is
not the desired state. The ideal is identical behaviour across all three
frontends; a difference is acceptable only where the environment forces it.
Spanning the Alt+Enter case, that means the three differing paths above are
**not acceptable as they stand** unless the difference is shown to be
environment-forced — and the missing test-phase flush on TSF is an
implementation difference, not an environmental one.

This is a reaffirmation of a rule the tree already carries, not a new one:
A2's "should support 3 frontends with identical behavior"
(`docs/SPEC-OVERVIEW.md`), ReqMode3's "Two frontends must never answer
differently on the same word" (`docs/SPEC-MODES.md`), and the exhaustive
"Everything else is not a permitted difference" in
`docs/DESIGN-FRONTEND.md` §3 "Allowed differences", which permits only
physical-key mapping, platform pass-through, and presentation/commit
granularity. No Alt+Enter or chord+Enter case is currently asserted across
frontends — the engine's chord test and Enter test are separate and per-frontend
(`libhunmin/tests/key-rule-test.cpp`), so this class of divergence is not
currently caught by a cross-frontend assertion. That test gap is the concrete
finding to carry forward; whether to close it is the maintainer's decision.
