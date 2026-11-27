---
title: "T2's direction: three proposals, and the gap between them and the issue tree"
created: 2026-11-27
updated: 2026-11-27
ingested: 2026-11-27
type: session
status: active
tags: [modes, t2, design, scope, expectability, josa, session]
sources:
  - ../../../docs/SPEC-MODES.md
  - ../../../docs/MODES-MEASUREMENT.md
  - ../../../docs/MODES-GATES.md
  - ../../../docs/MILESTONES.md
  - ../../../docs/SPEC-FRONTEND.md
  - ../../../docs/SPEC-ANDROID-KEYPAD.md
  - ../../../libhunmin/include/hunmin.h
  - ../../../tools/modes-mixed-tokens.py
  - https://github.com/scobyseo/hunmin/issues/282
  - https://github.com/scobyseo/hunmin/issues/284
  - https://github.com/scobyseo/hunmin/issues/295
confidence: medium
---

# T2's direction: three proposals, and the gap between them and the issue tree

## 0. Why this page exists

The maintainer stopped the G3 work and named the worry behind it, verbatim
(2026-11-27):

> "내가 기대하는 건 이런 아이디어들을 제안하고, 구현해서 실험해 보는 걸 T2의 주요
> 태스크로 생각하고 있었는데 그 방향으로 진행하지 않는 것 같아서, 작업을 다 마쳤을
> 때, 엉뚱한 결론을 내고 있을까 걱정된다."

That is a scope finding, not a style preference, and it is checkable: the issue
tree is in the repository, and so is what the work produced. §2 checks it. §1
records the proposals so they exist as words rather than as a conversation. §4
shows what would grade them, because that is where the "wrong conclusion" would
come from. No ruling is recorded here — the options are the maintainer's to
choose, and §6 says what would change each one.

## 1. The three proposals, in the maintainer's words

Given as one frame plus three items (2026-11-27, verbatim). The frame:

> "자동 변환 기능이 제공했으면 하는 기능 중, 중간 변환이 적었으면 하는 건, 이런
> 방식으로 개선할 수 있지 않을까 생각하고 있어."

1. > "다른 언어로의 입력이 가능하다는 것이 판단되면, 현재 입력 내용을 바꾸는 게
   > 아니라 다른 방식으로 이 대안이 있어요~ 하고 표시한다. 예를 들어
   > 내쭈(hello) 처럼 표시하는 거지. toast를 사용하거나, 자동 추천을 사용하거나,
   > 다양한 방법이 있을 것 같아."
2. > "지금처럼 모든 상황에서 변환을 하는 게 아니라 tab을 누르는 경우만 변환을
   > 하는 것도 방법일 수 있어. 이건 선택 기능으로 추가하면 어떨까? 결국 나중에는
   > 자동 추천 기능을 구현할 생각이니까, 그 기능의 일부로 생각할 수 있을 거야."
3. > "단순히 단어 단위로만 하는 게 아니라 영단어+한글조사 조합도 허용해야 해."

They are not three settings of one design; they change different things. Read
against what the engine may touch today (`judge_boundary` delivers text,
`take_judgement` asks the frontend for a mode change, `judged_display` changes
the screen), each proposal moves a different cell:

| | changes the screen | changes the composing text | changes the delivered text | changes the mode | who triggers |
|---|---|---|---|---|---|
| today (`docs/SPEC-MODES.md` ReqMode8/E6) | yes, every key (E6) | no | **yes**, at the boundary | **yes**, via `take_judgement` | the judgement, always |
| (1) hint | yes, as an *alternative* | no | no | no | the judgement offers; the **person accepts** |
| (2) on an explicit key | no change until asked | yes (or the delivery) | only when asked | only when asked | the **person presses** |
| (3) stem + particle | no | maybe | **a mixed string** (`IBM은`), with **no mode change** | no | the judgement, at the boundary |

Three consequences worth stating before any code:

- (1) and (2) remove E6's flicker **by construction** — they never change the
  input mid-word. G3's gate is then either trivially satisfied or vacuous
  (§2.3).
- (2) is not only a trigger: while the word is still composing the engine holds
  its keys, so "convert what I just typed" is a one-key repair that needs no
  backspace and no retype (`ReqMode11` is the requirement it would serve). That
  is a cheaper `k`, which §4 prices.
- (3) does not want a mode change at all: `IBM은` is one string in the person's
  own word, and the next word is presumably Korean. It is a *reading* the
  judgement does not have today (one eojeol = one language), and delivering it
  is a text operation, not a switch.

