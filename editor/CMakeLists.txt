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
    # Pin the QML resource prefix so the module path is stable across Qt versions.
    qt_policy(SET QTP0001 NEW)

    qt_add_executable(zypher_editor
        qt/main.cpp
        qt/EditorController.cpp
        qt/EditorController.h)

    # Metatype extraction for the QML module needs AUTOMOC; enable it before the
    # module is declared so qt_add_qml_module can register the controller types.
    set_target_properties(zypher_editor PROPERTIES AUTOMOC ON)

    # The QML files live in the executable's resources, so the shipped app never
    # depends on the source tree: `engine.loadFromModule` finds them anywhere.
    qt_add_qml_module(zypher_editor
        URI Zypher.Editor
        VERSION 1.0
        QML_FILES
            qml/Main.qml
            qml/SceneHierarchy.qml
            qml/Inspector.qml
            qml/AssetBrowser.qml
            qml/ConsolePanel.qml)

    target_link_libraries(zypher_editor PRIVATE
        zypher_editor_core
        Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)

    # Desktop application: no console window on Windows, a real .app on macOS.
    set_target_properties(zypher_editor PROPERTIES
        WIN32_EXECUTABLE TRUE
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Zypher Editor"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.zypher.editor"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

    install(TARGETS zypher_editor RUNTIME DESTINATION bin BUNDLE DESTINATION .)
else()
    message(STATUS "Qt 6 not found: building editor core only (no desktop shell)")
endif()

install(TARGETS zypher_editor_core ARCHIVE DESTINATION lib)
install(DIRECTORY core DESTINATION include/zypher-editor)
