---
title: "Nightly fuzz LSan: an error path, not startup data"
created: 2026-11-22
updated: 2026-11-22
type: postmortem
status: active
tags: [tooling, testing, postmortem, measurement]
sources:
  - https://github.com/scobyseo/hunmin/issues/309
  - ../../../.github/workflows/ci.yml
  - ../../../tools/sim.c
  - ../../../tools/fuzz-generate-keys.py
confidence: high
ingested: 2026-11-22
---

# Nightly fuzz LSan: an error path, not startup data

Issue #309 reported a nightly-only LeakSanitizer failure — `691455 byte(s) leaked
in 4517 allocation(s)` — and asked whether the startup-once data in
`hm_data_load`/`expr` should be suppressed or freed at shutdown. The measurement
answers a third question the issue did not ask: the allocations *are* the
startup data, but they are only reported because the process returned early
while they were still live. A normal `hunmin-sim` exit was already clean. No
suppression is needed, and no engine lifetime rule changes.

## The artefact says which run leaked

The failed scheduled run 44895832326 uploaded 125 traces. Exactly one,
`fuzz-125.trace`, contains a LeakSanitizer report; the 124 before it contain
ordinary `--trace` JSON. The first line of `fuzz-125.trace` is:

```
--keys: unknown key escape \  (\\ = literal, \s<char> = shift, \b = backspace)
```

That message comes from `keys_step` in `tools/sim.c#main`. The stack below it is
therefore not a soak accumulating leaks over 125 iterations: it is one process
that hit a malformed key and skipped its cleanup. The stacks name
`hunmin_ic_new`, `hm_data_load`, `load_layout_file`, `build_keys`,
`load_value_deep`, `hm_expr_parse` and `mk` — exactly the objects the normal
`done` path frees.

## Local replay, before and after

The pre-fix `tools/sim.c` was compiled from `HEAD` with the same sanitizer flags
as the nightly job and replayed against the shipped data:

| Input | Exit | LSan |
|---|---|---|
| `--keys '\ '` (the malformed nightly key) | 2 | `691452 byte(s) leaked in 4517 allocation(s)` |
| `--keys 'dkssud '` (a valid stream) | 0 | none |

The allocation count is identical to CI's; the three-byte difference in total
is the length of the data-directory string stored on the context, not a
different leak. After routing every post-load return through `done`, the same
malformed input still exits 2 with the same diagnostic and reports **no** leak.
That is the answer to the issue's measurement question: the leaked set is the
startup data, and it does not grow with fuzz iterations. A 300-iteration local
replay of the workflow's generate-then-replay loop with the patched generator
and the sanitized `hunmin-sim` had no non-zero exit and no LSan report.

## Why the generator produced that key

`tools/fuzz-generate-keys.py` printed tokens space-separated, including a bare
`\`, which `keys_step` reads as the start of an escape rather than a key. The
generator's alphabet also included uppercase tokens that no layout defines;
those become `\s<base>` in the explicit-shift grammar. The generator now
encodes a backslash key as `\\` and an uppercase token as `\s<base>`, and its
`--selfcheck` replays every alphabet member in the first position through
`hunmin-sim --batch`. A bare backslash can no longer hide behind a lucky seed.
ctest `fuzz-keys-selfcheck` (tier-G) runs that check.

## The second defect

The report step passed the whole trace as one argv element with
`--body "$(cat /tmp/fuzz-$i.trace)"`; a full LSan stack is larger than the
argument limit, so the step ended with `/usr/bin/gh: Argument list too long`
and lost the issue/annotation. The trace is already on disk, so the workflow
hands it over with `--body-file /tmp/fuzz-$i.trace`. A long stack no longer
depends on argv size. A second blocker was visible behind the first: the
`fuzz`, `crash`, `leak` and `auto-filed` labels the step asked for do not exist
in the repository (`gh label list`), so gh could not have added them even after
the argv fix. Auto-filed issues now use the existing `bug` label and the
`[ci] <brief>` title form.

## What is not measured here

Two consecutive scheduled nightly runs green on `master` cannot be produced in
a worktree — the trigger only exists on GitHub's schedule. That verification
stays environment-bound. The change touches `tools/sim.c`, the key generator,
one ctest registration and the workflow; no engine source, ABI symbol or layout
data changed. This page records measurements, not a ruling; the maintainer
decides what to do with them, as recorded in
[[decisions-belong-to-the-maintainer]]. The job's place in the suite is
[[test-scenario-guide]], and the earlier CI fixes are in
[[ci-android-lessons]].
