# CPack configuration for DEB, RPM and TGZ source/binary packages.
#   cpack -G DEB   /   cpack -G RPM   /   cpack -G TGZ

set(CPACK_PACKAGE_NAME "wireless-device-battery-monitor")
set(CPACK_PACKAGE_VENDOR "Wireless Device Battery Monitor contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Battery monitor for wireless devices (GTK4 Linux port)")
set(CPACK_PACKAGE_VERSION "${WB_VERSION}")
set(CPACK_PACKAGE_CONTACT "https://github.com/WCHSoftGroup/Wireless-Device-Battery-Monitor")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/WCHSoftGroup/Wireless-Device-Battery-Monitor")

set(CPACK_GENERATOR "TGZ;DEB;RPM")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${WB_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${WB_VERSION}-source")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")

# The udev rule needs root; keep it in a separate component so the Runtime
# component can be installed user-locally. Packages include both components.
set(CPACK_COMPONENTS_ALL Runtime Udev)
set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Runtime")
set(CPACK_COMPONENT_UDEV_DISPLAY_NAME "udev rule (system-wide)")

# DEB
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libgtk-4-1, libcairo2, libglib2.0-0")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "can-utils")
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")

# RPM
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-only")
set(CPACK_RPM_PACKAGE_GROUP "Applications/System")
set(CPACK_RPM_PACKAGE_REQUIRES
    "gtk4, cairo, glib2")
set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")

# TGZ: a relocatable tarball of the install tree.
set(CPACK_ARCHIVE_COMPONENT_INSTALL OFF)

include(CPack)
