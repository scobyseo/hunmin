# trace-spec — the platform-neutral interaction trace

Every hunmin frontend must be able to describe what it did, in one format, so that the
same input sequence can be replayed against any of them and diffed. This document defines
that format. It is the shared contract behind tier C (docs/TESTING.md "Tier C") and it is
what makes "engine-correct" a checkable claim for a platform that has no other test.

## Why a separate format

Tier A goldens (`tests/golden/*.jsonl`) record what the *engine* does with a stream of
**US-QWERTY characters**. That is the right level for testing composition, and it stays the
single source of truth. But an adapter's job is precisely the mapping the golden cannot
see: X keysym → hunmin key, Android `KeyEvent` → hunmin key, Win32 `WM_KEYDOWN` → hunmin
key. Those mappings are where adapters break, and nothing in tier A observes them.

So tier C reuses tier A's *sequences* but expresses the input in each platform's **native**
event vocabulary. Expectations are still tier A's, recomputed by `hunmin-sim --trace`, so
the two tiers cannot drift: tier C can only fail if an adapter mistranslates an event that
tier A already defines the answer for.

## The record

One line per event, UTF-8, LF-terminated. Line 1 of every file is a `meta` object; the
remaining lines are events.

```json
{"meta": {"platform": "x11", "layout": "sin3-p2", "case": "choseong-pair"}}
{"in": {"keysym": 0x006b, "mods": 0}, "commit": "", "preedit": "ㄱ", "consumed": true}
```

### `meta`

| field | meaning |
|---|---|
| `platform` | `x11` \| `android` \| `win32`. Names the native vocabulary used below. |
| `layout` | hunmin layout id the frontend was configured with. |
| `case` | free-form label; ties the trace back to the tier A case it replays. |
| `source` | optional provenance: which `tests/golden/*.jsonl` case this replays. |
| `excluded` | optional provenance: the source golden's cases this platform cannot encode, e.g. `[{"case": "k/", "state": ["some-option"]}]`. A trace carrying it covers every case of that golden **except** these; the omission lives in the artefact rather than in a terminal. `tools/gen-conformance.py` writes it only for a case state the platform has no encoding for (the 2026-10 win32 caps refusal was lifted by issue #184 — caps is now `mods` `0x02`). |
| `delivery` | optional: `{"model": "word-buffered", "total": "<text>"}` when the frontend buffers a whole eojeol and delivers it at a boundary, so tier A's per-key `commit` is not what the client receives. `total` is the cumulative text the case produces, computed from the golden by `tools/gen-conformance.py --delivery`. The runner rejects a model it does not know — see "Word-buffered adapters". |
| `commit_mode` | optional: `word` \| `syllable`, the delivery **policy** the adapter must run under for tier A's per-event expectations to be observable at the points they were recorded. Absent means the addon's own default (`word`). |

**`delivery` and `commit_mode` answer the same question from two sides, and a trace
carries at most one.** Tier A records commit and preedit **per event**, which is the
per-syllable delivery point: the engine finalises a syllable on a keypress it also
consumed, and the golden records that commit on that line. A word-buffered adapter holds
the same text until a declined key or teardown. Two honest answers follow:

