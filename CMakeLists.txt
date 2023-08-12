cmake_minimum_required(VERSION 3.7)

project(http-server-from-scratch C CXX)

# set(CXX_STANDARD 17)
# set(CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

# openssl
option(USE_OPENSSL "set ON to build with OpenSSL" OFF)
option(USE_LIBCURL "set ON to build with libcurl" OFF)
if (USE_OPENSSL)
    # libcurl4-openssl-dev
    find_package(OpenSSL)
endif()
if (OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
    target_compile_definitions(${PROJECT_NAME} PUBLIC HAVE_OPENSSL) # add macro to use in code
    target_link_libraries(${PROJECT_NAME} ${OPENSSL_LIBRARIES})
    add_executable(${PROJECT_NAME} src/SSLUtils.cpp)
endif()

if (USE_LIBCURL)
    # libssl-dev
    find_package(CURL)
endif()
if (CURL_FOUND)
    # set(CURL_LIBRARY "-lcurl")
    include_directories(${CURL_INCLUDE_DIR})
    target_compile_definitions(${PROJECT_NAME} PUBLIC HAVE_LIBCURL)
    target_link_libraries(${PROJECT_NAME} ${CURL_LIBRARIES})
endif()

# old approach
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++17 -g -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++17 -w -O3")

# new approach for compile options
# -Wsign-conversion
# add_compile_options(-Wall -Wextra)
# add_compile_options("$<$<CONFIG:Debug>:-g>")
# add_compile_options("$<$<CONFIG:Release>:-w;-O3>")
# set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME} test.cpp src/HttpMessage.cpp src/SimpleServer.cpp)
target_link_libraries(${PROJECT_NAME} pthread)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/static
        ${CMAKE_BINARY_DIR}/static
)