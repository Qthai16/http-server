cmake_minimum_required(VERSION 3.16)

project(dummy
    VERSION 1.0.0
    LANGUAGES CXX
)

include(GNUInstallDirs)

add_library(dummy SHARED
    HttpMessage.cpp
    SimpleServer.cpp
)

add_library(libs::dummy ALIAS dummy)

set_target_properties(dummy PROPERTIES
    VERSION     ${PROJECT_VERSION}
    SOVERSION   ${PROJECT_VERSION_MAJOR}
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Root of the repo is two levels up; expose it so #include "libs/..." and
# #include "json11/..." work from consumer code as well.
target_include_directories(dummy
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(dummy PRIVATE
    $<$<CONFIG:Debug>:-g -fsanitize=address -fno-omit-frame-pointer>
    $<$<CONFIG:Release>:-w -O3>
)

target_link_libraries(dummy
    PUBLIC  pthread
    PRIVATE $<$<CONFIG:Debug>:-fsanitize=address>
)

# ── Install: binaries ──────────────────────────────────────────────────────────
install(TARGETS dummy
    EXPORT      dummyTargets
    LIBRARY     DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE     DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME     DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ── Install: public headers ────────────────────────────────────────────────────
install(FILES
    HttpMessage.h
    SimpleServer.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dummy/http
)

# ── Install: CMake export set ──────────────────────────────────────────────────
install(EXPORT dummyTargets
    FILE        dummyTargets.cmake
    NAMESPACE   libs::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dummy
)

# ── Package config files ───────────────────────────────────────────────────────
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/dummyConfigVersion.cmake"
    VERSION       ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dummyConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/dummyConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dummy
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/dummyConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/dummyConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dummy
)
