# SPEC-FRONTEND — requirements common to every frontend

A requirement here binds **all** frontends: fcitx5, Android and Windows TSF. A
frontend that cannot satisfy one must say so in its own SPEC with the reason
(the platform does not offer the surface), not quietly do something else.
Frontend-specific requirements live in `SPEC-TSF.md`, `SPEC-RELEASE.md` and the
per-platform SPECs; they cite these ids instead of restating them.

`docs/DESIGN-FRONTEND.md` is the design document: it explains *why* these
hold and records which differences are permitted. It does not restate them.

# Requirements

### FE1. Same layout and keys give the same committed text

Given the same layout, the same options and the same key sequence, every
frontend produces the same committed text. *Ruling:* the equivalence invariant,
`docs/DESIGN-FRONTEND.md#part-a--equivalence-formerly-frontend-equivalencemd`. *Implementation:*
`libhunmin/src/buffered_preedit.c#hunmin_buffered_preedit_process_key`.
*Refuted by:* `engine-buffered-preedit`, `engine-key-rule`,
`engine-android-conformance-tests_conformance_android_basic-syllables-case01.trace.in`,
`tsf-mock-native-tests_conformance_win32_prose.trace.in`.

### FE2. One key rule, in the engine library

A frontend classifies each key with the shared classifier and obeys its answer —
feed the engine, pass the key at a boundary, perform the language action, or hand
it to the candidate list. No frontend carries its own copy of that precedence.
*Ruling:* issue #128 decision C (2026-10-25). *Implementation:*
`libhunmin/include/hunmin.h#hunmin_buffered_preedit_classify`.
*Refuted by:* `engine-key-rule`.

### FE3. No frontend holds a word buffer

The buffer between syllable and word is the library's. A frontend that keeps one
is the drift this requirement exists to prevent. *Ruling:* issue #128
(2026-10-25): three private buffers drifted and were deleted. *Implementation:*
`libhunmin/src/buffered_preedit.c#hunmin_buffered_preedit_new`.
*Refuted by:* `frontend-invariants`.

### FE4. One delivery per word, at a boundary

The finished word reaches the client exactly once, at a boundary (a declined key,
Enter, an edit key, a mode toggle, a chord). A key the engine consumes inserts
nothing. *Ruling:* issue #72 rule 4, generalised by #128. *Implementation:*
`libhunmin/include/hunmin.h#hunmin_buffered_preedit_commit_word`.
*Refuted by:* `tsf-mock-native-tests_conformance_win32_prose.trace.in`,
`engine-buffered-preedit`.

### FE5. The commit unit is a preference, and the text is the same

A frontend exposes `word` (default) or `syllable` as a user setting; which one is
chosen changes *when* the text lands, never *what* it says. *Ruling:* issue #128
§8.3 item 3. *Implementation:*
`libhunmin/include/hunmin.h#HUNMIN_COMMIT_AT_BOUNDARY`.
*Refuted by:* `engine-buffered-preedit`.

### FE6. Caps is per key event, and Caps Lock never toggles the language

The frontend passes the caps state with every key event and never flips the
language on Caps Lock; in Korean input mode the character is derived from the
virtual key and the Shift bit alone. *Ruling:* issue #112 — "wire and make it
common to all platform". *Implementation:*
`libhunmin/include/hunmin.h#hunmin_ic_set_option`. *Refuted by:*
`engine-key-rule`, `engine-buffered-preedit`.

### FE7. A platform key maps to the one abstract action

A frontend that owns a Han/Eng key maps it to the engine's single language
action, and performs no switch of its own. *Ruling:* the maintainer's split,
2026-10-25 — the requirement binds the platforms that own a key, and the platform
that owns the trigger has the duty FE13 states. *Implementation:*
`libhunmin/include/hunmin.h#hunmin_buffered_preedit_toggle_language`.
*Refuted by:* `tsf-test` (`CandidateTest.ModeToggleIsABoundaryWithTheWindowOpen`),
`tsf-test` (`TsfConfigTest.AdapterKeyDriveOverTheMockLayer`), tier E1's
`the language action is the layer's, not the adapter's`. **Implemented** on the
two frontends that own a key (TSF, Android); fcitx5 is FE13's case, not this
one's.

