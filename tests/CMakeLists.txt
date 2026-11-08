add_library(zypher_test_script SHARED TestScriptModule.cpp)
target_link_libraries(zypher_test_script PRIVATE zypher_engine)
set_target_properties(zypher_test_script PROPERTIES
    OUTPUT_NAME "zypher_test_script"
    PREFIX "")
if(APPLE)
    set_target_properties(zypher_test_script PROPERTIES SUFFIX ".dylib")
endif()

add_executable(zypher_tests
    test_main.cpp
    EngineTests.cpp
    ScriptTests.cpp)
target_link_libraries(zypher_tests PRIVATE zypher_engine ${CMAKE_DL_LIBS})
target_compile_definitions(zypher_tests PRIVATE
    ZYPHER_TEST_DEMO_SCENE="${CMAKE_SOURCE_DIR}/samples/demo/scene.zscene"
    ZYPHER_TEST_SCRIPT_MODULE="$<TARGET_FILE:zypher_test_script>")
add_dependencies(zypher_tests zypher_test_script)
set_target_properties(zypher_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_test(NAME zypher_unit_tests COMMAND zypher_tests)

if(ZYPHER_BUILD_RUNTIME)
    add_test(NAME zypher_runtime_smoke
        COMMAND zypher_runtime
            --scene "${CMAKE_SOURCE_DIR}/samples/demo/scene.zscene"
            --output "${CMAKE_BINARY_DIR}/test-output"
            --width 320 --height 240 --frames 2 --steps 60 --dump-frame)
    set_tests_properties(zypher_runtime_smoke PROPERTIES
        PASS_REGULAR_EXPRESSION "simulated")
endif()

# Optional integration test: requires a cooked pack produced by
# tools/assetcooker (see tools/scripts/run_checks.py).
add_test(NAME zypher_runtime_pack
    COMMAND zypher_runtime
        --scene "${CMAKE_SOURCE_DIR}/samples/demo/scene.zscene"
        --pack "${CMAKE_SOURCE_DIR}/samples/demo/dist/demo.zpkg"
        --output "${CMAKE_BINARY_DIR}/test-output-pack"
        --width 320 --height 240 --frames 2 --steps 60 --dump-frame)
set_tests_properties(zypher_runtime_pack PROPERTIES
    SKIP_RETURN_CODE 77)
