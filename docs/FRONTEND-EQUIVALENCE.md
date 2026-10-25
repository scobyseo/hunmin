# Cross-frontend equivalence

Status: **requirements recorded 2026-10-24; the three open decisions in §9 were
taken 2026-10-25 and the layer they describe is implemented in
`libhunmin/src/layer.c` (issue #128, phase 1). The corpus, the frontend
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

`hunmin_layer` in `libhunmin/src/layer.c`, exported as `hunmin_layer_*`
(`hunmin.h`, 11 symbols, appended to `libhunmin/tests/abi/symbols.txt`). It owns
the word buffer and the boundary rule:

- `hunmin_layer_key(l, base, shift, &out)` answers consumption and fills
  `out.commit` / `out.preedit`. A **declined key is the boundary**: the engine
  finalises the syllable it was holding, and that is when a buffered word is
  delivered. A frontend therefore never spells out "when a word ends" itself.
- `HUNMIN_COMMIT_BUFFERED` holds finalised syllables and delivers the word at
  the boundary; `HUNMIN_COMMIT_SYLLABLE` delivers each one as the engine
  finalises it. Same text, different delivery point.
- `hunmin_layer_mode_change()` is the Han/Eng action (§4): one abstract event,
  and it is a boundary in both directions.
- `hunmin_layer_peek_key()` is the read-only prediction a platform "test" phase
  needs, over the same state the real key will consume.

Tier A asserts the property the layer exists for, as equalities rather than
golden text: mode vs mode per key, and layer vs the engine driven directly
(`engine-layer`, `libhunmin/tests/layer-test.cpp`).

### 5.2 A wording question this implementation exposes

§1 says "the same preedit string after each key", but §1 and §5 also make the
commit boundary a frontend choice — and those two cannot both hold literally: a
buffered frontend shows the finished syllables of the word as preedit while a
syllable frontend has already committed them, so their preedit strings differ by
exactly the text one of them has delivered. The implementation therefore asserts
the observable quantity: **committed-so-far + preedit is equal per key**, and
the concatenation is equal. Whether the rule should be worded that way is the
maintainer's call; it is recorded here rather than silently rewritten.

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

## 8. Wording

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

## 9. Decisions taken 2026-10-25

The three questions this section used to hold were put to the maintainer with the
options measured, and all three were answered. Quoting the chosen option in each
case, because a paraphrase drifts:

1. **The layer's API shape** — chosen: "Exported hunmin_layer_* symbols in
   libhunmin (append-only ABI, one implementation, symbols.txt grows)". The
   alternative on the table was a static `layer.c` compiled into each frontend;
   it was not taken because it puts the same rule in three builds and #128 exists
   to have one.
2. **The corpus record format** — chosen: "Reuse the tier-C trace format
   (`*.trace.in` with per-key consumed/commit/preedit) as the one corpus; extend
   the win32 dialect to both delivery and both commit modes". Measure before
   trusting: the format already carries the three per-key fields, which is what
   §6 asks for; a second format would be a second home for the same expectations.
3. **The Han/Eng normalization** — chosen: "An abstract shared action in the
   layer (`hunmin_layer_mode_change` / a MODE_CHANGE key class); each adapter maps
   its own physical key to it". So no adapter synthesizes a Kana VK.

Still open, and not decided by the above:

- The §5.2 wording question (preedit equality against a free commit boundary).
- Whether per-key assertions are mandatory for every frontend's own tests, or only
  for the shared layer and tier C (§6 allows a frontend test to drive a different
  interface; it forbids a second copy of the expectations).
