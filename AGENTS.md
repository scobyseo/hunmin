# AGENTS.md — hunmin

Portable Hangul (한글) input engine: a C library with a stable ABI, and thin
frontends for fcitx5 and Android (a Windows TSF frontend is planned and does not
exist yet).

**Read this before contributing.** It is short on purpose. Anything about *this
machine* — toolchain locations, host quirks, what lives outside the repository
— is not here; it is in the workspace file named at the bottom.

## The one idea

**Layout behaviour is data, never code.** Adding a keyboard layout means adding a
JSON file and golden tests: no C, no engine rebuild. The engine knows how to
compose syllables and buffer an eojeol; it does not know what a keyboard is.

## Hard rules

- **So: a layout change stays inside `libhunmin/data/` and `tests/golden/`.**
  New *engine* behaviour needs justification in `docs/SPEC-P2.md`, and the
  default answer is to express it with the existing key primitives (`Literal |
  SlotJamo | Ambiguous | Sequence`) plus rule tables and per-layout flags — not a
  new branch in the engine.
- **No per-layout anything in the core.** No per-layout struct fields, no
  per-layout `switch`, no numeric layout ids in the ABI — strings only.
- **The ABI is append-only.** Accepting a new symbol is a deliberate act:
  ```sh
  nm --defined-only -g build/libhunmin.so | awk '{print $NF}' | grep '^hunmin_' | sort -u \
    > libhunmin/tests/abi/symbols.txt
  ```
  There is no `make abi-accept`; the `abi` test prints this command when it fails.
- **Never copy upstream code.** kime is GPL-3: read it for semantics, write the
  behaviour facts into `docs/` and golden vectors, copy nothing. libhangul
  upstream is frozen — no patches to the system libhangul, ever; we ship beside
  it.
- **pat.im/1136 and its `ist.xml` files are normative for the P2 layout
  specifically.** Where a P2 layout conflicts with an implementation, pat.im
  wins and the conflict is recorded in `docs/SPEC-P2.md`. No other layout has a
  normative source. Do not hand-edit `data/layouts/sin3-p2.json`; change
  `tools/ist2data.py` and re-run its selfcheck.
- **Commits keep `ctest` green**, and data changes must pass `tools/doctor.py`
  (which is also ctest `schema-contract`). No test is red in a fresh worktree
  by design; the dump-dependent data selfcheck was unregistered 2026-10-17
  (maintainer instruction — revisit it at its own dev stage), so its
  self-contained converter guard is ctest `krdict-fixture`. Check the baseline
  anyway before believing you caused a red run.

## Guidelines

Not hard rules, and they bend when the case demands — a rule you cannot weigh
is a rule you stop reading.

- **Frontends stay thin**, under roughly 700 lines: event mapping and
  presentation. Logic that grows in a frontend belongs in the core. The number is
  a smell, not a budget to shave a test to fit.

## Commands