- **name the model** (`delivery: word-buffered`): keep tier A's `.in` as the engine's
  answer, and state in the artefact what the client receives in total, so the runner's
  own sum can be compared against it (issue #220). This asserts the **shipped default
  policy**.
- **switch the policy** (`commit_mode: syllable`): run the adapter in per-syllable mode,
  where tier A's expectations hold verbatim, event by event (issue #187). This asserts
  the **per-syllable mode**, which is the other policy a user can select
  (`hunmin_buffered_preedit`, `docs/DESIGN-FRONTEND.md` Part A §5.1).

`tools/gen-conformance.py` writes `commit_mode` only for cases whose golden commits on a
key it also consumed — it asks the golden, not the adapter — and never together with
`--delivery`, because one trace naming both would assert two different things about one
run. Dropping such a case instead of choosing one of these answers is the drift issue
#187 measured: the x11 corpus silently replayed 22 fewer cases than its goldens after the
2026-10-25 word-buffering reduction.

A batch trace (one file, many cases) cannot carry a per-case policy or model; its runner
is expected to measure committed text cumulatively instead of asserting delivery points
(`frontends/tsf/test/trace-emit.cpp` does), and `--batch` refuses `--delivery` for that
reason.

`meta` is required and must be the first line. A runner must reject a trace whose `meta`
is absent or malformed — a trace without provenance is not a test.

### `in` — the native event

Exactly one key event. One of:

```json
{"keysym": 0x006b, "mods": 0}     // X11 / XKB, keysym as XKB defines it
{"keycode": 29, "mods": 0}        // Android, KEYCODE_*
{"vk": 0x48, "scan": 0x23, "mods": 0}  // Win32, virtual key + scan code
```

`mods` is a platform-native bitfield **except** that `CapsLock` must be reported in the
platform's own convention (`KeyState::CapsLock` / `KeyEvent.META_CAPS_LOCK_ON` /
`KEYEVENT` extended key bit) rather than a hunmin-level flag. An adapter that cannot see
CapsLock cannot be tested for it, and that is the point of the tier.

Win32 (`vk`/`scan`/`mods`): `vk` is the virtual-key code (`VK_*`), `scan` the US-keyboard
set-1 make code carried in `lParam` — informational; the shipped TIP reads `lParam` and
ignores it. `mods` bits: `0x01` = Shift, meaning
`GetKeyState(VK_SHIFT) < 0` at delivery time; `0x02` = CapsLock, meaning
`GetKeyState(VK_CAPITAL)` carries the toggled-on bit (`& 0x01`) at delivery
time. Shift is therefore a property of the
*event*, not of the character: `{"vk": 0x44, "scan": 0x20, "mods": 1}` is a shifted `D`,
exactly what a real `WM_KEYDOWN` carries. CapsLock is per-event for the same
reason: the shipped TIP feeds the engine `caps` option from
`hunmin_tsf_capslock_from_state()` on every key event, so a caps case replay
sets `0x02` on every event of that case; the trace runner mocks the bit
through the same registered key-state seam it uses for Shift
(`frontends/tsf/test/trace-emit.cpp`). The despatch of `VK_CAPITAL` itself as
a mode toggle is a separate key path and is not what this bit encodes.

Keys that are not single characters — BackSpace, Return, arrows, Tab — are named by their
platform constant (`{"keysym": 0xff08}`, `{"keycode": 67}`). They must still be traceable,
because the adapter's decision to pass them through is engine-visible.

### `out` — what the frontend did

```json
{"commit": "안", "preedit": "녕", "consumed": true}
```

| field | meaning |
|---|---|
| `commit` | text the frontend delivered to the client this event. `""` if none. |
| `preedit` | composing text published after this event. `""` if none. |
| `consumed` | whether the frontend filtered the event (kept it from the client). |

`candidates` is reserved and currently always empty, because no engine call produces a
candidate list yet (PLAN.md "P3"). It will be added here when that API lands — **this file
must change in the same commit** that adds the API, and the runner must reject a trace
that carries a `candidates` key until then. That is deliberate: a reserved field that
quietly starts being populated is how a spec stops describing the code.

`consumed` is what distinguishes "the adapter mistranslated nothing" from "the adapter
passed the key to the client". Without it, a trace that commits nothing at all would pass
a diff against an empty expectation.

## Invariants a conforming trace satisfies

1. **One `out` per `in`, always.** Including for pass-through events, where `consumed` is
   `false` and both strings are `""`. An adapter that filters nothing still traces.
2. **`commit` and `preedit` are independent.** ⚠️ An earlier draft of this file asserted
   they are never both non-empty, reasoning that one event either finalises text or leaves
   composition. **Tier A's own goldens contradict that**: 700+ lines across 16 corpora carry
   both, because one keystroke can finalise a syllable *and* leave the next one composing
   (`s` in 안녕 commits 안 and starts ㄴ). Tier A is the authority and the spec was wrong.
   The invariant that does hold is weaker and worth keeping: the pair must be recorded
   together on one line, never split across events, or a trace cannot be diffed.
