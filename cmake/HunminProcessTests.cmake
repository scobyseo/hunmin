# Release gates and process (tier-G + guard). Rationale: docs/DESIGN-BUILD.md.
if(NOT BUILD_TESTING OR NOT Python3_FOUND)
  return()
endif()
hunmin_add_test(NAME builds COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} -j 4 LABELS guard TIMEOUT 600)
add_test(NAME builds-fcitx5-addon
  COMMAND ${HUNMIN_SH} -c
    "if cmake --build ${CMAKE_BINARY_DIR} --target help 2>/dev/null | grep -q hunmin_addon; then
       cmake --build ${CMAKE_BINARY_DIR} --target hunmin_addon;
     else
       echo 'builds-fcitx5-addon: SKIPPED -- no hunmin_addon target, which means Fcitx5Core >= 5.1.7 was not found when this tree was configured (CMakeLists.txt printed \"Fcitx5Core not found: fcitx5 addon disabled\"). The addon was NOT compiled and NOT checked here; the tier-C/D rows that need it did not run either.';
       exit 77;
     fi")
set_tests_properties(builds-fcitx5-addon PROPERTIES LABELS "guard" SKIP_RETURN_CODE 77 TIMEOUT 600)
file(GLOB_RECURSE HUNMIN_SHELL_SCRIPTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/*.sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/*.sh)
find_program(SHELLCHECK_EXECUTABLE shellcheck)
if(HUNMIN_SHELL_SCRIPTS AND SHELLCHECK_EXECUTABLE)
  hunmin_add_test(NAME shell-lint COMMAND ${SHELLCHECK_EXECUTABLE} -S error ${HUNMIN_SHELL_SCRIPTS} LABELS guard TIMEOUT 300)
else()
  hunmin_add_test(NAME shell-lint COMMAND ${CMAKE_COMMAND} -E echo "shell-lint did not run: shellcheck not found, or no shell scripts matched. An absent linter is not a clean tree (the same rule the pyright assertion row follows)." LABELS guard TIMEOUT 300)
endif()
hunmin_add_test(NAME docs-lint COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/docs-lint.py --build-dir ${CMAKE_CURRENT_BINARY_DIR} LABELS tier-G slow guard)
hunmin_add_test(NAME req-map COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/req-map-check.py --build-dir ${CMAKE_CURRENT_BINARY_DIR} LABELS tier-G)
hunmin_add_test(NAME modes-corpus-selfcheck COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/build-modes-corpus.py --selfcheck --out ${CMAKE_CURRENT_SOURCE_DIR}/tests/modes-corpus LABELS tier-G SKIP_RETURN_CODE 77)
hunmin_add_test(NAME modes-metrics-selfcheck COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/modes-metrics.py --selfcheck --lib $<TARGET_FILE:hunmin> --sim $<TARGET_FILE:hunmin-sim> --keytable ${CMAKE_CURRENT_BINARY_DIR}/sin3-p2.keytable.json LABELS tier-G)
# G1/G2 are graded from a metrics report by autoswitch-gates.py; the checker's
# own guards need no corpus and no built engine (issue #291).
hunmin_add_test(NAME modes-gates-selfcheck COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/autoswitch-gates.py --selfcheck LABELS tier-G)
# The stem+particle probe (docs/MODES-MEASUREMENT.md §1.5): its tokeniser must
# keep splitting IBM은 and refusing PC방, and needs no dump or engine to say so.
hunmin_add_test(NAME modes-mixed-tokens-selfcheck COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/modes-mixed-tokens.py --selfcheck LABELS tier-G)
# Track 4 prefix-match measurement guard (issue #300): fixture aggregates,
# greedy-vs-optimal gap, negative control. No corpus, no engine needed.
hunmin_add_test(NAME hanja-prefix-selfcheck COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/measure-hanja-prefix.py --selfcheck LABELS tier-G)
# The nightly fuzz feed must stay inside hunmin-sim's --keys grammar. A bare
# backslash used to abort the replay with an error-path leak (issue #309).
hunmin_add_test(NAME fuzz-keys-selfcheck COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/fuzz-generate-keys.py --selfcheck --sim $<TARGET_FILE:hunmin-sim> --layout sin3-p2 LABELS tier-G)
