cmake_minimum_required(VERSION 3.16)

project(test)

include(GoogleTest)

set(TEST_SRC
    StrUtils_test.cpp
    FileUtils_test.cpp
)

# set_target_properties(test PROPERTIES
#         CXX_STANDARD 17
#         CXX_EXTENSIONS OFF
#         CXX_STANDARD_REQUIRED ON
# )
# target_include_directories(test PRIVATE
#         ${CMAKE_CURRENT_SOURCE_DIR}
#         ${CMAKE_PREFIX_PATH}/include
# )
# target_link_directories(test PRIVATE
#         ${CMAKE_PREFIX_PATH}/lib
#         ${CMAKE_PREFIX_PATH}/lib64
# )

add_executable(test
    ${TEST_SRC}
)

target_link_libraries(test
    pthread
    gtest gtest_main
)

gtest_discover_tests(test)