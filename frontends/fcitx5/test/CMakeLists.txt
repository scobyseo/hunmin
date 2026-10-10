configure_file(testdir.h.in ${CMAKE_CURRENT_BINARY_DIR}/testdir.h @ONLY)
add_subdirectory(addon)
add_subdirectory(inputmethod)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_executable(testhunmin testhunmin.cpp)
target_link_libraries(testhunmin Fcitx5::Core Fcitx5::Module::TestFrontend)
add_dependencies(testhunmin copy-addon copy-im)
add_test(NAME engine-d1-fcitx5 COMMAND testhunmin)
# LABELS is a test property, not an add_test() keyword -- see the note at
# the top of the root CMakeLists.txt.
set_tests_properties(engine-d1-fcitx5 PROPERTIES LABELS "tier-D1")

# Tier D1: ModeSwitchAdapter unit tests (R8 adapter).
add_executable(testmodeswitch testmodeswitch.cpp)
target_link_libraries(testmodeswitch hunmin Fcitx5::Core)
target_include_directories(testmodeswitch PRIVATE ${CMAKE_SOURCE_DIR}/frontends/fcitx5/src)
add_dependencies(testmodeswitch copy-addon)
# Link modeswitch.cpp directly since it's not part of the hunmin library
target_sources(testmodeswitch PRIVATE ${CMAKE_SOURCE_DIR}/frontends/fcitx5/src/modeswitch.cpp)
add_test(NAME engine-d1-modeswitch COMMAND testmodeswitch
         ENV HUNMIN_DATA_PATH=${CMAKE_SOURCE_DIR}/libhunmin/data)
set_tests_properties(engine-d1-modeswitch PROPERTIES LABELS "tier-D1")

# Tier D2: assert the InputPanel preedit the adapter publishes (text,
# clearing, cursor, and both capability branches). See docs/TESTING.md D2.
add_executable(testhunminui testhunminui.cpp)
target_link_libraries(testhunminui Fcitx5::Core Fcitx5::Module::TestFrontend)
add_dependencies(testhunminui copy-addon copy-im)
add_test(NAME engine-d2-ui COMMAND testhunminui)
set_tests_properties(engine-d2-ui PROPERTIES LABELS "tier-D2")

# Tier C: replay a platform-native trace (docs/trace-spec.md) through the real
# adapter. Off by default so a packaged build does not carry a trace reader; the
# tier C gate turns it on.
option(HUNMIN_TRACE_BUILD "Build the tier-C trace emitter" OFF)
if(HUNMIN_TRACE_BUILD)
  add_executable(trace-emit trace-emit.cpp)
  target_link_libraries(trace-emit Fcitx5::Core Fcitx5::Module::TestFrontend)
  target_include_directories(trace-emit PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  add_dependencies(trace-emit copy-addon copy-im)
endif()
