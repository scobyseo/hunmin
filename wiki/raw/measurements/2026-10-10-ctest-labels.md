---
title: Labels for the ctest suite, and two silent failures found while adding them
created: 2026-10-10
updated: 2026-10-10
type: measurement
status: active
tags: [testing, tooling, measurement, postmortem]
ingested: 2026-10-10
confidence: high
---

# Labels for the ctest suite, and two silent failures found while adding them

The label vocabulary, every measured wall clock and every test count live in
`docs/TESTING.md` §"Test labels"; `docs/` stays authoritative for the figures,
so they are not repeated here. What this page records is **how the scheme was
reached** — the designs not taken and the reason — and **the two defects found
on the way**, because both are silent in the exact way this project keeps
getting bitten: nothing errors and the count stays right.

## What the labels had to survive

The stated requirement was that a person could run "just the tier I touched"
without a hand-written `-R` regex over test names. The specific hazard is that
some names are machine-generated: `CMakeLists.txt` derives each tier-C test
name from the corpus path, so `engine-android-conformance-tests_conformance_android_basic-syllables-case01.trace.in`
changes whenever a case label in `tests/conformance/android/` changes — and a
saved `-R` expression then silently matches nothing.

**Three designs were available and the third was chosen:**

| design | selects | cost |
|---|---|---|
| rename the tests to stable hand-written names | nothing new | renames tests that CI job names, `tools/frontend-mutations.py:334-338` and docs all quote; the generated names would have to be preserved as aliases anyway, which is more mechanism than labels |
| one `tier=` label only | a tier | a tier does not answer "will this rewrite my source tree" or "will this cost me a minute", and the two mutation tests are the answer to both |
| **tier axis + orthogonal `mutation` / `slow` properties** | both questions independently | the two axes must be kept honest |

The deciding reason, stated once: the two questions a person actually has before
running a subset — *which tier did I touch* and *what does this cost me* — are
independent, and only the second one had no name in `docs/TESTING.md` before.

`frontend-mutations` is the one membership that is not derivable from a tier
name. It carries **both** `tier-D1` and `tier-D2`, because what is required to
catch each mutation is `ctest -R engine-d1-fcitx5` or `ctest -R engine-d2-ui`
(`tools/frontend-mutations.py:334-338`). Inventing a `tier-D-mutations` would
have been a lie about a tier the plan does not have, and hiding it under one of
the two would have made `-L tier-D2` under-report what it costs.

## Defect 1 — `LABELS` is a property, and the wrong form does not fail

Recorded here because it is the shape of the failures this project keeps having
([[2026-10-10-d3-case-coverage]]): the change looks applied, the count looks
right, and the failure would have appeared only as tests mysteriously dying at
run time. **The lesson and the reproduction are in `docs/TESTING.md`** under
"One CMake trap" — kept in one place, because a fix belongs next to the thing
that is easy to get wrong, and this wiki is not a second home for it.

## Defect 2 — the Android tier-C runner prints build chatter on stdout

**Found, reproducible, NOT fixed.** Left for whoever owns
`tools/android-trace-emit.sh` and `tools/build-host-jni.sh`.

Every `engine-android-conformance-*` test fails on a **cold**
`build/libhunjni-host.so`, and passes on a warm one:

```sh
rm -f build/libhunjni-host.so build/android-trace-emit-classpath.txt
ctest --test-dir build -R 'engine-android-conformance.*case01'
# 1/1 Test #40: ... ***Failed  0.98 sec
#   runner emitted bad JSON line 1: Expecting value: line 1 column 3 (char 2)
```

Three consecutive cold runs failed and the warm run passed, so this is
deterministic, not contention — the first reading, that fourteen concurrent
Kotlin compilations were racing over `frontends/android/app/build`, is **wrong**
and is recorded here because `tools/android-trace-emit.sh`'s own header already
describes exactly that race and claims it fixed the class one level up.

The actual cause is one stream. `tools/android-trace-emit.sh:92-95` runs
`tools/build-host-jni.sh` when the shim is missing, and that script ends with
two diagnostic `printf`/`echo` lines (`:63-64`, `ldd` output and the shim
path) that go to **stdout**, not stderr. They land ahead of the trace JSON on
the runner's stdout, and `tools/trace-check.py` parses line 1.

It is one line's worth of fix in either file, and it is not fixed here on
purpose: those harness files belong to the Android work in another worktree.
Left unfixed, its practical effect is that a fresh worktree's first `ctest`
reports fourteen confusing tier-C failures that have nothing to do with
conformance.

## Not ruled on

Nothing here is a ruling. The label scheme is a proposal the maintainer may
overrule, and neither defect above has been decided — only measured. The other
two facts found while labelling are configuration-dependent and already in
`docs/TESTING.md`: the test counts per Android configuration, and that
`-LE 'slow|mutation'` equals `-LE slow` today because both mutation tests are
also `slow` — a fact about those two tests, not a redundancy in the scheme.