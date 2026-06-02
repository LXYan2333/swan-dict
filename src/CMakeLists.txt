find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
find_program(WAYLAND_SCANNER_EXECUTABLE wayland-scanner REQUIRED)
find_file(
    PRIMARY_SELECTION_PROTOCOL
    NAMES primary-selection-unstable-v1.xml
    PATHS /usr/share/wayland-protocols/unstable/primary-selection
    REQUIRED
)
find_file(
    EXT_DATA_CONTROL_PROTOCOL
    NAMES ext-data-control-v1.xml
    PATHS /usr/share/wayland-protocols/staging/ext-data-control
    REQUIRED
)
set(WLR_DATA_CONTROL_PROTOCOL ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/wl-clipboard-protocols/wlr-data-control-unstable-v1.xml)

set(PRIMARY_SELECTION_PROTOCOL_HEADER ${CMAKE_CURRENT_BINARY_DIR}/primary-selection-unstable-v1-client-protocol.h)
set(PRIMARY_SELECTION_PROTOCOL_CODE ${CMAKE_CURRENT_BINARY_DIR}/primary-selection-unstable-v1-protocol.c)
set(EXT_DATA_CONTROL_PROTOCOL_HEADER ${CMAKE_CURRENT_BINARY_DIR}/ext-data-control-v1-client-protocol.h)
set(EXT_DATA_CONTROL_PROTOCOL_CODE ${CMAKE_CURRENT_BINARY_DIR}/ext-data-control-v1-protocol.c)
set(WLR_DATA_CONTROL_PROTOCOL_HEADER ${CMAKE_CURRENT_BINARY_DIR}/wlr-data-control-unstable-v1-client-protocol.h)
set(WLR_DATA_CONTROL_PROTOCOL_CODE ${CMAKE_CURRENT_BINARY_DIR}/wlr-data-control-unstable-v1-protocol.c)

add_custom_command(
    OUTPUT ${PRIMARY_SELECTION_PROTOCOL_HEADER}
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE}
        client-header
        ${PRIMARY_SELECTION_PROTOCOL}
        ${PRIMARY_SELECTION_PROTOCOL_HEADER}
    DEPENDS ${PRIMARY_SELECTION_PROTOCOL}
    VERBATIM
)

add_custom_command(
    OUTPUT ${PRIMARY_SELECTION_PROTOCOL_CODE}
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE}
        private-code
        ${PRIMARY_SELECTION_PROTOCOL}
        ${PRIMARY_SELECTION_PROTOCOL_CODE}
    DEPENDS ${PRIMARY_SELECTION_PROTOCOL}
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXT_DATA_CONTROL_PROTOCOL_HEADER}
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE}
        client-header
        ${EXT_DATA_CONTROL_PROTOCOL}
        ${EXT_DATA_CONTROL_PROTOCOL_HEADER}
    DEPENDS ${EXT_DATA_CONTROL_PROTOCOL}
    VERBATIM
)

add_custom_command(
    OUTPUT ${EXT_DATA_CONTROL_PROTOCOL_CODE}
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE}
        private-code
        ${EXT_DATA_CONTROL_PROTOCOL}
        ${EXT_DATA_CONTROL_PROTOCOL_CODE}
    DEPENDS ${EXT_DATA_CONTROL_PROTOCOL}
    VERBATIM
)

add_custom_command(
    OUTPUT ${WLR_DATA_CONTROL_PROTOCOL_HEADER}
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE}
        client-header
        ${WLR_DATA_CONTROL_PROTOCOL}
        ${WLR_DATA_CONTROL_PROTOCOL_HEADER}
    DEPENDS ${WLR_DATA_CONTROL_PROTOCOL}
    VERBATIM
)

add_custom_command(
    OUTPUT ${WLR_DATA_CONTROL_PROTOCOL_CODE}
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE}
        private-code
        ${WLR_DATA_CONTROL_PROTOCOL}
        ${WLR_DATA_CONTROL_PROTOCOL_CODE}
    DEPENDS ${WLR_DATA_CONTROL_PROTOCOL}
    VERBATIM
)

qt_add_qml_module(swandictplugin
    URI com.github.LXYan2333.SwanDict
    VERSION 1.0
    CLASS_NAME SwanDictPlugin
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/com/github/LXYan2333/SwanDict
    SOURCES
        clipboardwriter.h
        clipboardwriter.cpp
        selectionwatcher.h
        selectionwatcher.cpp
        waylandprimaryselection.h
        waylandprimaryselection.cpp
        ${PRIMARY_SELECTION_PROTOCOL_HEADER}
        ${PRIMARY_SELECTION_PROTOCOL_CODE}
        ${EXT_DATA_CONTROL_PROTOCOL_HEADER}
        ${EXT_DATA_CONTROL_PROTOCOL_CODE}
        ${WLR_DATA_CONTROL_PROTOCOL_HEADER}
        ${WLR_DATA_CONTROL_PROTOCOL_CODE}
        translator.h
        translator.cpp
)

target_include_directories(swandictplugin
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(swandictplugin
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Network
        Qt6::Qml
        Qt6::Sql
        KF6::I18n
        PkgConfig::WAYLAND_CLIENT
)

set_target_properties(swandictpluginplugin PROPERTIES
    INSTALL_RPATH "$ORIGIN"
)

set(SWANDICT_QML_INSTALL_DIR ${KDE_INSTALL_LIBDIR}/qt6/qml/com/github/LXYan2333/SwanDict)

install(TARGETS swandictplugin swandictpluginplugin
    LIBRARY DESTINATION ${SWANDICT_QML_INSTALL_DIR}
    RUNTIME DESTINATION ${SWANDICT_QML_INSTALL_DIR}
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/com/github/LXYan2333/SwanDict/qmldir
    ${CMAKE_CURRENT_BINARY_DIR}/com/github/LXYan2333/SwanDict/swandictplugin.qmltypes
    DESTINATION ${SWANDICT_QML_INSTALL_DIR}
)

add_executable(swan-dict-wayland-primary-poc
    ../tools/wayland-primary-selection-poc.cpp
    waylandprimaryselection.h
    waylandprimaryselection.cpp
    ${PRIMARY_SELECTION_PROTOCOL_HEADER}
    ${PRIMARY_SELECTION_PROTOCOL_CODE}
    ${EXT_DATA_CONTROL_PROTOCOL_HEADER}
    ${EXT_DATA_CONTROL_PROTOCOL_CODE}
    ${WLR_DATA_CONTROL_PROTOCOL_HEADER}
    ${WLR_DATA_CONTROL_PROTOCOL_CODE}
)

target_include_directories(swan-dict-wayland-primary-poc
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(swan-dict-wayland-primary-poc
    PRIVATE
        Qt6::Core
        PkgConfig::WAYLAND_CLIENT
)
