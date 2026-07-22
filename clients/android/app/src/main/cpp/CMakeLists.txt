cmake_minimum_required(VERSION 3.22.1)
project(finlink_android C)

# Pull in the shared protocol/codec/transport library directly from the repo
# root instead of copying/symlinking it into the Android project -- this is
# the whole point of core/ being a plain add_subdirectory()-able CMake
# target. FINLINK_BUILD_TESTS is skipped automatically here since
# CMAKE_SOURCE_DIR (this project) != core's own CMAKE_CURRENT_SOURCE_DIR.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../core ${CMAKE_CURRENT_BINARY_DIR}/finlink_core)

add_library(finlink_android SHARED
    jni_bridge.c
)

find_library(log-lib log)

target_link_libraries(finlink_android
    finlink_core
    ${log-lib}
)
