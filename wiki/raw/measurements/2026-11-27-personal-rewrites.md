---
title: "The auto-switch on my own typing: 138 of 1,608 commands rewritten"
created: 2026-11-27
updated: 2026-11-27
ingested: 2026-11-27
type: measurement
status: active
tags: [modes, t2, measurement, rewrites, shell, trace, josa]
sources:
  - ../../../docs/MILESTONES.md
  - ../../../docs/MODES-MEASUREMENT.md
  - ../../../docs/SPEC-MODES.md
  - ../../../docs/trace-spec.md
  - ../../../tools/modes-rewrites.py
  - ../../../frontends/ibus/src/hunmin_state.c
  - ../../../wiki/raw/sessions/2026-11-27-t2-direction-and-the-three-proposals.md
confidence: medium
---

# The auto-switch on my own typing: 138 of 1,608 commands rewritten

Track 2's purpose (`docs/MILESTONES.md` §0.4 Track 2, recorded 2026-11-27) is to
find the design, and the maintainer asked for the concrete instances where the
automatic switch replaces what was typed — *"자동 변환으로 재작성 하는 사례"*.
The gates count these; this page lists them, from text the maintainer actually
typed on this machine.

## What was run

`tools/modes-rewrites.py` (new): drive the shipped layer over a person's own
text and emit one record per **rewritten eojeol** (`delivered != typed`), with the
judgement the engine acted on. The layer is the oracle — same `Layer` object
`tools/modes-metrics.py` uses, so "rewritten" means the same thing in both tools —
and the start mode is the model's free parameter: a shell command is typed in
**Latin** mode, Korean prose in **Hangul** mode.

