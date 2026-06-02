find_package(Qt6 REQUIRED COMPONENTS Core DBus Quick Widgets)
find_package(KF6 REQUIRED COMPONENTS Config CoreAddons WindowSystem)
find_package(KWin CONFIG REQUIRED)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

set(_swandict_saved_build_shared_libs ${BUILD_SHARED_LIBS})
set(_swandict_saved_kde_install_plugindir ${KDE_INSTALL_PLUGINDIR})
set(BUILD_SHARED_LIBS ON)
set(KDE_INSTALL_PLUGINDIR ${QT6_INSTALL_PLUGINS})
kcoreaddons_add_plugin(swandictmousehelper
    SOURCES
        swandictmousehelper.cpp
    INSTALL_NAMESPACE "kwin/effects/plugins"
)
set(KDE_INSTALL_PLUGINDIR ${_swandict_saved_kde_install_plugindir})
set(BUILD_SHARED_LIBS ${_swandict_saved_build_shared_libs})
unset(_swandict_saved_kde_install_plugindir)
unset(_swandict_saved_build_shared_libs)

target_link_libraries(swandictmousehelper
    PRIVATE
        Qt6::Core
        Qt6::DBus
        KWin::kwin
)

target_compile_features(swandictmousehelper PRIVATE cxx_std_20)
set_target_properties(swandictmousehelper PROPERTIES PREFIX "")
