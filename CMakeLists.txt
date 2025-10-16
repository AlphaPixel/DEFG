cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0091 NEW)   # Ensure MSVC_RUNTIME_LIBRARY is used below.

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/installer/nsis")

# Install targets for debugging
# if(POLICY CMP0087)
#     cmake_policy(SET CMP0087 NEW)   # Required for X_VCPKG_APPLOCAL_DEPS_INSTALL
# endif()

set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install/DEFG")
set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON)
set(VCPKG_APPINSTALL_DEPS ON)

project(DEFG)

# Build settings
#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_MFC_FLAG 1) # 1 = static linking, 2 = dynamic linking

#
# Write a file with the value of CMAKE_MSVC_RUNTIME_LIBRARY
#
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/msvctype$<$<CONFIG:Debug>:Debug>.txt" CONTENT "${CMAKE_MSVC_RUNTIME_LIBRARY}")

#
# Version information
#
set(APP_VERSION_MAJOR "1")
set(APP_VERSION_MINOR "0")
set(APP_VERSION_PATCH "0")

set(APP_PACKAGE_DESCRIPTION "DEFG - Decent Extremely Fast Gridder")
set(APP_PACKAGE_SUMMARY "DEFG - Decent Extremely Fast Gridder")
set(APP_INSTALL_DIRECTORY "DEFG - Decent Extremely Fast Gridder")

add_subdirectory(src)
