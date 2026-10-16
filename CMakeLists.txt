cmake_minimum_required(VERSION 3.22)
project(hunmin LANGUAGES C VERSION 0.1.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# clangd (and any editor) needs a compilation database; without it every
# TU is parsed with guessed flags. gitignored, so nothing is committed.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(GNUInstallDirs)
add_compile_options(-Wall -Wextra)
option(BUILD_TESTING "" ON)
include(CTest)
find_package(PkgConfig QUIET)
find_package(Python3 QUIET COMPONENTS Interpreter)

# ---------------------------------------------------------------- libhunmin
add_library(hunmin SHARED
  libhunmin/src/version.c
  libhunmin/src/tables.c
  libhunmin/src/data.c
  libhunmin/src/expr.c
  libhunmin/src/ic.c
  libhunmin/src/suggest.c
  third_party/cJSON/cJSON.c)
target_include_directories(hunmin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/libhunmin/include)
target_include_directories(hunmin PRIVATE third_party/cJSON libhunmin/src)
set_target_properties(hunmin PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 0
    POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(hunmin PRIVATE
  HUNMIN_DEFAULT_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/hunmin")

install(FILES libhunmin/include/hunmin.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY libhunmin/data/ DESTINATION ${CMAKE_INSTALL_DATADIR}/hunmin)

# ------------------------------------------------- reference golden generator
set(HUNMIN_HAS_LIBHANGUL OFF)
if(PkgConfig_FOUND)
  pkg_check_modules(LIBHANGUL IMPORTED_TARGET GLOBAL libhangul)
  if(LIBHANGUL_FOUND)
    add_executable(gen-golden-libhangul tools/gen-golden-libhangul.c)
    target_link_libraries(gen-golden-libhangul PRIVATE PkgConfig::LIBHANGUL)
    set(HUNMIN_HAS_LIBHANGUL ON)
  endif()
endif()

find_program(HUNMIN_SH sh PATHS /bin /usr/bin)
if(HUNMIN_HAS_LIBHANGUL AND HUNMIN_SH)
  add_custom_target(goldens
    COMMAND ${HUNMIN_SH} ${CMAKE_CURRENT_SOURCE_DIR}/tools/regen-goldens.sh
            ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# --------------------------------------------------------------------- sim
add_executable(hunmin-sim tools/sim.c)
target_include_directories(hunmin-sim PRIVATE third_party/cJSON)
target_link_libraries(hunmin-sim PRIVATE hunmin)
install(TARGETS hunmin hunmin-sim
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# --------------------------------------------------------- fcitx5 frontend
# (tier D1; skipped when Fcitx5Core is unavailable)
option(HUNMIN_FCITX5 "Build the fcitx5 addon" ON)
if(HUNMIN_FCITX5)
  # 5.1.13 is the real API floor (FCITX_ADDON_FACTORY_V2, engine.cpp:619),
  # measured from upstream tags 2026-10-13 (issue #8). Ubuntu 24.04's distro
  # 5.1.7 is below it, so CI builds fcitx5 from source; the addon's build-time
  # macro gap on a bare 5.1.7 is still handled by the local fallback in
  # frontends/fcitx5/CMakeLists.txt, but that does not bridge the API gap.
  find_package(Fcitx5Core 5.1.13 QUIET)
  find_package(Fcitx5Module QUIET COMPONENTS TestFrontend)
  if(Fcitx5Core_FOUND)
    add_subdirectory(frontends/fcitx5)
  else()
    message(STATUS "Fcitx5Core not found: fcitx5 addon disabled")
  endif()
endif()

# ------------------------------------------------------------------- tests
# ----------------------------------------------------------------- labels
# Every test carries labels on two INDEPENDENT axes. Select with `ctest -L`
# (match) and `ctest -LE` (exclude); both take a REGEX, matched UNANCHORED
# against each label, so `-L tier-D` selects tier-D1, -D2 and -D3 together and
# `-L tier-E` selects tier-E0/E1/E2. The scheme and every label are documented
# in docs/TESTING.md "Test labels"; this file is the authority for the list.
#
#   axis 1  tier-*   the verification tier, in docs/TESTING.md's own
#                    vocabulary (A, B, C, D1-D3, E0-E2, G). One tier per test,
#                    except frontend-mutations, which guards D1 AND D2 and
#                    says so by carrying both.
#
#   axis 2  orthogonal properties, combinable with any tier and with each
#           other:
#             mutation  the test rewrites repo source and rebuilds, to prove a
#                       guard can fail. Not a tier test itself -- it is its
#                       own tier's falsifiability check.
#             slow      measured >= 2 s on this host. The 2 s cut is a measured
#                       gap rather than a round number: the seven slowest tests
#                       take 2.8-50.3 s and the eighth takes 1.7 s, so any cut
#                       in (1.7, 2.8] s gives the same partition and the label
#                       only has to be re-measured when that gap closes. It is
#                       host-relative -- engine-android-e1 is 2.8 s here and
#                       15.9 s against a cold Gradle cache.
#
# NOT a gate: .github/workflows/ci.yml's linux job runs the plain unfiltered
# `ctest`, because a label-selected run cannot prove what it left out. Labels
# exist so a person can run "just the tier I touched"; the selection must be
# visible, so every invocation in docs/TESTING.md and AGENTS.md names what it
# left out.
#
# What that costs, measured 2026-10-14 for issue #59: the linux job's ctest is
# 269 s and the deb job's was 402 s, and three tests were 252 and 386 of those
# respectively (docs/CI-TIMING.md §1). One exception is now in the tree and it
# is deliberate: debian/rules' override_dh_auto_test runs
# `ctest -j4 -LE 'tier-E1'`, because the .deb does not ship the Android adapter
# and the adapter's own gate is the android job. The recipe's own comment
# records that, which is the visibility the paragraph above requires -- the
# linux job remains unfiltered, and the property stands there.
#
# NOTE: LABELS is a test PROPERTY, not an add_test() keyword. `add_test(NAME
# x COMMAND y LABELS a)` does not error -- CMake appends "LABELS" and "a" to
# the command line as two more argv entries, `ctest -N` still reports the same
# test count, and nothing fails until the test runs. Hence
# set_tests_properties(... PROPERTIES LABELS ...), everywhere below.
if(BUILD_TESTING AND Python3_FOUND)
  add_test(NAME data-selfcheck
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/selfcheck.py
            ${CMAKE_CURRENT_SOURCE_DIR})
  # SKIP (rc 77) when the gitignored 368 MB krdict dump is absent — a fresh
  # worktree has no data-orig/krdict/, and the repo's tests must be
  # self-contained. krdict-fixture below is the self-contained guard; the
  # dump-dependent full run is a maintainer-only script in tools/, which is
  # deliberately unnamed in this file so that a grep for it in the ctest
  # registration returns nothing (issue #46's verification).
  set_tests_properties(data-selfcheck PROPERTIES
    LABELS "tier-B" SKIP_RETURN_CODE 77)
  add_executable(expr-test tools/expr_test.c)
  target_include_directories(expr-test PRIVATE libhunmin/src)
  target_sources(expr-test PRIVATE libhunmin/src/expr.c)
  add_test(NAME expr-vectors COMMAND expr-test)
  set_tests_properties(expr-vectors PROPERTIES LABELS "tier-A")
  # Tier A for the P3 candidate API. Runs against libhunmin/data via
  # HUNMIN_DATA_PATH so it exercises the shipped tables, not a copy.
  add_executable(suggest-test libhunmin/tests/suggest-test.c)
  target_link_libraries(suggest-test PRIVATE hunmin)
  add_test(NAME engine-suggest COMMAND suggest-test)
  set_tests_properties(engine-suggest PROPERTIES
    LABELS "tier-A"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    ENVIRONMENT "HUNMIN_DATA_PATH=${CMAKE_CURRENT_SOURCE_DIR}/libhunmin/data")
  # Tier A for invariant 5: hunmin_ic_commit_text(), the external-text
  # pass-through P5's voice path needs. Its interesting assertions are the
  # negative ones -- that the text is NOT put through the jamo pipeline.
  add_executable(commit-text-test libhunmin/tests/commit-text-test.c)
  target_link_libraries(commit-text-test PRIVATE hunmin)
  add_test(NAME engine-commit-text COMMAND commit-text-test)
  set_tests_properties(engine-commit-text PROPERTIES
    LABELS "tier-A"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    ENVIRONMENT "HUNMIN_DATA_PATH=${CMAKE_CURRENT_SOURCE_DIR}/libhunmin/data")
  # Structural invariants of the fcitx5 adapter that tier D1 provably does
  # not cover (docs/FRONTEND-CONTRACT.md §4: mutation testing showed the
  # commit/flush split and the caps option can both break with D1 still green).
  add_test(NAME frontend-invariants
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/check-frontend-invariants.py)
  # It carries tier-D1 rather than a tier of its own: the comment above says it
  # exists for the D1 coverage D1 does not have, so "I touched the fcitx5
  # adapter" has to select it.
  set_tests_properties(frontend-invariants PROPERTIES LABELS "tier-D1")
  # Self-test: plant each known-interesting mutation, assert the guard
  # catches it. Mutates and rebuilds, so it is only registered for a
  # generator-built tree (ninja/make), and it runs last.
  # Tier C step 4: keycode-map fixture (docs/TESTING.md). Reads the XKB header
  # as the authority for X11 keysym values rather than trusting a table that
  # merely restates the code under test.
  # The trace sink must not reach a packaged build (docs/TESTING.md tier C
  # step 2). Only meaningful when HUNMIN_TRACE is OFF, which is the default and
  # the only configuration a release uses.
  if(TARGET hunmin_addon AND NOT HUNMIN_TRACE)
    add_test(NAME trace-optout
      COMMAND ${Python3_EXECUTABLE}
              ${CMAKE_CURRENT_SOURCE_DIR}/tools/check-trace-optout.py
              $<TARGET_FILE:hunmin_addon>)
    set_tests_properties(trace-optout PROPERTIES LABELS "tier-C")
  endif()
  add_test(NAME keymap-fixture
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/keymap-fixture.py)
  set_tests_properties(keymap-fixture PROPERTIES LABELS "tier-C")
  # Tier E groundwork: the engine's output must not depend on the libc it was
  # linked against. It did -- compose-table dedup ran after an unstable qsort --
  # and no host-only tier could see it, because glibc and bionic disagree about
  # equal keys. Registered here so a libc-dependent divergence is a ctest
  # failure on a host with a device, and skips (rc 77) everywhere else.
  # Tier E1: the Android adapter's JVM unit tests (frontends/android). Needs a
  # JDK and the Android SDK but NOT a device, so it is registered separately
  # from E0/E2. Skips (rc 77) without a JDK, because CI's linux job may have
  # neither -- and tools/android-unit.sh reports a SKIP inside the suite as a
  # FAILURE, so an unconfigured host cannot produce a green-but-meaningless
  # report the way a bare `gradlew test` does.
  # --- the commit-level rung: does each part still build? ------------------
  #
  # The `guard` label (docs/TESTING.md "The three levels") is the check that runs
  # on every commit, and a label can only select tests -- so "did it build" has to
  # be a test. These three are the whole of it, and they are cheap precisely
  # because the build tree is already configured and mostly up to date:
  #
  #   measured on this host, incremental, after touching one source file:
  #     builds            (engine + fcitx5 addon)   2.8 s
  #     builds-android    (assembleDebug, warm)    12.0 s
  #     abi + schema-contract + docs-lint           5.1 s
  #     -> `ctest -L guard`                        ~20 s
  #
  # WHAT THIS DOES NOT PROVE, stated because it is the obvious objection: on an
  # unchanged tree every one of these is a no-op that exits 0, so a green guard
  # can mean "nothing changed" rather than "it builds". That is the intended
  # trade -- the guard runs after an edit, and an edit makes the build out of
  # date, which is exactly when this has teeth. A cold Gradle cache is the other
  # limit: measured 126 s on this host (docs/CI-TIMING.md), which does not fit a
  # 60 s commit rung, so the Android leg belongs to the PR rung on any machine
  # that has not run Gradle recently.
  add_test(NAME builds
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} -j 4)
  set_tests_properties(builds PROPERTIES LABELS "guard" TIMEOUT 600)

  # The fcitx5 addon, as its own test, because whether it EXISTS depends on the
  # machine and the cache -- and the difference has to be visible rather than
  # folded into a pass.
  #
  # Two measured facts make this shape necessary rather than fussy:
  #
  #  1. On a cold Actions cache the addon cannot build inside a 60 s rung: the
  #     linux job builds fcitx5 5.1.13 from source because Ubuntu 24.04 ships
  #     5.1.7, below the addon's real floor, and that build is 197 s
  #     (docs/CI-TIMING.md). With the install tree cached it restores in ~10 s.
  #  2. The repo's Actions caches are currently EMPTY (total_count 0), because no
  #     job has executed since 2026-10-13. So "the cache will be warm" is not an
  #     assumption this rung may make.
  #
  # So: build the addon when it is configured, and SKIP (exit 77, which ctest
  # prints by name) when it is not. A skip is not a pass, and the skip message
  # names the cause rather than leaving a green line to be read as coverage --
  # the same failure this repository keeps paying for, where a missing
  # `fcitx5-modules-dev` silently dropped the addon and 104 conformance tests
  # never registered, which reads as green (see ci.yml's build-deps comment).
  add_test(NAME builds-fcitx5-addon
    COMMAND sh -c
      "if cmake --build ${CMAKE_BINARY_DIR} --target help 2>/dev/null | grep -q hunmin_addon; then
         cmake --build ${CMAKE_BINARY_DIR} --target hunmin_addon;
       else
         echo 'builds-fcitx5-addon: SKIPPED -- no hunmin_addon target, which means Fcitx5Core >= 5.1.13 was not found when this tree was configured (CMakeLists.txt printed \"Fcitx5Core not found: fcitx5 addon disabled\"). The addon was NOT compiled and NOT checked here; the tier-C/D rows that need it did not run either.';
         exit 77;
       fi")
  set_tests_properties(builds-fcitx5-addon PROPERTIES
    LABELS "guard" SKIP_RETURN_CODE 77 TIMEOUT 600)

  # Code-level lint for the shell, at -S error only.
  #
  # -S error and not -S warning, measured 2026-10-16: at error severity the tree
  # is clean, while -S warning reports 15 findings that are largely DELIBERATE
  # here -- SC2046 unquoted word splitting in the resource-id greps is
  # documented at length in tools/android-e2e.sh, and SC2034 flags the
  # `$!`-handle variables the host rules require keeping. A gate that fires on
  # those gets switched off, which is worse than no gate. At error severity it
  # fires on exactly the class that matters: a script that would not parse or
  # would run wrong. Its first catch was a missing shebang on
  # tools/selfcheck-krdict.sh (SC2148).
  #
  # Fails if shellcheck is ABSENT rather than skipping, matching the precedent
  # the M5-CLOSED assertion row set for pyright: a missing linter read as a
  # clean tree is the failure mode this file exists to prevent.
  file(GLOB_RECURSE HUNMIN_SHELL_SCRIPTS CONFIGURE_DEPENDS
       ${CMAKE_CURRENT_SOURCE_DIR}/tools/*.sh
       ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/*.sh)
  find_program(SHELLCHECK_EXECUTABLE shellcheck)
  if(HUNMIN_SHELL_SCRIPTS AND SHELLCHECK_EXECUTABLE)
    add_test(NAME shell-lint
      COMMAND ${SHELLCHECK_EXECUTABLE} -S error ${HUNMIN_SHELL_SCRIPTS})
  else()
    add_test(NAME shell-lint
      COMMAND ${CMAKE_COMMAND} -E echo
        "shell-lint did not run: shellcheck not found, or no shell scripts matched. An absent linter is not a clean tree (the same rule the pyright assertion row follows).")
  endif()
  set_tests_properties(shell-lint PROPERTIES LABELS "guard" TIMEOUT 300)

  find_program(JAVA_EXECUTABLE java HINTS $ENV{JAVA_HOME}/bin)
  if(JAVA_EXECUTABLE AND DEFINED ENV{ANDROID_HOME})
    add_test(NAME engine-android-e1
      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/android-unit.sh)
    set_tests_properties(engine-android-e1 PROPERTIES
      LABELS "tier-E1;slow"
      SKIP_RETURN_CODE 77 TIMEOUT 900 RUN_SERIAL TRUE)
    # Tier E1's own falsifiability check: plant nine known-interesting mutations
    # in the Android adapter and assert the suite catches each one.
    #
    # This used to be RUN_SERIAL because it overwrote three Kotlin files in the
    # working tree and restored them from a snapshot taken at the start, so an
    # edit made during the run was lost -- and a concurrent build saw a
    # deliberately broken adapter. tools/android-mutations.py now has the same
    # strategies frontend-mutations got in ca31f14; the sandbox one copies the
    # tree to a temp dir and writes nothing outside it, which is what makes
    # RUN_SERIAL unnecessary. The strategy is named here rather than left to
    # the environment so CI is deterministic; tools/android-mutations.sh
    # --strategy in-tree is the old behaviour, where the exclusivity
    # requirement is the caller's to honour.
    # The Android half of the commit rung: does the Kotlin frontend still
    # compile. Same shape as every other Android test here -- registered when the
    # toolchain is configured, SKIP_RETURN_CODE 77 otherwise -- so a host without
    # an SDK reports "not run" rather than a green run that checked nothing.
    # RUN_SERIAL: Gradle takes a lock on its own caches, and two of these at once
    # is contention measured as seconds, not a correctness problem.
    add_test(NAME builds-android
      COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}/frontends/android
              ./gradlew --no-daemon assembleDebug)
    set_tests_properties(builds-android PROPERTIES
      LABELS "guard"
      SKIP_RETURN_CODE 77 TIMEOUT 900 RUN_SERIAL TRUE)
    add_test(NAME android-mutations
      COMMAND ${CMAKE_COMMAND} -E env HUNMIN_MUTATION_STRATEGY=sandbox
              ${CMAKE_CURRENT_SOURCE_DIR}/tools/android-mutations.sh)
    set_tests_properties(android-mutations PROPERTIES
      LABELS "tier-E1;mutation;slow"
      # No RUN_SERIAL: the sandbox strategy copies the sources out, so this
      # tier no longer holds the suite hostage for its duration. Measured
      # 78.8 s -> 54.0 s for the whole run at -j16.
      SKIP_RETURN_CODE 77 TIMEOUT 1800)
  endif()
  find_program(ADB_EXECUTABLE adb HINTS $ENV{ANDROID_HOME}/platform-tools)
  if(ADB_EXECUTABLE AND EXISTS "$ENV{ANDROID_NDK}")
    add_test(NAME engine-android-goldens
      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/android-golden-check.sh)
    set_tests_properties(engine-android-goldens PROPERTIES
      LABELS "tier-E0"
      SKIP_RETURN_CODE 77 TIMEOUT 600 RUN_SERIAL TRUE)
    # Tier E2: the real APK installed on the emulator, typed into with key
    # events, and the committed text read back out of a real EditText via
    # uiautomator. This is the only tier that can fail if the platform does not
    # route hardware keys to an IME at all -- a question no host tier can ask.
    add_test(NAME engine-android-e2e
      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/android-e2e.sh)
    set_tests_properties(engine-android-e2e PROPERTIES
      LABELS "tier-E2"
      SKIP_RETURN_CODE 77 TIMEOUT 1200 RUN_SERIAL TRUE)
    # Tier E2, Espresso half: the instrumentation suite under
    # frontends/android/app/src/androidTest/, run through `am instrument`
    # against the device. It needs adb and a JDK; the NDK is in the guard for
    # consistency with the tiers above, not because it builds anything here.
    #
    # Registered in CMakeLists.txt and not only in .github/workflows/ci.yml,
    # which is what the first version of this did: the script existed, the
    # board named `engine-android-espresso`, and `ctest -N` listed no test by
    # that name (measured 2026-10-13). A tier named in three documents and
    # reachable from no `ctest -R` is the "green having measured nothing"
    # shape issue #13 records.
    add_test(NAME engine-android-espresso
      COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/android-espresso.sh)
    set_tests_properties(engine-android-espresso PROPERTIES
      LABELS "tier-E2"
      SKIP_RETURN_CODE 77 TIMEOUT 1800 RUN_SERIAL TRUE)
  endif()
  # The mutation tier used to be RUN_SERIAL because it planted a deliberately
  # broken engine.cpp in the working tree and rebuilt the shared build/, so
  # nothing else could run while it did. It now has strategies: the sandbox
  # one copies the sources to a temp dir and touches nothing outside it, which
  # is what makes RUN_SERIAL unnecessary. The strategy is named here rather
  # than left to the environment so CI is deterministic; a person wanting the
  # faster in-tree path runs tools/frontend-mutations.py by hand, where the
  # exclusivity requirement is theirs to honour.
  add_test(NAME frontend-mutations
    COMMAND ${CMAKE_COMMAND} -E env HUNMIN_MUTATION_STRATEGY=sandbox
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/frontend-mutations.py)
  # Two tier labels, deliberately. tools/frontend-mutations.py plants each
  # mutation and requires something to catch it, and what catches it is
  # `ctest -R engine-d1-fcitx5` and `ctest -R engine-d2-ui` (its lines 334-338).
  # The falsifiability check therefore belongs to both tiers and to neither
  # alone; `-LE mutation` is how a person opts out of it.
  set_tests_properties(frontend-mutations PROPERTIES
    LABELS "tier-D1;tier-D2;mutation;slow"
    TIMEOUT 900)
  add_test(NAME schema-contract
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/doctor.py
            ${CMAKE_CURRENT_SOURCE_DIR}/libhunmin/data)
  # `guard` as well as tier-B: this is the data-lint gate, it costs ~0.3 s, and
  # it is one of the three tests that make up the commit-level check
  # (docs/TESTING.md §The three levels). The label selects what runs on every
  # commit; it adds no test and no build step.
  set_tests_properties(schema-contract PROPERTIES LABELS "tier-B;guard")
  # krdict-fixture: self-contained determinism check for tools/krdict2data.py.
  # The real 국립국어원 dump is 368 MB and gitignored (tools/fetch-krdict.sh),
  # so data-selfcheck cannot run the krdict converter in a fresh worktree; the
  # dump-dependent full run is a maintainer-only script in tools/, not a ctest
  # test. This test converts the committed fixture
  # tests/fixtures/krdict/01.xml into a temp dir and diffs it byte-identically
  # against the committed expected output — proving the converter's logic with
  # no dump present. Any diff means the converter or its expected output moved.
  add_test(NAME krdict-fixture
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/krdict-fixture-check.py)
  set_tests_properties(krdict-fixture PROPERTIES LABELS "tier-B")
  # docs-lint: mechanical check for figure-in-prose drift (test/trace counts,
  # makefile commands, wiki sources, index count). Runs against the build dir
  # so the ctest count check is meaningful.
  add_test(NAME docs-lint
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/docs-lint.py
            --build-dir ${CMAKE_CURRENT_BINARY_DIR})
  # tier-G, the tier docs/TESTING.md gives release gates and process. Two of
  # this test's own checks read what labels could have broken and do not:
  # check 1 compares `ctest -N`'s TOTAL, which labels leave unchanged, and
  # check 6 reads `add_test(NAME ...)` out of the source CMakeLists, which
  # set_tests_properties cannot disturb.
  # `guard`: one of the three commit-level tests. It is also where check 7
  # verifies that every mutation anchor still matches its source, which is what
  # keeps a rename from silently un-anchoring a mutation (2026-10-16).
  set_tests_properties(docs-lint PROPERTIES LABELS "tier-G;slow;guard")
  # The mode corpus tool's --selfcheck: proves the checks can fail. Runs on a
  # committed corpus when one is present, and skips (77) when it is not, so CI
  # -- which has no dump -- does not report a missing data file as a failure.
  # Deliberately only the check, not the build: building needs
  # mwparserfromhell, which CI does not install, and a tool that needs a
  # dependency CI lacks is a tool whose absence reads as green
  # (docs/MODES-DATA.md, "ctest에 등록되지 않은 이유").
  add_test(NAME modes-corpus-selfcheck
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/build-modes-corpus.py
            --selfcheck --out ${CMAKE_CURRENT_SOURCE_DIR}/tests/modes-corpus)
  set_tests_properties(modes-corpus-selfcheck PROPERTIES
    LABELS "tier-G"
    SKIP_RETURN_CODE 77)
  # Android 15+ on 16KB-page devices refuses an install whose native
  # libraries are not 16KB-aligned, and the failure is invisible on the build
  # machine (it compiles, links, packages, installs -- and only the device
  # complains). libhunjni.so shipped at the 4KB default for exactly that
  # reason: it is built by AGP's externalNativeBuild, which does not inherit
  # cmake/android.toolchain.cmake's linker flags. Skips (77) when no APK has
  # been built, so the inner loop stays green; run tools/build-android.sh +
  # ./gradlew assembleDebug first to make it real.
  add_test(NAME align-16k
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/check-align-16k.py)
  set_tests_properties(align-16k PROPERTIES
    LABELS "tier-E"
    SKIP_RETURN_CODE 77)
  if(HUNMIN_HAS_LIBHANGUL AND HUNMIN_SH)
    add_test(NAME goldens-reproducible
      COMMAND ${HUNMIN_SH} ${CMAKE_CURRENT_SOURCE_DIR}/tools/regen-goldens.sh
              ${CMAKE_CURRENT_BINARY_DIR}/goldens-check ${CMAKE_CURRENT_SOURCE_DIR} check)
    set_tests_properties(goldens-reproducible PROPERTIES LABELS "tier-A")
  endif()
endif()

if(BUILD_TESTING)
  # tier A: engine must reproduce every reference trace byte-exactly
  set(HUNMIN_STD_LAYOUTS dubeolsik dubeolsik-yet sebeolsik-3-2 sebeolsik-3-90
      sebeolsik-final sebeolsik-noshift sebeolsik-yet
      romaja dvorak colemak)
  foreach(l IN LISTS HUNMIN_STD_LAYOUTS)
    add_test(NAME engine-golden-${l}
      COMMAND hunmin-sim --data ${CMAKE_CURRENT_SOURCE_DIR}/libhunmin/data
              --check ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/ref-${l}.jsonl)
    set_tests_properties(engine-golden-${l} PROPERTIES LABELS "tier-A")
  endforeach()
  add_test(NAME engine-golden-sin3-p2
    COMMAND hunmin-sim --data ${CMAKE_CURRENT_SOURCE_DIR}/libhunmin/data
            --check ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/manual-sin3-p2.jsonl)
  set_tests_properties(engine-golden-sin3-p2 PROPERTIES LABELS "tier-A")
  add_test(NAME engine-golden-patim-sin3-p2
    COMMAND hunmin-sim --data ${CMAKE_CURRENT_SOURCE_DIR}/libhunmin/data
            --check ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/patim-sin3-p2-basic.jsonl)
  set_tests_properties(engine-golden-patim-sin3-p2 PROPERTIES LABELS "tier-A")
  add_test(NAME engine-golden-sin3-p2-yet
    COMMAND hunmin-sim --data ${CMAKE_CURRENT_SOURCE_DIR}/libhunmin/data
            --check ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/manual-sin3-p2-yet.jsonl)
  set_tests_properties(engine-golden-sin3-p2-yet PROPERTIES LABELS "tier-A")
  # 훈민정음 언해본 (옛한글), engine-measured keys via tools/type-hunminjeong.py
  # --golden-out. Coverage is 1/8 어절 byte-exact (나랏말씨미, 14 events); the
  # rest need the compose chain across a syllable boundary (the tool's TODO,
  # also in the golden's meta.coverage). Full provenance in the meta line.
  add_test(NAME engine-golden-hunminjeong-seosi
    COMMAND hunmin-sim --data ${CMAKE_CURRENT_SOURCE_DIR}/libhunmin/data
            --check ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/manual-hunminjeong-seosi.jsonl)
  set_tests_properties(engine-golden-hunminjeong-seosi PROPERTIES LABELS "tier-A")
  # P2 gate: one golden corpus per pat.im/1136 feature section (PLAN.md:85)
  set(HUNMIN_P2_SECTIONS p2-2-1-basic p2-2-2-tiers p2-2-3-chosung
      p2-2-4-1-yetrules p2-2-4-2-eung p2-2-4-3-caps p2-2-6-flags p2-2-6-flags-s
      p2-3-swapped)
  foreach(s IN LISTS HUNMIN_P2_SECTIONS)
    add_test(NAME engine-golden-${s}
      COMMAND hunmin-sim --data ${CMAKE_CURRENT_SOURCE_DIR}/libhunmin/data
              --check ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/${s}.jsonl)
    set_tests_properties(engine-golden-${s} PROPERTIES LABELS "tier-A")
  endforeach()
  if(HUNMIN_HAS_LIBHANGUL AND HUNMIN_SH)
    add_test(NAME parity-sweep
      COMMAND ${HUNMIN_SH} ${CMAKE_CURRENT_SOURCE_DIR}/tools/parity-sweep.sh
              ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} 25)
    set_tests_properties(parity-sweep PROPERTIES LABELS "tier-A")
  endif()
  # tier D3: stock distro fcitx5 + Xvfb full-stack (skips, rc 77, without Xvfb)
  #
  # D3 drives the *system* fcitx5, so its addon/pkgdata dirs must come from the
  # installed Fcitx5Core, not from our own prefix. FCITX_INSTALL_LIBDIR reflects
  # FCITX_INSTALL_USE_FCITX_SYS_PATHS, which Fcitx5UtilsConfig.cmake silently
  # turns off whenever CMAKE_INSTALL_PREFIX != /usr — with the default /usr/local
  # that yields /usr/local/lib/fcitx5, which does not exist on a distro box and
  # made this test skip with "fcitx5 system dirs not found".
  # pkg-config reports what is actually installed; prefix is irrelevant here.
  find_package(X11)
  # Ask pkg-config directly rather than pkg_check_modules: we only want two
  # path strings out of an already-installed package, and PkgConfig may be
  # absent on a build host that has Fcitx5 headers via cmake only.
  find_program(PKG_CONFIG_EXECUTABLE pkg-config)
  if(PKG_CONFIG_EXECUTABLE)
    execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=libdir Fcitx5Core
      OUTPUT_VARIABLE d3_libdir OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET RESULT_VARIABLE d3_rc)
    execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=prefix Fcitx5Core
      OUTPUT_VARIABLE d3_prefix OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()
  if(d3_rc EQUAL 0 AND d3_libdir)
    # FCITX_DATA_DIRS entries are used verbatim -- StandardPathsPrivate::
    # defaultPaths() (fcitx-utils/standardpath.cpp) appends packageName only to
    # the fallback, never to an environment value. So the pkgdata entry must
    # already be the fcitx5/ directory itself, not its parent: /usr/share/fcitx5
    # resolves to /usr/share/fcitx5/{addon,inputmethod}, whereas /usr/share
    # would resolve to /usr/share/{addon,inputmethod} and find nothing.
    set(d3_addon_dir "${d3_libdir}/fcitx5")
    set(d3_pkgdata_dir "${d3_prefix}/share/fcitx5")
  endif()
  if(X11_FOUND AND TARGET hunmin_addon
     AND EXISTS "${d3_addon_dir}" AND EXISTS "${d3_pkgdata_dir}")
    message(STATUS "D3: X11 + stock fcitx5 at ${d3_addon_dir}, ${d3_pkgdata_dir}")
    add_executable(xim-harness tests/e2e/xim-harness.c)
    # dkit, NOT X11::X11. The harness must speak the same XIM implementation as
    # fcitx5's XIM server, and that is libxcb-imdkit. Linking X11::X11 instead
    # resolves XOpenIM inside libX11's built-in XIM, which silently answers with
    # its own LOCAL IM (libX11 exports _XimLocalOpenIM and friends) -- the client
    # then decodes every key itself and no XIM traffic is ever generated.
    # docs/D3-ROOT-CAUSE.md has the measurements; this comment is the guard
    # against reintroducing it.
    #
    # There is no pkg-config for libxcb-imdkit on Debian/Ubuntu, and the dev
    # headers are not shipped, so the library is located directly. The harness
    # declares the small API slice it uses rather than including the missing
    # headers; see the comment block at the top of xim-harness.c.
    # Only the SONAME is installed (libxcb-imdkit.so.1); there is no .so
    # development symlink, so find_library cannot find it by name. Locate the
    # versioned file and hand the linker the full path, which GNU ld accepts.
    find_file(D3_IMDKIT_SO NAMES libxcb-imdkit.so.1 libxcb-imdkit.so
      HINTS ${d3_addon_dir}
            /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE} /usr/lib /usr/local/lib
      REQUIRED)
    find_file(D3_XCB_SO NAMES libxcb.so.1 libxcb.so
      HINTS /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE} /usr/lib /usr/local/lib
      REQUIRED)
    # xkbcommon resolves a forwarded keycode to the character the application
    # would receive. Without it the harness can only report "something came
    # back", which cannot distinguish a space from any other pass-through key.
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(D3_XKB xkbcommon)
    endif()
    target_link_libraries(xim-harness PRIVATE
      ${D3_IMDKIT_SO} ${D3_XCB_SO})
    if(D3_XKB_FOUND)
      target_include_directories(xim-harness PRIVATE ${D3_XKB_INCLUDE_DIRS})
      target_link_libraries(xim-harness PRIVATE ${D3_XKB_LIBRARIES})
      target_link_directories(xim-harness PRIVATE ${D3_XKB_LIBRARY_DIRS})
    else()
      message(WARNING "D3: xkbcommon not found; the XIM harness cannot resolve "
                      "forwarded keys and D3 will not observe pass-through keys")
    endif()
    message(STATUS "D3: XIM harness links ${D3_IMDKIT_SO}")
    add_test(NAME engine-d3-x11
      COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/run-d3.sh
              $<TARGET_FILE:hunmin-sim>
              ${CMAKE_CURRENT_SOURCE_DIR}/libhunmin/data
              $<TARGET_FILE:xim-harness>
              ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/xtest-type.py
              ${CMAKE_BINARY_DIR}/fcitx5-addon
              ${CMAKE_CURRENT_BINARY_DIR}/frontends/fcitx5/src/hunmin.conf
              ${CMAKE_CURRENT_BINARY_DIR}/frontends/fcitx5/src/hunmin-addon.conf
              ${CMAKE_CURRENT_SOURCE_DIR}
              ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/cases.tsv
              ${d3_addon_dir}
              ${d3_pkgdata_dir})
    set_tests_properties(engine-d3-x11 PROPERTIES
      LABELS "tier-D3;slow"
      SKIP_RETURN_CODE 77 TIMEOUT 300)

    # D3-GTK: the same stack, the same staged addon and the same case file,
    # with the fcitx5 GTK input method module in place of the XIM client. It
    # answers a question D3/XIM structurally cannot: XIM re-synthesises a key
    # event for a key the engine declined (xim.cpp:664), while a GTK
    # application never speaks XIM at all. Measured 2026-10-09: the declined
    # space reaches the GtkEntry buffer on both GTK 3 and GTK 4, by a
    # different mechanism -- see
    # wiki/raw/measurements/2026-10-09-gtk-forward-path.md.
    #
    # Two tests, not one, because fcitx5-frontend-gtk3 and -gtk4 are separate
    # packages with separate input method modules and they are not the same
    # code: gtk4/fcitximcontext.cpp implements forward_key as an EMPTY
    # function where gtk3's re-queues the event. Whatever carries the space
    # there is not forward_key, and one green test over both would have hidden
    # that.
    #
    # Skips rc 77 when the GTK runtime, the fcitx5 IM module or (for GTK 4)
    # xdotool is missing; none of them need sudo to install and this host has
    # no sudo. run-d3.sh does the probing.
    foreach(gtkver 3 4)
      add_test(NAME engine-d3-gtk${gtkver}
        COMMAND ${CMAKE_COMMAND} -E env HUNMIN_D3_CLIENT=gtk
                HUNMIN_GTK=${gtkver}.0
                bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/run-d3.sh
                $<TARGET_FILE:hunmin-sim>
                ${CMAKE_CURRENT_SOURCE_DIR}/libhunmin/data
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/gtk-harness.py
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/xtest-type.py
                ${CMAKE_BINARY_DIR}/fcitx5-addon
                ${CMAKE_BINARY_DIR}/frontends/fcitx5/src/hunmin.conf
                ${CMAKE_BINARY_DIR}/frontends/fcitx5/src/hunmin-addon.conf
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/cases.tsv
                ${d3_addon_dir}
                ${d3_pkgdata_dir})
      set_tests_properties(engine-d3-gtk${gtkver} PROPERTIES
        LABELS "tier-D3;slow"
        SKIP_RETURN_CODE 77 TIMEOUT 300)
    endforeach()
    # D3-CANDIDATE: the F9 candidate window, drawn for real (issue #53).
    # The same stack as engine-d3-x11 with ONE difference: classicui is
    # ENABLED, so the candidate window is actually drawn and the #45 abort
    # -- classicui calling label() past the CommonCandidateList's 10-label
    # vector -- is reachable. Every other tier goes green with the cap
    # reverted; this is the one that must not. It reads
    # tests/e2e/cases-candidate.tsv, whose expectations are hand-written
    # because the sim cannot type {F9}.
    #
    # Skips rc 77 exactly like the other D3 variants: same X11/fcitx5
    # probing inside run-d3.sh, same DISPLAY reservation, so it cannot
    # collide with a concurrent engine-d3-x11 run.
    add_test(NAME engine-d3-candidate
      COMMAND ${CMAKE_COMMAND} -E env HUNMIN_D3_CLIENT=candidate
              bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/run-d3.sh
              $<TARGET_FILE:hunmin-sim>
              ${CMAKE_CURRENT_SOURCE_DIR}/libhunmin/data
              $<TARGET_FILE:xim-harness>
              ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/xtest-type.py
              ${CMAKE_BINARY_DIR}/fcitx5-addon
              ${CMAKE_BINARY_DIR}/frontends/fcitx5/src/hunmin.conf
              ${CMAKE_BINARY_DIR}/frontends/fcitx5/src/hunmin-addon.conf
              ${CMAKE_CURRENT_SOURCE_DIR}
              ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/cases-candidate.tsv
              ${d3_addon_dir}
              ${d3_pkgdata_dir})
    set_tests_properties(engine-d3-candidate PROPERTIES
      LABELS "tier-D3;slow"
      SKIP_RETURN_CODE 77 TIMEOUT 300)
  else()
    message(STATUS "D3: skipped (X11=${X11_FOUND}, pkg-config=${PKG_CONFIG_EXECUTABLE})")
  endif()
  # Tier C: platform-native trace replayed through the real fcitx5 adapter,
  # diffed against the expectation tier A implies (docs/trace-spec.md).
  # Registered only where the emitter exists, so a host that cannot build it
  # reports a skip rather than a red. The corpus is generated from tier A
  # goldens by tools/gen-conformance.py -- never hand-typed.
  if(TARGET trace-emit)
    # GLOB, not GLOB_RECURSE, but note: a ';' inside a filename is a CMake list
    # separator the moment it enters a variable, so a case label containing one
    # (a tier id like "tier1-j;") splits into two entries and add_test() then
    # fails on the fragments. Listing the files one at a time keeps each path
    # intact -- the sanitised name below only affects the test NAME.
    file(GLOB HUNMIN_CONFORMANCE_TRACES
         "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/x11/*.trace.in")

    # A ';' in a filename is a CMake list separator from the moment GLOB stores
    # it, so a case label containing one ("tier1-j;" -- a key that is literally
    # part of a tier id) splits into two paths and add_test() fails on the
    # fragments. Escaping the ';' at read time keeps the path intact; the test
    # NAME is then sanitised separately so it too cannot contain a separator.
    set(HUNMIN_CONFORMANCE_SAFE "")
    foreach(t IN LISTS HUNMIN_CONFORMANCE_TRACES)
      string(REPLACE ";" "\;" t_esc "${t}")
      list(APPEND HUNMIN_CONFORMANCE_SAFE "${t_esc}")
    endforeach()

    foreach(t IN LISTS HUNMIN_CONFORMANCE_SAFE)
      string(REPLACE "\;" ";" t_real "${t}")
      file(RELATIVE_PATH rel "${CMAKE_CURRENT_SOURCE_DIR}" "${t_real}")
      string(REGEX REPLACE "[^A-Za-z0-9._-]" "_" name "${rel}")
      add_test(NAME conformance-${name}
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tools/trace-check.py "${t_real}"
                --runner $<TARGET_FILE:trace-emit>)
      set_tests_properties(conformance-${name} PROPERTIES
        LABELS "tier-C"
        SKIP_RETURN_CODE 77 TIMEOUT 120)
    endforeach()
  else()
    message(STATUS "Tier C: skipped (build with -DHUNMIN_TRACE_BUILD=ON)")
  endif()
  # Tier C for Android: same corpus, Android dialect, runner is a shell script.
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/android-trace-emit.sh")
    file(GLOB HUNMIN_ANDROID_CONFORMANCE_TRACES
         "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/android/*.trace.in")
    if(HUNMIN_ANDROID_CONFORMANCE_TRACES)
      set(HUNMIN_ANDROID_CONFORMANCE_SAFE "")
      foreach(t IN LISTS HUNMIN_ANDROID_CONFORMANCE_TRACES)
        string(REPLACE ";" "\;" t_esc "${t}")
        list(APPEND HUNMIN_ANDROID_CONFORMANCE_SAFE "${t_esc}")
      endforeach()
      foreach(t IN LISTS HUNMIN_ANDROID_CONFORMANCE_SAFE)
        string(REPLACE "\;" ";" t_real "${t}")
        file(RELATIVE_PATH rel "${CMAKE_CURRENT_SOURCE_DIR}" "${t_real}")
        string(REGEX REPLACE "[^A-Za-z0-9._-]" "_" name "${rel}")
        add_test(NAME engine-android-conformance-${name}
          COMMAND ${Python3_EXECUTABLE}
                  ${CMAKE_CURRENT_SOURCE_DIR}/tools/trace-check.py "${t_real}"
                  --runner ${CMAKE_CURRENT_SOURCE_DIR}/tools/android-trace-emit.sh)
        set_tests_properties(engine-android-conformance-${name} PROPERTIES
          LABELS "tier-C"
          SKIP_RETURN_CODE 77 TIMEOUT 120)
      endforeach()
    else()
      message(STATUS "Tier C Android: no trace files found")
    endif()
  else()
    message(STATUS "Tier C Android: skipped (tools/android-trace-emit.sh not found)")
  endif()
  # MEASURED 2026-10-14, issue #59, and the block that used to be here is
  # REMOVED as of the same date. It registered the shift-arrow corpus a second
  # time, under a hand-written name that duplicated the glob-derived
  # registration above -- a bridge left over from when the corpus lived in a
  # nested worktree that is gone. `ctest -N` reported 16 Android tier-C tests
  # against the 15 files under tests/conformance/android/; with the block
  # removed it reports 15 against 15, so the two commands agree again. The
  # corpus file is still registered, by the glob, under its derived name, which
  # is what docs/REMAINING-WORK.md's TC-SHIFTARROW row names as the tree fact.
  #
  # Why the derived name rather than the readable one is the survivor: test
  # names here are machine-generated from the corpus filename precisely so a
  # renamed case cannot silently break a saved -R expression (docs/TESTING.md
  # "Why labels rather than a -R regex over names"). Re-introducing a
  # hand-written alias for one corpus file would make that set of names
  # non-uniform for the one file it was written for.
  if(NOT WIN32)
    add_test(NAME abi
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tools/abi-check.sh
              $<TARGET_FILE:hunmin> ${CMAKE_CURRENT_SOURCE_DIR}/libhunmin/tests/abi/symbols.txt)
    set_tests_properties(abi PROPERTIES LABELS "tier-B;guard")
  endif()
endif()
