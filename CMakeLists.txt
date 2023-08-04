cmake_minimum_required(VERSION 3.10)

project(http-server-from-scratch C CXX)

find_package(Threads REQUIRED)

# openssl
# packages libcurl4-openssl-dev libssl-dev
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# set(CURL_LIBRARY "-lcurl")
# find_package(CURL REQUIRED)
# include_directories(${CURL_INCLUDE_DIR})

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++17 -g -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++17 -w -O3")

add_executable(${PROJECT_NAME} test.cpp src/HttpMessage.cpp src/SimpleServer.cpp src/SSLUtils.cpp)
target_link_libraries(${PROJECT_NAME} pthread ${OPENSSL_LIBRARIES})
# ${OPENSSL_LIBRARIES} ${CURL_LIBRARIES}

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/static
        ${CMAKE_BINARY_DIR}/static
)