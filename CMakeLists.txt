cmake_minimum_required(VERSION 3.10)

project(http-server-from-scratch C CXX)

find_package(Threads REQUIRED)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++17 -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++17 -w -g -O3")

add_executable(${PROJECT_NAME} test.cpp src/HttpMessage.cpp)
target_link_libraries(${PROJECT_NAME} pthread)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/static
        ${CMAKE_BINARY_DIR}/static
)