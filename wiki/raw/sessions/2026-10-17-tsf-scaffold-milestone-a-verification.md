---
title: Milestone A verification pass — the TSF scaffold measured on the Windows host
created: 2026-10-17
updated: 2026-10-17
type: session
status: active
ingested: 2026-10-17
tags: [session, windows, tsf, verification, environment]
sources:
  - ../../../docs/ENVIRONMENT.md
  - ../../../docs/MILESTONES.md
  - ../../../docs/REMAINING-WORK.md
confidence: high
---

# Milestone A verification pass — the TSF scaffold measured on the Windows host

The pre-close pass over issue #64 (milestone A, TSF frontend scaffold): what the
tree turned out to contain, what was measured on the Windows host, and which
documents stated host facts the measurements contradicted. Closing the issue
itself is the maintainer's call — see [[decisions-belong-to-the-maintainer]];
this page records the measurements that call would rest on.

## What the pass found in the tree

- The milestone-A branch (`windows-milestone-a-tsf-frontend-scaffold`) was
  **already merged upstream**: rebasing it onto `origin/master` was a no-op,
  and `master` carries the scaffold (the merges `054751c`/`a615710` of
  `be3ebcf`/`70a9022`), the follow-up `4004a86` (scaffold + `data.c`
  FindFirstFileA + env doc), and — beyond milestone A — the PR-#76
  buffering-adapter work (`93ebf87`, `6970d91`, `e88d41e`) that grows
  `frontends/tsf/src/tsf.cpp` to 1106 lines.
- `frontends/tsf` on master: `tsf.cpp`, `dllmain.cpp`, `hunmin-tsf.def`, the
  register/unregister/dump `.bat` scripts, and a CMakeLists that links
  `hunmin-tsf.dll` against a `HUNMIN_LIB` import library.

## What was measured on this host (2026-10-17)

- **MSVC build of the whole chain**: core `hunmin.lib` (VS2022 BuildTools
  14.44, `cl.exe` 19.44.35229, Windows SDK 10.0.26100) plus
  `frontends/tsf` → `hunmin-tsf.dll`, built through `vcvars64.bat` + the
  Ninja cache. One cosmetic C4819 (codepage 949) warning on `hunmin.h`.
- **Portable mingw build**: llvm-mingw gcc, Ninja — `hunmin-sim` builds and
  commits `dkssud ` → `녕`, closing the `getline` gap recorded 2026-10-16.
- **Inner ctest loop** (`ctest -j16 -LE slow -LE mutation`, scratch
  llvm-mingw build): **51/52 passed**. The one failure is `data-selfcheck`
  (tier-B) — by the maintainer's statement it **fails by nature on this
  host** (converter-vs-committed-data divergence is the host baseline, not a
  regression), and `engine-suggest` / `engine-commit-text` reported `Not Run`
  in the same run.

## Documents corrected in this pass

`docs/ENVIRONMENT.md` carried a three-way contradiction in the same file: the
2026-10-16 Windows section (VS BuildTools + SDK NOT installed, admin
required), batch 4's 2026-10-16 note (both installed, TSF scaffold builds), and
a section titled *Windows 0.2 development environment — not present on this
host* that said the host has no Windows environment at all. All three now
agree with the measurements above; the mingw failure list (`getline`,
unsuffixed links) is marked superseded by `8b36a6c`. Parallel corrections:
`docs/MILESTONES.md` §0.2 ("precondition … which this host does not have") and
the P6-exclusion paragraph in `docs/REMAINING-WORK.md` §intro. The board's
assertion table gained TSF rows so `tools/docs-lint.py` check 6 parses the
milestone-A claims.

## What this pass deliberately did not assert

- That the issue can close — the close is the maintainer's; the issue body
  gained a Verification checklist only.
- That the `windows` CI job should re-enable — it stayed `if: false` per
  issue #51, and re-enabling is a separate ruling.
- Anything about TIP registration/activation: that is device- and
  registry-bound (Win+Space selection, !Stale Assemblies state), not bridged
  by any of today's builds, and no TAT measurement exists on this host.
