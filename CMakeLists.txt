# SPDX-FileCopyrightText: 2026 LlamaAmp contributors
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.22)

project(LlamaAmpQt
    VERSION 0.7.0
    DESCRIPTION "A native Qt 6 music player for Linux"
    LANGUAGES CXX
)

option(LLAMAAMP_QT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets Sql)

qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

qt_add_library(llamaamp-qt-core STATIC
    Src/qt6/Equalizer.cpp
    Src/qt6/Equalizer.h
    Src/qt6/MediaLibrary.cpp
    Src/qt6/MediaLibrary.h
    Src/qt6/PlayerController.cpp
    Src/qt6/PlayerController.h
    Src/qt6/PluginManager.cpp
    Src/qt6/PluginManager.h
    Src/qt6/LlamaAmpPlugin.h
    Src/qt6/PlaylistModel.cpp
    Src/qt6/PlaylistModel.h
    Src/qt6/SkinManager.cpp
    Src/qt6/SkinManager.h
    Src/qt6/WasabiRuntime.cpp
    Src/qt6/WasabiRuntime.h
)

target_compile_features(llamaamp-qt-core PUBLIC cxx_std_20)
target_link_libraries(llamaamp-qt-core PUBLIC Qt6::Core Qt6::Gui Qt6::Multimedia Qt6::Sql)
target_include_directories(llamaamp-qt-core PUBLIC Src/qt6)

qt_add_plugin(llamaamp-softclip-plugin
    CLASS_NAME SoftClipPlugin
    SHARED
    Plugins/qt6/SoftClipPlugin.cpp
    Plugins/qt6/SoftClipPlugin.h
    Plugins/qt6/softclip.json
)
target_compile_features(llamaamp-softclip-plugin PRIVATE cxx_std_20)
target_include_directories(llamaamp-softclip-plugin PRIVATE Src/qt6)
target_link_libraries(llamaamp-softclip-plugin PRIVATE Qt6::Core)
set_target_properties(llamaamp-softclip-plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugins"
)

qt_add_executable(llamaamp-qt
    Src/qt6/AudioVisualizationWidget.cpp
    Src/qt6/AudioVisualizationWidget.h
    Src/qt6/main.cpp
    Src/qt6/MainWindow.cpp
    Src/qt6/MainWindow.h
    Src/qt6/WasabiPlayerWidget.cpp
    Src/qt6/WasabiPlayerWidget.h
    Src/qt6/resources.qrc
)

target_compile_features(llamaamp-qt PRIVATE cxx_std_20)
target_link_libraries(llamaamp-qt PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Sql
    llamaamp-qt-core
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(llamaamp-qt PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(llamaamp-qt-core PRIVATE -Wall -Wextra -Wpedantic)
    if(LLAMAAMP_QT_WARNINGS_AS_ERRORS)
        target_compile_options(llamaamp-qt PRIVATE -Werror)
        target_compile_options(llamaamp-qt-core PRIVATE -Werror)
    endif()
endif()

set_target_properties(llamaamp-qt PROPERTIES
    OUTPUT_NAME "llamaamp"
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

include(GNUInstallDirs)
install(TARGETS llamaamp-qt
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS llamaamp-softclip-plugin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/llamaamp/plugins
)
install(FILES packaging/linux/io.github.farokhpakbaz.LlamaAmp.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)
install(FILES packaging/linux/io.github.farokhpakbaz.LlamaAmp.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)
install(FILES Src/qt6/LlamaAmpPlugin.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/llamaamp-qt
)
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)

    add_test(NAME llamaamp-qt-cli-smoke COMMAND llamaamp-qt --help)
    set_tests_properties(llamaamp-qt-cli-smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 10
    )
    add_test(NAME llamaamp-qt-ui-smoke
        COMMAND llamaamp-qt --screenshot ${CMAKE_CURRENT_BINARY_DIR}/llamaamp-ui-smoke.png)
    set_tests_properties(llamaamp-qt-ui-smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LLAMAAMP_QT_NO_AUDIO=1;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-config"
        TIMEOUT 10
    )
    add_test(NAME llamaamp-qt-invalid-skin
        COMMAND llamaamp-qt --skin "This skin does not exist"
                --screenshot ${CMAKE_CURRENT_BINARY_DIR}/invalid-skin.png)
    set_tests_properties(llamaamp-qt-invalid-skin PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LLAMAAMP_QT_NO_AUDIO=1;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-config-invalid-skin"
        WILL_FAIL TRUE
        TIMEOUT 10
    )

    qt_add_executable(playlist-model-test Tests/qt6/PlaylistModelTest.cpp)
    target_link_libraries(playlist-model-test PRIVATE llamaamp-qt-core Qt6::Test)
    add_test(NAME playlist-model-test COMMAND playlist-model-test)

    qt_add_executable(core-subsystem-test Tests/qt6/CoreSubsystemTest.cpp)
    target_link_libraries(core-subsystem-test PRIVATE llamaamp-qt-core Qt6::Test)
    add_dependencies(core-subsystem-test llamaamp-softclip-plugin)
    add_test(NAME core-subsystem-test COMMAND core-subsystem-test)
    set_tests_properties(core-subsystem-test PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    qt_add_executable(wasabi-widget-test
        Tests/qt6/WasabiWidgetTest.cpp
        Src/qt6/AudioVisualizationWidget.cpp
        Src/qt6/AudioVisualizationWidget.h
        Src/qt6/WasabiPlayerWidget.cpp
        Src/qt6/WasabiPlayerWidget.h
    )
    target_link_libraries(wasabi-widget-test PRIVATE
        llamaamp-qt-core Qt6::Test Qt6::Widgets Qt6::MultimediaWidgets)
    add_test(NAME wasabi-widget-test COMMAND wasabi-widget-test)
    set_tests_properties(wasabi-widget-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;LLAMAAMP_QT_NO_AUDIO=1"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()
