# MODES-SURFACE-B — the settings row, implemented once

The automatic Hangul/English switch has two surfaces: a key combination and a settings
UI. The requirement is `docs/SPEC-MODES.md` §ReqMode13 and is **not restated here**; the
state it governs is described once in `docs/DESIGN-MODES.md` §3.

This page exists because that UI half is built three times, once per frontend, and only
one of the three exists (#176, fcitx5). #177 (Android) and #175/#203 (TSF) build theirs
next, and the shape below is the worked example: what the row is made of, which engine
calls answer each part, the four traps that cost the fcitx5 build real time, and the
questions each platform still has to answer for itself.

## 1. The row is four parts, and only one of them is a checkbox

| part | who owns it | what "owning it" means |
|---|---|---|
| the **value** | the engine | the frontend stores it (its own platform store) and reads back what the engine accepted, so a refused ON never shows as ON |
| the **key** | the engine | asked, never spelled: the frontend renders the modifier it is given and contributes only its own key's name |
| the **reason** | the engine | the presence answer decides "off by choice" from "off, data missing"; the frontend only chooses how to show that |
| the **persistence** | the frontend | no store crosses platforms, and none of them may copy the value: the store is an input to the engine, not a second home |

## 2. The engine calls the row is built from

Use the **layer** when the frontend has one; `hunmin_ic_*` is the same value underneath
and exists for hosts that hold an ic directly.

| need | call | answers |
|---|---|---|
| write the value | `hunmin_buffered_preedit_set_option(l, "autoswitch", v)` | `false` when the engine refuses — ON is refused where the mode word list is absent (`docs/SPEC-MODES.md` §ReqMode13's data gate, issue #169) |
| read the value | `hunmin_buffered_preedit_get_option(l, "autoswitch", &v)` | what the engine actually holds; this is what the row must show |
| is the feature possible here | `hunmin_ctx_autoswitch_data_present(ctx)` | the presence half of the pair above; computed once per context and cached |
| which key | `hunmin_buffered_preedit_special_chord(l, HUNMIN_SPECIAL_AUTOSWITCH)` | the modifier bits (`HUNMIN_MOD_*`) the engine binds to the function |
| in words | `hunmin_special_name(fn)` | a stable identifier ("voice", "autoswitch") for logs and widget ids |
| what the key does | `hunmin_buffered_preedit_special(l, ev)` | `>0` serviced, `0` not ours, `-1` bound but refused → the frontend passes the key through |

Two things about these calls that are not obvious from the signatures:

- **The chord lives on a layer, not on the context.** A dialog that opens before the user
  has typed a key has no layer yet. fcitx5 builds a throwaway one, asks, and frees it
  (`frontends/fcitx5/src/hunminadapter.cpp#refreshAutoswitchRow`); the probe never
  processes a key, which is why it is not a second composing owner
  (`tools/check-frontend-invariants.py` I6 counts held handles).
- **`set_option` is the gate, `get_option` is the truth.** Writing and then reading back
  is what makes "stored ON where the engine refused" impossible to leave in the store.

## 3. The shape on fcitx5 (the worked example)

`frontends/fcitx5/src/config.h#AutoswitchAnnotation` and
`frontends/fcitx5/src/hunminadapter.cpp#pushAutoswitch`, `#recordAutoswitch`,
`#refreshAutoswitchRow`, plus one call in `HunminState::ensureIc`.

The sequence, and each step's reason:

1. **load** — read the store, ask the engine's two answers, push the value, and keep what
   comes back (`reloadConfig()`). Order matters: the presence answer is consulted by the
   push, so a valid stored ON is not reconciled to OFF on the first load.
2. **dialog open** — the label is written at dump time from the engine's chord and the
   presence answer, not from a constant in the addon.
3. **write** — the platform's normal settings path (`setConfig()` in fcitx5's case) pushes
   into every live layer and stores the engine's answer.
4. **rebuild** — a layer is re-created on a layout or policy change and starts from the
   engine default, so `ensureIc()` re-applies the stored value. Without this the row says
   ON while the next word is not judged at all: that is `I12-autoswitch-not-reapplied` in
   `tools/frontend-mutations.py`, and it is the case D1 types a word twice to catch.

## 4. The traps, measured while building it

- **The engine's strings are borrowed.** `out.commit` / `out.preedit` are valid only until
  the next `hunmin_*` call, and the judgement read makes two of them. fcitx5 lost the
  delivered word on the boundary key that both delivered a word and switched language
  (`hello` reached the client as nothing). Copy before the judgement. Any frontend that
  reads a commit string and then asks the engine anything has this bug waiting.
- **The state is per input context; the store is per user.** A global settings value has
  to be pushed into *every live layer*, and re-applied to each new one. A frontend that
  applies it once, at dialog-close, is correct only until the next rebuild.
- **A per-context property must be registered before anything asks for it.** fcitx5's
  `InputContextManager::property()` asserts `factory->manager_ == this`; the fcitx5 build
  moved `registerProperty()` ahead of `reloadConfig()` because the reload now reaches live
  layers. Platform-specific, but the shape ("the settings path runs during construction")
  is not.
- **"Disabled" is not portable.** fcitx5's config protocol has no disabled state at all
  (no enable flag on `OptionBase`; `HiddenOption` is compile-time), so its row explains
  itself in the description and tooltip. Android already has the vocabulary —
  `OverlayActivity`'s dimmed `미정 (0.3)` row renders disabled and is not clickable — and
  TSF has a dialog. Both can do better than fcitx5 here; neither needs the fcitx5 answer.
- **The label has two halves.** The modifier comes from the engine; the *key's name*
  ("한/영") is the platform's, and each frontend knows its own. Spelling the whole chord
  in the frontend is what would let the three advertise three different keys.

## 5. What each task still decides

Recorded here rather than decided, because each is that task's own call:

- **Where the value persists.** fcitx5 stores it in its own config file, because the addon
  already has one; Android and TSF each have their own store. No store crosses platforms.
- **Whether a stored ON survives a machine that lost the data package.** fcitx5 reconciled
  it to OFF, so the row never claims a state the engine does not hold; the alternative
  ("keep the preference, show the engine's 0") cannot be expressed with one field and no
  disabled widget. A platform whose dialog *can* disable the control may prefer to keep
  the preference, and then must say which of the two the row is showing.
- **What "disabled" looks like** on that platform, and whether the row is hidden or shown
  with a reason. ReqMode13's discoverability rule applies to the disabled state too.
- **Whether the row also remaps the key.** The 2026-10-31 ruling says the composition is
  selectable in the config dialog; no frontend implements remapping yet, and it needs a
  store the engine can read at judgement time. fcitx5's row is read-only on the key.

## 6. What "verified" looks like

The engine's own contract is pinned in `libhunmin/tests` (`docs/DESIGN-MODES.md` §3
lists the cases). A new surface has to show four things of its own:

