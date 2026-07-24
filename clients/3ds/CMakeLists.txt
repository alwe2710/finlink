cmake_minimum_required(VERSION 3.13)

# Toolchain file must be included before project(): this is what picks up
# $DEVKITPRO/cmake/3DS.cmake and configures the arm-none-eabi compiler, same
# pattern as clients/switch/CMakeLists.txt for devkitA64/Switch.cmake.
if (NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE $ENV{DEVKITPRO}/cmake/3DS.cmake CACHE PATH "toolchain file")
endif ()

project(finlink-3ds C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# finlink_core (WS handshake/framing, app protocol, deflate) -- unchanged
# from the core used by every other client, see core/README.md.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../core ${CMAKE_BINARY_DIR}/finlink_core)

file(GLOB_RECURSE FINLINK_3DS_SRC ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
add_executable(${PROJECT_NAME} ${FINLINK_3DS_SRC})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_link_libraries(${PROJECT_NAME} PRIVATE finlink_core citro2d citro3d ctru m)

# smdhtool requires an exact 48x48 PNG (unlike the Switch/Android icons,
# which accept the full-resolution logo directly) -- pre-resized once from
# assets/logo/finlink-logo.png rather than depending on ImageMagick/PIL at
# build time.
set(APP_ICON ${CMAKE_CURRENT_SOURCE_DIR}/icon.png)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CTR_SMDHTOOL_EXE} --create "finlink" "GBA stream client" "finlink" ${APP_ICON} ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.smdh
    COMMAND ${CTR_3DSXTOOL_EXE} $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.3dsx --smdh=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.smdh
    BYPRODUCTS ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.3dsx ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.smdh
)
