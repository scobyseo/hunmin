add_custom_target(copy-im
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${PROJECT_BINARY_DIR}/frontends/fcitx5/src/hunmin.conf"
          "${CMAKE_CURRENT_BINARY_DIR}/hunmin.conf")
