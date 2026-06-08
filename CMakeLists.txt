cmake_minimum_required(VERSION 3.20)

project(swan-dict VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(SWAN_DICT_GENERATE_DICTIONARY "Generate the bundled ECDICT SQLite database during the build" ON)
option(SWAN_DICT_BUILD_KWIN_HELPER "Build the optional KWin effect helper for global mouse click detection" ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)

if (SWAN_DICT_GENERATE_DICTIONARY)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
endif()
find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Network Qml Sql)
if (COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
endif()
find_package(KF6 REQUIRED COMPONENTS I18n Package)
find_package(Plasma REQUIRED)
include(${KF6Package_DIR}/KF6PackageMacros.cmake)
include(${Plasma_DIR}/PlasmaMacros.cmake)

execute_process(
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/sync-digital-clock.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE sync_digital_clock_result
)
if (NOT sync_digital_clock_result EQUAL 0)
    message(FATAL_ERROR "Failed to sync KDE Digital Clock sources")
endif()

add_subdirectory(src)
if (SWAN_DICT_BUILD_KWIN_HELPER)
    add_subdirectory(kwin-helper)
endif()
ki18n_install(po)

if (SWAN_DICT_GENERATE_DICTIONARY)
    set(SWAN_DICT_ECDICT_CSV ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ECDICT/ecdict.csv)
    set(SWAN_DICT_ECDICT_DB ${CMAKE_CURRENT_SOURCE_DIR}/applet/contents/data/ecdict.sqlite)

    if (NOT EXISTS ${SWAN_DICT_ECDICT_CSV})
        message(FATAL_ERROR "ECDICT CSV not found: ${SWAN_DICT_ECDICT_CSV}")
    endif()

    add_custom_command(
        OUTPUT ${SWAN_DICT_ECDICT_DB}
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/import-ecdict/import_ecdict.py
            --source ${SWAN_DICT_ECDICT_CSV}
            --output ${SWAN_DICT_ECDICT_DB}
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/import-ecdict/import_ecdict.py
            ${SWAN_DICT_ECDICT_CSV}
        COMMENT "Generating Swan Dict ECDICT SQLite database"
        VERBATIM
    )
    add_custom_target(swandict_dictionary ALL DEPENDS ${SWAN_DICT_ECDICT_DB})
endif()

plasma_install_package(applet com.github.LXYan2333.swan-dict)
