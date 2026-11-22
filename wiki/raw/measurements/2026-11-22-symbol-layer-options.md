---
title: "Symbol-layer options: the third row is not engine-free"
created: 2026-11-22
updated: 2026-11-22
type: measurement
status: active
tags: [android, layouts, measurement, tooling, question]
sources:
  - ../../../tools/measure-symbol-layer.py
  - ../../../docs/DESIGN-ANDROID-SYMBOLS.md
  - https://github.com/scobyseo/hunmin/issues/298
confidence: high
ingested: 2026-11-22
---

# Symbol-layer options: the third row is not engine-free

Issue #298 asked for the Android symbol-layer home. The tracking issue's premise
was that a third per-key row would be "still data, no engine change". The probe
refutes that premise against the built tree: the loader ignores every modifier
except `plain` and `shift`, the public key API selects only those two rows, and
`doctor.py` rejects a `symbol` modifier while accepting the reserved `alt`
modifier that the engine also ignores. The figures and the comparable designs
live in `docs/DESIGN-ANDROID-SYMBOLS.md`; this page records how the measurement
was made and what it disturbed.

## What was run

`tools/measure-symbol-layer.py` was added as the reproducible procedure. It
copies layouts/compose into scratch directories under `build/`, adds one probe
row or one probe layout, runs `doctor.py`, and drives the built engine through
ctypes. It does not modify shipped data. The probe also checks the option
surface, the mode-toggle boundary path and the external-text path after a
buffered Hangul word.

The scratch `android-symbols` layout is a feasibility probe only: it shows that
a symbol page can be an ordinary layout file with no engine change, but it also
shows the integration cost — a `latin` layout appears in Android's Latin
settings list unless a new contract hides it.

## Verification baseline and what it disturbed

The worktree was configured with `JAVA_HOME`, `ANDROID_HOME` and `ANDROID_NDK`
exported, then built. The baseline `ctest -N` was captured before any edit and
was unchanged after the documentation/tool commit.

| command | outcome |
|---|---|
| `ctest -j16 -LE slow` | 151.90 s, over the 30 s inner-loop budget; one Espresso failure, three skips |
| `ctest -R '^engine-android-espresso$'` | 75.33 s, passed alone |
| `ctest -j16 -LE 'slow\|tier-E2'` | 12.73 s, passed, three skips |
| `doctor.py`, `docs-lint`, `req-map`, `wiki-lint` | 0 errors; pre-existing advisories only |

The inner-loop overrun is structural: tier E2 is not labelled `slow`, so the
emulator tests ran inside the inner loop. `engine-android-espresso` failed once
with "the device did not report an ABI" because the emulator from the preceding
E2 run disappeared between the serial check and the ABI read; run alone, it
booted its own AVD and passed. This is a baseline/environment finding, not a
regression from the analysis.

A second documentation finding was left open rather than silently resolved:
`docs/TESTING.md` Tier B still warns that `doctor.py` does not lint the hanja
directories, while the current `tools/doctor.py` and `docs/SPEC-SCHEMA.md` say
it does. The analysis uses the measured source; the stale warning is a separate
contradiction for the maintainer to dispose of.

## What this page does not do

It records no verdict. The choice between a per-key row, a separate page/table,
and a long-press interaction is the maintainer's, as recorded in
[[decisions-belong-to-the-maintainer]]. The premise drift is the kind of failure
[[the-rules-keep-breaking]] catalogues: a plausible sentence in a tracking issue
was not a tree fact. The tier placement finding belongs to
[[test-scenario-guide]].
