cmake_minimum_required(VERSION 3.16)

project(dummy
    VERSION 1.0.0
    LANGUAGES CXX
)

find_package(Threads REQUIRED)

option(USE_OPENSSL "Build with OpenSSL" ON)
option(USE_LMDB "Build with LMDB" ON)
# option(USE_LIBCURL "Build with libcurl" OFF)
# option(USE_BOOST "Build with boost" OFF)
# option(BUILD_TEST "Build test" OFF)

if (USE_OPENSSL)
    # libcurl4-openssl-dev
    find_package(OpenSSL REQUIRED)
    set(LIB_INCLUDE_DIRS ${LIB_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})
    list(APPEND LINK_LIBS ${OPENSSL_LIBRARIES})
    list(APPEND LIB_FILES ssl/SSLUtils.cpp) # helper utils
endif()

list(APPEND LIB_FILES
    http/HttpMessage.cpp
    http/SimpleServer.cpp
)

if (USE_LMDB)
    list(APPEND LINK_LIBS lmdb)
endif()

# if (USE_LIBCURL)
#     # libssl-dev
#     find_package(CURL REQUIRED)
#     set(LIB_INCLUDE_DIRS ${LIB_INCLUDE_DIRS} ${CURL_INCLUDE_DIR})
#     set(LINK_LIBS ${LINK_LIBS} ${CURL_LIBRARIES})
# endif()

# if (USE_BOOST)
#     include(BoostMacros)
#     REQUIRE_BOOST_HEADERS()
#     # set(BOOST_COMPONENTS filesystem)
#     # REQUIRE_BOOST_LIBRARIES(BOOST_COMPONENTS)
# endif()

include(GNUInstallDirs)

add_library(dummy SHARED
    ${LIB_FILES}
)

add_library(libs::dummy ALIAS dummy)

set_target_properties(dummy PROPERTIES
    VERSION     ${PROJECT_VERSION}
    SOVERSION   ${PROJECT_VERSION_MAJOR}
    CXX_STANDARD ${CMAKE_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
)

target_include_directories(dummy
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(dummy PRIVATE
    $<$<CONFIG:Debug>:-g -fsanitize=address -fno-omit-frame-pointer>
    $<$<CONFIG:Release>:-w -O3>
)

target_link_libraries(dummy
    PUBLIC  pthread ${LINK_LIBS}
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
install(DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dummy
    FILES_MATCHING PATTERN "*.h"
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