## 2. The gap between the expectation and the issue tree

### 2.1 What T2's issues asked for

Is **#284**'s scope, verbatim:

> "Improve auto-switch to fire *more correctly*, not *more*. As shipped the
> feature costs keystrokes (negative q), because it abstains on about half of
> transitions and every abstention delivers the wrong script. Wrong-direction
> rate is clean precisely because judgement rarely fires. Success stays R11/R12's
> keystroke reduction with repairs priced in."

and its Decisions list is eleven items, every one of them a *measurement*
decision (target corpus, aggregation, assumed repair cost `k`, whether a repair
counts as an intervention, two or three numbers, data-absent precondition,
numeric G1–G4 lines, R9 re-basing, second-dictionary shape, `word_cap`/
`judge_timing` settings rows, app preedit tolerance).

**#295** (G3, this lane) then states its own scope and verification:

> "Reduce mid-word display flips at equal pool share (E6 flicker…)" … "Mid-word
> flip count reduced at equal pool share on pinned corpus."

So: "improve" was written as **"fire more correctly inside the shipped design"**,
and every child issue is a gate over that design. Proposing a different design,
prototyping it and comparing the prototypes appears **nowhere** in the T2 tree —
not in #284, not in its four children, not in the Decisions list. The claim in §0
checks out, and the cause is mundane: nobody wrote the three proposals down, so
the only instructions in the tree pointed at measurement. The work followed them.

### 2.2 What that produced, and what of it survives