3. **Text is NFC, whole strings.** No partial code points, no escape sequences other than
   the standard JSON escapes.
4. **`consumed` is false exactly when the key is unmapped or a modifier chord or an empty
   buffer** — i.e. the engine declined or the adapter short-circuited. It is true when the
   engine consumed the character.
5. **The expectation is tier A's.** A `.gold` file is produced by running the same
   sequence through `hunmin-sim --trace` and recording commit/preedit. The runner never
   invents an expected value; it only compares.

## Word-buffered adapters (`meta.delivery`)

Invariant 5 assumes the frontend commits what the engine commits, key by key. A
word-buffering frontend does not: it holds the whole eojeol and delivers it when the word
ends, so tier A's mid-word `commit` is not a byte the client ever receives.

Measured (`sebeolsik-final`, keys `j f s h e a space`; tier A's preedit stream is
`ㅇ 아 안 ㄴ 녀 녕` with `안` committed on the `h`): the adapter's preedit accumulates the
word — `ㅇ 아 안 안ㄴ 안녀 안녕` — and the only `commit` is `안녕` on the space. Copying
tier A verbatim therefore produces a `.in` that its own `.gold` contradicts — which is why
the first 31 multi-syllable cases were dropped from the corpus rather than re-emitted
(docs/TESTING.md "Tier C", 2026-10-25).

`meta.delivery` names the model; the *expectation* is not projected from tier A at all.
It is asked from the object the frontends actually drive:

```
hunmin-sim --data libhunmin/data --layout <meta.layout> --keys <the trace's own keys> \
           --trace --layer word
```

That is the shared layer's per-key answer at `HUNMIN_COMMIT_AT_BOUNDARY` — the policy a
word-buffered frontend runs — so its `commit` IS the key the client is owed text at, and its
`preedit` is what the client should be showing. `tools/trace-check.py` runs it, and hands the
resulting per-event list to the runner through `HUNMIN_TRACE_EXPECT` so the emitter's
`TestFrontend` expectations and the checker's comparison cannot disagree.

**Why not a rule over tier A.** It cannot be one: tier A records the per-syllable engine
API, and there a word-ending symbol (`.` — the engine commits `가.` on a key it consumed)
and a committed digit arrive identically as "commit non-empty, preedit empty". Measured
2026-11-09: every local rule that catches the symbol also fires on the digit, and this case's
previous `.gold` was produced by such a rule — it placed `가.` on the next space and merged
`4"`+`~` into one commit at a later space (docs/OPTIONS-MEASURED.md §PLT-8).

A trace that opts in is checked on three legs, all of them in `tools/trace-check.py`:

1. the `.in`'s tier-A stream accumulates to `meta.delivery.total` (the generator's number is
   the golden's, not somebody's);
2. the `.gold` equals the layer's stream on the same keys, event for event — so a `.gold`
   that places a commit at the wrong key fails, and a hand-edited one fails;
3. both the `.gold` and the runner's output accumulate to `total` (nothing dropped or
   duplicated at a boundary).

What this deliberately does **not** assert: delivery at teardown (the runner flushes the
remaining word with no event to report it, so only leg 3 sees it). An adapter that got a
boundary wrong still fails before the diff: the fcitx5 emitter pre-loads the commits it was
handed and `TestFrontend` aborts on a mismatch — measured by replaying a `sebeolsik-final`
trace under `dubeolsik` (`commitString: ㅓㄹ녿ㅁ` vs `안녕`, runner rc=-6).

## Corpus layout

```
tests/conformance/
  <platform>/<case>.trace.in     # native events + the out they must produce
  <platform>/<case>.trace.gold   # the same file, produced by the runner from tier A
```

`.in` is authored and committed. `.gold` is **generated**, and regenerating it must never
be a way to make a red test green — the same rule as tier A's `tools/regen-goldens.sh`: diffs are
reviewed line by line, never blind-accepted (docs/TESTING.md "Tier G").

