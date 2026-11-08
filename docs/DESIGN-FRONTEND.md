# DESIGN-FRONTEND — Frontend equivalence and contract (consolidated)

> Consolidated design home per issue #272 (D3: DESIGN-* family). This document
> gathers the frontend design that was previously split between
> `FRONTEND-EQUIVALENCE.md` and `FRONTEND-CONTRACT.md`. Those paths remain as
> pointer stubs. Requirement text lives only in `docs/SPEC-*.md`; figures live
> in their authoritative homes and are cited by pointer.

---

## Related decisions index (gathered, #272)

| Decision/theme | Part A (equivalence) | Part B (contract/assumptions) | Notes |
|---|---|---|---|
| Same committed text / one delivery | §1 invariant, §5 shared layer, §6 corpus | invariants, A4/A5/A10 | Core delivers; frontends present/host-integrate. |
| TSF delivery modes | §2 interim/native, §2.1/§2.2 rulings | TSF assumptions | Interim/native differ in delivery, not composition. |
| Han/Eng / mode switch | §4 Han/Eng identity, §5.1 layer action | A3/platform key mapping | Abstract action in layer; frontend maps physical key. |
| Buffered-preedit layer | §5 (5.1-5.3) | adapter assumptions | One owner of word buffer (FE3). |
| Caps / modifiers | §8 measured asymmetries | A2/A3, Android A2'/A3' | Per key event; host models differ. |
| Candidate/presentation | §3 allowed differences | UI/panel notes | Host-owned presentation; core owns policy/state. |
| Corpus/conformance | §6, §6.1-6.3 | coverage/gaps | One corpus, frontend-specific delivery constraints. |

---

# Part A — Equivalence (formerly `FRONTEND-EQUIVALENCE.md`)

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
word to judge, and ReqMode10 forbids reassembling the syllable text already in the
client.

How it is implemented: `CTextService::ApplyAutoswitch()` is the one setter of the
option, and it answers `AutoswitchOn()` — the stored value AND the native
delivery. Every site that changes the delivery re-applies it (the layer rebuild,
`DetectLegacyMode`, and the mid-word flip in `OnCompositionTerminated`), so the
engine never holds a state this delivery cannot honour. Pinned by
`TsfConfigTest.TheInterimDeliveryDoesNotHoldTheSwitch`: the store says 1, the
layer answers 0, and a Latin-mode key still passes to the app.

