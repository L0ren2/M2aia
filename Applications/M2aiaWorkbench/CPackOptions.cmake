# Set M2aia specific CPack options

set(CPACK_PACKAGE_EXECUTABLES "M2aiaWorkbench;M2aiaWorkbench")
set(CPACK_PACKAGE_NAME "M2aiaWorkbench")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "M²aia - Mass spectrometry imaging applications for interactive analysis in MITK.")
# Major version is the year of release
set(CPACK_PACKAGE_VERSION_MAJOR "2022")
# Minor version is the month of release
set(CPACK_PACKAGE_VERSION_MINOR "08")
# Patch versioning
set(CPACK_PACKAGE_VERSION_PATCH "00")
set(CPACK_PACKAGE_VENDOR "Mannheim University of Applied Sciences")

# this should result in names like 2011.09, 2012.06, ...
# version has to be set explicitly to avoid such things as CMake creating the install directory "M2aiaWorkbench 2021.."
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
#set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}")#.${CPACK_PACKAGE_VERSION_PATCH}")

set(CPACK_PACKAGE_FILE_NAME "M2aia-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_ARCH}")

set(CPACK_PACKAGE_ICON "${PROJECT_SOURCE_DIR}/Applications/M2aiaWorkbench/icons/icon.png")

