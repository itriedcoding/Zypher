# Samples are data driven; this target stages them next to the build output so
# `zypher_runtime --scene samples/demo/scene.zscene` works from the build tree.
add_custom_target(zypher_samples ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/samples"
    COMMENT "Staging sample projects")

install(DIRECTORY demo DESTINATION share/zypher/samples)