**What the settings row may say — decided 2026-11-05 (issue #215).** The dialog
is a separate process and cannot know a host's runtime delivery mode, so it
cannot say whether the running host will honour the switch. The row therefore
names the support condition instead of the host's state. Maintainer's wording:

> 자동 한/영 전환 (지원 앱에서만, Ctrl+한/영)

The qualifier is true whatever host reads it, so the control no longer promises
something the interim delivery ignores, and the standalone rundll32 path (which
has no live service to ask) needs no new channel. The data-missing suffix is
unchanged: `… — 사전 없음: 사용할 수 없음`. The fcitx5 and Android rows stay
`자동 한/영 전환`: they have no interim delivery and their chord is already
correct.

What this ruling does NOT decide:

- **Whether a future interim shape could carry it.** The two shapes priced in the
  pre-ruling text — hold the word in one growing span, or keep the asymmetry —
  were the options; the ruling picks the second for now, and it does not say the
  first is forever excluded.

### 2.2 Ruled 2026-11-02: with the switch ON, English no longer passes through

Maintainer, 2026-11-02:

> autoswitch ON stops English passthrough to support eng -> han support

ReqMode8's E1 line (ruled 2026-11-01, "E1도 당연히 지원해야지") judges a word typed in
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
  layout per character (the text is what the host used to insert, ReqMode5) and judges
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
delivery timing where the timing is the assertion (ReqMode6's digit run). Its cases
are the lines of SPEC-MODES ReqMode8's formula and the guards around it: the two
switch directions, both-valid and both-invalid, ReqMode9's guard from each side, ReqMode4's
boundary-only moment, ReqMode13's off-is-the-identity state, E1's 0.3 gate sentence
typed as one stream, and the 세벌식 최종 cases ReqMode6 is actually about (its digit row
is the jongseong jamo ᇂ ᆻ ᆸ, so committing each key on its own is what ReqMode6
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
  autoswitch-ON cases. The lane unblocks it the ruled way (ReqMode13 surface A):
  the adapter now reports `language_key` for the COMPOUND press (Ctrl+한/영,
  SPEC-MODES §E5) and acts on `hunmin_buffered_preedit_special()`, and it registers
  a `hunmin_special_host` — without which the engine refuses every function with
  -1 by design. A bare Han/Eng press stays fcitx5's own trigger.

  ⚠️ **Corrected 2026-11-03 (issue #233): the lane writes the VALUE, not the
  chord.** The paragraph above is right about the adapter surface the case needs,
  and wrong about the lane's use of it: `Ctrl+한/영` *toggles* the state (SPEC-MODES
  §ReqMode13 surface A, and engine-d1's watcher case asserts ON → OFF), while the
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

**The Android lane exists: `engine-android-e1`** (`JudgementCorpusTest`, tier E1,
issue #211), and it reads the same corpus file. It replays every case through the
real pair adapter (`HunminAdapter` + the host JNI shim) and asserts three things
the adapter can get wrong: the per-key delivered text (`step_commits`, so ReqMode6's
held run is distinct from a per-key commit), the judgement sequence the adapter
reads, and the mode after the case through the corpus's `probe`. It takes two
Android-specific differences, both named in the lane:

- **It does not skip `mode: latin`.** The fcitx5 addon has no key that sets the
  layer's initial language, but Android's pair adapter owns both slots (FE7), so
  the lane asks it for the Latin start with `toggleLanguage()` and runs those
  cases.
- **The one mid-word case has no probe.** `r4-nothing-is-judged-in-flight` ends
  before a boundary, so a probe typed after it would append to the word still in
  the buffer. The corpus therefore carries no probe for it, and the lane asserts
  the adapter's own language read-back instead.

Measured 2026-11-04 on a host with `ANDROID_HOME` set: 17 corpus cases, 0 skipped,
0 failed, 0.44 s; removing the lane's `toggleLanguage()` call turns it red at
`r8-english-word-switches` (probe delivered `ㅏ` where the Latin reading `k` is
expected), so the probe is load-bearing rather than decorative.

**The lane is green, and so is the tier.** Measured 2026-11-05 on `1a61ed6`
plus this lane, with `ANDROID_HOME` set: `tools/android-unit.sh` (ctest
`engine-android-e1`) passes **157 of 157**. What this paragraph recorded before
was 8 `HunminAdapterTest` failures on 2026-11-04 at `c90e88c` — 158 tests, 8
failed, and 157 tests with the same 8 with the lane absent, so the lane neither
caused nor hid them. They were pre-existing (#246) and #257 fixed them on
2026-11-04, which is why the count is 157 and not 158. The correction is kept
rather than deleted: the earlier text concluded "the tier is red" from a
measurement that was true that day, and the fix is what changed it.

The lane's scope is the adapter, as §6.2.1 asks: it builds the pair adapter the
service builds and performs the frontend's half of ReqMode8 itself. It does **not**
route keys through `HunminInputMethodService`, so the service's choice of which
adapter receives a Latin-mode key is not measured here; that surface remains a
separate question.

**The TSF lane exists: `tsf-test`'s `TsfJudgementTest.CorpusReplay`** (tier F1,
issue #180) reads the same corpus file and replays every case through the real
adapter over the mock TSF layer. It takes one difference the other two lanes do
not, and names it: TSF native delivery inserts each completed syllable into the
document as it is typed, so the corpus's `step_commits` timing — the x11/Android
dialect — is not TSF's observable, and the lane asserts the delivered text
cumulatively (the corpus's `commit`) instead. The mode it asserts directly off
`Snapshot().hangulMode` (a read-back the fcitx5 lane cannot reach), and the
probe after each case; it does not skip `mode: latin`, because the service owns
its language and has a manual switch, so the lane toggles into Latin before
typing. `r4-nothing-is-judged-in-flight` ends mid-word with no boundary, so the
total is not asserted there — the mode read-back is its assertion, as in the
Android lane. Measured 2026-11-04 on a Windows host (MSVC, Visual Studio 17
2022): 17 cases, 0 failed; forcing the adapter's autoswitch OFF turns the lane
red (10 mode/total failures), so the probe is load-bearing rather than
decorative.

What the fcitx5 lane itself does not do, named rather than hidden: a case whose
`mode` is `latin` is skipped, because the starting language belongs to the host
and this addon has no key that sets the layer's initial mode — the lane rebuilds
the layer per case instead, which is the frontend characteristic taken here.
Mid-stream switches are covered (the mixed-sentence case flips four times). The commit unit
of §6.1 is likewise outside this corpus: every case uses
`HUNMIN_COMMIT_AT_BOUNDARY`, and the judgement cannot run in PER_SYLLABLE at all
(its earlier syllables are already in the client, which is ReqMode10's constraint).

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

**A second Korean text, the anthem** (`tests/golden/texts/anthem.txt`): 애국가
1~4절, ten lines, typed through the same two layouts. It is here because the
anthem is the only thing in the tree that types 2~4절, and because a four-verse
lyric reaches syllables 서시 never does — `닳도록`, `철갑`, `공활한데`.

| layout | cases | events | what it could not take |
|---|---|---|---|
| `dubeolsik` | 10 of 10 | 378 | — |
| `sin3-p2` | 10 of 10 | 378 | — |

That second row cost a defect to earn, and the defect is the interesting part. The
first run took **9 of 10** on `sin3-p2` and blamed the layout: `높고` rendered
놔고. It was the INSTRUMENT. `sin3-p2` has two keys for ㅗ — `/` and `v` — and
they are not interchangeable: `/` is the connecting form that builds 워/와/외
with a following 중성, `v` is the standalone one. The probe took whichever key
rendered the vowel first, so it picked `/`, and `/`+`f` is ㅏ (ㅗ+ㅏ=ㅘ) — while
the 종성 probe had found ㅍ on `f` using a ㄱ+ㅏ prime. 노+`f` was therefore 놔,
and 높 was called untypeable when 높 is ㄴ+`v`+`f`. ㅜ/`o`/`b` is the same pair.
`tools/jamo-keytable.py` now prefers the candidate that does not extend into
another 중성 (`v`, `b`), measured by appending every single key and counting the
compound vowels, and the same run reports 10 of 10.

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
| ReqMode8 language detection | no: the engine judges, the adapter acts | no: same | no: same | resolved 2026-11-01 (#164) — the judgement is the engine's for all three, and each frontend performs only the switch its host allows |
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
   *Measured 2026-11-04 (issue #246): "calls the layer" was not the whole
   contract.* The layer commits the pending word at the toggle's boundary and
   hands it back, and Android's `toggleLanguage()` dropped that commit — so the
   word being composed at the moment of the 한/영 press was lost on that
   frontend, while TSF's `ToggleModeCore` and fcitx5's `applyJudgement` both
   insert it. The adapter now takes the `InputConnection` and delivers it;
   `HunminAdapterTest.mode toggle is a boundary that flushes the word` pins the
   delivery and the `no-toggle-delivery` mutation proves the test can fail.
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


---

# Part B — Contract and assumptions (formerly `FRONTEND-CONTRACT.md`)

# hunmin — frontend contract audit

What each frontend file **assumes**, and whether the assumption holds. Written
2026-10-02 after reading every line of the fcitx5 addon against the fcitx5
5.1.19 sources, because several assumptions turned out to be wrong and were not
discoverable by re-reading our own code.

**Status: a dated audit, still cited for §3–§6.** The per-assumption entries
(A1–A13, A1′–A3′, B1–B7) are the record of what was believed and measured on
that date — keep them. What is NOT this document's job any more: describing the
interfaces (now `docs/SPEC-FRONTEND.md` and `docs/FRONTEND-EQUIVALENCE.md` §8)
and stating the invariants to preserve (now `docs/SPEC-FRONTEND.md` FE1–FE12).

Scope: `frontends/fcitx5/src/`, `frontends/android/`, and the Windows TSF
adapter in `frontends/tsf/src/`, plus the libhunmin ABI they depend on. The
engine core is covered by `docs/SPEC-P2.md`; this file is about the adapter
boundary where platform events meet the engine.

**Cross-frontend equivalence is a separate, normative requirement:**
`docs/FRONTEND-EQUIVALENCE.md`. This audit records what each adapter assumes;
that document defines what all adapters must agree on (the same key sequence
must yield the same commit, preedit, and consumption, and TSF's interim and
native delivery modes must agree).

Sections 1–5 are the fcitx5 addon. **§6 is the Android IME** and is written to
the same standard: every "Assumes" line is a claim about somebody else's code
(AOSP or the NDK) and is therefore the thing most likely to rot.

Read this before changing `hunminadapter.cpp`. Every "Assumes" line below is a claim
about somebody else's code (fcitx5 or libhunmin) and is therefore the thing
most likely to rot.

When adding a claim here, consult the upstream developer wiki first for
intent and prohibitions, and the 5.1.19 source for behaviour — see
`docs/FCITX5-ADDON-NOTES.md` §8c for which is which, and for the three
places the wiki is imprecise.

---

## 1. Role of each file

| File | Role |
|---|---|
| `src/hunminadapter.h` | addon entry surface: `HunminAdapter : InputMethodEngine`, config struct, `HunminAdapterFactory`, the `hunmin_ctx*` handle |
| `src/hunminadapter.cpp` | the whole adapter: key mapping, per-context state property, preedit/c commit presentation |
| `src/hunmin.conf.in` | `inputmethod/hunmin.conf` template → IM registration (`Addon=hunmin`) |
| `src/hunmin-addon.conf.in.in` | `addon/hunmin.conf` template → addon registration (`Library=libhunmin`, `OnDemand=True`) |
| `test/testhunmin.cpp` | tier D1: headless `Instance` + `testfrontend`, asserts commit strings |

Both sources are inside the "frontends stay thin" budget (rule #4): no
composition logic, no per-layout knowledge, no jamo handling.

⚠️ **Sizes are measured, not written here.** This table used to carry exact
per-file LOC counts, which rotted silently — they had drifted by 100+ lines with
no test noticing, because a number in prose cannot fail. The budget is now
invariant **I8** in `tools/check-frontend-invariants.py` (ctest
`frontend-invariants`): it counts code lines across every translation unit the
adapter owns — `hunminadapter.{h,cpp}`, `config.h` — comments and blanks
excluded, and fails above **700** — the maintainer's 2026-10-25 ruling; the 545
this line used to carry was a pre-ruling figure the checker never enforced.
(`language_detect.{h,cpp}` left that list with
the detector on 2026-11-01, #164.) Do not add counts back to this table;
add a rule to the checker instead. Reading the current number:

```sh
python3 tools/check-frontend-invariants.py | grep I8
```

The budget exists to catch *composition* logic or per-layout knowledge
appearing in the frontend, which is rule #4's actual failure mode.

## 2. The interfaces (a 2026-10-02 snapshot)

The description of what each frontend exposes, and the invariants that keep them
equivalent, live in `docs/SPEC-FRONTEND.md` (FE1–FE12) and
`docs/FRONTEND-EQUIVALENCE.md` §8. What stays here is the fcitx5 hook contract
this audit was written against, because it is the shape the addon must keep.

| Hook | When fcitx5 calls it | What we do |
|---|---|---|
| `activate(entry, icEvent)` | context becomes the active IM | `state->reset()` |
| `deactivate(entry, icEvent)` | context stops being the active IM | delegates to `reset` |
| `keyEvent(entry, keyEvent)` | every key press **before** the client sees it | map, then feed the layer: `filterAndAccept()` or pass |
| `reset(entry, icEvent)` | focus change / explicit reset | `flush()` then `reset()` |
| `reloadConfig()` | addon config (re)load | `readAsIni(config_, "conf/hunmin.conf")` |

`getConfig()`/`setConfig()` round-trip the addon's own `Layout` option through
`safeSaveAsIni`, so that file is user-visible. Per-context state is
`HunminState : InputContextProperty` registered as `"hunminState"`; the engine
handle lives there, so two open windows get two independent engines and no
shared buffer.

## 3. Assumptions, and whether they hold

### A1. Key convention is ASCII, not keysym-to-layout-table

`keyEvent` feeds `sym` straight through: `sym >= 0x20 && sym <= 0x7E` becomes the
character, `BackSpace` becomes `0x08`, everything else flushes and passes on.

**Assumes** the layout data is authored in the US-QWERTY character convention
(`docs/trace-spec.md`), so the engine can be layout-agnostic and fcitx5 need
not remap. **Holds** — this is what lets one addon serve all layouts and is the
whole reason the golden traces are byte-comparable with libhangul. The cost is
that a layout wanting a non-US physical key cannot be expressed here.

### A2. Shift is data, not a modifier

`hunminadapter.cpp:49-64` returns early for Shift and for Ctrl/Alt/Super/Hyper, but
deliberately lets Shift through as a plain character — uppercase letters
(`FcitxKey_A`…`FcitxKey_Z`) reach the engine as-is.

**Assumes** layouts encode shift as uppercase rows (sin3-p2's symbol tiers and
초성체 depend on it). **Holds.** If a layout needed Shift as a state bit, this
would have to change.

**Dual convention appended (issue #90, 2026-10-18):** the engine now also has
the explicit-modifier entry `hunmin_ic_process_key(ic, base, shift)`
(docs/SPEC-P2 "IC input entry points"; measurement in
`docs/OPTIONS-MEASURED.md` §IC-2-B). fcitx5 does not use it and everything in
A1–A3 above stays its contract unchanged; the value is that the case endpoint
— the CapsLock×Shift decision each adapter otherwise re-derives, and the one
Android shipped wrong once (A3') — now has one engine-owned home, engine-tested
without any host layout (ctest `engine-process-key`, tier A). A frontend that
migrates keeps its VK/keysym → base-character half and passes the bit; the
already-resolved rule covers a host that delivers the resolved character
itself (the fcitx5 shape).

### A3. CapsLock is inverted by the adapter

`hunminadapter.cpp:66-72`: when CapsLock is down and the sym is `A`–`Z`, it lowercases
the sym and sets the engine's `caps` option to 1.

**Assumes** fcitx5 delivers the *shifted* letter under CapsLock, so the addon
must undo the shift and tell the engine separately. **Holds** for the caps
semantics, but note this is **US-layout-specific**: with a different host
keyboard layout, CapsLock+letter may not arrive as an uppercase keysym at all,
and the adapter would then feed the lowercase form with `caps=1`, silently
producing caps semantics where the user typed something else. D1 asserts this
path with an explicit `KeyState::CapsLock`; nothing asserts it under a
non-US host layout.

### A4. `hunmin_ic_commit` is a destructive read — **our comment says the opposite**

`hunminadapter.cpp:91-96`:

```cpp
} else {
    // Engine declined (space, unknown row): flush emits the process
    // commit buffer *and* any in-flight syllable in one string —
    // hunmin_ic_commit does not clear on read, so never do both.
    flush();
}
```

**The comment is wrong.** `hunmin_ic_commit` *is* destructive:

```c
/* libhunmin/src/ic.c */
const char *hunmin_ic_commit(hunmin_ic *ic) {
  ...
  ic->commit[0] = 0; /* destructive read: a commit is delivered exactly once */
```

and `hunmin.h` documents it the same way. The **behaviour is still correct** —
reading `commit` and then calling `flush()` in the same keypress would
double-emit, and the `if (used)` / `else` split prevents that. But the stated
reason is the opposite of the real invariant, so anyone "simplifying" this
later will reason from a falsehood. Fix the comment.

The real invariant worth writing down: `hunmin_ic_flush` calls `save_commit`
(which *appends* the in-flight syllable to the existing commit buffer) and then
returns it via `hunmin_ic_commit`. So flush already includes anything a prior
`process` produced, which is exactly why the two must not be combined.

### A5. Preedit is non-destructive, commit is not

`updateUI()` re-reads `hunmin_ic_preedit` on every keystroke and the ABI calls
it non-destructive. **Holds**, and it is what lets `updateUI()` be idempotent.

### A6. Context capability decides where preedit goes

`hunminadapter.cpp:145-148`: `setClientPreedit` if the context advertises
`CapabilityFlag::Preedit`, else `setPreedit`.

**Assumes** the XIM/DBus clients set that flag and that the distinction is
meaningful. **Holds**, and D1's `testfrontend` IC exercises one branch only —
the other branch is not covered by any test today.

### A7. Layout changes are lazy

`ensureIc()` compares `config().hangulLayout` against the cached `layout_` and only
rebuilds the `hunmin_ic` on a key event. `setConfig` therefore only calls
`reset()` on live contexts and comments that the swap happens "lazily on the
next key".

**Assumes** nothing reads `hunmin_ic` between a layout change and the next key.
**Holds only because `updateUI()` is the sole reader and is itself called from
the key path.** If a future feature reads preedit on focus change, this
becomes a use-after-stale-layout bug rather than a crash.

The resolved layout is now passed in per key event from
`HunminAdapter::layoutFor()`, so a profile change takes effect on the next
keystroke without `setConfig` running.

### A8. Data is found via `HUNMIN_DATA_PATH`, else the installed dir

Constructor: `hunmin_ctx_new(nullptr, 0)` (which reads the env var) and falls
back to `HUNMIN_DEFAULT_DATA_DIR`. **Holds**; this is how D1 and D3 point at the
source tree. Note the fallback only triggers on a *null* context — a context that
loads but has no layout for the configured id fails later in `ensureIc`, per-key.

### A9. `propertyFor` is non-null after registration

`keyEvent` dereferences `state(...)` without a null check, while `activate` and
`reset` do check. **Holds** because the property is registered in the
constructor for the `InputContextManager`'s lifetime, but it is an inconsistency
worth noting: one of the three sites assumes, the other two defend.

### A10. `hunmin_ic_flush` on `reset` is deliberate

`reset()` calls `flush()` then `hunmin_ic_reset()` so in-flight composition is
committed rather than dropped when focus moves.

**Assumes** committing on focus-out is the desired UX. It is a product
decision, not a technical one, and it is *not* symmetric with
`HunminState::reset()` (the internal one), which drops everything. Both are
called from different places; the naming makes this easy to get wrong.

### The panel and the preedit are separate lifetimes

`updateUI()` does **not** call `inputPanel().reset()`. That call also clears the
candidate window, which has to outlive the preedit it was opened from — F9
anchors its window to the Hangul that was composing, and choosing a candidate
commits the Hanja in place of that Hangul. Instead `updateUI()` writes the
preedit slot on every update and an empty `Text` clears it.

Without that, a committed composition stayed on screen: `reset()` was what had
been clearing it. D2's flush case caught the absence immediately.

### The tests dlopen the addon, so they must build it

D1, D2 and Tier C all `dlopen` `TEST_ADDON_DIR/libhunmin.so` — they do not link
`hunminadapter.cpp`. `add_dependencies(testhunmin copy-addon copy-im)` used to satisfy
that orderingly, but `copy-addon` only copies `hunmin-addon.conf`; it does not
depend on `hunmin_addon`. So `make testhunminui` would not rebuild the library
after an `hunminadapter.cpp` edit, and **every mutation planted in the adapter
survived** while the tests still reported green.

That is the worst possible failure shape: a test that cannot fail looks exactly
like a test that has nothing to catch. Found by planting five mutations in the
F9 path and finding all five survive a green D1/D2.

`add_dependencies(copy-addon hunmin_addon)` in
`frontends/fcitx5/test/addon/CMakeLists.txt` closes it. Note that `DEPENDS` on
an `add_custom_target` only orders it — it does not make it produce the
library, so `add_dependencies` is the one that works.

**How to tell it regressed.** `touch frontends/fcitx5/src/hunminadapter.cpp && make
--build build --target testhunminui` must change the mtime of
`build/fcitx5-addon/libhunmin.so`. If it does not, no D1/D2/Tier C result from
that build means anything.

## 4. Gaps in coverage

Found by **mutation testing** on 2026-10-02: four realistic mutations were
planted in `hunminadapter.cpp` and checked against both §5 and the D1 suite. Two
mutations compiled, passed D1, and slipped past every written invariant:

| Mutation | build | D1 | caught by §5? |
|---|---|---|---|
| `commit` and `flush` both run for one keypress (drop the `else`) | ok | **PASS** | **no** |
| `caps` option no longer fed to the engine | ok | **PASS** | **no** |
| `filterAndAccept` moved out of the `if (used)` guard | ok | **PASS** | yes |
| `KeyState::Shift_L` added to the modifier mask | fails | FAIL | yes |

- **The commit/flush double-emit is invisible to D1.** §5 states the rule but
  gives no way to *check* it, and D1 only pushes commit expectations, none of
  which produce a commit-buffer-plus-in-flight-syllable pair. The `else` split
  is load-bearing and unguarded. *(Still open.)*
- **`hunmin_ic_set_option(ic, "caps", …)` was untested.** The option only affects
  `sin3-p2-yet`, but the addon config default is `Layout=sin3-p2`, which ignores
  it, so the CapsLock branch asserted nothing. **Closed:** D1 now pins
  the per-item layout to `sin3-p2-yet` and replays the golden's
  caps-tier-blocked / caps-tier-open pair. Dropping the `set_option` call now
  fails D1 — mutation-verified.

### Per-item `Layout=` (defect, found and fixed 2026-10-02)

`hunminadapter.cpp` used to read only `config().hangulLayout`, the addon-wide
`conf/hunmin.conf` option, so a `Layout=` beside an IM in the user's profile
had no effect.

The fix routes through `InputMethodGroup::layoutFor(entry.uniqueName())`.
`InputMethodEntry` has no layout accessor — grep shows the per-item field is
written back to `profile` by `inputmethodmanager.cpp:443` and otherwise never
read, so `layoutFor()` is the only way in. Empty means "not overridden", and
the addon default applies.

**Do not conclude from "fcitx5 does not read it" that it is your bug.** It
looked like a core defect at first and was not: the value is deliberately
carried for addons to consume. Check `grep` for the accessor before assuming.

Also untested:
- A3's non-US-host-layout case.
- A6's `setPreedit` branch (contexts without `CapabilityFlag::Preedit`).
- `setConfig` swapping the layout on a live context (A7).
- ~~Candidate lists: nothing in the addon uses them yet.~~ **Resolved 2026-10-03**
  when P3 landed: F9 opens the Hanja window and `testhunminui.cpp:168-200` asserts the
  candidate list's presence, size, kind label and text, plus commit-on-select. The
  remaining gap is the candidate list's *empty* case and the kind label for a source
  with more than one dictionary.

## 5. Invariants to preserve when editing

Each is stated with the check that would catch its violation, because §4
showed that a prose rule nobody can execute catches nothing. Run
`tools/check-frontend-invariants.py` before committing any change here.

1. **Never call `hunmin_ic_commit` and `hunmin_ic_flush` for one keypress.**
   Check: exactly one `hunmin_ic_commit(` call and one `flush();` call in the
   `process()` block, with the flush in the `else`. Both readers are
   destructive, so a second read double-emits to the client.
2. **Commit reaches the client exactly once per keypress.**
   Check: one `commitString(` call, inside the `used` branch.
3. **`filterAndAccept()` only when `hunmin_ic_process` returned true.**
   Check: it appears once, inside `if (used)`. Returning false is how the
   client gets space, backspace and unmapped keys.
   F9 does not need an exception: it is intercepted above the printable-ASCII
   test, on candidates, so it never reaches the engine and never depends on
   `used`.
4. **Shift is never a modifier chord.**
   Check: the `KeyStates modifiers` mask contains Ctrl/Alt/Super/Hyper and
   not Shift. Shift encodes layout rows.
5. **CapsLock state is handed to the engine.**
   Check: `hunmin_ic_set_option(ic, "caps", ...)` is present and driven by
   `rawKey().states().test(KeyState::CapsLock)`. Only meaningful for
   `sin3-p2-yet`; see §4 for why this was previously untested.
6. **`hunmin_ic` lifetime is owned by `HunminState`, one per input context.**
   Check: no `hunmin_ic` field anywhere outside `HunminState`.

7. **The per-item profile `Layout` wins over the addon-wide default.**
   Check: `layout.empty() ? *engine_->config().hangulLayout : layout` is present
   and reaches `keyEvent` via `layoutFor(entry)`. Added after finding that
   fcitx5 writes the per-item value back to the profile but the addon never
   read it — the two code paths look identical once the override is discarded,
   so only a structural check catches it. The accessor was renamed
   `config().layout` → `config().hangulLayout` by `1521fad` (separate
   Hangul/Latin pickers); this check and `tools/frontend-mutations.py`'s
   `I5-layout-override-ignored` anchor name it, so both move with it.
8. **The adapter stays inside the thin-adapter budget (rule #4).**
   Check: code lines across `hunminadapter.{h,cpp}`, `config.h`
   ≤ **700**, comments and blanks excluded. This is a measurement, not a
   description — see §1. Exceeding it means composition logic or per-layout
   knowledge has crept into the frontend, which is the failure mode rule #4
   exists to prevent. The count is of *every* translation unit the adapter
   owns, not just `hunminadapter.{h,cpp}`, because splitting logic into a new file
   would otherwise walk straight past the budget while the adapter grew. The
   limit has moved twice, each time for work that is frontend by nature and
   cannot live in the core: the F9/candidate presentation (400 → 465), and
   keyboard selection in the candidate window (465 → 545, 2026-10-11), which
   exists because fcitx5 does not intercept Up/Down/Return for a
   `CursorMovableCandidateList` — it only offers `prevCandidate()`/
   `nextCandidate()` and calling them is the input method's job.
9. **The Hanja window never reads or rewrites surrounding text.**
   Check (I9): no `SurroundingText` / `getSurroundingText` /
   `deleteSurroundingText` / `setCursorPos` call anywhere in `hunminadapter.cpp`;
   `openCandidates()` reads `hunmin_ic_preedit()` and nothing else;
   `commitCandidate()` commits and resets rather than re-inserting.
   PLAN.md's P3 note records why from the fcitx5 wiki Q&A: many clients do not
   implement it at all (XIM clients, SDL, terminals) and it is buggy in browsers
   and embedded terminals, so depending on it is worse than working without it.
   Hangul upstream uses surrounding text only for *temporary* Hanja mode.
   **No tier can catch this at runtime** — an adapter that read it would still
   type correctly, just less portably — which is why it is structural.
10. **`copy-addon` depends on `hunmin_addon`.**
   Check (I10): `add_dependencies(copy-addon hunmin_addon)` in the frontend
   test CMake. Without it D1/D2 link a stale `libhunmin.so`: `copy-addon`
   copies a `.conf` and does not otherwise produce the library, so editing
   `hunminadapter.cpp` does not rebuild it and the suite stays green. Found when five
   F9 mutations all survived a green run. A stale binary that passes is worse
   than a failing one — it is indistinguishable from a test with nothing to
   catch.

11. **The layout picker's list comes from the core, and the picker keeps it.**
    Check (I11): inside `refreshLayoutList()`, `count` is assigned from
    `hunmin_ctx_layout_count()` and `id` from `hunmin_ctx_layout_id()`; and
    `setLayouts()` in `hunminadapter.h` stores its argument in `layouts_`. The bindings
    are to those variable names deliberately — two looser forms were measured
    passing against a hardcoded picker that kept a call alive only to satisfy
    the grep. All three loosenesses are caught by D2 behaviourally; I11 is the
    check that needs no build. AGENTS.md rule 1 ("layouts are data, never
    code") has to hold at the UI boundary too — a picker with the ids written
    out in the frontend is a second place to edit per layout. Both halves are
    checked because a `setLayouts` with an empty body still *looks* right: an
    annotation that drops what it is handed still renders as a combobox, just
    an empty one. That is not hypothetical — D2 caught exactly this
    (`picker offers 0 layouts, data ships 12`).

Invariants 1, 2 and 5 are exactly the ones D1 does **not** cover. Do not
rely on the suite for them. 7 and 8 are likewise uncovered by any behavioural
test: 7 because the bug was invisible at runtime, 8 because no test can fail on
a number written in prose. 11 is covered by D2 *behaviourally* (row count
against the shipped data, and every id checked against a file on disk) and by
I11 *structurally*, so a build is not needed to catch it.

---

## 6. The Android IME (`frontends/android/`)

Four files, and the split is the same one the fcitx5 addon makes:

| File | Role | Code lines |
|---|---|---|
| `HunminInputMethodService.kt` | lifecycle, key dispatch, external-text door | ~230 |
| `HunminAdapter.kt` | the adapter proper: keycode → base + shift bit, commit/preedit | ~190 |
| `HunminKeyRegistry.kt` | the AOSP keycode table and the chord mask | ~200 |
| `HunminDataInstaller.kt` | extract the layout tables out of the APK into filesDir | ~120 |
| `cpp/hunmin-jni.c` | handle conversion and UTF-8 marshalling, nothing else | ~230 |

### A1'. The key convention is US-QWERTY, and the OS layout is deliberately NOT consulted

`KeyEvent.getUnicodeChar()` is the obvious implementation and it is wrong here:
it resolves the key through the *host* keyboard layout, so on a French host the
key that is `a` on a US board arrives as `q` and the engine composes the wrong
thing — with every test still green, because seeing it requires changing the OS
layout. The layout data is authored in the US-QWERTY convention, so the adapter
is too: **keycode → character**. This is the same decision as the fcitx5
adapter's A1, for the same reason, and inherits the same stated cost.

### A2'. metaState bits are AOSP's, not fcitx5's

**This one shipped a real bug and the mutation test caught it.** The chord mask
was first written as fcitx5's `0xf0000000` `KeyState` range, which matches *no
AOSP bit at all* — Ctrl+key was never recognised as a chord and every Ctrl
combination reached the engine as a plain letter. The correct values are read
from `android.jar` with `javap -constants` and written out in `HunminKeyRegistry.kt`:

```
META_SHIFT_ON = 0x1   META_ALT_ON = 0x2      META_CTRL_ON = 0x1000
META_META_ON  = 0x10000                   META_CAPS_LOCK_ON = 0x100000
```

Two traps inside those: Android has **no** `META_SUPER_ON`/`META_HYPER_ON`
distinct from the META bits ("super" is the Meta key, reported as
`META_META_*`), and there is **no** `META_CAPS_LOCK_MASK` — only the ON bit.
A mask of `0xf0000000` additionally sweeps up CapsLock, which is the `caps`
engine option, not a chord.

### A3'. CapsLock is the `caps` option; the shift bit travels verbatim

AOSP delivers the *shifted* form under CapsLock. Migrated to
`hunmin_ic_process_key` (issue #105): `onKey` passes the key's UNSHIFTED base
plus the AOSP `META_SHIFT_ON` bit, verbatim, and sets `set_option("caps", 1)`
from `META_CAPS_LOCK_ON`. The CapsLock×Shift matrix that used to live in
`HunminKeyRegistry.engineChar` is deleted — including its history: an early
revision returned uppercase under CapsLock and ignored Shift entirely, so
CapsLock+Shift+letter produced the wrong case, and the corrected matrix made
CapsLock win over Shift. Which row the (base, shift) pair selects is now the
engine's decision alone (SPEC-P2 "IC input entry points"; ctest
`engine-process-key`, tier A).

Two behaviour deltas ride on the deletion, measured, not re-decided here:

- CapsLock+Shift+letter now reaches the engine with shift=1 and composes the
  SHIFTED row; the deleted matrix made CapsLock win and composed the plain
  row. Restoring that inversion is a one-line adapter change and a ruling.
- Shift+`,` and Shift+`.` resolve through the loader's map to `<` and `>` —
  unmapped in every shipped layout, so they decline and pass through, which is
  the answer the fcitx5 host already gives — where the deleted matrix dropped
  the bit and composed the plain row.

CapsLock alone is unchanged: bit=0, plain row, `caps` option set.
Only `sin3-p2-yet` reads the option, so a test on `sin3-p2` asserts nothing —
`android-mutations` proves this by deleting the `set_option` call and watching
the suite catch it.

### A4. Pass-through is the return value, not a re-sent key event

`HunminAdapter.onKey` answers `false` for a key the engine declined — space,
Enter, punctuation, an arrow, an unmapped keycode — and
`HunminInputMethodService` then hands that key to the platform through
`super.onKeyDown()`. The adapter never calls `InputConnection.sendKeyEvent`:
doing both would deliver the character twice.

That makes `onKey`'s boolean the tier-E1 instrument for pass-through, and it is
what the edit-key cases assert (`enter after a committed word is not consumed`,
`comma after a word commits the word and passes through`). An earlier revision
of those cases counted `sendKeyEvent` payloads on the recording fake, which the
adapter never sends, so they could only ever read zero — the assertion measured
the fake, not the contract. The fake records the call now so a future
double-delivery would be visible rather than swallowed by a stub.

### B1. A hardware-key IME still needs an input view — measured, not assumed

`onCreateInputView` returns an empty `View`, not null, and
`onEvaluateInputViewShown()` returns true. The reason is the platform contract:
key events reach an IME through the IME window, so a service that shows no input
view has nowhere for them to arrive. Verified on the emulator — one
`input keyevent KEYCODE_K` alone puts `ㄱ` in the field — so the view is load-
bearing infrastructure, not decoration, and a future soft keyboard replaces its
*body*, not its existence.

Note it is a plain `android.view.View`, **not** `InputMethodSimpleView`: that
class is LatinIME's own (`android.inputmethodservice.InputMethodSimpleView` does
not exist on the platform) and is not available to a third-party IME.

### B2. The JNI shim must not be named `libhunmin.so`

The shim links the engine, so naming its output `libhunmin.so` made the link
input and the link output the same path. Ninja produced a hardlink back to the
engine: the JNI object was compiled and discarded, the library had **zero**
`Java_org_*` symbols, and the APK shipped the bare engine with no adapter — on
every build reporting success. It is `libhunjni.so`, loaded by
`System.loadLibrary("hunjni")`, and `CMakeLists.txt` now fails the build if the
output exports no `Java_org_*` symbol.

Two related traps: `@JvmStatic` is load-bearing on every `external fun` (without
it they are instance methods and JNI hands the C side a `jobject` where the
signature says `jclass` — both are pointers to `_jobject`, so nothing crashes);
and the layout tables are extracted from APK assets to `filesDir` because
libhunmin's loader is a filesystem scanner that `opendir`s `<dir>/layouts`.

### B3. Tier E1 can be green while measuring nothing — twice over

Both of these shipped here, and both are now guarded:

- Gradle reports `BUILD SUCCESSFUL` when every test was **skipped**. JUnit 4
  records an initialisation error in `@Before` as a skip, not a failure, so a
  missing main classes entry or a missing env var produced 17/17 "skipped" and a
  green build. `tools/android-unit.sh` parses the result XML and **fails** if
  anything was skipped.
- The test task is keyed on test *classes*, so changing only an environment
  variable leaves it `UP-TO-DATE` and the report on disk is the previous run's.
  The script passes `--rerun-tasks` for that reason.

### B4. Tier E2's assertion must not be anything hunmin wrote

`tools/android-e2e.sh` reads the committed text out of the platform's own view
hierarchy with `uiautomator`. A trace sink or a log line would be our code
judging our own code: a wrong adapter is wrong in its log too. Four platform
facts the harness encodes after each produced a misleading result first — see
TESTING.md tier E2 for the list and the measurements.

### Invariants for this frontend

1. **Never call `hunmin_ic_commit` and `hunmin_ic_flush` for one keypress.**
   Same rule as §5.1. ⚠️ **Measured: swapping flush for commit on the declined
   path is NOT observable.** Every decline goes through `ic_push()`, which
   clears the buffer and calls `save_commit()` *before* returning false
   (`ic.c:149-153`), so by the time the adapter chooses a branch both calls
   return the same text and leave the same preedit. The `else` is kept because
   it is the correct call and the equivalence is a property of the current
   engine, not a guarantee — but no test can hold it, and
   `tools/android-mutations.py` names the omission rather than hiding it.
2. **Commit reaches the client exactly once per keystroke.** Asserted on the
   call *count* (`HunminAdapterTest`), because the concatenated text is
   identical when doubled.
3. **Consumption follows the engine's answer, never the keycode table's.**
   Space, unmapped keys and modifier chords must reach the application;
   swallowing them would break every shortcut behind the IME.
4. **Shift is never a modifier chord**; CapsLock is the `caps` option, never a
   chord either.
5. **`hunmin_ic` is owned by the engine instance, one per `onStartInput`.** Two
   visible editors must not share a composition buffer.
6. **No surrounding text, ever.** `RecordingInputConnection` routes every
   surrounding-text call to a `violations` list, so this is asserted on every
   test that runs rather than stated in prose (I9).
7. **External text goes through `hunmin_ic_commit_text` only.** Feeding spoken
   syllables to `hunmin_ic_process` composes them a second time. Mutation-tested.

### B5. An IME cannot receive an activity result, and cannot ask for a permission

Both measured with `javap` on `android.jar` (API 35, 2026-10-04), and both
contradicted what the design assumed:

- **`InputMethodService` has no `onActivityResult`.** An IME therefore cannot be
  handed the result of an activity it started — `startActivityForResult` needs an
  Activity, and the service is not one. So the "fall back to the system voice
  input" idea cannot deliver text *into the engine*. The fallback launches the
  platform's recogniser UI, whose text reaches the focused field by its own
  route, and the code reports `VoiceResult.Failed` rather than waiting for a
  result that will never arrive.
- **No permission-request hook either**, so RECORD_AUDIO cannot be requested at
  runtime. It must be granted in system settings. The permission is declared but
  never required: with it denied the keyboard still types.

There is also no `RecognizerIntent.ACTION_VOICE_INPUT` and no
`RecognizerIntent.LANGUAGE_MODEL_FREE_FORM`; free-form is the default when
`EXTRA_LANGUAGE_MODEL` is unset. Two constants that existed in the first draft of
this and did not compile.

### B7. The tier E3 trace sink, and where it must be enabled

A tier-C trace sink lives in the **shipped** APK, not only in a test build. E3's
whole premise is that a manual session on a real keyboard leaves a trace behind, and
that is only true of the IME the person is holding — the same reasoning
`check-frontend-invariants.py` I10 records for the fcitx5 addon, where five
mutations survived a green run because the tests linked a stale library.

Three constraints, each measured:

- **Inert unless enabled**, by a broadcast. An IME's service cannot be started with
  `am start` and receives neither activity results nor permission results (B5), so a
  broadcast is the only channel from outside.
- **Debuggable builds only**, checked with `FLAG_DEBUGGABLE` rather than a build
  variant name: a release IME must not accept a broadcast that turns on file
  writing, and that has to hold for any packaging.
- **Enabled after the IME owns the field, not before.** The receiver lives in the
  service, so opening the sink before the warm-up keypress leaves the file present
  with its `meta` line and records nothing afterwards. E2 asserts the event count
  for exactly this reason: a sink that records nothing produces a file that looks
  like a result.

The commit text is captured where it still exists. `hunmin_ic_commit` is
destructive, so a trace written after the read would show `""` for every commit —
a trace that looks like nothing was ever committed while the user watched syllables
appear.

### B6. Voice failures must not reach the key path

A voice attempt runs from a click listener on the IME's own view, so anything it
throws is on the same footing as a keystroke handler: it can take the keyboard
down. Three consequences, all mutation-tested:

- A recogniser that throws from `start()` (or cannot be constructed) becomes a
  `VoiceResult`, never an exception.
- Whitespace-only audio is `NothingHeard`, not `Got("   ")`. The trim lives in
  `VoiceSession`, on the one path every `Recognizer` shares — trimming inside one
  implementation would leave every other one free to get it wrong, which is
  exactly what the first version did before its test caught it.
- Exactly one result per attempt. `SpeechRecognizer` delivers `onResults` and
  then `onError` on some teardowns, and `hunmin_ic_commit_text` is destructive,
  so a second delivery would not merely duplicate the words but corrupt the next
  composition.