That rule is a mechanism, not a request (issue #306). `tools/trace-check.py --emit`
compares the runner's output with the committed `.gold`; when they differ it prints the
unified diff, leaves the file untouched and exits 1. Overwriting a changed expectation
requires `--emit --accept-changed`, so every expectation change lands in a commit as a
reviewed diff instead of being the cheapest way to turn a red gate green. A `.gold` that
does not exist yet is written directly — there is no prior expectation to re-baseline.
ctest `trace-emit-guard` (`tools/trace-emit-guard-test.py`) asserts the refusal, the
explicit write and the no-op on an unchanged file.

A platform is engine-correct iff its traces diff-clean against `.gold`, *and* the `.gold`
it diffs against is the one tier A implies. The runner therefore verifies the runner, not
just the adapter.

### What tier C evaluates, and what the other tests pin

Tier C is **adapter conformance**: does the frontend relay what the shared layer
delivered, event for event, in the platform's native vocabulary. It is not where the
delivery rules themselves are asserted. Those are pinned by name in
`libhunmin/tests/buffered-preedit-test.cpp` —
`LayerTest.ASymbolEndsTheWordInHangulModeToo`,
`LayerTest.TheSymbolEndsTheWordSoTheJudgementRunsBeforeIt`,
`LayerTest.ASymbolDoesNotSplitTheDigitRun` — and the characters are pinned by the
tier-A goldens and ctest `engine-off-identity` (`tools/off-identity-check.py`), which
replays the same corpus through the layer. The measurement that forced this split is
docs/OPTIONS-MEASURED.md §PLT-8; the figures live there, not here.

So a simultaneous layer + adapter change, re-baselined with `--accept-changed`, leaves
tier C green — deliberately: tier C asserts the relay, and what catches that change is
the named layer tests and tier A, provided nobody regenerates those expectations too.
The guard above makes the tier-C half of that regeneration a reviewed act; it does not
make tier C a second home for the delivery rules.

### Multi-case batch files

A trace file may additionally carry `{"meta": {"case": ...}}` separator lines after its
header (as win32's `words.trace.in` does): the runner resets adapter state at each
separator, and `tools/trace-check.py` compares only the event lines, so a batch diffs
like any other file. This exists so a corpus does not flood ctest with one test per
two-event case (win32's tier F1 chose it, and android's runs with it since issue #186;
x11's per-case files were registered earlier and keep the split layout). The isolation
rule is unchanged: every case still replays from a fresh engine, the separator only
moves where that fresh engine is created.

### What a case can express on each platform (measured 2026-11-03, issue #186)

The format is one vocabulary; the *emitters* differ in what they can observe, and a
`.in` whose expectations an emitter cannot reproduce is a corpus that cannot be built,
not a frontend that fails. Three facts, each measured rather than assumed:

- **x11 carries a commit only at a boundary event.** The emitter buckets tier A's
  per-key commits into the buffered word and pushes one expectation per word
  (`frontends/fcitx5/test/trace-emit.cpp`), so a golden whose events commit mid-word
  has no x11 form; the measurement and the corpus reduction it forced are
  `docs/TESTING.md` "Tier C". A Latin layout is that same fact in its extreme — every
  key commits its own character, so no `colemak`, `dvorak` or `us-qwerty` case has an
  x11 form at all (measured: `hello` differs on all six events, the adapter holding
  `h`, `he`, `hel` as preedit and committing the word at the space). A case for x11 is
  therefore ONE word whose syllables only finalise at the boundary; the per-layout
  traces are `words-<layout>.trace.in` for exactly this reason.
- **android carries Shift two ways, and a case must say which it means.** A letter
  carries its own case (`keycode` of `T` is the shifted `t`), and a shifted SYMBOL —
  which has no case — is named by `"shift": true` (or `mods` bit `0x01`, the bit the
  arrow-selection corpus already used); the emitter feeds `hunmin_ic_process_key`,
  the explicit-modifier entry, for the latter. Its output echoes the `in` it was
  given, field for field. It reads the expectation verbatim from the `.in` and
  replays through `hunmin_ic`, so mid-word commits are fine, and it resets its input
  context at a case separator, so a batch file is one ctest here too.
- **win32 case separator `options`** — a case separator may carry an `options` object
  (`{"meta": {"case": "...", "options": {"w": 0}}}`) whose keys are engine
  option names (`w`, `W`, `z`, `h`, `H`) and values are 0/1. The runner
  applies them via `hunmin_buffered_preedit_set_option` at the case
  boundary, mirroring the TIP's per-event option feed. CapsLock stays in
  `mods` (`0x02`), not in `options`.
- **win32 can express any golden** — `mods` carries Shift (`0x01`) and the emitter
  reconstructs tier A's per-event commit from the adapter's own buffered word
  (`frontends/tsf/test/trace-emit.cpp`) — but emitting a `.gold` needs a Windows host.

Which layouts appear in the corpus at all is `docs/DESIGN-FRONTEND.md` Part A §6.3, not a
second list here.

## Runner

`tools/trace-check.py <trace.in> <trace.gold>` — emits a unified diff, exit 1 on any
mismatch, exit 77 to skip when the platform's frontend is not built. Exit 77 matters: tier
C is host-only and must not turn red on a machine that legitimately lacks the toolchain
(docs/TESTING.md tier D3 uses the same convention for Xvfb).

Two runner-channel rules, each learned from a deterministic 14-of-14 failure of the
Android conformance suite inside `dpkg-buildpackage`, green in every local run:

- **stdout is JSONL only.** `trace-check.py` parses the runner's *entire* stdout as
  records; one progress line from a nested build step fails every case. A runner that
  invokes another tool for setup must route that tool's human output to stderr
  (`build-host-jni.sh`'s ldd proof, `tools/android-trace-emit.sh:84`).
- **stdout is UTF-8 by declaration, not by locale.** The JVM does not follow Python's
  PEP 540 coercion: under `LC_ALL=C` — which `debian/rules` exports — Java's default
  stdout charset mangles every non-ASCII preedit into `?` while the JSON still parses.
  The runner passes `-Dfile.encoding=UTF-8` and both stdout-encoding properties
  (`sun.stdout.encoding` for Java 17, `stdout.encoding` for 18+).

## What is deliberately not here

- **No timing, no focus events, no candidate-window geometry.** Those belong to tiers
  D–E, which run a real framework. Tier C's job is the event→text mapping, and everything
  in this file exists to make that mapping diffable.
- **No per-platform toolchain.** A trace is data; producing one must not require building
  fcitx5, the Android SDK, or Windows. That is why the format is JSONL rather than a
  harness protocol.

## Personal key log — the frontend-independent dialect

Tier C above records each platform's native event (`keysym`, `keycode`, `vk`) so an
adapter's mapping can be diffed. For a person measuring their own typing the native
event is the wrong level: the same physical key produces a different `keycode` on
Android than a `keysym` on X11, so two people's logs cannot be compared, and one
person's cannot be analysed by a tool that does not know every platform's constants.
What is identical on every frontend is the value at the engine boundary —
`hunmin_key_event` (`libhunmin/include/hunmin.h`): `(base, shift, mods, id)`.

The personal dialect uses that shared value. It is a second dialect of this format,
not a replacement for tier C; tier C keeps native events because its job is to test
the mapping, and personal logs keep shared events because their job is to measure
behaviour after the mapping.

One line per event, UTF-8, LF-terminated. Line 1 is a `meta` object with the same
fields as tier C's `meta`, plus `dialect: "personal-key"` and (since 2026-11-27)
`started: "<UTC ISO-8601>"`, the wall clock at the log's first line, so a log can
be lined up against a session or a journal, and (since 2026-11-28) `data: "<dir>"`,
the `HUNMIN_DATA_PATH` the engine ran with. **A replay of the same keys is only
faithful against the same data**: a dev build defaults to its own install prefix and
the packaged one to `/usr/share/hunmin`, and the two layout files differ — a replay
against the wrong one produced 229 flips where the log showed 18. Each event line is:

```json
{"in": {"base": 97, "shift": 0, "mods": 0, "id": 1}, "char": "a", "commit": "", "preedit": "ㄱ", "consumed": true}
{"in": {"base": 32, "shift": 0, "mods": 0, "id": 1}, "commit": "hello", "preedit": "", "judged": "latin", "consumed": false}
```

| field | meaning |
|---|---|
| `in.base` | `hunmin_key_event.base` — the shared base value, before any layout resolves it |
| `in.shift` | `hunmin_key_event.shift` — the row bit |
| `in.mods` | `hunmin_key_event.mods` — `HUNMIN_MOD_*` bits, not platform-native bits |
| `in.id` | `hunmin_key_event.id` — `HUNMIN_KEY_*`, so a BackSpace or Han/Eng key is named without a platform constant |
| `char` | optional adapter-chosen character, for readability; not the input |
| `commit` / `preedit` / `consumed` | the same output fields as tier C |
| `lang` | **added 2026-11-27:** present when this key was the frontend's Han/Eng key. It is *consumed* by the engine, so a reader that segmented words by `consumed` would glue it onto the next word; it is the person changing their assumption, not typing (`docs/MODES-MEASUREMENT.md` §1.6) |
| `mode` | **added 2026-11-27:** the layer's language AFTER this key, `"latin"` or `"hangul"`. With `preedit` it is what makes a flicker readable — the same text rendered by the other language is the flip (`docs/MODES-MEASUREMENT.md` §2.5), and with `judged` it separates "the person was in Latin" from "the engine switched" |
| `t` | **added 2026-11-27:** milliseconds since the log opened (monotonic). "The person noticed and stopped" is only visible with a clock; it is also the first half of telling a typo from a repair (the second half is the key relation, which no field can supply) |
| `judged` | **added 2026-11-27:** the language the engine's judgement acted on for THIS key, `"latin"` or `"hangul"`; absent when the key was not judged. It is the rewrite itself — a `commit` of Hangul cannot tell "the person typed Hangul" from "the engine replaced it", which is the only event Track 2 exists to measure (`docs/MILESTONES.md` §0.4 Track 2 purpose). Spelled exactly as `hunmin-sim --judge --trace` spells it, so one vocabulary reads both |

Because `in.id` names the key and `in.mods` uses shared bits, analysis need no
platform table. The BackSpace key is `id == HUNMIN_KEY_TEXT, base == 0x08` on every
frontend, and a chord is `mods & HUNMIN_MOD_CHORD` on every frontend. A person's log
from one platform can be handed to an analyzer written for another.

A teardown flush has no driving key, so it is a line of its own rather than a
fabricated key event:

```json
{"flush": "<text delivered at teardown>"}
```

An analyzer folds a `flush` line into the commit sequence but gives it no
trigger, so a flush cannot be mistaken for a key-driven commit.

A passed-through key (Enter, Alt+Enter, a chord) is also a key the user pressed
and must be recorded, not only keys the engine consumed. It is a normal event line
with `commit: ""`, `preedit: ""` and `consumed: false`, followed by a separate
`flush` line if the boundary delivered a buffered word. Recording only consumed
keys loses exactly the boundary cases where behaviour diverges from expectation.

Each frontend produces this dialect only in an opt-in build or an explicitly
enabled session; the default path writes nothing. The sink is not part of tier C's
runner and emits no `{"kind":...}` records — those belong to the older build-flag
sink in fcitx5 and are not used for personal logs.

Producers: fcitx5 (`frontends/fcitx5/src/hunminadapter.cpp`, `-DHUNMIN_TRACE=ON`),
Android (`HunminTraceRecorder.kt`), TSF (`frontends/tsf/src/keydispatch.cpp`) and,
since 2026-11-22, the IBus engine (issue #292). IBus keeps the sink in
`frontends/ibus/src/trace/` behind the same `HUNMIN_TRACE` option; a default build
has neither the sink nor the `HUNMIN_TRACE_OUT` getenv, and ctest `trace-optout-ibus`
asserts that. Its `meta.platform` is `x11` because the D3 host is X11. The `judged`
field is set at the one place the engine acts on a judgement
(`apply_judgement`, `frontends/ibus/src/hunmin_state.c`) and asserted by ctest
`engine-d1-ibus-trace-judged`, which compiles the state object with
`-DHUNMIN_TRACE=1` and reads the line back.