```sh
cmake -B build && cmake --build build          # engine + hunmin-sim + tools

# Tests. Everything is labelled; select rather than grep for names, because the
# Android corpus test names are generated from filenames and break silently.
# -L/-LE take an UNANCHORED regex, so -L tier-D selects D1/D2/D3.
# -j is this machine's core count, not a rule; drop it on a smaller box.
ctest --test-dir build --output-on-failure      # the full suite
ctest --test-dir build -N -L tier-D            # list a selection, run nothing
ctest --test-dir build -j16 -L tier-A          # "I touched the engine"
ctest --test-dir build -j16 -LE slow           # the inner loop: everything cheap
ctest --test-dir build -j16 -LE mutation       # nothing that rewrites your tree
tools/ctest-select.sh -L tier-D                # prints what it runs AND what it
                                               # will not, then runs it
tools/ci-time-budget.sh build                  # measure the suite against the
                                               # time budgets, BEFORE pushing

## Time budgets, and why they are rules

CI minutes are a cost, and a job with no ceiling is a job whose cost is
unbounded — the same overspend shape that got CI usage suspended, one level
down. So the budgets are enforced, not aspirational:

| What | Budget | Where |
|---|---|---|
| inner loop `ctest -j16 -LE slow` | **30 s** | here |
| slow tier `ctest -j16 -L slow` | **60 s** | here |
| the `linux` CI job, wall clock | **3 min** | `.github/workflows/ci.yml` |

The 3-minute job ceiling is met by two levers, both measured in
`docs/CI-TIMING.md` §5: excluding tier E1 from the linux job's suite (ctest
269 s → ~37 s, because the `android` job already runs those tests) and caching
the fcitx5 5.1.13 install tree (197 s on every commit with nothing cached →
~10 s on a cache hit). Together the job is ~78 s against 180 s.

**On a cache MISS the job cannot fit** — the fcitx5 build alone is 197 s. That
is the cold path, paid once per tag bump, and it surfaces as a timeout rather
than a silent slow run. The fix then is to let the cold build finish and cache
it, not to raise the number.

`tools/ci-time-budget.sh` measures all three and fails when one is over. Run
it before pushing anything that **adds a test**, because that is the change
that spends the budget and the one whose cost is invisible until CI is red.

**A budget is not a reason to drop coverage.** When a new test pushes a budget
over, the first move is to make the test cheaper — fewer fixed sleeps, fewer
cases, a tighter selection — not to leave the behaviour unasserted. A bug fix
whose regression test cannot be afforded is a fix that will regress again. If
the test genuinely cannot be made cheap, it belongs in the `slow` tier and the
budget line moves with a measurement recorded next to it, not by feel.

**Local and CI do not measure the same suite, and the script says so rather
than hiding it.** tier E1 registers only where `ANDROID_HOME` is set, so a
host without the Android SDK never sees its ~232 s; tier D3 skips on the
runner image, so CI never pays for what local runs. A green local run is
therefore not by itself evidence about the CI budget — read the delta the
script prints.


./build/hunmin-sim --data libhunmin/data --layout dubeolsik --keys 'dkssud ' [--trace]
./build/hunmin-sim --data libhunmin/data --check tests/golden/ref-dubeolsik.jsonl

python3 tools/kbd2data.py --orig data-orig/libhangul --out libhunmin/data
python3 tools/extract_tables.py                # regen src/tables.{c,h} from libhangul
python3 tools/ist2data.py                      # sin3-p2 / sin3-p2-yet, with selfcheck
python3 tools/doctor.py libhunmin/data         # data lint
python3 tools/docs-lint.py --build-dir build   # guards figures quoted in prose
python3 tools/wiki-lint.py

# Tier C: regenerate the conformance corpus from a tier A golden, then re-run the
# gate. Review diffs; never blind-accept (docs/trace-spec.md).
cmake -B build -DHUNMIN_TRACE_BUILD=ON
python3 tools/gen-conformance.py --golden tests/golden/p2-2-1-basic.jsonl \
    --platform x11 --layout sin3-p2-basic --case basic-syllables \
    --out tests/conformance/x11/basic-syllables.trace.in
python3 tools/trace-check.py tests/conformance/x11/basic-syllables.trace.in \
    --runner build/frontends/fcitx5/test/trace-emit --emit
```

Test counts vary with the configuration (Android SDK, JDK, NDK, trace build), so
no bare count belongs in prose. `docs-lint` checks that it has not crept back in.

## Layout

```
libhunmin/     core C + data/{layouts,compose,hanja,krdict,idioms}/ + tools/
frontends/     fcitx5/ (C++ addon), android/ (Kotlin IME + JNI);
               android builds with Gradle, NOT the top-level CMake
tests/         golden/ conformance/ e2e/ manual-checklists/
libhunmin/tests/abi/symbols.txt   the ABI symbol list lives under libhunmin/
wiki/          how we got there: session records, overturned verdicts, decisions
```

## Licences, and data that is deliberately absent

Bundled data carries its own terms, recorded per directory in
`libhunmin/data/*/LICENSE.md` and in `debian/copyright`: `hanja/` is BSD-3 from
libhangul, `idioms/` is MIT, and **`krdict/` is CC-BY-SA derived from the
국립국어원 한국어기초사전** and is kept in its own directory so the ShareAlike
term does not reach the rest of the data.

Two rules follow from this and they are not optional:

- **An artefact without retrievable provenance is not evidence.** When a tool
  produces data, that tool writes the provenance next to it — source, method,
  what was excluded and by how much, and what the artefact does *not* represent.
  A `meta.json` edited afterwards has already drifted from the tool.
- **Bulk corpora live outside this repository**, for licence and size reasons.
  Do not commit them, and do not hardcode a path to one into a committed tool —
  take it as an argument or derive it from the script's own location.

## Working rules

- **Never decide on your own, and never write the verdict down.** A negative
  result is a finding; "therefore reject it" is a decision, and the maintainer
  makes it. The same rule seen from the writing side: a record of measurements
  is a contribution, a ruling pre-empts the person entitled to make it. Equally,
  a ruling is not closure — it authorises implementation and verification, not
  the end of measuring what it did not cover.
- **Check the maintainer's claims as carefully as your own.** Stated premises get
  verified too. Twice a stated premise did not hold — that commercial software
  works a particular way, and that a symbol ends composition — and both were
  worth checking.
