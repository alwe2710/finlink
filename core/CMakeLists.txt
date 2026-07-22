cmake_minimum_required(VERSION 3.13)
project(finlink_core C)

option(FINLINK_BUILD_TESTS "Build finlink_core unit tests" ON)

add_library(finlink_core STATIC
    src/protocol.c
    src/inflate.c
    third_party/miniz/miniz_tinfl.c
)

target_include_directories(finlink_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        third_party/miniz
)

target_compile_features(finlink_core PUBLIC c_std_99)

if (FINLINK_BUILD_TESTS AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(tests)
endif ()
