# Tier D1 and D2 for the IBus frontend (issue #288). Plain add_test +
# set_tests_properties, matching frontends/fcitx5/test: hunmin_add_test is
# defined by cmake/HunminTestHelpers.cmake, which the root includes AFTER the
# frontend directories, so it does not exist yet here.
#
# D1 links hunmin_ibus_state only: no ibus, no bus, no daemon. D2 links the
# engine object and captures what it publishes on IBus's own D-Bus interface
# over a private session bus (GTestDBus), which is the analog of fcitx5's
# InputPanel assertions.
# The settings store is read through GSettings. Tests use the memory backend:
# a dconf-backed store cannot be isolated from a test by environment — a write
# under dbus-run-session with a redirected XDG_CONFIG_HOME lands in the
# developer's own ~/.config/dconf/user (measured, docs/OPTIONS-MEASURED.md
# §IBC-5) — so the round-trip lives in one process and the persistence case
# belongs to CI.
set(HUNMIN_IBUS_TEST_ENV "HUNMIN_DATA_PATH=${CMAKE_SOURCE_DIR}/libhunmin/data")
if(GLIB_COMPILE_SCHEMAS)
  list(APPEND HUNMIN_IBUS_TEST_ENV
       "GSETTINGS_BACKEND=memory"
       "GSETTINGS_SCHEMA_DIR=${HUNMIN_IBUS_SCHEMA_DIR}")
endif()

add_executable(testhunminibusstate test_state.cpp)
target_link_libraries(testhunminibusstate PRIVATE GTest::gtest_main hunmin_ibus_state)
add_test(NAME engine-d1-ibus COMMAND testhunminibusstate)
set_tests_properties(engine-d1-ibus PROPERTIES LABELS "tier-D1"
                     ENVIRONMENT "${HUNMIN_IBUS_TEST_ENV}")

add_executable(testhunminibusui test_engine_ui.cpp)
target_link_libraries(testhunminibusui PRIVATE GTest::gtest_main hunmin_ibus_engine)
add_test(NAME engine-d2-ibus-ui COMMAND testhunminibusui)
set_tests_properties(engine-d2-ibus-ui PROPERTIES LABELS "tier-D2"
                     ENVIRONMENT "${HUNMIN_IBUS_TEST_ENV}")

# §FE17: the store and the fifth row's grammar. Links the engine target (the
# reader is engine-side code, not the host-independent state object) but needs no
# bus, no daemon and no display.
if(GLIB_COMPILE_SCHEMAS)
  add_executable(testhunminibussettings test_settings.cpp)
  target_link_libraries(testhunminibussettings PRIVATE GTest::gtest_main hunmin_ibus_engine)
  add_dependencies(testhunminibussettings hunmin-ibus-schema)
  add_test(NAME engine-d1-ibus-settings COMMAND testhunminibussettings)
  set_tests_properties(engine-d1-ibus-settings PROPERTIES LABELS "tier-D1"
                       ENVIRONMENT "${HUNMIN_IBUS_TEST_ENV}")
endif()

# §FE17's persistence claim is about two PROCESSES (the window writes, a later
# engine start reads), so this case drives the built binary instead of a GSettings
# object in its own memory. The keyfile backend makes the store a file in a
# scratch directory: a dconf store cannot be isolated from a test by environment
# (docs/OPTIONS-MEASURED.md §IBC-5), and a test that edits a person's input
# method settings is not one anybody can run.
if(GLIB_COMPILE_SCHEMAS AND TARGET hunmin-ibus-settings)
  add_executable(testhunminibussettingspersist test_settings_persist.cpp)
  target_link_libraries(testhunminibussettingspersist PRIVATE GTest::gtest_main PkgConfig::GIO)
  target_compile_definitions(testhunminibussettingspersist PRIVATE
      HUNMIN_SETTINGS_BIN="$<TARGET_FILE:hunmin-ibus-settings>"
      HUNMIN_GSETTINGS_BIN="${GSETTINGS_EXEC}")
  add_dependencies(testhunminibussettingspersist hunmin-ibus-settings)
  add_test(NAME engine-d1-ibus-settings-persist COMMAND testhunminibussettingspersist)
  set_tests_properties(engine-d1-ibus-settings-persist PROPERTIES LABELS "tier-D1"
                       ENVIRONMENT "${HUNMIN_IBUS_TEST_ENV}")
endif()

# The window's widgets (tier D2). Presentation is not asserted for its pixels but
# for what it OFFERS and WRITES: which layouts, which commit units, whether a
# capture appends once and refuses what cannot fire, and that a removal reaches
# the store. Registered only where a display can be had — xvfb-run if present,
# otherwise the configure-time DISPLAY — because GTK has no headless mode and a
# red that only means "no display" is a red nobody reads.
if(GLIB_COMPILE_SCHEMAS AND TARGET hunmin-ibus-settings)
  find_program(XVFB_RUN xvfb-run)
  if(XVFB_RUN OR DEFINED ENV{DISPLAY})
    add_executable(testhunminibussettingsui test_settings_ui.cpp)
    target_link_libraries(testhunminibussettingsui PRIVATE GTest::gtest_main
        hunmin_ibus_settings_ui)
    add_dependencies(testhunminibussettingsui hunmin-ibus-settings)
    if(XVFB_RUN)
      add_test(NAME engine-d2-ibus-settings-ui
               COMMAND ${XVFB_RUN} -a $<TARGET_FILE:testhunminibussettingsui>)
    else()
      add_test(NAME engine-d2-ibus-settings-ui COMMAND testhunminibussettingsui)
    endif()
    set_tests_properties(engine-d2-ibus-settings-ui PROPERTIES LABELS "tier-D2"
                         ENVIRONMENT "${HUNMIN_IBUS_TEST_ENV};GDK_BACKEND=x11")
  endif()
endif()

# FE10's corpus lane for this frontend: the same tests/golden/judgement-r8.jsonl
# the fcitx5 and Android lanes replay. cJSON is the corpus parser, already
# vendored for hunmin-sim.
add_executable(testhunminibusjudgement test_judgement.cpp
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c)
target_include_directories(testhunminibusjudgement PRIVATE ${CMAKE_SOURCE_DIR}/third_party/cJSON)
target_link_libraries(testhunminibusjudgement PRIVATE GTest::gtest_main hunmin_ibus_state)
add_test(NAME engine-d1-ibus-judgement COMMAND testhunminibusjudgement)
set_tests_properties(engine-d1-ibus-judgement PROPERTIES LABELS "tier-D1"
                     ENVIRONMENT "${HUNMIN_IBUS_TEST_ENV};HUNMIN_JUDGEMENT_CORPUS=${CMAKE_SOURCE_DIR}/tests/golden/judgement-r8.jsonl")

# Tier C (issue #316): the SAME x11 corpus the fcitx5 lane replays, through
# this frontend's own translation + key path. The .gold files are shared --
# one expectation, two frontends -- and tools/trace-check.py derives a
# word-buffered case's deliveries from hunmin-sim, not from a second copy.
# Off by default, like the fcitx5 emitter: a packaged build carries no trace
# reader.
option(HUNMIN_TRACE_BUILD "Build the tier-C trace emitter" OFF)
if(HUNMIN_TRACE_BUILD)
  add_executable(ibus-trace-emit trace-emit.cpp
      ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c)
  target_include_directories(ibus-trace-emit PRIVATE ${CMAKE_SOURCE_DIR}/third_party/cJSON)
  target_link_libraries(ibus-trace-emit PRIVATE hunmin_ibus_engine)
endif()