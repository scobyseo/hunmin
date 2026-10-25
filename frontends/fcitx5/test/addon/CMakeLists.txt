# DEPENDS on the addon library, not just the conf. D1/D2/Tier C dlopen
# TEST_ADDON_DIR/libhunmin.so, so building the test target must rebuild it --
# otherwise a source edit leaves a stale .so in place and every mutation planted
# in hunminadapter.cpp survives, which reads as "the test does not catch this" when the
# truth is that the test never saw the change.
add_custom_target(copy-addon
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${PROJECT_BINARY_DIR}/frontends/fcitx5/src/hunmin-addon.conf"
          "${CMAKE_CURRENT_BINARY_DIR}/hunmin.conf")
# DEPENDS on a custom target only orders it; it does not make it produce
# anything. This is what actually rebuilds libhunmin.so when hunminadapter.cpp
# changes, so the tests are not asserting against a stale addon.
add_dependencies(copy-addon hunmin_addon)
