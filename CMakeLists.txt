cmake_minimum_required(VERSION 3.16)

project(http-server-from-scratch C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# # uncomment to use clang build with sanitize address option
# set(CMAKE_C_COMPILER /usr/bin/clang)
# set(CMAKE_CXX_COMPILER /usr/bin/clang++)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")

find_package(Threads REQUIRED)

set(SRC_FILES
    test.cpp
    src/HttpMessage.cpp
    src/SimpleServer.cpp
)

set(LINK_LIBS
    pthread
)

# openssl
option(USE_OPENSSL "set ON to build with OpenSSL" OFF)
option(USE_LIBCURL "set ON to build with libcurl" OFF)

if (USE_OPENSSL)
    # libcurl4-openssl-dev
    find_package(OpenSSL REQUIRED)
    set(LINK_LIBS ${LINK_LIBS} ${OPENSSL_LIBRARIES})
    set(SRC_FILES ${SRC_FILES} libs/SSLUtils.cpp) # helper utils
endif()

if (USE_LIBCURL)
    # libssl-dev
    find_package(CURL REQUIRED)
    set(LINK_LIBS ${LINK_LIBS} ${CURL_LIBRARIES})
endif()


# old approach
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++17 -g -Wall")
# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++17 -w -O3")

# new approach for compile options
# -Wsign-conversion
# add_compile_options(-Wall -Wextra)
add_compile_options("$<$<CONFIG:Debug>:-g>")
add_compile_options("$<$<CONFIG:Release>:-w;-O3>")
# set_target_properties(http-server PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)


add_executable(http-server ${SRC_FILES})
target_include_directories(http-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OPENSSL_INCLUDE_DIR} # should check if enable by opt
    ${CURL_INCLUDE_DIR} # should check if enable by opt
)
target_link_libraries(http-server ${LINK_LIBS})


# add_custom_command(TARGET http-server POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_directory
#         ${CMAKE_SOURCE_DIR}/static
#         ${CMAKE_BINARY_DIR}/static
# )

set(TEST_SRC
    socket_pairs.cpp
    libs/SSLUtils.cpp
)

add_executable(socket_test
    ${TEST_SRC}
)
target_include_directories(socket_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(socket_test pthread ssl crypto)





# if (OPENSSL_FOUND)
#     # target_include_directories(http-server ${OPENSSL_INCLUDE_DIR})
#     # target_compile_definitions(http-server PUBLIC HAVE_OPENSSL)
#     # target_link_libraries(http-server ${OPENSSL_LIBRARIES})
#     set(LINK_LIBS ${LINK_LIBS} ${OPENSSL_LIBRARIES})
#     set(SRC_FILES ${SRC_FILES} libs/SSLUtils.cpp) # helper utils
# endif()

# if (CURL_FOUND)
#     # set(CURL_LIBRARY "-lcurl")
#     # target_include_directories(${CURL_INCLUDE_DIR})
#     # target_compile_definitions(http-server PUBLIC HAVE_LIBCURL)
#     set(LINK_LIBS ${LINK_LIBS} ${CURL_LIBRARIES})
#     # target_link_libraries(http-server ${CURL_LIBRARIES})
# endif()