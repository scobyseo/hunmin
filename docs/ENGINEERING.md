# hunmin — engineering practices

How to work in this repo, learned the hard way. Not architecture and not
product rules — those live in `AGENTS.md` (invariants), `PLAN.md` (phases),
`docs/SPEC-P2.md` (normative behavior), `docs/TESTING.md` (tiers).

This file is the **process** layer: how to investigate, when to believe a
result, what to write down. Append to it whenever a mistake costs real time.

---

## Ground rules

**Verify with the project's own command.** `ctest --test-dir build` is the
arbiter. An ad-hoc script is a supplement, never a substitute — and if a
script contradicts `ctest`, `ctest` is right.

**Never claim green you did not see.** If a test is red, say so plainly and
say why. A skip is not a pass; a timeout is not a failure; a test you could
not run is not a pass either.

**State what is unverified, every time.** "The script runs in 6s under a
shell but hangs under ctest" is the useful report. "Tests pass" is not.

**One root cause per commit.** If you are fixing A and discover B, either
bundle them when the commit message explains why, or leave B staged. Do not
let an unrelated fix ride along unremarked.

**Revert changes whose premise turned out wrong.** A plausible-looking edit
made on a bad hypothesis is still a liability. If removing it does not help,
revert it rather than leaving it as a comment about a dead theory.

## Investigating

The recurring error classes are also encoded as a skill
(`verify-before-asserting`), with per-rule status and a record of which rules
are expected to retire. Read it before investigating a third-party API or
writing a checker.



**Grep the source before trusting an API.** Three of my wrong turns this
session (`fcitx5-remote -o` vs `-s`, a `xim.conf` key that does not exist,
`FCITX_DATA_DIRS` semantics) were assumptions a one-minute grep would have
refuted. Each grep took a minute; each wrong guess cost ten. When the
dependency source is on disk, read it.

**Cheapest command that discriminates between hypotheses, first.** For
"is the IM active at all?", `fcitx5-remote -n` beats reading any log. Build
the habit of ordering checks by cost, not by how interesting they look.

**Absence of evidence in a filtered log is not evidence of absence.** I
raised `FCITX_LOG_RULE` for a tag, saw nothing, and nearly concluded the
code path never ran. The tag is the **source file basename**, and I had the
wrong name. Check that your verbosity is actually pointed at the file.

**When a run's symptoms change between identical attempts, suspect the
environment, not the code.** A stale `Xvfb :95` from a previous session
held a display number and turned one bug into two different-looking ones.
`pgrep -a -f 'Xvfb|fcitx5|xim-harness'` and `ls /tmp/.X*-lock` before
believing any e2e result.

**Redirection vs pipes changes behaviour of e2e harnesses.** `script | grep`
hangs; `script > file` returns. When a harness behaves differently under
ctest than by hand, suspect fd/pipe inheritance before logic.

**Watch for your own tooling as the bug.** A probe script of mine lost a
variable across a `bash -c` boundary and I spent a long time debugging
fcitx5 for it. When the answer is "the tool attached to the wrong
display/nothing at all", verify the tool before the target.

**Timeouts are not safety nets.** ctest's `TIMEOUT` never applied because a
grandchild held the output pipe. Always confirm termination yourself.

**When you hit a wall, count how many guesses it took.** If the third
attempt at the same subsystem is still wrong, stop and re-read the source
from scratch rather than trying a fourth variant. This session: CMake scope
propagation, a Ctrl+space toggle, a missing ibusfrontend dependency, a
fabricated `IHunminConfigurable` interface — four plausible-sounding guesses,
all wrong, each cheap to have refuted with a grep.

**A symptom that changes between identical runs means the environment.**
Same test, same code, two different failures — look for leftovers
(`pgrep`, `/tmp/.X*-lock`, an X server holding a display number) before
touching the code.

**Your ad-hoc verifier is not more trustworthy than the thing it checks.**
Twice this session a verification script misread its own target — counting a
mention inside a comment as a call, matching a column header instead of a
table row — and reported FAIL for code that was correct. When the script and
the code disagree, suspect the script first. And when an ad-hoc script is
all the evidence you have, fall back to the canonical command rather than
patching the script until it agrees with you.

**A defect report is a claim; check who owns it.** The profile's per-item
`Layout=` looked like an fcitx5 core bug. It is not: fcitx5 stores it and
`InputMethodGroup::layoutFor()` exists precisely so addons can read it. Grep
for the accessor before writing "upstream is broken".

