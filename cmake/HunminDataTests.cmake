# Data and ABI contracts (tier-B). Rationale: docs/DESIGN-BUILD.md.
if(BUILD_TESTING AND Python3_FOUND)
  hunmin_add_test(NAME schema-contract COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/doctor.py ${CMAKE_CURRENT_SOURCE_DIR}/libhunmin/data LABELS tier-B guard)
  hunmin_add_test(NAME data-split COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check-data-split.py --build-dir ${CMAKE_CURRENT_BINARY_DIR} --cmake ${CMAKE_COMMAND} --sim $<TARGET_FILE:hunmin-sim> --config "$<CONFIG>" LABELS tier-B)
  hunmin_add_test(NAME krdict-fixture COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/krdict-fixture-check.py LABELS tier-B)
  # App-read Android keypad symbol data (issue #297, KP11): a frontend data
  # contract, deliberately NOT part of doctor.py -- the engine never loads
  # this file, so its contract lives beside the other data contracts but
  # checks frontends/android/app/src/main/assets/keypad/symbols.json.
  hunmin_add_test(NAME keypad-symbol-data COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/symbol-data-check.py LABELS tier-B guard)
  hunmin_add_test(NAME host-guards COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/host-guard-check.py LABELS tier-B SKIP_RETURN_CODE 77)
endif()
if(BUILD_TESTING AND NOT WIN32)
  hunmin_add_test(NAME abi COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tools/abi-check.sh $<TARGET_FILE:hunmin> ${CMAKE_CURRENT_SOURCE_DIR}/libhunmin/tests/abi/symbols.txt LABELS tier-B guard)
endif()