### FE13. Where the platform owns the trigger, the duty is the mapping

A frontend whose host owns the language trigger does not implement the switch; it
ensures that the language the host decides reaches the engine as the layout slot
to use. *Ruling:* the maintainer's split, 2026-10-25. *Implementation:*
`frontends/fcitx5/src/hunminadapter.cpp#HunminState` (the slot is chosen at
`Activate`). *Refuted by:* `none` — the mapping is exercised by the fcitx5 tiers
indirectly; no test asserts "the host's trigger reached the slot".

### FE14. Data and its terms travel together

A frontend distributes the data it needs with the terms that data is under, and
records where generated tables came from. *Ruling:* `docs/SPEC-OVERVIEW.md#§A10`.
*Implementation:* `tools/doctor.py#check_terms` (ctest `schema-contract`) and the
`LICENSE.md`/`meta.json` pairs beside every data directory. *Refuted by:* the
`schema-contract` test.

**Resolved 2026-11-22 (issue #292).** The 2026-10-25 measurement was partial:
`hanja/`, `idioms/`, `krdict/` carried `LICENSE.md`, while `layouts/` (15 layouts
— derived work of libhangul's kbd tables and pat.im's `ist.xml`) and `compose/`
carried neither a licence nor a provenance `meta.json`. `layouts/` and `compose/`
now each carry both files, and `check_terms` fails `schema-contract` if any data
directory loses its licence or its provenance record. The older dictionaries
satisfy the provenance half through their `LICENSE.md`'s `## Source` section, so
the check does not require a second copy of a statement already made there.

### FE15. The frontend/library boundary is append-only

A frontend built against one version of the library keeps working against later
ones; adding capability never removes or redefines an exported symbol. *Ruling:*
`docs/SPEC-OVERVIEW.md#§A13`. *Implementation:*
`libhunmin/tests/abi/symbols.txt`. *Refuted by:* the `abi` test
(`tier-B;guard`, `add_test(NAME abi)`; WSL runs it as part of the ladder).
**Recorded discrepancy:** registered `NOT WIN32`, so a Windows host never runs
it — there the symbol diff is a review step.

### FE8. Text from outside the keyboard is not composed

Voice input and any other externally produced text reaches the client as it
arrived, after the pending word is delivered; it is never fed through the jamo
pipeline. *Ruling:* `libhunmin/include/hunmin.h` ("must reach the client as-is
and must NOT be fed through the jamo pipeline"). *Implementation:*
`libhunmin/include/hunmin.h#hunmin_buffered_preedit_external_text`.
*Refuted by:* `none` on two frontends — only Android has the path.

### FE9. What is shown comes from the layer

The composing text a frontend displays is the layer's display string (or, where
a host distinguishes them, its two halves); a frontend does not re-derive the
text from the engine itself. *Ruling:* issue #128 decision (one source of the
display). *Implementation:*
`libhunmin/include/hunmin.h#hunmin_buffered_preedit_display`. *Refuted by:*
`frontend-invariants`.

### FE10. One corpus, every frontend

Input behaviour is defined once, as a trace corpus with the layout, options and
key sequence in its meta, and every frontend replays it. The commit-unit axis is
asserted where it is cheap: tier A equality, and per frontend where a delivery
makes the moment observable. *Ruling:* `docs/DESIGN-FRONTEND.md#6-one-corpus-frontend-specific-delivery-not-a-second-copy` and
`#§6.1`. *Implementation:* `tools/gen-conformance.py`,
`tools/trace-check.py`. *Refuted by:*
`engine-android-conformance-tests_conformance_android_basic-syllables-case01.trace.in`,
`tsf-mock-native-tests_conformance_win32_prose.trace.in`.

### FE11. A frontend stays thin

