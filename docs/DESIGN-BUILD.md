# Build system — where the decisions live

Status: **the home for build-architecture decisions that affect two or more
places.** A rationale repeated at several registration sites lives here once;
the `cmake/*.cmake` files carry a one-line pointer to the section that owns
it. Measured figures never live here — they live once in the document the
entry points at, and this file cites REQ ids without restating them.

Per-entry shape: Decision / Affects (cmake symbols) / Alternatives /
Evidence → pointer. A cmake comment that would make code misleading if
deleted stays in the cmake file; this file holds the multi-place rationale.

## File layout

Decision: The root `CMakeLists.txt` is an index (libraries, install, toolchains,
frontend discovery, then `include()`s of the test files). Test registration
lives in `cmake/HunminTestHelpers.cmake` and one file per coverage subject:
`HunminLibhunminTests`, `HunminEngineTests`, `HunminDataTests`,
`HunminConformanceTests`, `HunminFrontendTests`, `HunminAndroidTests`,
`HunminProcessTests`.
Affects: `hunmin_add_test`, `hunmin_android_shell_command`,
`cmake/Hunmin*.cmake`.
Alternatives: One file per tier letter; keeping everything in `CMakeLists.txt`.
Evidence → issue #271 (d2158 description), A6 in `docs/SPEC-OVERVIEW.md`.

## Test helper (H2)

Decision: `hunmin_add_test(NAME … COMMAND … LABELS …)` registers a test and
its `LABELS`/`TIMEOUT`/`SKIP_RETURN_CODE`/`RUN_SERIAL`/`WORKING_DIRECTORY`/
`ENVIRONMENT` properties in one call. `hunmin_android_shell_command` builds a
shell invocation available on POSIX and Windows.
Affects: `hunmin_add_test`, `hunmin_android_shell_command`,
`tools/docs-lint.py`, `tools/req-map-check.py` (both glob `cmake/*.cmake`).
Alternatives: H1 (helper builds the executable only, `add_test` stays literal
at the call site).
Evidence → issue #271 (H2 ruling).

## Labels

Decision: Two independent axes. Axis 1 is the verification tier; axis 2 is
orthogonal cost (`mutation`, `slow`). Selection is via `ctest -L`/`-LE`, whose
regexes match unanchored.
Affects: `hunmin_add_test`, `LABELS` in every `cmake/Hunmin*.cmake` file.
Alternatives: Encoding cost in the test name.
Evidence → `docs/TESTING.md` "Test labels".

## Labels are a property, not a keyword