- **The maintainer's words are the requirement, not your paraphrase.** Quote them
  where the exact wording matters, and check the quotation against what was
  actually said. A paraphrase drifts, and the drift is what gets built.
- **The purpose of an analysis is to find the optimum design, not to judge one
  candidate.** Present designs as comparable; leave the choice open. Analysis
  that concentrates on whether one design is right has missed its job.
- **Mine the data before adding a dependency.** When something already on disk
  could answer the question, measure it rather than importing something new.
- **Do not argue that a requested feature is unnecessary.** The answer to "should
  we" is what it costs and how it compares, never a verdict. Absence of prior art
  is a gap, not a judgement.
- **Document first, and never skip it.** Undocumented work gets repeated.
- **Record what you got wrong, so it is not repeated** — including that a past
  mistake may later be the right call; say so, and say what would change it.
- **Keep the documents consistent, and ask when two disagree.** Where two
  documents contradict, do not pick the newer one — the older may encode a
  decision, or the newer measurement may be wrong.
- **Commit per phase.** A commit carries the measurements and the still-failing
  parts, not only the diff.
- **Give choices clearly, with a recommendation** and the deciding reason in one
  sentence. Say what evidence would change it.
- **Hand over procedures as files, not as prose in chat** — anything a person
  would type goes in a script under `tools/` or `~/.local/bin/`.
- **Write in English unless the content is specifically Korean.** This is about
  prose: documents, comments, commit messages. It is **not** about data files —
  layout tables, the hanja dictionaries and the corpus extracts are Korean text
  and stay that way. Technical terms, paths and identifiers stay in English
  regardless.
- **Hanja only when it is the subject matter.** The hanja-conversion feature may
  show `(ㄷ,旦)→旦`, a pat.im citation, a glossary entry. Hanja characters in
  ordinary English or Korean prose are contamination — a term that happened to be
  known in Chinese leaking through.
- **No invented abbreviations.** If an institution has no common short form,
  write the full name — `국립국어원` / National Institute of Korean Language,
  never `NIKE` or `NIKL`. `docs/GLOSSARY.md` is the reference; `docs-lint`
  warns on the two wrong forms because this file names them in order to ban them.

## The wiki

`wiki/` is **not** a second copy of `docs/`.

- **`docs/` stays authoritative.** A figure that lives in `docs/MODES-DATA.md` is
  cited from there with a provenance marker. Never write the same number in two
  places — a second home is how they drift apart.
- **`wiki/` holds how we got there and what we decided**: session records,
  overturned verdicts, open decisions.

A retracted page stays, marked — deleting it destroys the evidence that the
mistake was made. Every page needs frontmatter, at least two `[[wikilinks]]`, an
entry in `index.md` and a line in `log.md`; then run `tools/wiki-lint.py`, which
checks all of that mechanically.

**Decisions are the maintainer's** — see Working rules above. Record
measurements, not rulings, and see
`wiki/decisions/decisions-belong-to-the-maintainer.md`.

## GitHub issue tracking

Use GitHub Issues as the working surface and external coordination point; PRs are
the merge surface. The repository `scobyseo/hunmin` has hasIssuesEnabled:true,
hasProjectsEnabled:true, hasWikiEnabled:false, no issue templates yet, and a
`task` label for tracked work, so this section defines how work becomes an issue
and how the status board stays auditable. **Issue counts do not belong here** —
read them from GitHub; a number in this file is a second home for a fact that
changes without a commit, which is the disease `docs/REMAINING-WORK.md` §7
describes.

**The one idea.** An issue carries a measurement or a request for a ruling, not a
decision. GitHub is where work is assigned, claimed and closed; what this repo
calls a verdict lives in `docs/` and the wiki, and only the maintainer makes it.

**Scope of the policy.**

- **Agent-measurable items** — ctest outcomes, golden/test paths, linter results,
  data-format drift, docs counts. These become issues whose body states the
  command and the expected tree fact, and they are closed only by the maintainer
  after a PR that satisfies them.
- **Maintainer-gated items.** Rulings, decisions and phase boundaries (the
  outstanding B1–B9 set) are not agent-closable. They stay as issues only so a
  reviewer can see what was requested and what was measured; the decision itself
  is never recorded in an issue.
- **Environment-bound items** — device tests, signed-release builds, emulator runs
  with no CI substitute. Listed as environment-bound with the missing measurement
  named; they are not "open" so much as "cannot be measured here".

**Issue lifecycle and status.**

| State | Meaning | Who closes |
|---|---|---|
| `open` | Claimed, work not started | assignee, or anyone |
| `in progress` | Worktree checked out, branch active | assignee when done |
| `blocked` | Decision-gated, environment-bound, or awaiting a measurement | assignee |
| `closed` | Verification passed, decision rendered by the maintainer | maintainer |

