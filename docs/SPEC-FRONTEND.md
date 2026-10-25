# SPEC-FRONTEND — requirements common to every frontend

A requirement here binds **all** frontends: fcitx5, Android and Windows TSF. A
frontend that cannot satisfy one must say so in its own SPEC with the reason
(the platform does not offer the surface), not quietly do something else.
Frontend-specific requirements live in `SPEC-TSF.md`, `SPEC-RELEASE.md` and the
per-platform SPECs; they cite these ids instead of restating them.

`docs/FRONTEND-EQUIVALENCE.md` is the design document: it explains *why* these
hold and records which differences are permitted. It does not restate them.

# Requirements

### FE1. Same layout and keys give the same committed text

Given the same layout, the same options and the same key sequence, every
frontend produces the same committed text. *Ruling:* the equivalence invariant,
`docs/FRONTEND-EQUIVALENCE.md#§1`. *Implementation:*
`libhunmin/src/buffered_preedit.c#hunmin_buffered_preedit_process_key`.
*Refuted by:* `engine-buffered-preedit`, `engine-key-rule`,
`engine-android-conformance-tests_conformance_android_basic-syllables-case01.trace.in`,
`tsf-mock-tests_conformance_win32_prose.trace.in`.

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
*Refuted by:* `tsf-mock-tests_conformance_win32_prose.trace.in`,
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

### FE7. Han/Eng maps to one abstract action

Every frontend maps its own Han/Eng key to the engine's single language action
instead of implementing the switch locally. *Ruling:*
`docs/FRONTEND-EQUIVALENCE.md#§4`. *Implementation:*
`libhunmin/include/hunmin.h#hunmin_buffered_preedit_toggle_language`.
*Refuted by:* `none` — **unmet**: the symbol exists and no frontend calls it
(`docs/FRONTEND-EQUIVALENCE.md#§8`).

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
makes the moment observable. *Ruling:* `docs/FRONTEND-EQUIVALENCE.md#§6` and
`#§6.1`. *Implementation:* `tools/gen-conformance.py`,
`tools/trace-check.py`. *Refuted by:*
`engine-android-conformance-tests_conformance_android_basic-syllables-case01.trace.in`,
`tsf-mock-tests_conformance_win32_prose.trace.in`.

### FE11. A frontend stays thin

A frontend is an adapter under the ~700 code-LOC budget (comments and blanks
excluded); logic that grows past event mapping and presentation belongs in the
core. *Ruling:* `AGENTS.md#§Guidelines` (maintainer: "~700 is okay",
2026-10-25). *Implementation:* `tools/check-frontend-invariants.py`.
*Refuted by:* `frontend-invariants`.

### FE12. No per-layout anything in a frontend

Layout behaviour is data: a frontend names layouts and their ids, never their
keys or rules, and a new layout needs no frontend code. *Ruling:* `AGENTS.md#§The one idea`.
*Implementation:* `tools/check-frontend-invariants.py`.
*Refuted by:* `frontend-invariants`.
