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

TSF still carries its own `wordbuf_`; it is the rest of this issue.

## 6. One corpus, every frontend, both TSF modes

Input behavior is defined once and run everywhere:

- The shared corpus names the layout, options, key sequence, and expected
  per-key output (consumed, commit, preedit), not only the final text. Final
  text alone cannot catch a key that is eaten in one mode and passed in
  another.
- It runs against the shared layer, fcitx5, Android, and the TSF mock in both
  native and interim delivery, and in both buffered and syllable commit modes.
- Tier C (`docs/trace-spec.md`, `tests/conformance/`) is the cross-platform
  event format. The Windows TSF emitter already exists for tier F1; it must be
  extended to run the same corpus in both delivery modes.
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
| commit unit | `CommitBehavior` config | `commit_behavior` preference | policy fixed to the buffered word; a seam exists for the corpus | allowed, §3.3 — but only two of three are user-selectable |
| preedit presentation | client preedit, else the panel | `setComposingText` | `ITfComposition`, or the interim document span | allowed, §3.3 |
| delivery strategies | one | one | two (native, interim) | allowed; only TSF meets classic hosts |
| candidate anchor | `display()` | the service owns the Hanja UI | native `display()`, interim `composing()` | allowed, §3.3 |
| candidate key routing | the adapter's own switch | none in the adapter | the adapter's own window | allowed — but see 8.3 (2) |
| Han/Eng | not in the addon: fcitx owns the trigger | Kana VK to `KeyAction.HANGUL_ENG` | `ToggleModeCore` plus configurable bindings | **open**, §4 |
| R8 language detection | yes: `language_detect.cpp` | no | no | allowed; R8 is adaptor-owned |
| voice / external text | — | yes | — | allowed, but invariant 5 is exercised on one frontend only |

### 8.3 Open asymmetries, to be resolved where the platform allows

1. **Han/Eng is not normalised (§4).** TSF and Android each own a mechanism and
   fcitx5 has none in the addon. `hunmin_buffered_preedit_toggle_language()`
   exists and no frontend calls it, because each carries a layout-pair model the
   layer's two-slot version does not yet describe. Resolving it means deciding
   that model first; the verification is a tier-A case plus one per frontend
   driving the same abstract action.
   *Environmental part:* fcitx5's trigger belongs to fcitx, so the addon cannot
   see a Han/Eng key. What the platform allows is the mapping, not the key.
2. **The classifier's `CANDIDATE` answer is used by one frontend.** fcitx5 asks
   and then routes locally, Android reports no candidate window, TSF never
   reaches it. The rule is shared; the use is not. Resolving it means routing
   each frontend's list keys through the answer instead of around it.
3. **TSF's commit unit is not user-selectable.** fcitx5 and Android read a
   setting; TSF pins the buffered word and keeps an environment seam for the
   corpus. The missing piece is a settings-dialog row; the layer already takes
   the choice.
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
