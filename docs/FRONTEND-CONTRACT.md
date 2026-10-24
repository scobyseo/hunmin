# hunmin — frontend contract audit

What each frontend file **assumes**, what role it plays, and which interface it
exposes. Written 2026-10-02 after reading every line of the fcitx5 addon
against the fcitx5 5.1.19 sources, because several assumptions turned out to be
wrong and were not discoverable by re-reading our own code.

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

Read this before changing `engine.cpp`. Every "Assumes" line below is a claim
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
| `src/engine.h` | addon entry surface: `HunminEngine : InputMethodEngine`, config struct, `HunminAdapterFactory`, the `hunmin_ctx*` handle |
| `src/engine.cpp` | the whole adapter: key mapping, per-context state property, preedit/c commit presentation |
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
adapter owns — `engine.{h,cpp}`, `config.h`, `modeswitch.{h,cpp}` — comments and
blanks excluded, and fails above **545**. Do not add counts back to this table;
add a rule to the checker instead. Reading the current number:

```sh
python3 tools/check-frontend-invariants.py | grep I8
```

The budget exists to catch *composition* logic or per-layout knowledge
appearing in the frontend, which is rule #4's actual failure mode.

## 2. The interfaces we expose

**To fcitx5** — `HunminEngine` implements five virtuals:

| Hook | When fcitx5 calls it | What we do |
|---|---|---|
| `activate(entry, icEvent)` | context becomes the active IM | `state->reset()` |
| `deactivate(entry, icEvent)` | context stops being the active IM | delegates to `reset` |
| `keyEvent(entry, keyEvent)` | every key press **before** the client sees it | map + `hunmin_ic_process`, then `filterAndAccept()` or let it through |
| `reset(entry, icEvent)` | focus change / explicit reset | `flush()` then `reset()` |
| `reloadConfig()` | addon config (re)load | `readAsIni(config_, "conf/hunmin.conf")` |

Plus `getConfig()` / `setConfig()` for the addon's own `Layout` option.
`setConfig` writes back with `safeSaveAsIni`, so the config file is
user-visible and round-trips.

**Per-context state** — `HunminState : InputContextProperty`, registered as
`"hunminState"` on the `InputContextManager`. This is fcitx5's mechanism for
attaching per-input-context state; the property factory is what makes
`propertyFor(&factory_)` return the same object for the same `InputContext`.
Our `hunmin_ic` lives here, so **two open windows get two independent engines**
— no shared buffer between contexts. Correct for a real IME.

**To the engine** — the only four libhunmin calls that matter here:
`hunmin_ic_new`, `hunmin_ic_process`, `hunmin_ic_commit` / `hunmin_ic_flush`,
`hunmin_ic_preedit`. Plus `hunmin_ic_set_option(ic, "caps", …)` and
`hunmin_ic_free`.

**The caps option is a common frontend contract** (issue #112, ruling
"wire and make it common to all platform"): every frontend sets
`hunmin_ic_set_option(ic, "caps", 1 | 0)` **per key event** from its
platform's CapsLock state — fcitx5 from `KeyState::CapsLock`
(engine.cpp:327), Android from `META_CAPS_LOCK_ON` (HunminAdapter.kt), the
TSF TIP from `GetKeyState(VK_CAPITAL)`'s toggle bit read beside the modifier
snapshot (`hunmin_tsf_capslock_from_state`, keybinding.cpp). CapsLock feeds
the yethangul P variable only (hunmin.h:93) and selects **no** row on any
platform — the Shift bit (or, for fcitx5, the host-resolved keysym case) is
the only row transport.

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

`engine.cpp:49-64` returns early for Shift and for Ctrl/Alt/Super/Hyper, but
deliberately lets Shift through as a plain character — uppercase letters
(`FcitxKey_A`…`FcitxKey_Z`) reach the engine as-is.

**Assumes** layouts encode shift as uppercase rows (sin3-p2's symbol tiers and
초성체 depend on it). **Holds.** If a layout needed Shift as a state bit, this
would have to change.

