add_fcitx5_addon(hunmin_addon engine.cpp modeswitch.cpp)

# fcitx dlopens <Library>.so from the addon dir: file must be libhunmin.so
# (core libhunmin.so.0 is a DT_NEEDED, resolved by soname — same layout as
# fcitx5-hangul + system libhangul coexistence).
set_target_properties(hunmin_addon PROPERTIES
  OUTPUT_NAME hunmin
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/fcitx5-addon")

target_link_libraries(hunmin_addon PRIVATE Fcitx5::Core Fcitx5::Config hunmin)
target_compile_definitions(hunmin_addon PRIVATE
  HUNMIN_DEFAULT_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/hunmin")

# Tier C trace mode (docs/trace-spec.md step 2, docs/TESTING.md). OFF by
# default: the code is #ifdef'd out, so a packaged addon carries no trace sink
# and no getenv on the key path. Enable with -DHUNMIN_TRACE=ON to make the
# SHIPPED library emit traces -- which matters because D1/D2/Tier C already
# dlopen this exact .so, so the traced binary is the shipped one.
option(HUNMIN_TRACE "Build the addon with the tier-C trace sink" OFF)
if(HUNMIN_TRACE)
  target_compile_definitions(hunmin_addon PRIVATE HUNMIN_TRACE)
endif()

install(TARGETS hunmin_addon DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")

configure_file(hunmin.conf.in hunmin.conf @ONLY)
configure_file(hunmin-romaja.conf.in hunmin-romaja.conf @ONLY)
configure_file(hunmin-addon.conf.in.in hunmin-addon.conf @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/hunmin.conf"
              "${CMAKE_CURRENT_BINARY_DIR}/hunmin-romaja.conf"
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod" COMPONENT config)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/hunmin-addon.conf" RENAME hunmin.conf
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon" COMPONENT config)
