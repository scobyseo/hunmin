# TEST-FRAMEWORK-OPTIONS — what a test-framework migration would cost, measured

**Status: measurements only. No option is recommended, and none is ruled out.**
Every section reports what a choice costs and what it would change.
The choice is the maintainer's.

Produced 2026-10-19 on `scobyseo/ci-test-framework-migration-google-test-gmock-wh`
(base 5d4206d), Windows 11 host, llvm-mingw 20261006 msvcrt and VS 2022
BuildTools 14.44. This branch is docs-only: `libhunmin/`, `frontends/`,
`tools/` and the tests are unchanged. `docs/REMAINING-WORK.md` and `AGENTS.md`
are deliberately untouched — a parallel session
(`ci-audit-existing-test-harnesses-for-mock-opport`) may be patching the same
lines; board rows for this issue can be added after the merge.

It responds to issue #109 (label `question`), whose request is quoted once:

> Whether the test suite migrates to Google Test (+gmock where a mock pays),
> and — since other candidates are comparable — which framework the ruling
> picks. The options below are measured against this tree, not assumed.

---

## 0. The premise, checked first

Each premise in the issue body was re-verified against this tree before being
relied on.

1. **"No external test-framework dependency anywhere" — verified.**
   `grep -rni -E 'gtest|gmock|googletest' CMakeLists.txt frontends/tsf/
   libhunmin/tests/ frontends/fcitx5/` → 0 hits (2026-10-19, base 5d4206d).
2. **"Engine: hand-rolled `check()`/`expect_str()`" — verified, with a line
   correction.** The pair sits at `libhunmin/tests/commit-text-test.c:23-38`
   (16 lines including the `static int failures` counter), not 22-33. Three C
   executables carry 63 assertion sites total: 28 in commit-text-test.c,
   11 in process-key-test.c, 24 in suggest-test.c.
3. **"mock-tsf.h, ~643 lines" — corrected: 658 lines** at base 5d4206d. The
   tier-F1 C++ surface is also wider than the issue's framing (mock + trace
   runner): five files totalling about 2,390 lines, of which the mock is 658
   and the runner is 480. Three files each re-define the same `CHECK` macro
   (tsf-candidate-test.cpp:58, tsf-config-test.cpp:58, tsf-settings-test.cpp:42)
   with 139 `CHECK(` call sites between them (70 + 44 + 25);
   tsf-datadir-test.cpp asserts by exit code with no shared macro, and
   trace-emit.cpp emits traces instead of asserting.
4. **"fcitx5: same trace-emit.cpp pattern" — verified,** plus three more
   runner executables (testhunmin.cpp, testhunminui.cpp, testmodeswitch.cpp)
   with 5 / 69 / 35 assertion sites, registered as `engine-d1-fcitx5`,
   `engine-d1-modeswitch`, `engine-d2-ui`.
5. **"Android: JUnit, outside C++ scope" — verified** (9 `.kt` files under
   `frontends/android/app/src/test`). Nothing below touches it.
6. **"gmock brings no COM QI support; the QI/vtable plumbing stays
   hand-written either way" — verified by inspection.** The mock's
   `QueryInterface` implementations are hand-written per interface
   (mock-tsf.h:76, 152, 189, 252, ...); gmock's model mocks ordinary C++
   classes and has no COM `IUnknown` machinery, so under any framework option
   that plumbing remains manual.
7. **A premise the issue did not state, which the tree forces:** the engine
   public header already carries `extern "C"` guards (libhunmin/include/hunmin.h:10),
   so compiling the C engine tests under a C++ framework requires no header
   retrofit. The "C/C++ split" cost in option 1 is a build-mode decision, not
   an API change.
8. **Another unstated premise:** the tree already vendors a third-party
   dependency — `third_party/cJSON` (MIT, recorded at debian/copyright:65-68,
   "distributed unmodified"), compiled into four targets. The mechanics option
   1 needs (vendored source, per-directory licence recording) are an exercised
   practice here, not a novel pattern.

## 1. What "the suite" is here

