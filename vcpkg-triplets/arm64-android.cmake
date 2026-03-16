set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CMAKE_SYSTEM_NAME Android)
set(VCPKG_CMAKE_SYSTEM_VERSION 28)
set(VCPKG_LIBRARY_LINKAGE static)
# Only build release to save disk space on CI
set(VCPKG_BUILD_TYPE release)
