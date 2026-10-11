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

`meta` is required and must be the first line. A runner must reject a trace whose `meta`
is absent or malformed — a trace without provenance is not a test.

### `in` — the native event

Exactly one key event. One of:

```json
{"keysym": 0x006b, "mods": 0}     // X11 / XKB, keysym as XKB defines it
{"keycode": 29, "mods": 0}        // Android, KEYCODE_*
{"vk": 0x48, "scan": 0x1f, "mods": 0}  // Win32, virtual key + scan code
```

`mods` is a platform-native bitfield **except** that `CapsLock` must be reported in the
platform's own convention (`KeyState::CapsLock` / `KeyEvent.META_CAPS_LOCK_ON` /
`KEYEVENT` extended key bit) rather than a hunmin-level flag. An adapter that cannot see
CapsLock cannot be tested for it, and that is the point of the tier.

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