`ctest -N` on this host (configured without `ANDROID_HOME`) registers 52
entries; the workspace-documented 55 includes tier E1, which registers only
when the Android SDK is found at configure time. Of those 52, most are
data- or script-driven gates (31 data-driven golden entries
(`ls tests/golden/*.jsonl | wc -l`), the schema/lint/docs gates, the android
tiers). **A C++ test framework migrates assertion code, not those entries.**
The migration surface is the hand-written assertion code:

| surface | code | assertion sites | ctest entries (this host) |
|---|---|---|---|
| engine C executables | 3 files, C | 63 | 3 |
| tsf tier-F1 runners + COM mock | 5 files, ~2,390 lines | 139 `CHECK(` + exit-code + trace emission | 7 (3 per-trace + 4 runners) |
| fcitx5 runners | 4 files | ~111 | 3 |

Three assertion shapes coexist: the C `check()`/`expect_str()` pair, the TSF
per-file `CHECK` macro (defined three times), and exit-code-only files. A
framework would replace the first two shapes; the third (tsf-datadir-test)
could adopt either the shared macro or a framework with no behaviour change.

The property that makes tier F1 correct is **not an assertion at all**: it is
the differential comparison `tools/trace-check.py` performs between the
runner's emitted trace and the tier-A expectations carried in each
`.trace.in` file. No framework produces this by itself; every option below
keeps `trace-check.py` and the `.trace.in` contract intact.

## 2. The options, measured

Figures measured on this host this session; the spike lived entirely in the
scratch directory (`$TMPDIR/googletest-1.17.0`), not in the repo.

### Option 1 — GoogleTest + gmock, vendored

- **Dependency weight, measured:** the v1.17.0 release tarball is 885,595
  bytes; the unpacked tree — what a vendored commit actually carries — is
  4,095,045 bytes (~3.9 MiB). The issue's "order-of-magnitude
  ~1 MB" describes the tarball; the tree is ~4 MB.
- **Both toolchains compile it, measured:** llvm-mingw (Ninja) builds
  gtest + gmock + both `_main` libraries in 4.3 s, exit 0. VS 2022 x64
  (Release) configures in 7.4 s and builds in 19.9 s, exit 0, with one benign
  MSB8029 warning caused by placing the build under the system temp directory
  (the spike's scratch location), not by gtest.
- **Per-case ctest spawn overhead, measured:** a scratch binary of 20 trivial
  cases, registered via `gtest_discover_tests`, runs 0.595 s under `ctest`
  (20 process spawns) and 0.052 s as a single process — **~27 ms per spawned
  case** on this host. Against the 30 s inner-loop budget, spawn overhead is
  irrelevant at today's granularity (52 entries) and becomes the binding cost
  only if registration granularity explodes to about a thousand cases (for
  scale: the largest golden file alone carries 401 lines of expectations, so
  per-case registration of goldens — which nobody proposes — would spend the
  budget on its own).
  Registration shape, not framework choice, is the budget lever.
- **A host friction, measured:** `gtest_discover_tests` executes the test
  binary at build time; an llvm-mingw-linked binary without `-static` cannot
  launch outside the toolchain's PATH (its runtime DLLs do not resolve) and
  discovery fails with "Error running test executable". With `-static` (or
  the toolchain bin on PATH) discovery works. Linux CI is unaffected; on this
  host, per-case discovery needs one of the two.
- **gmock vs the COM mock:** the QI/vtable plumbing stays hand-written
  (premise 6). gmock's matchers would buy assertion introspection over the
  mock's interaction records that the current printf-style CHECKs do not
  have — the "where a mock pays" part of the request. What it would replace
  first is not the COM plumbing but the three duplicated CHECK macros and
  their 139 sites.
- **C engine tests:** compilable as C++ (premise 7). Sixty-three sites across
  three files map to TEST/EXPECT macros 1:1, or stay hand-rolled if the
  ruling scopes the framework out of C (option 2).

### Option 2 — GoogleTest for the C++ frontend tiers only

Touches 10 of the migration-surface entries (7 tsf + 3 fcitx5); the 3 C
executables keep the 16-line `check()`/`expect_str()` pair. Same dependency
weight and toolchain measurements as option 1. The C tier's remaining
hand-rolled shape stays a shape (three shapes become two).

### Option 3 — Catch2 / doctest

Same C++-only reach as option 2 with a lighter dependency (amalgamated
header). **Not measured on this host** — only gtest was compiled this
session; the figures above must not be extrapolated to them. Neither has a
CTest module of gtest's maturity (doctest ships `doctest_discover_tests`,
Catch2 has its own CMake integration), and neither offers something this
tree needs that gtest lacks; gmock's matchers have no equivalent in doctest.

