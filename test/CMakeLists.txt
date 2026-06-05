cmake_minimum_required(VERSION 3.16)

project(test)

include(GoogleTest)

set(test_SRC
    StrUtils_test.cpp
    FileUtils_test.cpp
    JoinableThread_test.cpp
    SSLUtils_test.cpp
    HttpMessage_test.cpp
    HttpServer_test.cpp
    MemBuffer_test.cpp
)

add_executable(dummy-test
    ${test_SRC}
)

set_target_properties(dummy-test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_include_directories(dummy-test PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

# notes: disable address space layout randomization (ASLR) when build test with ASan
# echo 0 | sudo tee /proc/sys/kernel/randomize_va_space # disable
# echo 2 | sudo tee /proc/sys/kernel/randomize_va_space # re-enable
# https://stackoverflow.com/questions/78136716/addresssanitizerdeadlysignal-from-fsanitize-address-flag

target_compile_options(dummy-test PRIVATE
    -g
    -fsanitize=address -fno-omit-frame-pointer
)

# target_link_directories(dummy-test PRIVATE
#         ${CMAKE_PREFIX_PATH}/lib
#         ${CMAKE_PREFIX_PATH}/lib64
# )

target_link_libraries(dummy-test PRIVATE
    libs::dummy
    ssl crypto curl
    gtest gtest_main
    pthread
    -fsanitize=address
)

# PROPERTIES ENVIRONMENT "GTEST_OUTPUT=xml:${CMAKE_BINARY_DIR}/test_results/"

gtest_discover_tests(dummy-test)

# todo: test http server
# add_executable(test-http-server)
# set_tests_properties(test-http-server PROPERTIES LABELS "integrate")
# add_test(NAME test-http-server COMMAND test-http-server 0.0.0.0 11225)


# todo: benchmark http server throughput
# add_executable(benchmark-http-server)