Decision: `LABELS` goes through `set_tests_properties`, never as an
`add_test` keyword.
Affects: `hunmin_add_test`, every registration in `cmake/Hunmin*.cmake`.
Alternatives: `add_test(NAME … COMMAND … LABELS …)` (silently appends labels to
the command line).
Evidence → `docs/TESTING.md` "One CMake trap, recorded because it is silent"
(issue #59 measurement, `docs/CI-TIMING.md`).

## Guard levels

Decision: The commit rung is the test set labelled `guard`. The PR rung is the
unfiltered suite; the full `linux` CI job runs it unfiltered so a
label-selected run cannot hide what it left out.
Affects: `builds`, `builds-fcitx5-addon`, `shell-lint`, `conformance-coverage`,
`schema-contract`, `abi`, `docs-lint` in `cmake/Hunmin*.cmake`.
Alternatives: Making the linux job label-selected.
Evidence → `docs/TESTING.md` "The three levels"; `docs/CI-TIMING.md` §5.

## GoogleTest is one entry per binary

Decision: Each gtest executable is one `add_test`. No `gtest_discover_tests`.
Affects: `candidate-tables-test`, `commit-text-test`, `process-key-test`,
`buffered-preedit-test`, `wordset-test`, `hangul-valid-test`, `key-rule-test`
in `cmake/HunminLibhunminTests.cmake`; `testfcitx5d3` in
`cmake/HunminFrontendTests.cmake`.
Alternatives: `gtest_discover_tests`.
Evidence → `docs/OPTIONS-MEASURED.md` §TST (spawn-cost measurement).

## GoogleTest fetch and `INSTALL_GTEST`

Decision: GoogleTest is fetched at configure time with
`FetchContent_Declare`/`FetchContent_MakeAvailable`. The fetched SDK must not
install: `option(INSTALL_GTEST … OFF)` is declared before
`FetchContent_MakeAvailable`, as a cache option. Offline configures point
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST` at a checkout.
Affects: `FetchContent_Declare`, `FetchContent_MakeAvailable`,
`INSTALL_GTEST` in `CMakeLists.txt`; `HUNMIN_TEST_LINK_OPTS` (MinGW static
runtimes).
Alternatives: System `find_package(GTest)`; `set(INSTALL_GTEST OFF)` (not
overridable by `-D`).
Evidence → `docs/OPTIONS-MEASURED.md` (install-artifact measurement);
`docs/REMAINING-WORK.md` rows `GTEST-FETCH-109`, `DEB-GTEST-150`.

## fcitx5 floor and optional addon

Decision: The addon is built only when `Fcitx5Core >= 5.1.13` (and
`Fcitx5Module` TestFrontend) is found. Absence is a status message, not an
error.
Affects: `HUNMIN_FCITX5`, `Fcitx5Core`, `HUNMIN_HAS_FCITX5`,
`frontends/fcitx5` in `CMakeLists.txt`; `frontend-mutations`,
`builds-fcitx5-addon` in `cmake/Hunmin*.cmake`.
Alternatives: Treating a missing addon as a configure error.
Evidence → `docs/REMAINING-WORK.md` C3 (measured across upstream tags).

## Data installs as two components

Decision: `libhunmin/data` installs as two CMake components: `data` (all but
`modes/`) and `modes-data` (`modes/` alone). Debian's two binary packages
are the same partition.
Affects: `install(DIRECTORY … COMPONENT …)`, `data-split` in
`cmake/HunminDataTests.cmake`.
Alternatives: One component; excluding `modes/` by default.
Evidence → `docs/OPTIONS-MEASURED.md` §EDW-4; `docs/REQ-MAP.md` (ReqMode8);
verified by `data-split`, `engine-wordset`.

## MSVC `/utf-8`, PE exports, MSVC static runtime

Decision: MSVC gets `/std:c11 /W4 /utf-8`. Windows PE exports are MinGW-only
(`-Wl,--export-all-symbols`); MSVC uses `CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS`
and the static engine archive for the TSF TIP. The packaged TIP links the
MSVC CRT statically (`HUNMIN_MSVC_STATIC_RUNTIME`).
Affects: `add_compile_options`, `hunmin`, `hunmin_static`,
`HUNMIN_MSVC_STATIC_RUNTIME` in `CMakeLists.txt`.
Alternatives: Guarding the flags with `if(NOT MSVC)`; shipping `/MD`.
Evidence → `docs/ENVIRONMENT.md` batch 4; `docs/REMAINING-WORK.md` rows
`MSVC-CORE-TESTS-85`, `CI-WINDOWS-OUT-OF-SCOPE`.

## Shell and Python UTF-8 defaults

Decision: Tests needing a POSIX shell resolve one shell
(`HUNMIN_SH`, plus `HUNMIN_BASH_EXECUTABLE` on Windows) rather than assuming
`sh` on `PATH`. Every test gets `ENVIRONMENT_MODIFICATION "PYTHONUTF8=set:1"`.
Affects: `HUNMIN_SH`, `hunmin_android_shell_command`, `HUNMIN_TESTS` in
`CMakeLists.txt`.
Alternatives: Spawning shebang scripts directly (fails on Windows hosts).
Evidence → `docs/ENVIRONMENT.md`; `tools/_hostcompat.py`.

## Android sandbox mutations

Decision: `frontend-mutations` and `android-mutations` name their strategy
(`HUNMIN_MUTATION_STRATEGY=sandbox`) at registration and copy the tree to a
temp dir. `RUN_SERIAL` is not used for the sandbox form.
Affects: `frontend-mutations`, `android-mutations`,
`HUNMIN_MUTATION_CACHE` in `cmake/Hunmin*.cmake`.
Alternatives: The in-tree strategy (held the suite hostage); environment-set
strategy.
Evidence → `docs/TESTING.md` (mutation label); `docs/REMAINING-WORK.md`
(rows for the sandbox migration).

## D3 system directories

Decision: D3 drives the system fcitx5. Its addon/pkgdata dirs come from
pkg-config (`Fcitx5Core` libdir/prefix), never from the build prefix, and the
pkgdata entry is the `fcitx5/` directory itself.
Affects: `d3_libdir`, `d3_addon_dir`, `d3_pkgdata_dir`,
`HUNMIN_D3_HEAD`/`HUNMIN_D3_MID`/`HUNMIN_D3_FOOT` in
`cmake/HunminFrontendTests.cmake`.
Alternatives: `FCITX_INSTALL_*` variables from the build prefix.
Evidence → `docs/D3-ROOT-CAUSE.md`.

## Trace sink opt-out

Decision: `trace-optout` guards the trace build shape: the trace sink must
not reach a packaged build. It is registered only when the addon exists and
`HUNMIN_TRACE` is off.
Affects: `trace-optout` in `cmake/HunminConformanceTests.cmake`.
Alternatives: A configure-time error for packaged trace builds.
Evidence → `docs/TESTING.md` tier C step 2.
