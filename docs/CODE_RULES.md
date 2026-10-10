# hunmin Code Rules

Distilled from the `clean-code-principles` skill (`~/.agents/skills/clean-code-principles/`,
AsyrafHussin v1.0.2 — SOLID, DRY, KISS, YAGNI, separation of concerns, fail-fast,
encapsulation). Project-specific binding rules; audits report `file:line - [rule] text`.

## Hard rules (review-blocking)

1. **[core-fail-fast] Converters never silently swallow unknown input.**
   `tools/ist2data.py`, `kbd2data.py`, `ohi2compose.py`: an unrecognized token in
   normative data is an ERROR, not a dropped branch or a coerced literal.
   Explicit, auditable exceptions live in a named allowlist in the converter with a
   reason (e.g. `GATED_DROPS` for branches behind default-off flags h/H). The
   `Ɨ` incident: a non-jamo fallback turned virtual unit 407 into a Latin letter —
   this class of bug is banned, not fixed.
2. **[core-dry] Hangul-domain knowledge exists once: `tools/jamo.py`.**
   Slot ranges (incl. Jamo Ext-A/B), virtual-unit handling, US shift pairs, the
   Naraeseot unit-ID rule (id = codepoint − 0x1081 for slot-list ids). kbd2data,
   ist2data, doctor, ohi2compose import from it. Adding a jamo fact = one place.
3. **[solid-srp / func-small] One job per function, ~60-line budget.**
   Loading, validating, rewriting, and emitting stay separate; `load_layout_file`
   and `value_json` are the cautionary examples — split them when touched.
4. **[core-separation-concerns] Engines never special-case a layout.**
   A new layout is JSON + compose tables + golden vectors. Any `if (layout is X)`
   in `libhunmin/src` or a frontend is a defect. `docs/SPEC-P2.md` owns P2 semantics.
5. **[doc-why-not-what] libhangul-faithful ports keep verbatim quirks but MUST
   carry a `why` comment** (e.g. the `!push(x) && !push(x)` double-push idiom,
   the `goto flush` in the jamo port, libhangul's A960-as-choseong ranges).
   Parity sweeps are the safety net for these regions; do not "simplify" them.
6. **[core-encapsulation] `hm_*` symbols are internal.** Only `hunmin_*` in
   `libhunmin/include/hunmin.h` cross the ABI. ABI changes = reviewed append-only
   + `abi-check` re-approval.
7. **[core-kiss / name-searchable] No magic numbers in C:** named constants
   (`HUNMIN_LAYOUT_CAP`, `KEY_TABLE_SIZE`, …); in Python: module constants.
8. **[pattern-strategy] Frontends map events, they don't interpret Hangul.**
   Commit/preedit/tier logic lives behind the C ABI only.
9. **[core-fail-fast / tests-first] Refactors require a green test that already
   covers the behavior.** No refactor without ctest passing before and after;
   new engine mechanics land with vectors the same commit (see docs/TESTING.md).
10. **[pattern-repository] Data files are the repository:** loaders resolve,
    validate schemaVersion, and refuse the future (loader supports ≤ current);
    unknown value forms mark the layout `supported=false`, never guess.

## Exemptions

- `libhunmin/src/tables.{c,h}`: generated (see header) — style belongs to the
  extractor; audit the extractor, not the output.
- `third_party/`: vendored.

## Machine-enforced gates (CI/ctest)

- `schema-contract` — doctor validates every data file (incl. self-test vectors)
- `data-selfcheck` — converter determinism (regenerate + diff)
- `engine-golden-*` / `goldens-reproducible` / `parity-sweep` — behavior
- future: `clang-tidy` + `cppcheck` clean; `ruff` over tools/ (adopt when deps land)
