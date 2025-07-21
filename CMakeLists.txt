cmake_minimum_required(VERSION 3.16)

project(http-server-from-scratch C CXX)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
    message(STATUS "Unset CXX standatd, default to C++17.")
    # message(STATUS "To specify a different C++ language level, set CMAKE_CXX_STANDARD")
endif()

# uncomment to hard-coded clang build
# set(CMAKE_C_COMPILER /usr/bin/clang)
# set(CMAKE_CXX_COMPILER /usr/bin/clang++)

find_package(Threads REQUIRED)

option(USE_OPENSSL "Build with OpenSSL" OFF)
option(USE_LIBCURL "Build with libcurl" OFF)
option(USE_BOOST "Build with boost" OFF)
option(BUILD_TEST "Build test" OFF)

if (USE_OPENSSL)
    # libcurl4-openssl-dev
    find_package(OpenSSL REQUIRED)
    set(LIB_INCLUDE_DIRS ${LIB_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})
    set(LINK_LIBS ${LINK_LIBS} ${OPENSSL_LIBRARIES})
    set(SRC_FILES ${SRC_FILES} libs/SSLUtils.cpp) # helper utils
endif()

if (USE_LIBCURL)
    # libssl-dev
    find_package(CURL REQUIRED)
    set(LIB_INCLUDE_DIRS ${LIB_INCLUDE_DIRS} ${CURL_INCLUDE_DIR})
    set(LINK_LIBS ${LINK_LIBS} ${CURL_LIBRARIES})
endif()

if (USE_BOOST)
    include(BoostMacros)
    REQUIRE_BOOST_HEADERS()
    # set(BOOST_COMPONENTS filesystem)
    # REQUIRE_BOOST_LIBRARIES(BOOST_COMPONENTS)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# add_definitions(-DOPTION_ABC)

set(SRC_FILES
    ${SRC_FILES}
    src/HttpMessage.cpp
    src/SimpleServer.cpp
)

set(LINK_LIBS
    ${LINK_LIBS}
    pthread
)

add_executable(http-server
    test.cpp
    ${SRC_FILES}
)

# set_target_properties(http-server PROPERTIES
#     CXX_STANDARD 17
#     CXX_STANDARD_REQUIRED ON
# )
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
    ${LIB_INCLUDE_DIRS}
)
target_link_libraries(http-server PRIVATE
    ${LINK_LIBS}
    $<$<CONFIG:Debug>:-fsanitize=address>
)

add_custom_command(TARGET http-server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_SOURCE_DIR}/static
        ${CMAKE_BINARY_DIR}/static
        DEPENDS ${CMAKE_SOURCE_DIR}/static
)

if (BUILD_TEST)
    message(STATUS "Building tests")
    enable_testing()
    add_subdirectory(test)
endif()