1. the label and the reason come from the engine's answers, not constants;
2. a written value is stored as what the engine accepted (ON accepted where the data is
   present, refused where it is not);
3. the value reaches the layer the user types into (fcitx5 asserts this behaviourally: a
   judged word is delivered in the judged language and the next key is Latin);
4. the value survives a rebuild.

fcitx5's four live in `engine-d1-fcitx5` (`frontends/fcitx5/test/testhunmin.cpp`). The
data-missing row text is asserted on `HunminConfig` directly, because a context caches its
presence answer at construction — a platform that can point a context at an empty data
tree should assert the refused write instead, as the engine's tier-A cases do.

## 7. Not this page's subject

- **Making the key actually fire.** On fcitx5 the addon dispatches no special-function
  chord at all, so the row is currently the only surface that flips the state; the
  measured cost of the fix is [`docs/OPTIONS-MEASURED.md` §E5-2 addendum 2](docs/OPTIONS-MEASURED.md#E5-2-addendum-2-the-chord-executed-on-this-host-measured-2026-11-01).
- **The switch itself** (#164's ruling), the OFF-never-changes-behaviour invariant (#179)
  and the three-frontend equivalence test (#180).

## Provenance

Measured 2026-11-01 on this host: fcitx5 5.1.19 from the `hunmin-fcitx5-dev` prefix,
`engine-d1-fcitx5` driven headless (`ctest --test-dir build -R engine-d1-fcitx5`), and the
`AutoswitchAnnotation` text read back out of `Configuration::dumpDescription()`. What was
**read, not run**: Android's rows (`frontends/android/app/src/main/java/org/hunmin/ime/OverlayActivity.kt`)
and TSF's dialog — no device, no Windows host in this session. The fcitx5 dispatch
measurement lives in `docs/OPTIONS-MEASURED.md` §E5-2 addendum 2, not here.
