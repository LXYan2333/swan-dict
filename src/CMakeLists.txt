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
        translator.h
        translator.cpp
)

target_link_libraries(swandictplugin
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Qml
        Qt6::Sql
        KF6::I18n
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
