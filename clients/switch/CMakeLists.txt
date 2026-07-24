cmake_minimum_required(VERSION 3.13)

set(BOREALIS_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/borealis)

# Options + toolchain selection must happen before project(): this is what
# picks up $DEVKITPRO/cmake/Switch.cmake and configures the aarch64 compiler,
# following borealis's own root CMakeLists.txt pattern (see clients/switch/
# README.md for why this project vendors borealis/ as plain source rather
# than a submodule, and only the pieces needed for the Switch+glfw driver).
include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)
include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)

project(finlink-switch C CXX)

set(VERSION_MAJOR "0")
set(VERSION_MINOR "1")
set(VERSION_ALTER "0")
set(PROJECT_AUTHOR "finlink")
# Arbitrary but fixed placeholder: this never gets published/registered
# anywhere, it only needs to be a stable 16-hex-digit id for a sideloaded
# homebrew .nro to have a distinct save/config folder.
set(PROJECT_TITLEID "0100FIN0A0000000")
string(REGEX REPLACE "[^0-9A-Fa-f]" "0" PROJECT_TITLEID "${PROJECT_TITLEID}")
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/../../assets/logo/finlink-logo.png)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)

if (NOT PLATFORM_SWITCH)
    message(FATAL_ERROR "This project only builds for the Switch: pass -DPLATFORM_SWITCH=ON.")
endif ()

add_subdirectory(${BOREALIS_LIBRARY} ${CMAKE_BINARY_DIR}/borealis)

# finlink_core (WS handshake/framing, app protocol, deflate) -- unchanged
# from the core used by every other client, see core/README.md.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../core ${CMAKE_BINARY_DIR}/finlink_core)

find_package(Threads REQUIRED)

file(GLOB_RECURSE FINLINK_SWITCH_SRC ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
program_target(${PROJECT_NAME} "${FINLINK_SWITCH_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections)
target_link_libraries(${PROJECT_NAME} PRIVATE borealis finlink_core Threads::Threads)

add_custom_target(${PROJECT_NAME}.nro DEPENDS ${PROJECT_NAME}
    COMMAND ${NX_NACPTOOL_EXE} --create "finlink" "${PROJECT_AUTHOR}" "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}" ${PROJECT_NAME}.nacp --titleid=${PROJECT_TITLEID}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_BINARY_DIR}/resources
    COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro --icon=${PROJECT_ICON} --nacp=${PROJECT_NAME}.nacp --romfsdir=${CMAKE_BINARY_DIR}/resources
)
