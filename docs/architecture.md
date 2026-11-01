# hunmin — module architecture

Single source of module contracts, dependencies, and the data-vs-code rule.
Status: **shape + contract reference only** — no design verdict, no implementation.

---

## 1. Modules and their boundaries

| Module | Lives in | Role | Depends on |
|---|---|---|---|
| **libhunmin** | `libhunmin/` | Core engine: `hunmin_ctx` (layout/load), `hunmin_ic` (per-key composition, commit/preedit, options). Stable ABI, append-only. | `data/` (JSON: layouts, compose, hanja, krdict, idioms) |
| **fcitx5 addon** | `frontends/fcitx5/` | Thin frontend: fcitx5 `InputMethodEngine`/`InputContext`. It carries no judgement of its own since 2026-11-01 (#164) — the engine's answer is `hunmin_buffered_preedit_take_judgement`. | `libhunmin` ABI |
| **Android IME** | `frontends/android/` | Kotlin IME (`HunminInputMethodService`), JNI shim (`HunminNativeBridge`), layout/keys data, settings/UI. | `libhunmin` ABI (shipped as `.so`) |
| **tools** | `tools/` | `kbd2data`, `extract_tables`, `gen-conformance`, `trace-check`, `doctor.py`, `docs-lint`, etc. | `libhunmin` dev headers |

## 2. Ownership rule

- **Engine owned**: layout discovery, composition, commit/preedit, buffer lifecycle, options, `hunmin_ctx_layout_mode()` (the per-layout `hangul`/`latin` split).
- **Frontend owned**: key mapping, preedit/candidate display, per-IM settings, on/off *surface* (R13) — the engine owns the state, the frontend exposes it.
- **None owned in the core**: anything that differs between frontends (adaptation of the same rule to different event spaces) belongs in the frontend, not `libhunmin`.

## 3. The language-detection question (R8) — design, now decided

Requirement (SPEC-MODES.md): a single judgement rule, owned by one component,
delivered identically in fcitx5, Android, and (planned) Windows. Two virtual
architectures satisfy it:

- **Engine-owned** — guessing/shadow-composition inside `libhunmin`, one ctx
  rule, ABI change.
- **Adaptor-owned** — `language_detect.*` (fcitx5) keeps a shadow `hunmin_ctx` on the
  Latin layout and replays the buffered word at the judgement boundary.

**Decision (2026-10-13): Adaptor-owned.** R8 is too heavy for the engine: it
touches composition-unit (syllable→word), the composition boundary on every
layout, `STR_CAP` truncation semantics, and preedit length — a load question
with no measurement yet (`MODES-ENGINE-RULES.md §4`), plus a `hunmin_ctx` state
model that is not built for dual-layout shadow sessions. The engine's job is
composition and commit/preedit; a cross-frontend validity judgement is a
frontend concern. The engine keeps `hunmin_ctx` as a single-layout session and
stays ABI-stable. `language_detect.*` was the fcitx5 implementation; the same
contract (shadow `hunmin_ctx` + per-boundary replay) was the record for what
Android would do under the same rule.

*Superseded 2026-11-01 (#164):* the maintainer ruled the judgement **engine-owned
and the switch frontend-owned** (`docs/OPTIONS-MEASURED.md` §MRO). The replay now
lives in `libhunmin/src/buffered_preedit.c#latin_reading`, the formula in
`#judge_boundary`, the answer in `hunmin_buffered_preedit_take_judgement`, and
`language_detect.{h,cpp}` is retired. The paragraph above is kept as the record of
the 2026-10-13 decision and of why the load argument did not survive the data
moving into the core first.

The maintainer's earlier recorded position (`SPEC-MODES.md` R3, now
requirement-only) is superseded for design; the rule — one state,
`hunmin_ic_set_option(autoswitch)`, no per-frontend settings store, no new data
schema for guessing — is unaffected.

## 4. Data vs code

- **Layout behaviour is data, never code.** `data/layouts/*.json`, `data/compose/*.json` are JSON; adding a layout = adding a file + golden test. The engine knows composition; it does not know what a keyboard is.
- **Layout ids are strings, never numeric**, in the ABI (`id` in layout JSON, `hunmin_ctx_layout_id()`).
- **Invariants**: `id` must equal the filename; `mode` ∈ {`hangul`, `latin`}; `schemaVersion` validated by `tools/doctor.py`. A misspelled mode costs one mis-filed row, not a build failure.
- `romaja` reports `hangul` as its mode: it is typed in Latin but composes Hangul, and its mode says what the layout *produces*.

## 5. Cross-module contracts (anchor symbols)

| Contract | Anchor | Notes |
|---|---|---|
| Context + layout discovery | `hunmin_ctx_new/dirs`, `hunmin_ctx_layout_count/id/mode` | dirs scanned at runtime; NULL dirs → `HUNMIN_DATA_PATH` |
| Input context | `hunmin_ic_new/process/commit/preedit/flush/reset/set_option` | commit is destructive; one read per consumer |
| Mode query | `hunmin_ctx_layout_mode(ctx,i) → "hangul"\|"latin"` | `HM_MODE_HANGUL=0`, `HM_MODE_LATIN=1` |
| Options | `hunmin_ic_set_option(ic,"caps",1)` etc., read back with `hunmin_ic_get_option(ic,"caps",&v)` | engine-owned state, one home per value; a frontend surfaces it and keeps no copy. `"autoswitch"` (R13-정정) is one of these: OFF on a fresh ic, and it survives the layer's ic re-creation on a language toggle |
| External text pass-through | `hunmin_ic_flush` (commits in-flight first) | voice / out-of-band text must never go through the jamo pipeline |
| Android data path | `HunminNativeBridge.kt` ↔ `libhunmin.so` | native shim bridges Kotlin → C ABI |
| fcitx5 addon lifecycle | `HunminAdapter`, `HunminState` | `hunminadapter.cpp#HunminAdapter` owns the adapter; `HunminState::keyEvent` feeds the layer and `HunminState::applyJudgement` performs the mode change the engine's judgement asks for |
| Cross-frontend equivalence | `docs/FRONTEND-EQUIVALENCE.md` | same key sequence → same commit/preedit/consumed; TSF interim and native agree |

## 6. Version / ABI policy

- `HUNMIN_ABI_VERSION` (1) in `libhunmin/include/hunmin.h`.
- Append-only ABI; new symbols added deliberately, not by `switch`/`per-layout` numeric ids.
- Layout identity is always a string.
