# IBus frontend — event mapping and presentation only (AGENTS.md rule 4).
# Built from the root project when ibus-1.0 >= 1.5.26 is found; the floor is
# Ubuntu 22.04's own package (the release that ships this frontend), and the
# same sources build against 26.04's 1.5.34 — the two headers were diffed for
# this: only ibus_engine_send_message and
# ibus_bus_set_global_shortcut_keys_async differ, and neither is used here.
#
# The key path is src/hunmin_state.c, which includes no IBus header: that split
# is what lets tier D1 drive the frontend with a fake sink (issue #288).

add_library(hunmin_ibus_state STATIC src/hunmin_state.c)
target_include_directories(hunmin_ibus_state PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(hunmin_ibus_state PUBLIC hunmin)
set_target_properties(hunmin_ibus_state PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

# The store's plumbing (schema, grammar, the daemon's trigger list) is its own
# small library: the engine reads it and the settings window writes it, and
# neither of them is where that plumbing belongs. It is deliberately outside
# src/, which is what tools/check-frontend-invariants.py measures as the ADAPTER.
add_library(hunmin_ibus_store STATIC store/store.c)
target_include_directories(hunmin_ibus_store PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/store
                                                  ${CMAKE_CURRENT_SOURCE_DIR}/src)
pkg_check_modules(GIO QUIET IMPORTED_TARGET GLOBAL gio-2.0)
target_link_libraries(hunmin_ibus_store PUBLIC PkgConfig::IBUS PkgConfig::GIO)

add_library(hunmin_ibus_engine STATIC src/hunmin_engine.c src/settings.c)
target_link_libraries(hunmin_ibus_engine PUBLIC hunmin_ibus_state hunmin_ibus_store PkgConfig::IBUS)

# The five stored rows (docs/SPEC-FRONTEND.md §FE17). The schema is compiled into
# the build tree so a test can read its defaults with no install, and installed to
# the same place every other GSettings schema lives. glib-compile-schemas ships
# with libglib2.0-bin, which gio-2.0 already requires here; a host without it gets
# a warning rather than a configure failure, and the tests that need the schema
# are then skipped by name rather than silently passing.
find_program(GLIB_COMPILE_SCHEMAS glib-compile-schemas)
set(HUNMIN_IBUS_SCHEMA_SRC ${CMAKE_CURRENT_SOURCE_DIR}/schema/org.freedesktop.ibus.engine.hunmin.gschema.xml)
set(HUNMIN_IBUS_SCHEMA_DIR ${CMAKE_CURRENT_BINARY_DIR}/glib-2.0/schemas)
if(GLIB_COMPILE_SCHEMAS)
  add_custom_command(
    OUTPUT ${HUNMIN_IBUS_SCHEMA_DIR}/gschemas.compiled
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HUNMIN_IBUS_SCHEMA_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${HUNMIN_IBUS_SCHEMA_SRC} ${HUNMIN_IBUS_SCHEMA_DIR}/
    COMMAND ${GLIB_COMPILE_SCHEMAS} ${HUNMIN_IBUS_SCHEMA_DIR}
    DEPENDS ${HUNMIN_IBUS_SCHEMA_SRC}
    COMMENT "Compiling the hunmin IBus GSettings schema")
  add_custom_target(hunmin-ibus-schema ALL DEPENDS ${HUNMIN_IBUS_SCHEMA_DIR}/gschemas.compiled)
  add_dependencies(hunmin_ibus_engine hunmin-ibus-schema)
  install(FILES ${HUNMIN_IBUS_SCHEMA_SRC}
          DESTINATION ${CMAKE_INSTALL_DATADIR}/glib-2.0/schemas COMPONENT ibus)
else()
  message(WARNING "glib-compile-schemas not found: the IBus settings store is not built "
                  "(the engine then runs the layer's own defaults)")
endif()
# T2-1's personal key log (docs/trace-spec.md), compiled out of the shipped
# engine by default: no sink and no getenv on the key path unless opted in.
# The same option name the fcitx5 and TSF frontends use.
option(HUNMIN_TRACE "Build the opt-in personal key-log sink" OFF)
if(HUNMIN_TRACE)
  # trace/ is opt-in measurement infrastructure: it carries the T2-1 sink and
  # is not part of the shipped adapter's 1000-line budget (the invariant check
  # counts only the top-level src/*.c and src/*.h files).
  target_sources(hunmin_ibus_engine PRIVATE src/trace/trace.c)
  target_include_directories(hunmin_ibus_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/trace)
  target_compile_definitions(hunmin_ibus_engine PRIVATE HUNMIN_TRACE=1)
endif()
# A dev package unpacked under a prefix (no root needed) supplies the headers;
# the runtime library is still the distribution's. docs/ENVIRONMENT.md records
# the host this is measured on.
if(HUNMIN_IBUS_PREFIX)
  target_include_directories(hunmin_ibus_engine BEFORE PUBLIC ${HUNMIN_IBUS_PREFIX}/include/ibus-1.0)
endif()

add_executable(hunmin-ibus src/main.c)
target_link_libraries(hunmin-ibus PRIVATE hunmin_ibus_engine)
install(TARGETS hunmin-ibus RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ibus)

# The settings window (issue #327). A separate process because presentation is
# not what FE11's budget measures, and because the IBus panel launches it from
# Preferences through the component XML's <setup> — the hook ibus-hangul uses.
# Optional: a host without GTK3 headers builds the engine and skips the window,
# exactly as a host without libibus skips this whole directory.
pkg_check_modules(GTK3 QUIET IMPORTED_TARGET GLOBAL gtk+-3.0)
if(GTK3_FOUND AND GLIB_COMPILE_SCHEMAS)
  add_library(hunmin_ibus_settings_ui STATIC settings/ui.c)
  target_include_directories(hunmin_ibus_settings_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/settings)
  target_link_libraries(hunmin_ibus_settings_ui PUBLIC hunmin_ibus_engine PkgConfig::GTK3)

  add_executable(hunmin-ibus-settings settings/main.c)
  target_link_libraries(hunmin-ibus-settings PRIVATE hunmin_ibus_settings_ui)
  add_dependencies(hunmin-ibus-settings hunmin-ibus-schema)
  install(TARGETS hunmin-ibus-settings RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
          COMPONENT ibus-settings)
else()
  message(STATUS "gtk+-3.0 not found: the IBus settings window is not built "
                 "(the store and the engine-side reader still are)")
endif()

# The component descriptor is how the daemon learns the engine exists; IBus
# 1.5.26 has no per-user component directory (src/ibusregistry.c), so an
# out-of-tree run points IBUS_COMPONENT_PATH at this file's directory.
set(HUNMIN_IBUS_EXEC ${CMAKE_INSTALL_FULL_BINDIR}/hunmin-ibus)
set(HUNMIN_IBUS_SETTINGS_EXEC ${CMAKE_INSTALL_FULL_BINDIR}/hunmin-ibus-settings)
set(HUNMIN_IBUS_VERSION ${PROJECT_VERSION})
configure_file(component/hunmin.xml.in ${CMAKE_CURRENT_BINARY_DIR}/hunmin.xml @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/hunmin.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/ibus/component COMPONENT ibus)

# gsettings(1) is the writer the persistence case drives; libglib2.0-bin ships it.
find_program(GSETTINGS_EXEC gsettings)

if(BUILD_TESTING AND TARGET GTest::gtest_main)
  add_subdirectory(test)
endif()