| run | source | mode | layout |
|---|---|---|---|
| shell | `~/.zsh_history`, 1,608 commands (EXTENDED_HISTORY, multi-line joined) | Latin | sin3-p2-basic + us-qwerty (the maintainer's stored rows) |
| Korean | 105 quoted Korean sentences from this wiki (the maintainer's own words) | Hangul | same |

The shell artefact is `~/work/hunmin-measurements/rewrites/shell.jsonl` — outside
the repository, as `~/.zsh_history` must be. The tool redacts secret-shaped
strings before anything is written or printed (5 masks in this run); the artefact
never leaves the machine and no history line is quoted here.

## Shell history: 1 in 12 commands damaged

| | count |
|---|---|
| commands read | 1,608 |
| Latin tokens typed in Latin mode | 3,379 |
| **rewritten tokens** | **214 (6.33%)** |
| … replaced by a judgement the engine acted on | **149**, from **52 distinct tokens** |
| … tokens after a switch on the same line, typed in the wrong mode | 65 |
| **commands carrying at least one rewrite** | **138 (8.6%)** |

Every one of the 65 "silent" tokens follows an earlier switch on the same line
(checked: 65/65, and every affected line begins with a judgement rewrite). They are
the *consequence* of the first rewrite: the mode moved, so the rest of the command
is typed in Hangul. One cause, two symptoms.

The offenders are ordinary commands and paths, not typos:

| token | rewrites | what it became |
|---|---|---|
| `opencode` | 60 | Hangul |
| `kime` | 10 | Hangul |
| `pwd` | 8 | `퍟` |
| `mpv` | 8 | Hangul |
| `~/.config/kime/config.yaml` | 5 | partly Hangul |
| `ibus-hangul` | 3 | `무뎨-hangul` |
| `nano`, `lsusb`, `lsof`, `libdbus-1-dev`, … | 2 each | Hangul |

`pwd` is the smallest case and the one to keep: three keys, past ReqMode9's guard,
`pwd` is not an English word and its Hangul reading is a valid syllable — so the
engine switches. `hunmin-sim --judge --start-latin --keys 'pwd '` answers `퍟`,
`judged=hangul`, which is the oracle agreeing with the collector.

## Why it fired: 100% of these words are missing from the dictionary

Every one of the 149 judgement rewrites is a token the shipped English list does
not contain (`libhunmin/data/modes/en-US.json`, 118,150 entries): 149 of 149, and
none of them a word the list knew. The corpus's harm is therefore **coverage
first, rule second** — the judgement said "not a word of mine, but its Hangul
reading is valid syllables, so switch", and that is the rule working as ruled.

| rank | token | rewrites | cumulative |
|---|---|---|---|
| 1 | `opencode` | 60 | 40% |
| 2 | `kime` | 10 | 47% |
| 3 | `pwd` | 8 | 52% |
| 4 | `mpv` | 8 | 58% |
| 5 | `~/.config/kime/config.yaml` | 5 | 61% |
| 6 | `ibus-hangul` | 3 | 63% |
| 7-10 | `nano`, `lsusb`, `https://pi.dev/install.sh`, `libdbus-1-dev` | 2 each | 68% |

Ten tokens account for 68% of the rewrites, and the mechanism to fix them already
ships: `hunmin_buffered_preedit_add_user_word`. Verified on this build — `pwd`
alone still judges `hangul` and delivers `퍟`; with `pwd` added as a user word the
engine keeps it and delivers `pwd`. So one experiment Track 2 can run before any
design change is "the same corpus with the top ten words in a user dictionary":
it prices the coverage lever against the design levers on the same text
(`hunmin_buffered_preedit_add_user_word` + `tools/modes-rewrites.py`).

## Korean side: the engine abstains, and the word is lost anyway

105 quoted Korean sentences (940 tokens) → **6 rewrites (0.64%)**, and **none of
them carried a judgement**: the engine delivered something other than the typed
word *and stayed silent*. Five are Latin tokens inside Korean text that the
judgement did not rescue (`B3`→`ㅜ3`, `IM`→`…`, `us`→`뎨`), one is a Hangul token
the replay typed after an earlier switch. Two model caveats belong to the number:
the replay encodes a Latin capital as (lower, shift) on the Hangul layout, where a
person typing `B` would be in Latin mode anyway, and a digit token is held by
ReqMode6. Read as: **the Korean side lost 0.6% of words to abstention, not to
rewriting** — ReqMode8/E1's gap, not E6's.

## The live engine can log it now too

The replay answers "what would the engine do to this text"; the next question is
"what did it do while I typed". The IBus personal key log (`docs/trace-spec.md`
§"Personal key log") had no field for the judgement, so a log could not tell a
typed Hangul word from a rewritten one. It has one now: `judged` on the event line
(`latin`/`hangul`, the same spelling `hunmin-sim --judge --trace` uses), set at the
one place the engine acts on a judgement (`apply_judgement`), compiled out of
shipped builds like the rest of the sink, and asserted by ctest
`engine-d1-ibus-trace-judged`.

To use it: `cmake -B build-trace -DHUNMIN_TRACE=ON`, then
`tools/ibus-real-use.sh portable` — the script copies the trace engine into a
private prefix and sets `HUNMIN_TRACE_OUT=<prefix>/key-log.jsonl` with
`HUNMIN_TRACE_RAW=1`. That is a **private** session (Xephyr + its own daemon); the
live desktop is not touched, and logging the live desktop would mean replacing
`/usr/bin/hunmin-ibus`, which is a decision, not a procedure.

## What this does not represent

One person, one machine, one layout pair, one dictionary. zsh history is what was
typed *at a prompt*, not prose; the Korean side is 105 quoted sentences, not a
sample. Nothing here is frequency-weighted, and the collector does not judge
whether a rewrite was wrong — a Latin→Hangul rewrite of a word typed in Hangul mode
is correct behaviour, which is why every record says which way it went.

What it does settle: the harm Track 2 exists for is not hypothetical. It is 8.6% of
one person's commands, it is concentrated in a handful of words, and `opencode`
alone accounts for 60 of them — which is why the design alternatives in
[[2026-11-27-t2-direction-and-the-three-proposals]] and the prior art in
`docs/MODES-PRIOR-ART.md` are about *not rewriting the input*, not about judging
it better.