Use the existing label set (`bug`, `enhancement`, `documentation`, `question`,
`help wanted`, `good first issue`, `duplicate`, `invalid`, `wontfix`) and add
`task` for tracked work items. Do not invent a new label taxonomy — a label
should match exactly one in-scope category.

**Title format.**

```
[<scope>] <brief>
```

`<scope>` is `engine`, `layout`, `fcitx5`, `android`, `docs` or `ci`. Examples:
`[engine] sin3-p2 dual-slot key missing compose test`, `[android] missing
RECORD_AUDIO permission request`.

**Issue ↔ worktree linkage (the handoff contract).**

- A task's worktree, branch and issue exist together: `task/<id>-<slug>` on
  `master`, issue `<scope>-...`, worktree at `~/work/hunmin-<task>`.
- A PR closes at most one issue; the PR body states `Closes #N`.
- Closing an issue is never part of a single "fix" commit — the commit sequence
  is: worktree + branch + issue → work → PR → issue closed by the maintainer.
  `Fixes #N` in the PR body links the PR to the issue; the issue itself is closed
  only after verification has passed.

**Evidence and audit.**

- Every task issue carries a **Verification** checklist naming the command or
  tree fact that would prove it, e.g. `ctest --test-dir build -j16 -LE slow`, or
  `grep -n "HUNMIN_AVD" tools/android-e2e.sh`.
- The in-repo status board `docs/REMAINING-WORK.md` keeps its machine-readable
  assertion table; `tools/docs-lint.py` check 6 parses it. A claim in an issue
  that has a tree-fact basis keeps a matching assertion row — don't write the
  same item in both places with conflicting verdicts. That's the "no second home"
  rule: issue ↔ board ↔ tree must agree, and the check is mechanical.
- Commits on a task branch link to the issue (`Fixes #N` in the body, or the
  branch name `task/<id>-<slug>` that the PR references). A PR closes at most one
  issue.

**Concurrency.**

- One task, one worktree off `master`; branch `task/<id>-<slug>`. Never start a
  second task in a tree that already has an open issue — a claim is not a task
  until its branch and worktree exist.
- A task's worktree is cleaned up only when the diff would leave `ctest` green
  on master and nothing else is planned — the merge-if-master-can-take-it rule.
  A dirty-but-unmergeable worktree is a finding, not a cleanup target.

**Template.**

- **task** — tracks work; body: scope, verification command, link to the branch.
- **bug** — reproduction steps, expected vs actual, failing ctest label.
- **question** — request for a ruling; body must quote the exact wording the
  maintainer is asked to decide.

**Where this comes from.** Results measured, not rulings. The section above is
written so that no agent ever records a verdict it is not entitled to.

## Orchestration — a second agent on this repository

One agent per checkout is what the Worktrees rule below protects. When the
session runs inside an Orca terminal, Orca's orchestration layer is how a second
agent is made safe: it owns placement, authority and settlement, and it records
which attempt is authoritative.

- **`docs/ORCHESTRATION.md` is the procedure**; `tools/orchestrate.sh` is that
  same sequence as a script, carrying this repository's defaults — branch
  `task/<id>-<slug>`, `--base-branch master`, the issue number. Read the doc
  before improvising a command; the flag surface is one
  `orca agent-context --json` call away and guessing it is what costs turns.
- **A Task spec names the acceptance command, not the topic.** "Fix the
  keystore path" is not a task; "`grep -n storeFile …` shows only the
  `local.properties` reference" is. Section "The task spec" of that doc maps a
  kind of change to the command that settles it.
- **The dispatch preamble already tells the worker to send `worker_done`.** Do
  not repeat the settlement protocol in the spec; do correct the branch, the
  issue and the acceptance command there.
- **A worker never closes anything.** Not the issue, not the PR, not a decision
  (Working rules). It reports; the maintainer disposes.

## Worktrees

One task, one worktree off master, so concurrent sessions cannot collide.

- Branch `task/<id>-<slug>`; delete the worktree when the task is done.
- **Cleanup is conditional, not automatic.** Is the diff in a state master can
  take — would `ctest` be green if merged right now? Will the worktree be reused?
  Only delete when both hold. A dirty-but-unmergeable worktree is a finding;
  deleting it is a decision.

## This machine is not in this file

Host facts, toolchain locations, corpora kept outside the repo, and the working
conventions for the surrounding workspace are in the **workspace** `AGENTS.md`
one directory above this checkout. They are deliberately not here: they describe
one machine, not the project, and they change without anyone editing the code.