**A keystroke for a given jamo is an automaton search, not a table lookup.**
`sin3-p2*` key values are `case` trees over D/E/F/T/P/J plus the wWzHh
flags, so one key id emits a vowel or a consonant depending on the slots
already filled — measured: `F`→ㅏ alone, `f`→ㅍ alone, `cF`→ㄱㅏ after a
cho, `cf`→ㄱㅍ again. Jong never comes from a single key at all, only from
a consonant pressed when D and E are already filled. Inverting that by
hand (`resolve_default` walking the `else` chain) silently drops every
state-dependent branch and produces a keystroke the engine never renders
the same way twice.

So do not write a jamo→key map for these layouts. Search the automaton
instead: `tools/sin3p2-ksgen.py` does BFS over engine states with
`hunmin-sim --batch` as the oracle, shortest keystroke per target,
deduping nodes on *rendered output* rather than key string (two
keystrokes that leave the same visible state are the same node). It takes
a `--text-file` and returns the keystrokes for every syllable in it —
which is exactly what a golden built from a text needs.

Two traps that cost time before the tool existed:

- **A per-key process spawn is 3.9 ms and makes the search take hours.**
  `--batch` amortises the cost (~869k evaluations/second); use it.
- **`target in output` is the wrong success test.** BFS reaches 가 as
  `nkf`, which renders `ㅅ가` — the syllable is present only because a
  bare ㅅ committed in front of it, so the keystroke types ㅅ first. The
  hit must be the *whole* final preedit (or the whole commit when
  nothing is composing), which is what `exact_hit()` checks.

Reverse-inversion is still fine for layouts whose key values are plain
constants — that is why the first attempt at the 훈민정음 preamble golden
worked at all on `dubeolsik` and failed on `sin3-p2-yet`.

**Fix the defect, then prove the test catches it.** Adding a test and having
it pass proves nothing on its own. Revert the fix, confirm the test fails,
restore. Same for guard scripts — `tools/frontend-mutations.py` exists so
the guard's own claims stay falsifiable.

## Scope and honesty

**Say when you are out of depth.** Hitting a wall inside a third-party
system's internals is a legitimate stopping point; guessing past it is not.
Two sessions of "almost" cost more than one honest "here is where it ends
and here is what we know".

**Do not leave a test red without a written reason.** `engine-d3-x11` ran for
weeks red **on purpose** — hunmin was silently not loading, so the trace
recorded nothing and the test passed vacuously. That reason is written down
three times over: `docs/D3-ROOT-CAUSE.md`, `docs/FCITX5-ADDON-NOTES.md` §11,
and `docs/TESTING.md`. It is **fixed and green** as of 2026-10-03; the red is
history, kept here because the *reason* is the lesson and the reason outlives
the redness. A red test someone else has to re-diagnose is a defect in the
team, not the code.

> Corrected 2026-10-05. This line said "`engine-d3-x11` currently fails on
> purpose" while `docs/TESTING.md` said green (2026-10-03),
> `docs/D3-ROOT-CAUSE.md` said "Status: FIXED", and `PLAN.md` said the whole
> suite passes — the drift this same file warns about forty lines below,
> recurring in the file that names it.

**Distinguish "scaffolded", "runs", and "green".** D3 went through all
three; conflating them is how a test suite starts lying about itself.

**A test that cannot fail is worse than no test, because it reads green.**
⚠️ 2026-10-03: D1/D2/Tier C `dlopen` the addon instead of linking
`engine.cpp`, and `copy-addon` only copied the `.conf` — so
`make testhunminui` never rebuilt the library after an adapter edit. Every one
of five planted mutations survived a green suite. Nothing in the output
distinguishes "the test cannot fail" from "the test has nothing to catch", and
the second looks like a design decision while the first is a lie. Fixed with
`add_dependencies(copy-addon hunmin_addon)` and now asserted by invariant I10,
but the habit to keep is: **when a mutation survives, suspect the wiring before
the assertion.** Verify by touching the source and watching the artifact's
mtime, not by re-reading the test.

**Check what a fix broke before declaring victory.** After every change,
run the full suite, not just the test you touched. Measure wall-clock too —
D3 went from an infinite hang to 6s and that number was the whole story.

## Writing it down

