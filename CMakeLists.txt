cmake_minimum_required(VERSION 3.16)

project(cpptrace VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(cpptrace
    src/runtime.cpp
    src/trace.cpp
)

target_include_directories(cpptrace
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(cpptrace PRIVATE /W4 /permissive-)
else()
    target_compile_options(cpptrace PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(example
    examples/basic.cpp
)

target_link_libraries(example PRIVATE cpptrace)

enable_testing()

add_executable(cpptrace_tests
    tests/basic_tests.cpp
)

target_link_libraries(cpptrace_tests PRIVATE cpptrace)

add_test(NAME cpptrace_tests COMMAND cpptrace_tests)
