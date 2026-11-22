# System installation rules for the Linux port.
# Respects CMAKE_INSTALL_PREFIX and DESTDIR (e.g. `cmake --install build --prefix /usr`).

include(GNUInstallDirs)

# systemd only reads /usr/lib/udev/rules.d (never lib64 on RPM systems), so
# keep this literal instead of ${CMAKE_INSTALL_LIBDIR}.
set(WB_UDEV_DIR "lib/udev/rules.d"
    CACHE PATH "Directory (relative to prefix) for the hidraw uaccess udev rule")

install(TARGETS wireless-battery
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT Runtime)

install(FILES "${PROJECT_SOURCE_DIR}/linux/wireless-battery.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    COMPONENT Runtime)

install(FILES "${PROJECT_SOURCE_DIR}/linux/wireless-battery.svg"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
    RENAME "wireless-battery.svg"
    COMPONENT Runtime)

install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
    RENAME "copyright"
    COMPONENT Runtime)

install(FILES "${PROJECT_SOURCE_DIR}/linux/99-wireless-battery.rules"
    DESTINATION "${WB_UDEV_DIR}"
    COMPONENT Udev)
