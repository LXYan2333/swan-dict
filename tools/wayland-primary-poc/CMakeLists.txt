cmake_minimum_required(VERSION 3.20)

project(wayland-primary-poc LANGUAGES C CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)

find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

set(PRIMARY_XML "${WAYLAND_PROTOCOLS_DIR}/unstable/primary-selection/primary-selection-unstable-v1.xml")
set(DATA_CONTROL_XML "${WAYLAND_PROTOCOLS_DIR}/staging/ext-data-control/ext-data-control-v1.xml")
set(WLR_DATA_CONTROL_XML "${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-data-control-unstable-v1.xml")

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}")

add_custom_command(
    OUTPUT
        "${GENERATED_DIR}/primary-selection-unstable-v1-client-protocol.h"
        "${GENERATED_DIR}/primary-selection-unstable-v1-protocol.c"
    COMMAND "${WAYLAND_SCANNER}" client-header "${PRIMARY_XML}"
            "${GENERATED_DIR}/primary-selection-unstable-v1-client-protocol.h"
    COMMAND "${WAYLAND_SCANNER}" private-code "${PRIMARY_XML}"
            "${GENERATED_DIR}/primary-selection-unstable-v1-protocol.c"
    DEPENDS "${PRIMARY_XML}"
    VERBATIM
)

add_custom_command(
    OUTPUT
        "${GENERATED_DIR}/ext-data-control-v1-client-protocol.h"
        "${GENERATED_DIR}/ext-data-control-v1-protocol.c"
    COMMAND "${WAYLAND_SCANNER}" client-header "${DATA_CONTROL_XML}"
            "${GENERATED_DIR}/ext-data-control-v1-client-protocol.h"
    COMMAND "${WAYLAND_SCANNER}" private-code "${DATA_CONTROL_XML}"
            "${GENERATED_DIR}/ext-data-control-v1-protocol.c"
    DEPENDS "${DATA_CONTROL_XML}"
    VERBATIM
)

add_custom_command(
    OUTPUT
        "${GENERATED_DIR}/wlr-data-control-unstable-v1-client-protocol.h"
        "${GENERATED_DIR}/wlr-data-control-unstable-v1-protocol.c"
    COMMAND "${WAYLAND_SCANNER}" client-header "${WLR_DATA_CONTROL_XML}"
            "${GENERATED_DIR}/wlr-data-control-unstable-v1-client-protocol.h"
    COMMAND "${WAYLAND_SCANNER}" private-code "${WLR_DATA_CONTROL_XML}"
            "${GENERATED_DIR}/wlr-data-control-unstable-v1-protocol.c"
    DEPENDS "${WLR_DATA_CONTROL_XML}"
    VERBATIM
)

add_executable(wayland-primary-poc
    main.cpp
    "${GENERATED_DIR}/primary-selection-unstable-v1-protocol.c"
    "${GENERATED_DIR}/ext-data-control-v1-protocol.c"
    "${GENERATED_DIR}/wlr-data-control-unstable-v1-protocol.c"
)

target_include_directories(wayland-primary-poc PRIVATE
    "${GENERATED_DIR}"
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
)

target_link_libraries(wayland-primary-poc PRIVATE ${WAYLAND_CLIENT_LIBRARIES})
