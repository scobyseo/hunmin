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

add_library(hunmin_ibus_engine STATIC src/hunmin_engine.c)
target_link_libraries(hunmin_ibus_engine PUBLIC hunmin_ibus_state PkgConfig::IBUS)
# A dev package unpacked under a prefix (no root needed) supplies the headers;
# the runtime library is still the distribution's. docs/ENVIRONMENT.md records
# the host this is measured on.
if(HUNMIN_IBUS_PREFIX)
  target_include_directories(hunmin_ibus_engine BEFORE PUBLIC ${HUNMIN_IBUS_PREFIX}/include/ibus-1.0)
endif()

add_executable(hunmin-ibus src/main.c)
target_link_libraries(hunmin-ibus PRIVATE hunmin_ibus_engine)
install(TARGETS hunmin-ibus RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ibus)

# The component descriptor is how the daemon learns the engine exists; IBus
# 1.5.26 has no per-user component directory (src/ibusregistry.c), so an
# out-of-tree run points IBUS_COMPONENT_PATH at this file's directory.
set(HUNMIN_IBUS_EXEC ${CMAKE_INSTALL_FULL_BINDIR}/hunmin-ibus)
set(HUNMIN_IBUS_VERSION ${PROJECT_VERSION})
configure_file(component/hunmin.xml.in ${CMAKE_CURRENT_BINARY_DIR}/hunmin.xml @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/hunmin.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/ibus/component COMPONENT ibus)

if(BUILD_TESTING AND TARGET GTest::gtest_main)
  add_subdirectory(test)
endif()