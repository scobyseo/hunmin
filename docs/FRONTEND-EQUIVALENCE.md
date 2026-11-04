# Cross-frontend equivalence

Status: **requirements recorded 2026-10-24; the three open decisions in §9 were
taken 2026-10-25 and the layer they describe is implemented in
`libhunmin/src/buffered_preedit.c` (issue #128, phase 1). The corpus, the frontend
migrations and the TSF emitter extension follow on the same branch.** This
document is the single home of the rule.

Source: maintainer instructions in the TSF frontend refactor session,
2026-10-24. Exact wording is quoted in "Wording" below so a paraphrase cannot
drift from what was said.

---

## 1. The invariant

For the same layout, options, and key sequence, every hunmin frontend must
produce the same output:

- the same committed text;
- the same preedit string after each key;
- the same consumed/pass decision for each key.

The commit **boundary** is not part of the contract. A frontend may buffer the
word and commit once at the boundary, or commit syllable by syllable for
platform compatibility. The result must be identical either way.

Presentation is also not part of the contract: the same output may reach the
client through an `ITfComposition`, a document span, an `InputConnection`, or a
fcitx5 client preedit.

## 2. TSF: interim and native modes

TSF has two delivery modes: the native `ITfComposition` path (#98) and the
interim document-span path (#119) for classic default-sink hosts. They are
delivery strategies, not separate input rules.

For the same key sequence, interim mode and native mode must produce the same
committed text, the same preedit string, and the same per-key consumption
decision. A key must not be typeable in one mode and not the other.

The original defect (digits `1`-`0` not insertable in interim dubeolsik) is an
instance of this rule.

### 2.1 Ruled 2026-11-02: the interim delivery does not carry the automatic switch

Maintainer, 2026-11-02:

> interim model cannot support autoswitch.

So the two delivery modes are ALLOWED to differ on this feature, and the
difference is named here rather than left as a defect. The evidence that makes it
structural (measured 2026-11-01, issue #175): `InterimWrite` moves the layer's
finished word into the document on every key (`hunmin_buffered_preedit_take_word`
REMOVES it) and `InterimDrain` closes with `ComposeReset` rather than
`commit_word()`, so at the boundary the layer holds only the syllable still in
flight — the frozen log line is `interim boundary: word len=0`. There is no whole
word to judge, and R10 forbids reassembling the syllable text already in the
client.

How it is implemented: `CTextService::ApplyAutoswitch()` is the one setter of the
option, and it answers `AutoswitchOn()` — the stored value AND the native
delivery. Every site that changes the delivery re-applies it (the layer rebuild,
`DetectLegacyMode`, and the mid-word flip in `OnCompositionTerminated`), so the
engine never holds a state this delivery cannot honour. Pinned by
`TsfConfigTest.TheInterimDeliveryDoesNotHoldTheSwitch`: the store says 1, the
layer answers 0, and a Latin-mode key still passes to the app.

What this ruling does NOT decide:

- **The settings row still shows the stored value.** The dialog is a separate
  process and cannot know a host's runtime delivery mode, so on a classic
  default-sink host the row can advertise ON while that host does nothing with it.
  Whether the row should say so (and how it could know) is open.
- **Whether a future interim shape could carry it.** The two shapes priced in the
  pre-ruling text — hold the word in one growing span, or keep the asymmetry —
  were the options; the ruling picks the second for now, and it does not say the
  first is forever excluded.

### 2.2 Ruled 2026-11-02: with the switch ON, English no longer passes through

Maintainer, 2026-11-02:

> autoswitch ON stops English passthrough to support eng -> han support

R8's E1 line (ruled 2026-11-01, "E1도 당연히 지원해야지") judges a word typed in
Latin mode, and TSF fed the engine only in Hangul mode: `OnTestKeyDown` answered
`eat = FALSE` for everything once `hangulMode_` was false and `OnKeyDown` returned
at `if (!hangulMode_) return S_OK; // passthrough English`. Measured before the
change, on the shipped data with the switch ON: `sms` + space in Latin mode left
the mode Latin, the client document unchanged, and the test phase for `D` answered
`eat = 0` — the E1 half, and with it the 0.3 gate sentence, could not fire on TSF
at all.

How it is implemented, in both key phases (they must answer alike or the host
loses a key the down phase would claim):

- the passthrough is now `if (!hangulMode_ && !AutoswitchOn())`. With the switch
  ON the engine keeps seeing the keys in Latin mode, resolves them on the Latin
  layout per character (the text is what the host used to insert, R5) and judges
  at the boundary; with it OFF the branch is exactly what shipped.
- the two candidate surfaces stay Hangul-mode (`IsCandidateToggle`, the open
  list's keys): a Hanja lookup of a Latin word is not a question anyone asked, and
  a list opened in Latin mode would make a passthrough key dead.
- with the interim delivery the passthrough stands, because §2.1 leaves the switch
  off there and feeding the engine could only cost the app its own key.

Pinned by `TsfConfigTest.TheEnglishDirectionIsJudgedOnceTheSwitchIsOn` (one
service: `english` → Latin, then `sms` typed in Latin mode → 는 and the mode back
to Hangul, document `english는`) and `TsfConfigTest.ThePassthroughStaysWhenTheSwitchIsOff`
(switch OFF: a Latin-mode key is the app's, `eat = FALSE`).

What this ruling does NOT decide, and what is still unmeasured:

- **The live cost of routing English-mode keys through the engine.** Native
  delivery now composes the Latin characters instead of letting the host insert
  them, so a live round should confirm caret, undo units and selection behaviour
  in an ordinary edit control with the switch ON. Not measured here: the mock
  harness has no host insertion policy to measure against. **Measured
  2026-11-02 (issue #216):** the live round is
  `docs/WINDOWS-TSF-BUFFERING.md` §"The automatic switch, measured live" — the
  same seven keys are one inline composition with the switch ON and the host's
  own insertion with it OFF, the text and the caret agree at every key, the undo
  unit is one character either way with one extra selection-restoring entry in
  the engine path, and typing over a selection replaces it in both. Two
  regressions that round found are named there rather than here, each with its
  own issue (#221, #222).
- **Whether Android and fcitx5 need the same rule.** Their English-mode paths are
  their own (issue #180 is the equivalence lane); this ruling names TSF's.

## 3. Allowed differences

1. **Physical-key mapping.** Each frontend maps its platform event to the
   shared `(base, shift)` input event. Custom key mapping is provided only for
   the Han/Eng mode change, Hanja, and speech recognition. Every other key is
   fixed per frontend.
2. **Platform pass-through.** A frontend may pass a key through when the
   platform cannot deliver it to the shared layer. The Android example is the
   Han/Eng key, which arrives as a Kana VK. A key that *is* delivered to the
   shared layer must be processed identically.
3. **Presentation and commit granularity.** Composition API, preedit
   placement, candidate UI, and buffered-versus-syllable commit are frontend
   choices.

Everything else is not a permitted difference.

## 4. The Han/Eng key identity

Android delivers the Han/Eng key as a Kana VK and treats it as the mode-change
action. The other frontends must map their Han/Eng key to that same abstract
event. The raw platform code stays in the adapter; the shared layer sees one
Han/Eng action, not a Windows VK and an Android keycode.

This is the current behavior to normalize to, not a claim that the platform
codes are equal.

## 5. Shared buffered-preedit layer

The word buffering and preedit composition currently implemented separately in
the TSF, fcitx5, and Android adapters must move to a shared layer above the
current libhunmin implementation.

- It sits above `hunmin_ic`; it does not replace or reimplement the engine.
- It selects buffered (word) or syllable preedit output.
- Frontends may select the mode they need for compatibility, but the decision
  logic and the resulting text are shared.
- It is the single place that answers consumption and produces commit and
  preedit for the frontends.

The layer is a libhunmin extension, not a per-frontend reimplementation. New
symbols follow the append-only ABI rule.

### 5.1 What the layer is (implemented 2026-10-25)

`hunmin_buffered_preedit` in `libhunmin/src/buffered_preedit.c`, exported as `hunmin_buffered_preedit_*`
(`hunmin.h`, 11 symbols, appended to `libhunmin/tests/abi/symbols.txt`). It owns
the word buffer and the boundary rule:

- `hunmin_buffered_preedit_process_key(l, base, shift, &out)` answers consumption and fills
  `out.commit` / `out.preedit`. A **declined key is the boundary**: the engine
  finalises the syllable it was holding, and that is when a buffered word is
  delivered. A frontend therefore never spells out "when a word ends" itself.
- `HUNMIN_COMMIT_AT_BOUNDARY` holds finalised syllables and delivers the word at
  the boundary; `HUNMIN_COMMIT_PER_SYLLABLE` delivers each one as the engine
  finalises it. Same text, different delivery point.
- **Both are selectable per user, not baked in** (2026-10-25): fcitx5 exposes the
  choice as its `CommitBehavior` option (`word`/`syllable`, `frontends/fcitx5/src/config.h`).
  The policy is part of a live layer's identity -- switching it rebuilds the layer
  the same way a layout change does -- and a D1 case types one key sequence under
  both policies to show the units differ while the text does not. Android and TSF
  take the same choice from their own settings when they are migrated.
- `hunmin_buffered_preedit_toggle_language()` is the Han/Eng action (§4): one abstract event,
  and it is a boundary in both directions.
- `hunmin_buffered_preedit_peek_key()` is the read-only prediction a platform "test" phase
  needs, over the same state the real key will consume.

Tier A asserts the property the layer exists for, as equalities rather than
golden text: mode vs mode per key, and layer vs the engine driven directly
(`engine-buffered-preedit`, `libhunmin/tests/buffered-preedit-test.cpp`).

### 5.2 A wording question this implementation exposes

§1 says "the same preedit string after each key", but §1 and §5 also make the
commit boundary a frontend choice — and those two cannot both hold literally: a
buffered frontend shows the finished syllables of the word as preedit while a
syllable frontend has already committed them, so their preedit strings differ by
exactly the text one of them has delivered. The implementation therefore asserts
the observable quantity: **committed-so-far + preedit is equal per key**, and
the concatenation is equal. Whether the rule should be worded that way is the
maintainer's call; it is recorded here rather than silently rewritten.

### 5.3 Frontends on the layer

**fcitx5, 2026-10-25.** `HunminState` holds a `hunmin_buffered_preedit` instead
of a `hunmin_ic` and owns no buffer of its own: it maps the keysym to
`(base, shift)`, inserts `out.commit`, renders `out.preedit`, and lets a declined
key be the boundary. The policy is `HUNMIN_COMMIT_AT_BOUNDARY`, which is what
#74 asked this frontend for; the text the client receives is unchanged, the point
it lands is the word boundary.

What moved with it, because each of these encoded the old per-syllable shape and
would otherwise have been a stale assertion:

- `testhunmin` (tier D1) and `testhunminui` (tier D2): the preedit expectations
  now show the word plus the syllable in flight, and the symbol-tier cases gain
  the space that is their boundary. The committed TEXT is identical.
- `tools/check-frontend-invariants.py`: its feed-block anchors and two I9
  patterns followed the code ("one commit delivery from the layer's answer").
- `tools/frontend-mutations.py`: three patterns were re-anchored. One of them,
  `I1-commit-flush-both`, is now rejected by BOTH tiers where it used to be a
  recorded gap -- buffered delivery is exactly the "commit plus in-flight
  syllable in one key" case D1 never had.

**Android, 2026-10-25.** `HunminAdapter` holds a `hunmin_buffered_preedit` and its
`wordbuf` is gone: `onKey` feeds `layerKey`, inserts whatever `layerDelivered`
returns, and shows `layerDisplay`. The commit unit comes from the
`commit_behavior` preference (`word` | `syllable`, default `word`), read by the
service at session start, so a change takes effect on the next input session.

JNI cannot return a struct, so a key's answer is three reads of layer state:
`layerKey` -> `layerDelivered` -> `layerDisplay`. External text (voice) goes
through `hunmin_buffered_preedit_external_text`, which delivers the word first
and then the engine's pass-through, keeping invariant 5's ordering.

All three frontends are on the layer (§8.1): TSF keeps no `wordbuf_` — the seam exposes the layer and its two halves (commit 66a446c).

## 6. One corpus, every frontend, both TSF modes

Input behavior is defined once and run everywhere:

- The shared corpus names the layout, options, key sequence, and expected
  per-key output (consumed, commit, preedit), not only the final text. Final
  text alone cannot catch a key that is eaten in one mode and passed in
  another.
- It runs against the shared layer, fcitx5, Android, and the TSF mock in both
  native and interim delivery, and in both buffered and syllable commit modes.
- Tier C (`docs/trace-spec.md`, `tests/conformance/`) is the cross-platform
  event format. The Windows TSF emitter (tier F1) runs the corpus in BOTH
  delivery modes since issue #185: every `tests/conformance/win32/*.trace.in`
  is registered twice (`tsf-mock-native-<trace>` and
  `tsf-mock-interim-<trace>`) against the same `.trace.gold`, so a divergence
  between the modes fails one of them against the shared expectation. The
  interim run reads the client control instead of a composition
  (`frontends/tsf/test/mock-tsf-edit.h`), and the two modes' in-process
  assertions differ only where §2.1 records a ruled difference.
- A per-frontend test may drive a different interface, but it must not carry a
  second copy of the expectations.

### 6.1 The commit unit is not a corpus dimension on TSF (decided 2026-10-25)

The TSF tier-F1 corpus runs the **buffered** unit only. The unit axis is asserted
where it is cheap and real:

- tier A `engine-buffered-preedit` — the two units produce identical text, by
  equality over the same key sequences;
- fcitx5 D1 — one key sequence typed under both units, against one set of
  `hunmin-sim --trace` expectations;
- Android E1 — 116 cases on the buffered unit, the one Android ships.

**Why not the corpus, measured rather than assumed.** A `PER_SYLLABLE` run of
`tests/conformance/win32/prose.trace.in` through `tsf-trace-emit` fails at event
3 (`expected "ㅂ", got "ㅊ"`), and the cause is structural: under that unit one
consumed key both *ends* the composition with the finalised syllable and
*immediately opens the next one* showing the syllable now in flight. By the time
the emitter reads anything, `MockContext.compositionText` — and a document delta
read the same way — describes the NEW composition, because the mock records the
fused text on more than one channel when a close and an open land in one event.

So the emitter cannot derive "what reached the document" from the fields the
mock has, and a variant registered today would assert a mock artefact rather than
the path. What would change this: one observation field on `MockContext` for the
text a composition actually fused with (set where the mock handles
`EndComposition`) -- recorded as a work item on issue #107, which owns the
unmodelled mock surfaces. With that, both units derive correctly and the corpus can own
the axis. Interim × both units stays in the sandbox lane (§7) for the same
reason: a real client distinguishes what the mock conflates.

### 6.2 The judgement corpus, and what it does not yet cover (2026-11-01, #178)

The first corpus that carries a JUDGEMENT rather than only text is
`tests/golden/judgement-r8.jsonl` (ctest `engine-golden-judgement`). Each line is
a case the engine can replay: the input (`layout`, `latin`, `mode`,
`autoswitch`, `keys`) and the engine's own answer — the delivered text, the
judgements taken in order, and the mode after the boundary, with per-key
delivery timing where the timing is the assertion (R6's digit run). Its cases
are the lines of SPEC-MODES R8's formula and the guards around it: the two
switch directions, both-valid and both-invalid, R9's guard from each side, R4's
boundary-only moment, R13's off-is-the-identity state, E1's 0.3 gate sentence
typed as one stream, and the 세벌식 최종 cases R6 is actually about (its digit row
is the jongseong jamo ᇂ ᆻ ᆸ, so committing each key on its own is what R6
forbids).

It is driven by `hunmin-sim --judge`, which sits on `hunmin_buffered_preedit`
and performs the frontend half of the rule (read `take_judgement`, act on it),
and the expected values come back from the engine through
`tools/gen-judgement-golden.py` — never typed. Replay one case:
`hunmin-sim --data libhunmin/data --layout dubeolsik --latin us-qwerty --judge
--autoswitch 1 --keys 'hello ' --trace`.

### 6.2.1 The corpus is the evaluation instrument (ruled 2026-11-01)

> "프런트엔드는 모두 동일한 동작을 해야 하니까, 동일한 코퍼스로 평가하는 게 옳아.
> 프런트엔드의 특성이 있으면 그 차이는 허용할 수 있게 하자."

So one corpus is what every frontend is measured against, and a frontend's own
characteristics are an allowed difference **only where §3 lists them**
(physical-key mapping, platform pass-through, presentation/commit granularity).
Where a lane takes one of those, the case has to say so and the lane has to
report it — a difference that passes silently is not an allowed difference, it is
unmeasured behaviour.

**The first lane exists: `engine-d1-judgement`** (`frontends/fcitx5/test/testjudgement.cpp`,
tier D1), and it reads the corpus file rather than a copy of its expectations.
Getting there needed two things the addon did not have, both found by building it:

- **A way to turn the judgement on.** Measured 2026-11-01: `config.h` carries
  `HangulLayout`, `LatinLayout` and `CommitBehavior` and **no** autoswitch row, and
  the key path never called `hunmin_buffered_preedit_special()` — so the layer sat
  at the engine default (OFF, B3) and `applyJudgement()` answered -1 on every word;
  a replay would have asserted the OFF identity path for all fifteen
  autoswitch-ON cases. The lane unblocks it the ruled way (R13-정정 surface A):
  the adapter now reports `language_key` for the COMPOUND press (Ctrl+한/영,
  SPEC-MODES §E5) and acts on `hunmin_buffered_preedit_special()`, and it registers
  a `hunmin_special_host` — without which the engine refuses every function with
  -1 by design. A bare Han/Eng press stays fcitx5's own trigger.

  ⚠️ **Corrected 2026-11-03 (issue #233): the lane writes the VALUE, not the
  chord.** The paragraph above is right about the adapter surface the case needs,
  and wrong about the lane's use of it: `Ctrl+한/영` *toggles* the state (SPEC-MODES
  §R13-정정 surface A, and engine-d1's watcher case asserts ON → OFF), while the
  corpus's `autoswitch` field states a value per case. A first cut pressed the
  chord once per case, so the state alternated with the case count and
  `r9-three-keys-are-judged` — the sixth case to press it — ran with the value OFF
  and failed, delivering the Hangul reading `ㅏ됴` where the corpus says `key`. The
  lane now sets the state through `setConfig` (surface B's path) and leaves the
  chord's flip to engine-d1. Lesson, and it generalises to the other two lanes:
  **a corpus field that states a value must be set, not toggled** — a toggle makes
  the measurement depend on what ran before it.
- **A delivery-order defect the corpus found.** `out.commit` is valid only until
  the layer's next call, and the adapter read it AFTER `applyJudgement()` — whose
  switch performs a boundary that sets the commit to "". So the judged word was
  silently dropped: `hello ` delivered nothing. TSF and Android read the commit
  before their judgement call and were never affected. Fixed by copying first.

**The other two frontends have no lane yet**, and that is the remaining half of
this ruling: Android (tier E1) and the TSF mock (tier F1) still carry their own
corpora, so a judgement case is asserted against the shared layer and against
fcitx5, not yet against them. Both can be replayed the same way — the corpus
needs nothing a lane does not already have (`step_commits` for the delivery
timing, `probe` for the mode) — but neither runs on this host (no device, no
Windows), so writing those lanes unverified would be a claim rather than a test.

What the fcitx5 lane itself does not do, named rather than hidden: a case whose
`mode` is `latin` is skipped, because the starting language belongs to the host
and this addon has no key that sets the layer's initial mode — the lane rebuilds
the layer per case instead, which is the frontend characteristic taken here.
Mid-stream switches are covered (the mixed-sentence case flips four times). The commit unit
of §6.1 is likewise outside this corpus: every case uses
`HUNMIN_COMMIT_AT_BOUNDARY`, and the judgement cannot run in PER_SYLLABLE at all
(its earlier syllables are already in the client, which is R10's constraint).

### 6.3 Every shipped layout now appears on some platform (measured 2026-11-03, #186)

The corpus exercised 4 of the 15 shipped layouts: layout wiring is the class of
regression tier C exists to catch, and 11 layouts had no adapter-path event at all.
Each one now carries a **short word in its own key table** — `words-<layout>.trace.in`
under `tests/conformance/{x11,android}/` — built by `tools/regen-layout-words.sh`:
`tools/sin3p2-ksgen.py` searches the layout's own key table for the word's syllables
(BFS with `hunmin-sim --batch` as the oracle), `tools/wordlist2golden.py` records the
engine's per-key answer as a tier-A golden (`tests/golden/words-<layout>.jsonl`), and
`tools/gen-conformance.py` copies those expectations into the platform's vocabulary.
No expectation is typed by hand.

Measured on this tree (`grep -ho '"layout": "[^"]*"' tests/conformance/*/*.trace.in |
sort -u`): **15 of 15 layouts appear in at least one trace** — 12 on x11, 14 on android,
4 on win32 (unchanged). What is NOT covered, named rather than implied:

- **win32 for the 11 new layouts.** The TSF emitter can replay any of these goldens,
  but a `.gold` must come from the runner (`tools/trace-check.py --emit`) on a Windows
  host, and this host has none. Environment-bound, tracked on the board as
  `TRACE-LAYOUTS-186-WIN32`.
- **x11 for `colemak`, `dvorak`, `us-qwerty`.** Not a layout gap: a Latin layout commits
  on every key, and the x11 dialect carries a commit only at a boundary event. The
  reason and the measurement are `docs/trace-spec.md` "What a case can express on each
  platform"; those three layouts are covered on android instead.
- **Depth of the one-word probe.** `나` is one syllable, chosen because a multi-syllable
  word finalises a syllable before the boundary, which the x11 dialect cannot carry. It
  asserts layout wiring, composition of that layout's own key table, the boundary commit
  and `consumed` — not the layout's depth, which tier A's parity walk carries.

**The Latin layouts are measured by a text, not a word** (`tools/regen-cultural.sh`,
`tests/golden/texts/pangram.txt`). A Latin key table is 47 keys — 26 letters, 10 digits,
21 punctuation — and one English sentence reaches 27 of them (measured over
`us-qwerty`, `colemak` and `dvorak`), because no sentence uses the digit or punctuation
rows. The text therefore carries the pangram in both cases plus those two rows: measured
**47 of 47 keys on all three layouts**, five cases, 167 key events, one batched android
trace per layout. That is also why those three layouts no longer have a one-word probe:
the text subsumes it.

**A real text, for the same reason** (`tests/golden/texts/seosi.txt`, issue #227): 서시,
one batched android trace per layout, and each layout reports what it could take.

| layout | cases | events | what it could not take |
|---|---|---|---|
| `dubeolsik` | 11 of 11 | 308 | — (its punctuation has no key, so the client inserts it) |
| `sin3-p2` | 11 of 11 | 310 | — (`'` is Shift+`j` and `/` is Shift+`'`) |

A layout's reach over the text is a MEASUREMENT, not an assumption, and the measurement
has to be made the right way. `tools/gen-cultural-golden.py` asks, per character: which
single key event emits it? It probes the character itself AND every key of the layout
with Shift held, and takes the first that renders it — three outcomes, three meanings:
the layout spells it (it is delivered text), no such key (the adapter declines the event
and the client inserts the character, which is every symbol on `dubeolsik`), or nothing
emits it (untypeable). Reading that from the data file instead is what got `sin3-p2`
wrong once: its values are nested `case` trees and its `'`/`/` live on the SHIFT of
other keys, so a flat `=x` lookup reported the imprint line as untakeable — measured
`J` → `'` and `"` → `/`, and the line is in the corpus.

Two more things the artefact states rather than hides: hanja (`序詩`) is a
`hanja_segments` run — no layout emits it, so no key event can assert it, but an
environment that can enter hanja can evaluate the title line — and a syllable's
keystroke is found in its layout's own terms (`sin3p2-ksgen.py`'s BFS where a table
suffices, `jamo-keytable.py`'s cho/jung/jong probe on a 세벌식 automaton, where the BFS
reached 81 of 124 targets and neither 없 nor 워).

## 7. Live TSF tests

Requirement: a live TSF run must not leave the worktree registered or otherwise
undeletable, and its result must not depend on which worktree was registered
last. The installed `C:\Program Files\Hunmin IME` copy is not the test target.

**Decision (2026-10-24): every live TSF test runs in Windows Sandbox.** There
is no dev-host registration lane and no stable-deploy fallback. The split is:

- **Default lane (every commit):** host-free. The shared-layer tests, tier A
  goldens, tier C traces, and the TSF mock in both modes. No registry, no
  elevation, no sandbox.
- **Authoritative live lane (on demand, before merge):** disposable Windows
  Sandbox. Build a self-contained kit from the worktree (static CRT, data,
  probe), map the kit read-only, map one output folder writable, then register,
  run, read the results, unregister, and discard the sandbox. The worktree is
  never registered and can be deleted immediately. `tools/tsf-sandbox.ps1` is
  that lane.

Why: the sandbox has no host state to restore, so there is no cleanup path to
get wrong. Dev-host registration accumulates failure modes (elevation, a
loaded DLL holding the worktree, `ctfmon`/`explorer` restarts, HKCU leftovers)
and cannot run two worktrees at once. A host without Windows Sandbox simply
does not run the live lane; it does not fall back to the dev registry.

Measured in the sandbox on 2026-10-24 with the static-CRT kit:

- `tools\tsf-sandbox.ps1 -Layout dubeolsik` (the default keys) sends
  `dkssud gksrmf\b\bdl \b\n` and gets `안녕 한ㄱ이\n`, `pass=1`. This covers a
  literal space, two backspaces inside a composition, retyping after the
  backspaces, a backspace over a committed space, and Enter.
- `-Keys 'dkssud\s1'` gets `안녕!`, `pass=1` (Shift).
- A cold Sandbox boot once sent keys before the probe window held the
  foreground (empty text, immediate rerun passed). The probe now waits for
  foreground and TIP activation and writes `ime-probe.log`; the rerun with the
  long sequence passed.

## 8. The three frontends, measured 2026-10-25

Read from the three trees after the #128 migration, not from the design. §3 says
which differences are permitted; this section records which ones the code
actually has, so a later reader can tell an allowed difference from an unfinished
normalisation.

### 8.1 Shared

| | fcitx5 | Android | TSF |
|---|---|---|---|
| composing state | `hunmin_buffered_preedit` | same, through JNI | same, through `TsfCore` |
| word buffer | none | none | none |
| key rule | `hunmin_buffered_preedit_classify()` | same, through `nativeLayerClassify` | same |
| `caps` option | per key event | per key event | per key event |
| boundary | a declined key, or the classifier's `BOUNDARY_PASS` | same | same |
| preedit source | `display()` | `display()` | `display()`, and `composing()` for the interim span |
| external text | — | the layer's pass-through | — |

### 8.2 Differences

| dimension | fcitx5 | Android | TSF | §3 |
|---|---|---|---|---|
| key to `(base, shift, mods)` | `kSymKeyIds` table + `KeyStates` | `HunminKeyRegistry.engineBase` + metaState | `VkToBase` + keymods | allowed, §3.1 |
| commit unit | `CommitBehavior` config | `commit_behavior` preference | `CommitUnit` (Win32 dialog and the WinUI settings row) | allowed, §3.3 — resolved 2026-10-25, commit 1430ab7 |
| preedit presentation | client preedit, else the panel | `setComposingText` | `ITfComposition`, or the interim document span | allowed, §3.3 |
| delivery strategies | one | one | two (native, interim) | allowed; only TSF meets classic hosts |
| candidate anchor | `display()` | the service owns the Hanja UI | native `display()`, interim `composing()` | allowed, §3.3 |
| candidate key routing | the adapter's own switch | none in the adapter | the adapter's own window | allowed — but see 8.3 (2) |
| Han/Eng | not in the addon: fcitx owns the trigger (FE13) | Kana VK to `KeyAction.HANGUL_ENG`, performed by the layer | `ToggleModeCore`, performed by the layer, plus configurable bindings | resolved 2026-10-25 (#136 ruling): the two frontends that own a key call `hunmin_buffered_preedit_toggle_language()`; fcitx5's half is environmental (FE13) |
| R8 language detection | no: the engine judges, the adapter acts | no: same | no: same | resolved 2026-11-01 (#164) — the judgement is the engine's for all three, and each frontend performs only the switch its host allows |
| voice / external text | — | yes | — | allowed, but invariant 5 is exercised on one frontend only |

### 8.3 Open asymmetries, to be resolved where the platform allows

1. **~~Han/Eng is not normalised (§4).~~** Resolved for the two frontends that
   own a key, 2026-10-25 (#136 ruling; commits `8b8a2f9` TSF, `5d4b3f0`
   Android): both build the layer with two slots and map their Han/Eng key to
   `hunmin_buffered_preedit_toggle_language()`, so neither performs a switch of
   its own. The tier-A case is `LayerTest.ModeChangeIsABoundaryAndSwitchesLayout`
   (`engine-buffered-preedit`); the per-frontend mapping is asserted by
   `TsfConfigTest.AdapterKeyDriveOverTheMockLayer`,
   `CandidateTest.ModeToggleIsABoundaryWithTheWindowOpen`, and
   `HunminAdapterTest.the language action is the layer's, not the adapter's`.
   *Environmental part:* fcitx5's trigger belongs to fcitx, so the addon cannot
   see a Han/Eng key. What the platform allows is the mapping, not the key
   (FE13).
2. **The classifier's `CANDIDATE` answer is used by one frontend.** fcitx5 asks
   and then routes locally, Android reports no candidate window, TSF never
   reaches it. The rule is shared; the use is not. Resolving it means routing
   each frontend's list keys through the answer instead of around it.
3. **~~TSF's commit unit is not user-selectable.~~** Resolved 2026-10-25
   (commit `1430ab7`): `CommitUnit` in the TSF config store, with the row on
   both settings surfaces, so all three frontends now read a setting. The
   env -> registry -> default chain keeps the corpus seam.
4. **External text exists on Android only.** Not a defect — the voice path is
   Android's — but it means invariant 5 has no second frontend exercising it, and
   nothing would notice if the layer's pass-through regressed for others.

Not on this list, deliberately: the per-frontend key mapping, the presentation
APIs, and TSF's two delivery modes. Those are the differences §3 permits, and
normalising them would be the thing that reintroduces drift.

## 9. Wording

Quoted from the maintainer, 2026-10-24:

> tsf interim mode and normal mode should have same output with same key
> sequence; this is the fundamental rule in this work.

> between frontends, which may not support some keys forwarding - for example,
> android passes kana other than han/eng, custom key mapping is allowed. but
> other than that, same input sequence should produce same output.

> commit boundary is not a matter. all front-ends may have buffered commit /
> syllable commit mode for compatibility. but the result should be identical.

> in android, han/eng key produces kana vk input unexpectedly. so you should
> map han/eng key input from other frontend to that behavior of android.

> custom key mapping is only provided for han/eng mode change, hanja, speech
> recognization.

> extend libhunmin to select buffered / syllable preedit, but it should be a
> separate layer above current libhunmin implementation.

> for tsf frontend, the test should be run on sandbox, not in dev env. or, it
> should be possible to clean up dev env setting after test, to make the
> worktree deletable.

## 10. Decisions taken 2026-10-25

The three questions this section used to hold were put to the maintainer with the
options measured, and all three were answered. Quoting the chosen option in each
case, because a paraphrase drifts:

1. **The layer's API shape** — chosen: "Exported hunmin_buffered_preedit_* symbols in
   libhunmin (append-only ABI, one implementation, symbols.txt grows)". The
   alternative on the table was a static `buffered_preedit.c` compiled into each frontend;
   it was not taken because it puts the same rule in three builds and #128 exists
   to have one.
2. **The corpus record format** — chosen: "Reuse the tier-C trace format
   (`*.trace.in` with per-key consumed/commit/preedit) as the one corpus; extend
   the win32 dialect to both delivery and both commit modes". Measure before
   trusting: the format already carries the three per-key fields, which is what
   §6 asks for; a second format would be a second home for the same expectations.
3. **The Han/Eng normalization** — chosen: "An abstract shared action in the
   layer (`hunmin_buffered_preedit_toggle_language` / a MODE_CHANGE key class); each adapter maps
   its own physical key to it". So no adapter synthesizes a Kana VK.

Still open, and not decided by the above:

- The §5.2 wording question (preedit equality against a free commit boundary).
- Whether per-key assertions are mandatory for every frontend's own tests, or only
  for the shared layer and tier C (§6 allows a frontend test to drive a different
  interface; it forbids a second copy of the expectations).