**Dual convention appended (issue #90, 2026-10-18):** the engine now also has
the explicit-modifier entry `hunmin_ic_process_key(ic, base, shift)`
(docs/SPEC-P2 "IC input entry points"; measurement in
`docs/IC-INPUT-OPTIONS.md` §2-B). fcitx5 does not use it and everything in
A1–A3 above stays its contract unchanged; the value is that the case endpoint
— the CapsLock×Shift decision each adapter otherwise re-derives, and the one
Android shipped wrong once (A3') — now has one engine-owned home, engine-tested
without any host layout (ctest `engine-process-key`, tier A). A frontend that
migrates keeps its VK/keysym → base-character half and passes the bit; the
already-resolved rule covers a host that delivers the resolved character
itself (the fcitx5 shape).

### A3. CapsLock is inverted by the adapter

`engine.cpp:66-72`: when CapsLock is down and the sym is `A`–`Z`, it lowercases
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

`engine.cpp:91-96`:

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

`engine.cpp:145-148`: `setClientPreedit` if the context advertises
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
`HunminEngine::layoutFor()`, so a profile change takes effect on the next
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
`engine.cpp`. `add_dependencies(testhunmin copy-addon copy-im)` used to satisfy
that orderingly, but `copy-addon` only copies `hunmin-addon.conf`; it does not
depend on `hunmin_addon`. So `make testhunminui` would not rebuild the library
after an `engine.cpp` edit, and **every mutation planted in the adapter
survived** while the tests still reported green.

That is the worst possible failure shape: a test that cannot fail looks exactly
like a test that has nothing to catch. Found by planting five mutations in the
F9 path and finding all five survive a green D1/D2.

`add_dependencies(copy-addon hunmin_addon)` in
`frontends/fcitx5/test/addon/CMakeLists.txt` closes it. Note that `DEPENDS` on
an `add_custom_target` only orders it — it does not make it produce the
library, so `add_dependencies` is the one that works.

**How to tell it regressed.** `touch frontends/fcitx5/src/engine.cpp && make
--build build --target testhunminui` must change the mtime of
`build/fcitx5-addon/libhunmin.so`. If it does not, no D1/D2/Tier C result from
that build means anything.

## 4. Gaps in coverage

Found by **mutation testing** on 2026-10-02: four realistic mutations were
planted in `engine.cpp` and checked against both §5 and the D1 suite. Two
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

`engine.cpp` used to read only `config().hangulLayout`, the addon-wide
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
   Check: code lines across `engine.{h,cpp}`, `config.h`, `modeswitch.{h,cpp}`
   ≤ **545**, comments and blanks excluded. This is a measurement, not a
   description — see §1. Exceeding it means composition logic or per-layout
   knowledge has crept into the frontend, which is the failure mode rule #4
   exists to prevent. The count is of *every* translation unit the adapter
   owns, not just `engine.{h,cpp}`, because splitting logic into a new file
   would otherwise walk straight past the budget while the adapter grew. The
   limit has moved twice, each time for work that is frontend by nature and
   cannot live in the core: the F9/candidate presentation (400 → 465), and
   keyboard selection in the candidate window (465 → 545, 2026-10-11), which
   exists because fcitx5 does not intercept Up/Down/Return for a
   `CursorMovableCandidateList` — it only offers `prevCandidate()`/
   `nextCandidate()` and calling them is the input method's job.
9. **The Hanja window never reads or rewrites surrounding text.**
   Check (I9): no `SurroundingText` / `getSurroundingText` /
   `deleteSurroundingText` / `setCursorPos` call anywhere in `engine.cpp`;
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
   `engine.cpp` does not rebuild it and the suite stays green. Found when five
   F9 mutations all survived a green run. A stale binary that passes is worse
   than a failing one — it is indistinguishable from a test with nothing to
   catch.

11. **The layout picker's list comes from the core, and the picker keeps it.**
    Check (I11): inside `refreshLayoutList()`, `count` is assigned from
    `hunmin_ctx_layout_count()` and `id` from `hunmin_ctx_layout_id()`; and
    `setLayouts()` in `engine.h` stores its argument in `layouts_`. The bindings
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
