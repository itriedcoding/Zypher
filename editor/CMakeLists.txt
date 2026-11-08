# Zypher editor.
#
# The editor *core* (project/session/undo logic) is dependency free and always
# builds with ZYPHER_BUILD_EDITOR=ON, including in headless CI. The Qt 6/QML
# desktop shell is built only when Qt 6.5+ was found.

add_library(zypher_editor_core STATIC core/EditorSession.cpp)
add_library(Zypher::EditorCore ALIAS zypher_editor_core)
target_include_directories(zypher_editor_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(zypher_editor_core PUBLIC zypher_engine)

if(ZYPHER_BUILD_TESTS)
    add_executable(zypher_editor_tests tests/editor_tests.cpp)
    target_link_libraries(zypher_editor_tests PRIVATE zypher_editor_core)
    set_target_properties(zypher_editor_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME zypher_editor_tests COMMAND zypher_editor_tests)
endif()

if(ZYPHER_EDITOR_QT)
    qt_add_executable(zypher_editor
        qt/main.cpp
        qt/EditorController.cpp
        qt/EditorController.h)
    target_link_libraries(zypher_editor PRIVATE
        zypher_editor_core
        Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
    target_compile_definitions(zypher_editor PRIVATE
        ZYPHER_EDITOR_QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")

    qt_add_qml_module(zypher_editor_ui
        URI Zypher.Editor
        VERSION 1.0
        QML_FILES
            qml/Main.qml
            qml/SceneHierarchy.qml
            qml/Inspector.qml
            qml/AssetBrowser.qml
            qml/ConsolePanel.qml)
    target_link_libraries(zypher_editor PRIVATE zypher_editor_uiplugin)

    install(TARGETS zypher_editor RUNTIME DESTINATION bin)
else()
    message(STATUS "Qt 6 not found: building editor core only (no desktop shell)")
endif()

install(TARGETS zypher_editor_core ARCHIVE DESTINATION lib)
install(DIRECTORY core DESTINATION include/zypher-editor)
