# --- the test-only addons have to be reachable from the BUILD tree ------------
# Fcitx5's setupTestingEnvironmentPath() REPLACES the addon and data directories,
# so the runners read descriptors from <this dir>/addon/*.conf and dlopen the
# module libraries from <build>/fcitx5-addon/. A source install into /usr/local
# happens to satisfy that from the standard locations; a prefix install does not,
# and the runners then segfault on a null `testfrontend` -- which the mutation
# tier reads as "every mutation was caught" (measured: 7 expectation mismatches,
# all of them this).
#
# Neither FCITX_INSTALL_DATADIR nor FCITX_INSTALL_LIBDIR can be trusted here: the
# exported config falls back to /usr/local/share and /usr/local/lib unless fcitx
# was installed with its own sys-paths flag, and on a prefix install it is simply
# wrong (measured: datadir=/usr/local/share while the package was found under
# ~/toolchains/fcitx5/usr). Derive the roots from where the package WAS found.
set(_fcitx_roots "")
if(Fcitx5Core_DIR)
  string(REGEX REPLACE "/cmake/Fcitx5Core$" "" _fl "${Fcitx5Core_DIR}")  # <root>/usr/lib[/<arch>]
  list(APPEND _fcitx_roots "${_fl}")
  string(REGEX REPLACE "/lib(/[^/]+)?$" "" _fu "${_fl}")                 # <root>/usr
  list(APPEND _fcitx_roots "${_fu}")
endif()
# IN LISTS takes VARIABLE names, so the environment has to be split first.
set(_env_prefixes "$ENV{CMAKE_PREFIX_PATH}")
string(REPLACE ":" ";" _env_prefixes "${_env_prefixes}")
foreach(_p IN LISTS _env_prefixes)
  list(APPEND _fcitx_roots "${_p}")
endforeach()
list(APPEND _fcitx_roots /usr/local /usr)

set(_addon_conf_dirs "")
set(_module_dirs "")
foreach(_r IN LISTS _fcitx_roots)
  list(APPEND _addon_conf_dirs
       "${_r}/share/fcitx5/testing/addon" "${_r}/share/fcitx5/addon"
       "${_r}/usr/share/fcitx5/testing/addon" "${_r}/usr/share/fcitx5/addon")
  list(APPEND _module_dirs
       "${_r}/lib/fcitx5" "${_r}/usr/lib/fcitx5"
       "${_r}/lib/${CMAKE_LIBRARY_ARCHITECTURE}/fcitx5"
       "${_r}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}/fcitx5")
endforeach()

# Distro packaging ships testing/addon/*.conf with @PROJECT_VERSION@ UNsubstituted,
# which makes the addon unloadable; substitute while copying.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/addon")
foreach(_d IN LISTS _addon_conf_dirs)
  file(GLOB _confs "${_d}/test*.conf")
  foreach(_c IN LISTS _confs)
    get_filename_component(_name "${_c}" NAME)
    if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/addon/${_name}")
      file(READ "${_c}" _text)
      if(Fcitx5Core_VERSION)
        string(REPLACE "@PROJECT_VERSION@" "${Fcitx5Core_VERSION}" _text "${_text}")
      endif()
      file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/addon/${_name}" "${_text}")
    endif()
  endforeach()
endforeach()

file(GLOB _found_confs "${CMAKE_CURRENT_BINARY_DIR}/addon/test*.conf")
if(NOT _found_confs)
  message(STATUS "fcitx5 test addons: no test*.conf found; the D1/D2 runners will not "
                 "find TestFrontend (searched: ${_addon_conf_dirs})")
endif()

add_custom_target(copy-test-modules
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/fcitx5-addon")
# Fcitx5::Module::<Name> is an INTERFACE target, so there is no $<TARGET_FILE:>
# to ask for; find the shipped module libraries themselves.
set(_seen_modules "")
foreach(_d IN LISTS _module_dirs)
  file(GLOB _mods "${_d}/libtest*.so" "${_d}/libtest*.so.*")
  foreach(_lib IN LISTS _mods)
    get_filename_component(_libname "${_lib}" NAME)
    if(NOT _libname IN_LIST _seen_modules)
      list(APPEND _seen_modules "${_libname}")
      add_custom_command(TARGET copy-test-modules POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f "${CMAKE_BINARY_DIR}/fcitx5-addon/${_libname}"
        COMMAND ${CMAKE_COMMAND} -E create_symlink "${_lib}" "${CMAKE_BINARY_DIR}/fcitx5-addon/${_libname}")
    endif()
  endforeach()
endforeach()

configure_file(testdir.h.in ${CMAKE_CURRENT_BINARY_DIR}/testdir.h @ONLY)
add_subdirectory(addon)
add_subdirectory(inputmethod)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_executable(testhunmin testhunmin.cpp)
target_link_libraries(testhunmin Fcitx5::Core Fcitx5::Module::TestFrontend)
add_dependencies(testhunmin copy-addon copy-im copy-test-modules)
add_test(NAME engine-d1-fcitx5 COMMAND testhunmin)
# LABELS is a test property, not an add_test() keyword -- see the note at
# the top of the root CMakeLists.txt.
set_tests_properties(engine-d1-fcitx5 PROPERTIES LABELS "tier-D1")

# Tier D2: assert the InputPanel preedit the adapter publishes (text,
# clearing, cursor, and both capability branches). See docs/TESTING.md D2.
add_executable(testhunminui testhunminui.cpp)
target_link_libraries(testhunminui Fcitx5::Core Fcitx5::Module::TestFrontend)
add_dependencies(testhunminui copy-addon copy-im copy-test-modules)
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
  add_dependencies(trace-emit copy-addon copy-im copy-test-modules)
endif()
