cmake_minimum_required(VERSION 3.21)

# DEFG - Decent Extremely Fast Gridder
project(DEFG
  VERSION 0.1.0
  DESCRIPTION "Decent Extremely Fast Gridder (legacy codebase)"
  LANGUAGES CXX C)

# Options
option(DEFG_BUILD_TESTS "Build unit/regression test helpers" ON)
option(DEFG_WARN_PEDANTIC "Enable extra warnings" ON)
option(DEFG_BUILD_WITH_VNS "Build DEFG with VNS Integration enabled" OFF)

# Determine C++ standard (prefer 20 for toolchain compatibility)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Honor warning flags
if (MSVC)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/permissive->)
  if (DEFG_WARN_PEDANTIC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/W4>)
  endif()
else()
  if (DEFG_WARN_PEDANTIC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Wpedantic>)
  endif()
endif()

# Check whether DEFG is to be built with VNS, or as a standalone tool
if (DEFG_BUILD_WITH_VNS)
    add_compile_definitions(defg PUBLIC DEFG_WITH_VNS)
endif()

# Library target
add_library(defg STATIC
  src/DEFG/DEFG.cpp
  src/DEFG/DEFGDiag.cpp
  src/DEFG/DEFGSpline.cpp
  src/DEFG/DEFGSupport.cpp
)

target_include_directories(defg
  PUBLIC
    ${PROJECT_SOURCE_DIR}/src/DEFG
)

# If you later adopt vcpkg packages, they'll link here.
# target_link_libraries(defg PUBLIC ...)

# Example/test harness (optional)
if (DEFG_BUILD_TESTS)
  enable_testing()
  add_executable(defg-util tests/defg-util.cpp)
  target_link_libraries(defg-util PRIVATE defg)
  add_test(NAME defg-smoke COMMAND defg-util --help)
endif()

# Install rules
include(GNUInstallDirs)
install(TARGETS defg
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY src/DEFG/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/DEFG
  FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# Package config (basic)
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/DEFGConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DEFGConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/DEFGConfig.cmake"
  @ONLY
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/DEFGConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/DEFGConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DEFG
)
