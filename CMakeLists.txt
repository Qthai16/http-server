cmake_minimum_required(VERSION 3.16)

project(cpp-playground C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
    message(STATUS "Unset CXX standard, set to: ${CMAKE_CXX_STANDARD}")
endif()

# uncomment to hard-coded clang build
# set(CMAKE_C_COMPILER /usr/bin/clang)
# set(CMAKE_CXX_COMPILER /usr/bin/clang++)

find_package(Threads REQUIRED)

option(BUILD_TEST "Build test" ON)
option(BUILD_EXP "Build experience code" OFF)

# build libdummy
add_subdirectory(libs)
# build json11 lib
add_subdirectory(json11)

add_executable(http-server
    test.cpp
)

set_target_properties(http-server PROPERTIES
    CXX_STANDARD ${CMAKE_CXX_STANDARD}
    CXX_STANDARD_REQUIRED ON
)
# notes: -fsanitize=thread cannot use with -fsanitize=address
target_compile_options(http-server PRIVATE
    $<$<CONFIG:Debug>:-g -fsanitize=address -fno-omit-frame-pointer>
    $<$<CONFIG:Release>:-w -O3>
    # -Wall -Wextra -Wsign-conversion
)

target_compile_definitions(http-server PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)

target_include_directories(http-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(http-server PRIVATE
    libs::dummy
    json11
    pthread
    $<$<CONFIG:Debug>:-fsanitize=address>
)

add_custom_command(TARGET http-server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_SOURCE_DIR}/static
        ${CMAKE_BINARY_DIR}/static
        DEPENDS ${CMAKE_SOURCE_DIR}/static
)

if (BUILD_TEST)
    include(FetchGoogleTest)
    REQUIRE_GTEST(1.16.0)
    message(STATUS "Building tests")
    enable_testing()
    add_subdirectory(test)
endif()

if (BUILD_EXP)
    add_subdirectory(exp)
endif()

# HTTP server + message types are provided by libs/http (libdummy).
# See libs/http/CMakeLists.txt for the shared-library build and install rules.