**Record the debugging, not just the conclusion, and correct the record when
the conclusion turns out wrong.** ⚠️ 2026-10-03: this section used to hold up
"a note that says `FCITX_DATA_DIRS` gets `/fcitx5` appended" as its example of
a useful debugging note. It was the *cause* of a bug, not a neutral example —
that belief is why `run-d3.sh` staged the addon one level too deep and
`engine-d3-x11` ran for weeks with hunmin silently not loaded (every stock
addon loaded, so it read as a filter problem). The truth is the opposite:
entries are used **verbatim**. An example that survives being wrong is a
liability, so examples here are drawn from observations that were re-checked,
and the wrong turns get named rather than quietly deleted.

The general form is still right: a note that also says *what the symptom was
and what refuted the guess* saves the next person the search. The failure mode
is silence, so record the silence — and when the silence turns out to have been
your own false note, say that too.

**Record the wrong turns, explicitly, with what refuted them.** Three
guesses this session (CMake scope propagation, a Ctrl+space toggle, a
missing ibusfrontend dependency) were each plausible and each wrong. Naming
them is what stops the next session from re-deriving them.

**Put knowledge where it will be looked up.** Host quirks go in
`docs/ENVIRONMENT.md`; third-party API semantics in
`docs/FCITX5-ADDON-NOTES.md`; verification structure in
`docs/TESTING.md`. Cross-link from `AGENTS.md`.

**Refresh status lines when reality moves.** `PLAN.md` phases and
`ENVIRONMENT.md`'s present/missing lists are only useful while they are
current. Both had drifted when this session started.

**A figure in prose is a claim that will expire.** This repo's drift is
mechanical, not careless: 2026-10-09 found fifteen documents asserting work
was undone that the tree had finished, and at least three of those rested on
a *measurement that was itself wrong* — a `grep -c` run against the wrong
path, a trace count, a test count. Nothing noticed, because nothing checked.
So: prefer "`ctest` is green" to a bare count (ENGINEERING.md's own example,
written as the thing not to do), and when a number has
to be written down, write **the command that produces it** next to it.

**Mark a conditional as conditional.** "Candidate assertions remain impossible
until P3 adds candidates" was a correct prediction that outlived its subject by
days. Predictions belong with a link to whatever will falsify them.

**Re-verify on the merge, not on the write.** Two sessions documenting one tree
will disagree if they start from different checkouts — during this pass the
base moved under us. A doc edit that touches a count gets re-checked at merge
time.

`tools/docs-lint.py` mechanises the first one and runs in ctest; the full
remaining-work list is `docs/REMAINING-WORK.md`.

## Before committing

- Full `ctest` run, with wall-clock, in this session.
- **No concurrent `ctest` runs.** The fcitx5/Xvfb/dbus test stack does not
  support parallel invocations — a second `ctest` kills the shared fcitx5
  daemon and makes D1/D3/conformance tests abort. Before running, check:
  ```sh
  ps aux | grep -E 'fcitx5|Xvfb|dbus-daemon' | grep -v grep
  ```
  If another session is testing, wait or use a separate build dir with
  `-DBUILD_TESTING=OFF`.
- `git status` clean of stray temp files, build dumps, and vendor tarballs.
- Commit message explains *why*, names the evidence, and states what is
  still broken. Do not write "fixed X" where X is only scaffolded.
- If a scratch/probe script was needed to find the bug and is not part of
  the fix, delete it.
- Re-read the diff for changes made on a premise later disproved.
- If a guard script or test was added, confirm it fails when the thing it
  guards is broken. A guard that has never rejected anything is a comment.

## Worktree cleanup (when a unit of work finishes)

Worktrees are the default work unit (AGENTS.md "Worktree discipline"). Cleanup
is **conditional** — the sequence is:

```sh
# 1. Can master take this right now? ctest green in the worktree?
ctest --test-dir build --output-on-failure
# 2. Will this worktree be reused for the next task?
#    Yes → keep it. Reuse beats cleanup when master cannot accept the change.
# 3. Both fine → merge to master, then remove:
git -C ~/work/<repo> merge --no-ff <branch>
git worktree remove ~/work/<repo>-<task>
git -C ~/work/<repo> branch -d task/<id>-<slug>
```

**Never** merge into a master that would be red — that poisons every later
session's baseline. **Never** delete a dirty worktree; that is a decision only
the maintainer makes. Report the state and leave it.