A frontend is an adapter under the ~1000 code-LOC budget (comments and blanks
excluded); logic that grows past event mapping and presentation belongs in the
core. *Ruling:* `AGENTS.md#§Guidelines` (maintainer: "~700 is okay",
2026-10-25; raised to 1000 on 2026-11-22, issue #322). *Implementation:*
`tools/check-frontend-invariants.py`. *Refuted by:* `frontend-invariants`.

### FE12. No per-layout anything in a frontend

Layout behaviour is data: a frontend names layouts and their ids, never their
keys or rules, and a new layout needs no frontend code. *Ruling:* `AGENTS.md#§The one idea`.
*Implementation:* `tools/check-frontend-invariants.py`.
*Refuted by:* `frontend-invariants`.

### FE16. Candidate gloss display survives the IBus switch

The maintainer, 2026-11-22: "ibus로 전환하기 위해서는 그 기능을 지원할 방법을
찾아야 해" — the feature being the candidate gloss (뜻풀이) fcitx5 shows beside
each Hanja candidate. Switching the Linux frontend to IBus requires a way to
show the gloss; silently dropping it is not the requirement. *Ruling:* option A,
2026-11-22 — the gloss rides the host's auxiliary text, which the GTK3 candidate
panel draws beside the list. The paths that were rejected and the correction of
the earlier "IBus cannot" claim are
`wiki/raw/measurements/2026-11-22-ibus-gloss-paths.md`; the ruling with the
maintainer's words is `wiki/decisions/2026-11-22-ibus-gloss-option-a.md`.
*Implementation:* `frontends/ibus/src/hunmin_engine.c#sink_candidates` — the
highlighted candidate's gloss is published with the list and tracks the cursor
as it moves. *Refuted by:* `engine-d2-ibus-ui`.

### FE17. The five option rows are the user's, and one of them is the language-key list

A frontend stores, and lets its user change, the rows of the shared option vocabulary it
can express: the Hangul layout, the Latin layout, the commit unit, the automatic-switch value,
and the list of keys that frontend treats as the language key. A row that cannot be changed is
a default, not an option; a row present in the store and absent from the engine (or the
reverse) is the drift this requirement exists to prevent. The vocabulary is what is shared —
which rows a given platform can express is not, and a frontend that cannot express one records
which and why in its own SPEC (this file's opening paragraph). The IBus frontend stores none
today, so this binds it as all five. *Ruling:* the maintainer, 2026-11-22 — "3 -> a로 설정할 수
있어야 함" (all five rows settable, the key list among them) and "나머지는 추천대로 갈게" (the
store, the editing surface, live apply and the packaging are the analysis's recommendations).
Costs, the options and the decided shape: `docs/OPTIONS-MEASURED.md#§IBC`.
*Implementation:* `frontends/ibus/src/settings.c` (the store) with
`frontends/ibus/schema/org.freedesktop.ibus.engine.hunmin.gschema.xml` (the defaults), read
into `hunmin_ibus_settings_apply` and applied live through `hunmin_ibus_settings_watch`; the
window is `frontends/ibus/settings/main.c`, which the IBus panel opens from its Preferences
item through the component XML's `<setup>` element. A captured key is spelled by
`hunmin_ibus_key_entry_from_event` and judged by `hunmin_ibus_stored_key_ok`, so the window
holds no second opinion about what a binding means. The
fifth row is frontend data (which keysyms count as the language key), so implementing this adds
no `hunmin_*` symbol; the append-only list in `libhunmin/tests/abi/symbols.txt` is untouched.
*Refuted by:* `engine-d1-ibus-settings` — defaults, a stored value reaching the engine, an
unloadable layout degrading and being reported, the stored key list driving the translation, the
two grammar corners refused with a reason, a bad entry costing only itself, a captured key being
spelled once, and a change arriving through the watch. **Not asserted:** the window's widgets.
It is presentation and no ctest drives it (`engine-d2-ibus-ui` asserts the engine's surface, not
a settings dialog), so the widget layout is a manual check; what a capture *stores* is asserted
above, and the layout list it offers comes from the core (`tools/check-frontend-invariants.py`
I11 fails a hardcoded picker).
