# Issue #272 P1 — doc single-home dedupe audit (2026-11-08)

**Status:** audit/proposal notes. No design decisions made. Requirement text remains only in `docs/SPEC-*.md`; figures/measurements remain in their authoritative homes unless deliberately moved with anchor/provenance updates.

## Completed in this branch

- D3/D4 consolidation: `DESIGN-BUILD.md`, `DESIGN-MODES.md`, `DESIGN-FRONTEND.md` created; `BUILD-SYSTEM.md`, `MODES-ENGINE-RULES.md`, `FRONTEND-EQUIVALENCE.md`, `FRONTEND-CONTRACT.md` left as pointer stubs. MODES-METRICS harness/design folded into `DESIGN-MODES.md` §5 (anchor alias kept); figures/definitions remain in `MODES-METRICS.md`/`MODES-DATA.md`.
- ReqMode migration collapsed SPEC-MODES correction IDs to `ReqMode1..15` and collapsed corresponding REQ-MAP rows.
- `fd95ae1`: retargeted `docs/SPEC-FRONTEND.md` links from `FRONTEND-EQUIVALENCE.md#§1/#§6` stub anchors to `DESIGN-FRONTEND.md` anchors. `req-map-check` still passes.
- 2026-11-08 (candidate 1 completion): retargeted the remaining *content*
  references in active normative/design/testing docs from the old stub filenames
  to `DESIGN-FRONTEND.md` Part A/Part B — `SPEC-FRONTEND.md`, `SPEC-MODES.md`,
  `architecture.md`, `TESTING.md`, `trace-spec.md`, `ANDROID-BUFFERING.md`,
  `WINDOWS-TSF-BUFFERING.md`, `WINDOWS-TSF-CANDIDATES.md`, the live pointer in
  `REMAINING-WORK.md`, and three self-references inside `DESIGN-FRONTEND.md`.
  Part B is named explicitly wherever the old `§5` meant the contract invariants
  rather than the shared layer.

## References to old stub filenames (inventory)

`grep -rn 'FRONTEND-EQUIVALENCE.md|FRONTEND-CONTRACT.md|MODES-ENGINE-RULES.md|BUILD-SYSTEM.md'` found 77 hits (post-fix count will vary). Categories:

| Category | Action | Risk |
|---|---|---|
| Pointer stubs (`docs/BUILD-SYSTEM.md`, `MODES-ENGINE-RULES.md`, `FRONTEND-EQUIVALENCE.md`, `FRONTEND-CONTRACT.md`) | Keep: they preserve old resolution and history. | None. |
| `wiki/log.md`, `wiki/raw/*`, `wiki/sessions/*` historical/source links | Leave: history record; changing can misrepresent what existed at the time. | Medium if rewritten. |
| `docs/REMAINING-WORK.md` grep/assertion rows | Update only when assertion breaks due to consolidation (done for TSF autoswitch rows). Do not rewrite historical evidence text unnecessarily. | Medium (docs-lint/board semantics). |
| `docs/SPEC-*`, `docs/REQ-MAP.md`, `docs/DESIGN-*` normative/pointer links | Prefer authoritative design homes when anchors are stable; verify `req-map-check`/`docs-lint`. | Medium (anchors). |
| Code/tool comments | Safe to retarget to design/spec homes when comment-only. | Low if behavior unchanged. |

## Anchors/authoritative homes to preserve

- **REQ-MAP pointers (must keep resolving):** e.g. `docs/MODES-DATA.md#§4-7`, `docs/MODES-METRICS.md#§1.4/#§1/#§2/#§1.3`, `docs/DESIGN-MODES.md#§1/#§2/#§3`, `docs/DESIGN-FRONTEND.md#§5/#§2.2` (verify exact headings before changing). `req-map-check` validates these.
- **Figure/data authority:** `MODES-DATA.md` and `MODES-METRICS.md` remain homes for measurements/figures/provenance where already cited. Do not create second homes; add pointers instead.
- **Requirements:** only `docs/SPEC-*.md`.
- **Build invariants:** consolidated in `DESIGN-BUILD.md`; `BUILD-SYSTEM.md` stub remains for old refs.

## Candidate P1 dedupes (needs maintainer choice before edit)

1. **Normative stub-anchor links outside REQ-MAP:** scan for `FRONTEND-EQUIVALENCE.md#...` / `FRONTEND-CONTRACT.md#...` used as content anchors (not just filename mentions) in active specs/docs and retarget to `DESIGN-FRONTEND.md` anchors with `req-map-check`/`docs-lint` verification. *Evidence needed:* exact anchor mapping table.
   **Done 2026-11-08** for active normative/design/testing docs. Deliberately left:
   the pointer stubs themselves, `DESIGN-FRONTEND.md`'s "formerly ..." history
   lines, `OPTIONS-MEASURED.md` (a dated measurement record whose citations
   describe what was measured then), and `REMAINING-WORK.md`'s historical
   `file:line` evidence row.
2. **Duplicated frontend equivalence/contract prose in active docs:** only if text is a true second home (not a summary/pointer). Prefer deleting duplication and leaving one pointer to `DESIGN-FRONTEND.md`/`SPEC-FRONTEND.md`.
3. **MODES-DATA `§4-*` historical/design narrative:** large and heavily referenced (`#§4-7` in REQ-MAP). D4 asked to fold “design halves”; moving it risks anchor breakage and mixes provenance history with design. Recommendation: **defer** unless a specific subsection is clearly non-authoritative and all anchors are remapped.
4. **REMAINING-WORK historical prose refs to stub filenames:** low value; avoid unless causing lint/board failures.

## Verification commands

```sh
python3 tools/req-map-check.py
python3 tools/docs-lint.py
python3 tools/wiki-lint.py
ctest --test-dir build -R '^(req-map|abi|schema-contract)$' --output-on-failure
ctest --test-dir build --show-only=json-v1  # compare to docs/issue-272-phase0-baseline.json
```

## Adjacent migration fallout found while verifying (2026-11-08)

Recovered by running the generators' own `--check` gates, not by inspection:

- `tests/golden/judgement-r8.jsonl` was stale against
  `tools/gen-judgement-golden.py`: the committed `req`/`meta` fields still held
  `R4/R6/R8/R9/R13` and `R8-정정-3` while the generator emits `ReqMode*`, so
  `--check` failed. Regenerated; the diff is id text only.
- The four designed `case-*.jsonl` provenance lines still held `R6` in
  `meta.spec` after `tools/gen-case-golden.py` began emitting `ReqMode6`.
  Regenerated and re-merged with `tools/merge-golden-cases.py` (the documented
  one-trace-per-family flow); the diff is the meta id text only.

Both were invisible to ctest because `goldens-reproducible` covers the
libhangul reference set, not these generators, and the case corpora are
replayed as merged traces. The remaining `R6`/`R*-정정*` text in the tree is
either the historical quote in `OPTIONS-MEASURED.md` or the migration note in
`WINDOWS-TSF-DEVENV-CHECKS.md`.