### Option 4 — keep hand-rolled everywhere

Zero new dependency, zero migration cost; the cheapest shape measured (the
single-process runner at 0.052 s for 20 cases). What it does not buy: one
assertion idiom across the three shapes, the duplicated-CHECK collapse, and
matcher-level introspection on the COM mock's interaction records.

### What would change the answer

- **If the ruling wants one assertion idiom and the CHECK duplication gone**,
  the framework cost is ~3.9 MiB vendored and two toolchain builds that are
  already measured cheap; the deciding residual is whether the C tier
  migrates too (option 1 vs 2).
- **If the 30 s inner-loop budget is the binding constraint**, it binds on
  registration granularity, not on the framework: keep one ctest entry per
  binary under any option and the measured overhead is ~ms, not seconds.
- **If tier-F1 correctness ever needs richer interaction introspection**, that
  is gmock's matcher layer — and it layers on top of the hand-written COM
  plumbing either way; it is not a reason to switch framework candidates.
- **Nothing above is verifiable on CI today**: the GitHub Actions jobs fail
  at billing level (issue #102), so toolchain and budget verification is
  host-measured until that clears.

## 3. Summary

| option | new vendored tree | C engine tier | tsf tier-F1 | fcitx5 | ctest entries at risk | measured toolchain cost |
|---|---|---|---|---|---|---|
| 1 gtest+gmock vendored | ~3.9 MiB | migrates (63 sites) | migrates CHECK sites; QI stays manual | migrates | 13 | 4.3 s + 19.9 s one-off |
| 2 gtest, C++ tiers only | ~3.9 MiB | stays hand-rolled | same as 1 | migrates | 10 | same as 1 |
| 3 Catch2/doctest | lighter, not measured | C++-only reach | same reach as 2 | migrates | 10 | not measured here |
| 4 keep hand-rolled | none | unchanged | unchanged | unchanged | 0 | 0 |

Under every option: `trace-check.py` and the `.trace.in` differential
contract stay; the 31 data-driven golden entries and the lint/schema gates
are untouched; the registered-entry count cannot shrink below the 3 + 7 + 3
code-driven entries listed in §1 (each hand-rolled binary maps to at least
one framework case).

## 4. Provenance

- Tree state: base 5d4206d, worktree
  `C:/Workspace/hunmin/ci-test-framework-migration-google-test-gmock-wh`,
  clean at start; this branch changes `docs/` only.
- `grep -rni -E 'gtest|gmock|googletest' CMakeLists.txt frontends/tsf/
  libhunmin/tests/ frontends/fcitx5/` → 0 hits.
- `wc -l` on the five tier-F1 files (2,390 total; mock-tsf.h 658).
- `ctest --test-dir build-mingw -N` → 52 entries (llvm-mingw configure,
  no `ANDROID_HOME`; 55 with tier E1 per the workspace record).
- `ls tests/golden/*.jsonl | wc -l` → 31; `wc -l tests/golden/ref-dubeolsik.jsonl`
  → 401 lines.
- Googletest v1.17.0 tarball downloaded to scratch: 885,595 bytes;
  unpacked 4,095,045 bytes. Compiled with llvm-mingw (Ninja, 4.3 s, exit 0)
  and VS 2022 x64 Release (19.9 s, exit 0) — both from the scratch tree,
  never added to this repo.
- Spawn-overhead spike: 20-case scratch binary, `gtest_discover_tests`
  registration, `time ctest` 0.595 s vs `time` direct run 0.052 s; discovery
  without `-static` reproduced the launcher failure, with `-static` it passed.
- Read-but-not-executed (named so they are not mistaken for measurements):
  Catch2 and doctest builds on this host; gmock matcher behaviour against
  the COM mock's interaction records; anything CI-side (jobs fail at billing,
  issue #102). If a framework ever lands, building it as a repo target and
  running it in the cheap tier would turn these host-side figures into
  in-tree ones.