The G3 work (#295) is exactly what its issue asked: a flip unit, a counter, an
equal-q gate, and an option that steadies the per-key display. Under proposals (1)
or (2) the screen stops showing the judgement at all, so:

| piece | under the shipped design | under (1)/(2) |
|---|---|---|
| `tools/modes-metrics.py`'s `flips` block | the G3 measurement | **survives** — it measures *any* display design, including "0 changes because nothing is shown" |
| `autoswitch-gates.py --flip-baseline` (equal q, fewer flips) | the G3 gate | survives as a regression guard, but grades a design nobody may want |
| `judge_hold` (E6 stability knob) | reduces flashes 80% | **dead weight** — nothing to steady if the input is never rewritten |
| `judge_hold`'s run counter (consecutive agreeing keys) | internal to the display | possibly **reusable as confidence**: "apply when the verdict held N keys, hint otherwise". Unexposed today, so this is a hypothesis, not a plan |
| the golden cases `e6-*` | pin the per-key display | pin today's design; they would be *deleted*, not updated, if E6's display changes |

That is the honest answer to "would we have concluded something irrelevant": yes
for the knob and the e6 goldens, no for the counter and the gate. Recording it
here is the point — a future session should not have to rediscover that G3 was a
gate on a design under discussion.

## 3. The surfaces T1 and T3 already built (the maintainer's point, checked)

The claim was that the usability work is happening in T1/T3. It is, and the
inventory says exactly which part:

| surface | where it exists today | what a T2 proposal would reuse it for |
|---|---|---|
| candidate list the **host** draws | fcitx5 (its own panel), IBus `IBusLookupTable` (page size 9, `frontends/ibus/src/hunmin_engine.c`), TSF's own window | (1): offer the other-language reading as a *candidate* — accepting it is already a host gesture, and nothing in the person's text changed |
| a gloss beside the candidates | FE16 option A, implemented (#319): the highlighted candidate's 뜻풀이 rides the host's auxiliary text | (1): the same field can carry "이 대안이 있어요" without a new surface |
| the IME's own composing text | Android draws its own preedit and its own Hanja candidate overlay (`OverlayActivity`) | (1): an inline hint is cheap where the IME owns the pixels; Linux hosts would need the candidate table instead |
| a settings store with live apply | FE17 (#327): five rows — Hangul layout, Latin layout, commit unit, automatic-switch value, language-key list | the *mechanism* is built; **`judge_timing`/`judge_hold` are not among the five rows**, so T2's own settings rows are still unbuilt |
| a key the engine owns | the E5 special-function namespace (`HUNMIN_SPECIAL_VOICE`, `_AUTOSWITCH`, `_LIMIT`), bound per frontend, refusable | (2): "convert now" can be a third function on a chord the engine resolves, instead of a host `Tab` whose meaning belongs to the panel |
| gestures as a trigger vocabulary | KP14's long swipe (delete units 키/음절/단어/전체), #312's symbol swipes (open) | (2) on Android: no `Tab` key exists on the soft keypad, so the trigger has to be a gesture or a keycap |
| cheap deletion | KP14: word/all units, **while composing** | not a T2 lever — see §4: a misfire is delivered by design (ReqMode10), so it is *no longer composing* when the person notices |

Two honest corrections to the overlap claim: (a) T3's delete units do **not**
lower T2's repair cost, because the word they would delete has already left the
buffer; (b) T1's five rows are not T2's rows — the store is reusable, the rows
are not built.

## 4. What each proposal would be graded by — and why G1–G3 cannot grade them

G1 grades `q`, and `q` is defined (`docs/MODES-MEASUREMENT.md` §2.1) for one
structure: the judgement delivers, the person repairs what it got wrong.
`q = a − (2k+1)r`. Take the recorded runs' own `a`, `r`, `k` and change only the
repair cost — arithmetic on recorded inputs, not a new run:

| repair cost per misfire | kldp-772 | wiki-2500 |
|---|---|---|
| today, `2k+1` (backspace k, 한/영, retype k) | −4.288 | −6.350 |
| a word-level delete, `k+2` (one gesture, 한/영, retype k) | −2.569 | −3.924 |
| a free repair, `c = 1` (nobody can actually do better) | **+0.115** | **−0.202** |

(kldp-772: `a` 0.597, `r` 0.482, `k` 4.568; wiki-2500: 0.446, 0.648, 4.742; the
model `q` is quoted because that is the formula's own answer — the runs' measured
`q` is beside them in `docs/MODES-GATES.md`.)

The reading: **the repair cost is not the lever, the misfire rate is.** Even a
repair that costs one key leaves wiki-2500 negative, and kldp-772 barely above
zero. So T3's cheaper deletion cannot rescue the shipped design, and proposals
(1)/(2) are not competing with T3 — they attack `r` by never delivering the wrong
word.

They also cannot be graded by G1 as it stands, because their outcome is not a
keystroke delta:

| proposal | what it changes | the unit it needs | what G1 would read |
|---|---|---|---|
| (1) hint | offers an alternative; text untouched | offers shown, offers accepted, **cost of a wrong offer** (a glance ignored, not `2k+1` keys) | undefined: `t` counts transitions, not offers |
| (2) on a key | the person asks; the word converts before the boundary | keys per transition with repair `≈1`, wrong-delivery count (`r`) | `q ≈ 0` by construction (one press per transition) — G1's K4 no-net-loss passes trivially |
| (3) stem+particle | delivers a mixed string, no mode change | share of mixed tokens delivered correctly; **no transition is involved** | not measured by `q` at all — it is not a pool |

So a re-scoped T2 needs one new unit per proposal, and the honest thing to say is
that B2's pass-line shapes do not cover them. Which is also why grading the
shipped design harder (G1–G3) was never going to answer the maintainer's question.

## 5. Where the stem+particle population stands

Measured, and it is not small: `docs/MODES-MEASUREMENT.md` §1.5 — 0.27% of words,
2.8% of *sentences* in a 300-page dump window, with five particles covering 92% of
occurrences. The important half is a negative: the sentence corpora G1/G2 run on
**drop exactly this population** (`mixed_word` in the builder's drops), so no
existing run has ever seen it. A corpus with these tokens kept would have to be
built before (3) can be priced, and §1.5 says what it would take.

## 6. Next step: three experiments, priced

Each is a prototype plus a measurement, not a ship. Sizes are what the surfaces
in §3 say, not estimates of effort:

| option | smallest prototype | what it would measure | what would kill it |
|---|---|---|---|
| **A. hint** | fcitx5 or IBus: publish the other-language reading as the *first candidate* (the host already draws and returns candidates). Nothing new in the engine except exposing the reading the judgement already computes | offers shown / accepted / ignored per transition; wrong-delivery count → 0 by construction; the *cost of a wrong offer* has to be assumed, so state it as an assumption, not a number | a wrong offer that lands in the text when accepted, i.e. the person reads the hint as a correction — measurable in the same replay |
| **B. explicit trigger** | the E5 mechanism: one more special function (`CONVERT`), Android first (it owns its keypad) or fcitx5 (its config dialog already binds chords) | presses per transition, `r` and `k` (should be 0 backspaces, 1 key), and whether a person can *see* when to press — that last one is not measurable without a person, which is why the hint and the trigger are one design in practice | if the convert cannot be offered while the word is composing, the whole saving disappears (state that in the prototype's first test) |
| **C. stem+particle** | engine: split the key run at the Latin/Hangul boundary, look the stem up in the shipped English list, and deliver the mixed string without a mode change. Particle list as data, five entries to start (92% of §1.5) | tokens delivered correctly out of the §1.5 population; no new corpus needed for the *unit*, only for the rate | if a mixed delivery has to enter through `take_judgement`'s mode channel, it needs a text-only answer the ABI does not have today — that is the first thing to check |

**Recommendation, one reason:** start with **C**, because its unit is settled (a
token either comes out right or not), its population is measured and currently
invisible, and it needs no new user gesture — whereas A and B are one design
(hint + accept) whose unit is an assumption (the cost of a wrong offer) until
someone uses it.

**What would change that:** if the maintainer's priority is E6's flicker rather
than coverage, A/B move first, because C does not touch the display at all; and
if the five-row settings store is the intended home for the options, B needs a
sixth row that FE17 does not have.

## 7. What this page does not do

It records the proposals, the gap, the inventory and the arithmetic; it decides
none of it. Re-scoping T2 (from "grade the judgement" to "compare judgement
designs") is the maintainer's call, as is which experiment runs first, and the
`judge_hold` work already committed stays as the design it was asked to steady
until that call is made.
## 8. Prior art, and what it changed (later the same day)

The maintainer asked for the cases, so `docs/MODES-PRIOR-ART.md` now records six,
read from sources rather than blogs: Mozc's `ms-ime` keymap, Rime's
`recognizer/patterns`, and the Korean IMEs on this machine. Three facts from it
change §6's plan:

- **Mozc only converts when asked** (`Space`/`Henkan`/`Tab` = `Convert`/
  `PredictAndConvert`), and it can re-render the *composing* run in another script
  with one key (`F9`/`F10` = full/half-width alphanumeric). Proposals (1) and (2)
  are shipped practice, not inventions.
- **The mechanism (2) needs already exists in our own tree for the Hanja
  question**: one key, the composing word, a candidate list, in-place replacement
  (`hunmin_suggest_*` → `commitCandidate`; `ibus-hangul`'s `hanja_list` is the same
  shape). So (1)/(2) are a *re-wiring* of a built path, which is cheaper than §6
  priced them.
- **Reconverting committed text is the expensive variant** (surrounding text, which
  this tree's notes already record as unreliable across applications). Converting
  while the word is composing needs nothing extra.
- **No Korean IME on this machine has an automatic script switch** — so this
  feature has no Korean precedent to match, and its shape is a design decision.
- **Rime handles mixed Latin/Chinese with a pattern**, offered as a candidate, not
  with segmentation — the cheaper shape for (3).

One premise was corrected before it became prose: `Tab`'s meaning in *our* Linux
hosts while a candidate list is open is **not measured anywhere in the tree**, so
the survey says so and names the capture that would settle it, instead of
asserting it.

## 9. What this lane's issue should become (proposal, not a ruling)

`#295` as written grades one design. The proposal, for the maintainer:

1. **Keep the measurement, drop the pass line**: `tools/modes-metrics.py`'s `flips`
   block and `autoswitch-gates.py --flip-baseline` are design-independent — they
   measure any display, including "nothing is shown". They stay.
2. **Make G3 a regression line, not the track's success**: it binds whichever
   display ships. `#295`'s verification becomes "the display the shipped design has
   does not flicker more than the recorded run", not "flips fall".
3. **`judge_hold` is held, not shipped**: it stays as an option at its 0 default,
   and if proposals (1)/(2) win it is deleted rather than carried. Its run counter
   is the one part worth keeping in view (a possible confidence signal).
4. **A new issue for the comparison**, whose verification is per design, not per
   gate: offers shown / accepted / ignored for the hint; presses and wrong
   deliveries for the explicit trigger; mixed-token accuracy for the pattern — and
   each prototype built on the Hanja candidate path (`docs/MODES-PRIOR-ART.md` §4).

Nothing above is applied: `#295`'s body still carries its original verification, and
the re-scope is the maintainer's to make.

## 10. Hand-off: what a new session needs to continue

The maintainer paused this lane to type for a while and collect the log; this
section is what a fresh session reads first. (The measurements live in
[[2026-11-27-personal-rewrites]]; the open decisions are D5–D9 in
`wiki/decisions/the-outstanding-decisions.md`.)

**Where the work is.** Branch `0.4-t2-3-g3-expectability`, worktree
`~/work/hunmin/0.4-t2-3-g3-expectability`, 16 commits on `master` (G3's engine
option and gates at the bottom, then the trace work, the collectors, the intent
model, the enable script). `git status` is clean.

**What is runnable now.**

| piece | command | state |
|---|---|---|
| flip counter on a corpus | `tools/modes-metrics.py … ` / `--judge-hold N` | green (G3) |
| G3 gate | `tools/autoswitch-gates.py check --flip-baseline` | green; fixture in `tests/fixtures/modes-gates/g3/` |
| rewrites on text (a corpus or shell history) | `tools/modes-rewrites.py --history … --mode latin` | 214/3,379 on the maintainer's history |
| flakes of the *screen* from a live log | `tools/personal-log-flips.py --log LOG` | ready, waits for a log |
| intent classes from a live log | `tools/personal-log-words.py --log LOG` | ready, waits for a log |
| enable the live log | `tools/ibus-trace-enable.sh enable` | fixed (rename, not write-through) |

**What is blocked, and on what.** Nothing in the code: the log only needs the
maintainer to run

```sh
sudo tools/ibus-trace-enable.sh enable     # installs /usr/local/lib/hunmin/* and wraps /usr/bin/hunmin-ibus
ibus restart                               # spawns the wrapper; logging starts with the first key
```

and then their typing lands in `~/.local/state/hunmin/key-log-<date>-<time>-<pid>.jsonl`
(one file per engine process, mode 600). The first run failed on "Text file busy"
because the engine being replaced was the engine running; `put()` now renames over
it, and the partial state is re-runnable. An apt upgrade of `hunmin-ibus`
overwrites the wrapper — logging stops silently and `enable` must be re-run.

**What the new session does first, when logs exist.**

1. `tools/ibus-trace-enable.sh status` — is logging on, which files, how many lines.
2. `tools/personal-log-words.py --log <newest>` — the intent table: how many words
   were clean, how many the engine rewrote and the person kept, how many they
   **undid** (the discomfort the model names), and the examples per class.
3. `tools/personal-log-flips.py --log <newest>` — the flicker count and the same
   keys replayed at `judge_hold` 0/1/2.
4. Bring the per-class word lists to the maintainer for review — they are the
   failure set the three proposals are then measured against, case by case.

**What NOT to repeat** (the two drifts this lane already made): do not turn the
maintainer's three proposals into a measurement programme (the purpose paragraph
in `docs/MILESTONES.md` §0.4 Track 2 and §2.2 here), and do not let a gate on the
shipped design stand in for choosing a design (D8).

### The first real `enable` broke typing entirely — cause and fix (2026-11-28)

What happened: after `enable`, Korean could not be typed at all. The log recorded
it exactly — `"layout":""` in meta and **every** event `consumed:false`, 0 preedits,
~1,300 of them — and the engine declined every key because it had no state.

Cause, reproduced without a bus: a **dev build** compiles
`HUNMIN_DEFAULT_DATA_DIR=${CMAKE_INSTALL_FULL_DATADIR}/hunmin`, which for the
worktree's default prefix is `/usr/local/share/hunmin` — a directory that does not
exist. With no data, `hunmin_ctx_new` returns NULL, `hunmin_state_new` returns NULL,
and `hunmin_state_key` answers `declined()` for every event. The packaged lib has
`/usr/share/hunmin` compiled in, which is why the packaged engine never showed it.
Check: the installed lib with no `HUNMIN_DATA_PATH` → ctx NULL; with
`HUNMIN_DATA_PATH=/usr/share/hunmin` → `sin3-p2-basic` loads and composes.

Three fixes (commit `5043157`): the wrapper exports `HUNMIN_DATA_PATH`
(`HUNMIN_TRACE_DATA` overrides); `enable` refuses to install without a data dir
that has `layouts/`, and **preflights** the installed lib the way the engine
starts (`hunmin_ctx_new(NULL, 0)` must be non-NULL), restoring the packaged engine
before dying if it is not; and the engine now warns in the journal when no layouts
loaded instead of declining everything silently.

The lesson for a hand-off: an installed dev engine is not the packaged one — its
compiled paths are the build's, and `status` should have been read as
`"layout":""` the first time. The three log files from that window are void.
