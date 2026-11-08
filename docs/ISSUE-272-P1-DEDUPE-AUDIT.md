# Issue #272 P1 — doc single-home dedupe audit (2026-11-08)

**Status:** audit/proposal notes. No design decisions made. Requirement text remains only in `docs/SPEC-*.md`; figures/measurements remain in their authoritative homes unless deliberately moved with anchor/provenance updates.

## Completed in this branch

- D3/D4 consolidation: `DESIGN-BUILD.md`, `DESIGN-MODES.md`, `DESIGN-FRONTEND.md` created; `BUILD-SYSTEM.md`, `MODES-ENGINE-RULES.md`, `FRONTEND-EQUIVALENCE.md`, `FRONTEND-CONTRACT.md` left as pointer stubs. MODES-METRICS harness/design folded into `DESIGN-MODES.md` §5 (anchor alias kept); metric definitions, corpus terms and reproduction rules now live in `docs/MODES-MEASUREMENT.md`, and `MODES-DATA.md`/`MODES-METRICS.md` were removed (D4, 2026-11-08 — see below).
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

- **REQ-MAP pointers (must keep resolving):** e.g. `docs/MODES-MEASUREMENT.md#§1.4/#§1/#§2/#§1.3`, `docs/DESIGN-MODES.md#§1/#§2/#§3`, `docs/DESIGN-FRONTEND.md#§5/#§2.2` (verify exact headings before changing). `req-map-check` validates these.
- **Figure/data authority:** `MODES-MEASUREMENT.md` is the home for metric definitions, corpus terms and reproduction rules; it deliberately carries no current figures (2026-11-08 removal). Do not create second homes; add pointers instead.
- **Requirements:** only `docs/SPEC-*.md`.
- **Build invariants:** consolidated in `DESIGN-BUILD.md`; `BUILD-SYSTEM.md` stub remains for old refs.

## Candidate P1 dedupes (needs maintainer choice before edit)

1. **Normative stub-anchor links outside REQ-MAP:** scan for `FRONTEND-EQUIVALENCE.md#...` / `FRONTEND-CONTRACT.md#...` used as content anchors (not just filename mentions) in active specs/docs and retarget to `DESIGN-FRONTEND.md` anchors with `req-map-check`/`docs-lint` verification. *Evidence needed:* exact anchor mapping table.
   **Done 2026-11-08** for active normative/design/testing docs. Deliberately left:
   the pointer stubs themselves, `DESIGN-FRONTEND.md`'s "formerly ..." history
   lines, `OPTIONS-MEASURED.md` (which now holds only the seven priced questions
   that are not modes measurement — its five modes sections were deleted
   2026-11-08), and `REMAINING-WORK.md`'s historical `file:line` evidence row.
2. **Duplicated frontend equivalence/contract prose in active docs:** only if text is a true second home (not a summary/pointer). Prefer deleting duplication and leaving one pointer to `DESIGN-FRONTEND.md`/`SPEC-FRONTEND.md`.
3. **MODES-DATA `§4-*` historical/design narrative:** large and heavily referenced (`#§4-7` in REQ-MAP). D4 asked to fold “design halves”; moving it risked anchor breakage and mixing provenance history with design. **Resolved 2026-11-08** by removal rather than folding: the maintainer ruled the corpus material stale (“no need to retain. remove the file and extract minimal knowledges to reproduce it with new data and approach”). Both files are gone; the surviving definitions are `docs/MODES-MEASUREMENT.md` §1–§5, the removed records are named in §6, and every live anchor was remapped in the same change.
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
replayed as merged traces. The last `R6`/`R*-정정*` text in the tree was the
historical quote in `OPTIONS-MEASURED.md`'s §BND-7, which was deleted with that
section on 2026-11-08; the migration note in `WINDOWS-TSF-DEVENV-CHECKS.md`
remains.

## Open tooling observation (not fixed)

`tools/gen-case-golden.py --check` cannot pass for the merged case families
(`case-sebeolsik-final.jsonl`, `case-sin3-p2.jsonl`): the generator emits one
`meta.case` header per case, while the committed files were merged by
`tools/merge-golden-cases.py` under the one-trace-per-family policy. The check
therefore reports "stale" for a file that the merge tool confirms is current.
The ctest gate is unaffected (`engine-golden-case-*` replays the merged files),
but the generator's own `--check` has no merged-form comparison. Whether to
teach `--check` the merge step or document the two-stage flow in its usage is a
maintainer choice.

## Phase completion, measured (2026-11-08)

| Phase | What it did | Measured |
|---|---|---|
| P2 `tools/_common.py` | every tool that needed the repo root takes it from `_common`, with the file-relative fallback so each stays runnable from anywhere | 46 of 70 tools import `_common`; no tool outside `_common` computes a repo root any more (the other 24 need none) |
| P3 core comments | multi-place rationale cites the design heading that owns it; the substance moved into `DESIGN-CORE` rather than disappearing | `hunmin.h` 453 → 295, `buffered_preedit.c` 446 → 260, `wordset.c` 83 → 70, `hangul_valid.c` 55 → 26 (comment lines against master `8cbaf01`); `ic.c` untouched — its comments are the libhangul-parity quirks `CODE_RULES` rule 5 requires |
| P4 test fixtures | one `hunmin_test::CtxTest` base owns the context; each fixture keeps only what differs | seven fixtures, 34 insertions / 44 deletions in `libhunmin/tests` |
| P5 / D1 | closed as no core move — the seam is `DESIGN-FRONTEND.md` §11 | no production code moved |
| P6 (#271) | CMake split landed separately | closed 2026-11-07 |

What the sweep deliberately leaves: per-symbol contract comments in `hunmin.h`
that a caller cannot get from the design document (return values, lifetimes,
error cases), the `ic.c` parity recordings, and generated `tables.c`.
