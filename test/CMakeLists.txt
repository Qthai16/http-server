cmake_minimum_required(VERSION 3.16)

project(test)

include(GoogleTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(libs_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../libs/SSLUtils.cpp
)

# test libs
set(test_SRC
    StrUtils_test.cpp
    FileUtils_test.cpp
    JoinableThread_test.cpp
    SSLUtils_test.cpp
)

add_executable(test-libs
    ${libs_SRC}
    ${test_SRC}
)

# set_target_properties(test-libs PROPERTIES
#     CXX_STANDARD 20
#     # CXX_EXTENSIONS OFF
#     CXX_STANDARD_REQUIRED ON
# )

target_include_directories(test-libs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_PREFIX_PATH}/include
)

# notes: disable address space layout randomization (ASLR) when build test with ASan
# echo 0 | sudo tee /proc/sys/kernel/randomize_va_space # disable
# echo 2 | sudo tee /proc/sys/kernel/randomize_va_space # re-enable
# https://stackoverflow.com/questions/78136716/addresssanitizerdeadlysignal-from-fsanitize-address-flag

target_compile_options(test-libs PRIVATE
    -g
    -fsanitize=address -fno-omit-frame-pointer
)

# target_link_directories(test-libs PRIVATE
#         ${CMAKE_PREFIX_PATH}/lib
#         ${CMAKE_PREFIX_PATH}/lib64
# )

target_link_libraries(test-libs PRIVATE
    ssl crypto
    gtest gtest_main
    pthread
    -fsanitize=address
)

gtest_discover_tests(test-libs)

# todo: test http server
# add_executable(test-http-server)
# set_tests_properties(test-http-server PROPERTIES LABELS "integrate")
# add_test(NAME test-http-server COMMAND test-http-server 0.0.0.0 11225)


# todo: benchmark http server throughput
# add_executable(benchmark-http-server)

add_test(NAME test-libs COMMAND test-libs)
set_tests_properties(test-libs PROPERTIES LABELS "unit")

