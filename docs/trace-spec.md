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
| `excluded` | optional provenance: the source golden's cases this platform cannot encode, e.g. `[{"case": "k/", "state": ["caps"]}]`. A trace carrying it covers every case of that golden **except** these; the omission lives in the artefact rather than in a terminal. `tools/gen-conformance.py` writes it (win32 refuses caps cases — see `in` below). |
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
  (`hunmin_buffered_preedit`, `docs/FRONTEND-EQUIVALENCE.md` §5.1).

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

`meta.delivery` states the model instead, and the model is derived from tier A's own
numbers — never typed:

```
word := ""                              # what the eojeol has accumulated
for each tier-A event e:
    word += e.commit
    if e.consumed:   out = commit "",      preedit word + e.preedit
    else:            out = commit word,    preedit e.preedit ; word = ""
total = Σ e.commit + (last e.preedit)   # the text the client ends up holding
```

A trace that opts in is checked on three legs, all of them in `tools/trace-check.py`:

1. the `.in`'s tier-A stream accumulates to `meta.delivery.total` (the generator's number
   is the golden's, not somebody's);
2. the `.gold` equals the model projected from the `.in`, event for event (the committed
   artefact is what the model says, so a hand-edited `.gold` fails);
3. both the `.gold` and the runner's output accumulate to `total` (nothing dropped or
   duplicated at a boundary).

What this deliberately does **not** assert: a per-event commit mid-word (the model says
there is none), and delivery at teardown (the runner flushes the remaining word with no
event to report it, so only leg 3 sees it). Both are properties of the adapter, and an
adapter that got a boundary wrong fails before the diff: the fcitx5 emitter pre-loads the
commit it expects and `TestFrontend` aborts on a mismatch — measured by replaying a
`sebeolsik-final` trace under `dubeolsik` (`commitString: ㅓㄹ녿ㅁ` vs `안녕`, runner rc=-6).

## Corpus layout

```
tests/conformance/
  <platform>/<case>.trace.in     # native events + the out they must produce
  <platform>/<case>.trace.gold   # the same file, produced by the runner from tier A
```

`.in` is authored and committed. `.gold` is **generated**, and regenerating it must never
be a way to make a red test green — the same rule as tier A's `tools/regen-goldens.sh`: diffs are
reviewed line by line, never blind-accepted (docs/TESTING.md "Tier G").

A platform is engine-correct iff its traces diff-clean against `.gold`, *and* the `.gold`
it diffs against is the one tier A implies. The runner therefore verifies the runner, not
just the adapter.

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

Which layouts appear in the corpus at all is `docs/FRONTEND-EQUIVALENCE.md` §6.3, not a
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