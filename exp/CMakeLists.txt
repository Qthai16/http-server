cmake_minimum_required(VERSION 3.16)

project(exp)

# io_uring
add_executable(uring-exp
    iouring-test.cpp
)

target_link_libraries(uring-exp PRIVATE
    pthread
    rt
    uring
    $<$<CONFIG:Debug>:-fsanitize=address>
)

target_compile_options(uring-exp PRIVATE
    $<$<CONFIG:Debug>:-g -fsanitize=address -fno-omit-frame-pointer>
)

# coroutine
add_executable(coro-exp
    coroutine-test.cpp
)

target_compile_options(coro-exp PRIVATE
    -g
    -fcoroutines
)
set_target_properties(coro-exp PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_link_libraries(coro-exp PRIVATE
    pthread
    rt
)

add_executable(manual-event
    manual-event.cpp
)

target_compile_options(manual-event PRIVATE
    -g
    -fcoroutines
)

target_link_libraries(manual-event PRIVATE
    pthread
    rt
)
set_target_properties(manual-event PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)