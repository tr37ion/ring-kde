cmake_minimum_required(VERSION 3.0)

project(jamihistoryview)

if(POLICY CMP0063)
   CMAKE_POLICY(SET CMP0063 NEW)
endif(POLICY CMP0063)

find_package(ECM 1.1.0 REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}")

include(ECMInstallIcons)
include(ECMOptionalAddSubdirectory)
include(CMakePackageConfigHelpers)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)

SET(CMAKE_C_VISIBILITY_PRESET hidden)
SET(CMAKE_CXX_VISIBILITY_PRESET hidden)
SET(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# When used with `add_subdirectory`, assume it is a static Qt plugin
get_directory_property(USES_ADD_SUBDIRECTORY PARENT_DIRECTORY)
if(USES_ADD_SUBDIRECTORY)
   set(BUILD_SHARED_LIBS OFF)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 14)

if(NOT BUILD_SHARED_LIBS)
    set(ENABLE_STATIC_PLUGIN 1) # Used in Config.cmake.in
    add_definitions(-DQT_PLUGIN)
    add_definitions(-DQT_STATICPLUGIN=1)
    add_definitions(-DJAMIJAMIHISTORYVIEW_USE_STATIC_PLUGIN=1)
endif()

find_package(Qt5 CONFIG REQUIRED
    Core Gui Quick QuickControls2
)

set(GENERIC_LIB_VERSION "1.0.0")

#File to compile
set( jamihistoryview_LIB_SRCS
    plugin.cpp
)

qt5_add_resources(jamihistoryview_LIB_SRCS
   jamihistoryview.qrc
)

set(AUTOMOC_MOC_OPTIONS -Muri=org.kde.ringkde.jamihistoryview)

add_library(jamihistoryview ${jamihistoryview_LIB_SRCS} )

target_link_libraries( jamihistoryview
    # Project
    ringqt
    ringqtquick

    # Qt
    Qt5::Core
    Qt5::Gui
    Qt5::Quick
    Qt5::QuickControls2
)

# Configure the target config
set(jamihistoryview_CONFIG_PATH "${CMAKE_CURRENT_BINARY_DIR}/JamiHistoryViewConfig.cmake")

if(USES_ADD_SUBDIRECTORY)
   set(JamiHistoryView_DIR ${CMAKE_CURRENT_BINARY_DIR} PARENT_SCOPE)
endif()

configure_package_config_file(
   "${CMAKE_CURRENT_SOURCE_DIR}/cmake/JamiHistoryViewConfig.cmake.in" ${jamihistoryview_CONFIG_PATH}
   INSTALL_DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/JamiHistoryView/
   PATH_VARS INCLUDE_INSTALL_DIR
)

install( FILES ${jamihistoryview_CONFIG_PATH}
    DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/JamiHistoryView/
    COMPONENT Devel
)

# Create the target
target_include_directories(jamihistoryview
    PUBLIC
        $<INSTALL_INTERFACE:include/JamiHistoryView>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/
)

set_target_properties(jamihistoryview PROPERTIES
    PUBLIC_HEADER
    "${jamihistoryview_EXPORT_HDRS}"
)

export(TARGETS jamihistoryview
    FILE "${PROJECT_BINARY_DIR}/cmake/JamiHistoryViewTargets.cmake"
)

install(TARGETS jamihistoryview
    EXPORT JamiHistoryViewTargets
    LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}" COMPONENT jamihistoryview
    ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    COMPONENT Devel
)

install(EXPORT JamiHistoryViewTargets
    DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/JamiHistoryView
    COMPONENT Devel
)
