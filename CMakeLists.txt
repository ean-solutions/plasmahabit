cmake_minimum_required(VERSION 3.16)

project(plasmahabit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_MIN_VERSION "6.5.0")
set(KF6_MIN_VERSION "6.0.0")

find_package(ECM 6.0.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core
    Gui
    Qml
    QuickControls2
    Sql
)

find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    CoreAddons
    I18n
    Kirigami
)

include(ECMFindQmlModule)
ecm_find_qmlmodule(org.kde.kirigami 2.0 REQUIRED)

add_subdirectory(src)

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

install(PROGRAMS org.eansolutions.plasmahabit.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES org.eansolutions.plasmahabit.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
