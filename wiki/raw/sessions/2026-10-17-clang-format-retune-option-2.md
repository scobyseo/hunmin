---
title: clang-format retune to the tree's own style — option 2 applied
created: 2026-10-17
updated: 2026-10-17
type: session
status: active
ingested: 2026-10-17
tags: [session, ci, formatting, clang-format, clean-code]
sources:
  - ../../../docs/TESTING.md
confidence: high
---

# clang-format retune to the tree's own style — option 2 applied

The implementation pass over
scobyseo/hunmin#77, after the maintainer picked option 2. This page is a session
record of what was measured; the ruling is quoted so nobody mistakes it for the
agent's own
(see [[decisions-belong-to-the-maintainer]]).

## The ruling as given

Asked to choose among option 1 (delete or pin the config), option 2 (split
configs, apply once as a formatting-only commit) and option 3 (full reformat
with the committed config), the maintainer answered with exactly:

> Option 2: split configs, apply once as formatting-only commit

plus one steering correction after implementation began:

> make clang format to be ideal for AI workload.

That second sentence translated into checkable budget lines: no re-sorted
includes, no comment reflow, no alignment that ripples one edit into its
neighbours, and — the decisive lever — **`ColumnLimit: 0`**, which forbids
clang-format from re-wrapping the tree's hand-shaped lines at all.

## What the knobs measured (2026-10-17, clang-format 23.1.3 on Windows)

`-w` (non-whitespace) changed-line totals over the 33 tracked C-family files,
excluding the vendored `data-orig/`/`third_party/cJSON` trees and the generated
`libhunmin/src/tables.{c,h}`:

| Config | -w churn | Note |
|---|---|---|
| committed config (LLVM 4/120) | not re-measured | issue #77 body: 404 lines in `ic.c` alone (whole-tree output) |
| retuned, `ColumnLimit: 100` | 1491 | the hand-shaped churn classes of the issue remain |
| retuned, `ColumnLimit: 0` (no `AllowShort*`) | 1673 | single-line `if (x) y;` bodies get broken: LLVM's `AllowShort*` defaults are off |
| retuned, `ColumnLimit: 0` + `AllowShort*` on (final) | 853 | hand wraps, packed typedefs and single-line macros — the classes clang-format cannot express without re-writing — dominate the remaining diff |

- `AlignTrailingComments: false` vs `true` under `ColumnLimit: 0`: measured
  **identical** (673 over the sample set). The switch costs nothing one-time
  and prevents steady-state churn, so it stays `false`.
- `AlignConsecutive*` forced to `false` were no-ops: the LLVM style already
  ships them off. The shipped configs state nothing that would contradict the
  base.
- Second-pass idempotence (3 successive `-i` runs over the applied tree):
  numstat byte-identical.
- Real-tree `-w` churn after the application pass: **853 lines** across 16
  files, matching the scratch prediction exactly (`tsf.cpp` 3,
  `frontends/android` JNI 21, `ic.c` 118 incl. wrap+pack classes).
- A variant measured with a bogus key
  (`AlignConsecutiveTableGenConsecutiveCondColon`) and one with a real but
  wrong-in-LLVM-23 name were caught by `--dump-config` before any tree apply;
  the parse-error class was met three times and repaired each time by
  verifying every key against `--dump-config`, not by reading docs.

## Toolchain facts worth keeping

- `clang-format` 23.1.3 (`C:/Program Files/LLVM/bin/clang-format.exe`) honours
  `.clang-format-ignore` at the repo root, verified by probe before relying on
  it: a tracked scratch file re-formats while a sibling of the ignored prefix
  stays byte-identical.
- `--style=<path>` is **not** a style value; the accepted form is
  `--style=file:<path>` (or bare `--style=file` for auto-discovery from the
  file's directory upward). The failed-first probes returned "Invalid value
  for -style", which reads like a config parse failure and is not — that cost
  a diagnostic loop and belongs in this record.

## What shipped on the task branch

- `.clang-format` (root, C core): 2-space, brace-attached, the tree's compact
  style, `ColumnLimit: 0`, `AllowShort*` on, `SortIncludes: Never`,
  `ReflowComments: false`, `AlignTrailingComments: false`,
  `PointerAlignment: Right`, `DerivePointerAlignment: false`.
- `frontends/fcitx5/.clang-format` and `frontends/tsf/.clang-format`: the same
  normalisation for the two 4-space C++ frontends; `frontends/tsf/` had
  initially fallen through to the root 2-space config and re-indented
  `tsf.cpp` wholesale (measured 45+/-44 raw churn), so its own config was
  required, not optional.
- `.clang-format-ignore`: `data-orig/`, `third_party/`, and the two generated
  `libhunmin/src/tables.{c,h}` files.
- The application pass itself is a separate commit whose diff is formatting
  only; the committed config documents the tree's own style from that commit
  onward.

## Verification status at ingest

- `ctest --test-dir build-mingw -j16 -LE slow -LE mutation`: 51/51 Passed
  (Skips are environment-bound: E1 registers only with `ANDROID_HOME`).
- `libhunmin/data`, `tests/golden`, `tests/conformance`: untouched by
  `git status`.
- `tools/docs-lint.py` re-run after the `docs/TESTING.md` correction; see the
  task branch commits for the recorded result.

The issue close, as always, is the maintainer's (see
[[decisions-belong-to-the-maintainer]]).
