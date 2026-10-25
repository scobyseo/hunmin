---
title: Session 2026-10-19 — the test-framework migration, ruled and shipped
created: 2026-10-19
updated: 2026-10-19
type: measurement
status: current
ingested: 2026-10-19
tags: [testing, googletest, cmake, tsf, provenance]
confidence: high
---
> **Paths updated 2026-10-25.** The four options papers became one file,
> `docs/OPTIONS-MEASURED.md`, whose sections are prefixed `MUT-`, `AND-`,
> `TST-` and `IC-`. The section references below use the old names because
> they were measured then; the numbering is unchanged apart from the prefix.


# Session 2026-10-19 — the test-framework migration, ruled and shipped

The session that turned issue #109 (question) into a ruling and then into the
migration itself, on one branch. The measurements live in
`docs/OPTIONS-MEASURED.md` §TST — figures are not duplicated here; this page
records the shape of the session: what was assumed, what overturned it, what
was ruled, what shipped, what is deliberately absent.

## The overturned premise, and the pattern it confirms

The issue body disqualified FetchContent outright: "CI cannot fetch
dependencies while the billing failure stands". The billing fact was true;
the disqualification did not survive measurement. The GIT_REPOSITORY form
fetches in 7.2 s cold and the URL form's failure on this host turned out to
be a cmake-downloader TLS defect, not a property of the mechanism. The
billing block is transient; it gates CI-side *verification*, not the
mechanism. This is the second time a written "disqualified" was really
"unmeasured" — the pattern is in [[the-rules-keep-breaking]].

## The rulings, all the maintainer's (see [[decisions-belong-to-the-maintainer]])

- **Option 1b** — GoogleTest + gmock via FetchContent, not vendored.
- **The four TSF runner groups merge into ONE executable** — the white-box
  seam (`src/tsf.cpp`, private opened) compiles once; adapter-touch rebuild
  12.7 s → 8.8 s measured.
- **ONE `add_test`** for the merged executable; the wrapper
  (`tools/tsf-run-if-data.py`) probes both data conditions and 77s outside
  the binary, because a gtest skip reports rc 0 — the contract the suite's
  skip class exists for.
- **The never-shrinks gate amended**: the registered count on the MSVC tree
  went 59 → 56 by the merge; recorded in the options doc §3.

## What the tree taught that neither the issue nor the doc anticipated

- A fresh configure stores `CMAKE_C_COMPILER:UNINITIALIZED` — the shim
  builder's explicit suffix list missed it and the issue-#86 `hunmin.lib`
  failure returned by another route. The fix reads any type suffix.
- llvm-mingw C++ test binaries need `-static`: their runtime DLLs are off
  ctest's PATH and the suite dies 0xc0000135 — the friction the spike
  measured first, then the tree confirmed.
- The CHECK→gtest conversion went through an ADD_FAILURE shim and then to
  native matchers; the shim was a stop, not an end — the tier compiles on
  the host, so "cannot verify" never justified stopping at the shim.

## What is deliberately absent

The three fcitx5 tier-D runners are not converted — no compiler for fcitx5
exists on this host and CI is billing-blocked ([[the-rules-keep-breaking]]:
an unverified conversion gambles the linux job). Tracked as issue #118; the
board's absence row `GTEST-FCITX5-109` points there. The TSF native-composition
session ([[2026-10-19-tsf-native-composition-preedit]]) ran the day before;
its tier-F1 suite is what this migration converted.
