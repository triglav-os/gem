add_executable(demo27 main.c)
add_executable(demo27_rscgen make_rsc.c)

target_include_directories(demo27
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(demo27_rscgen
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(demo27 PRIVATE aes vdi)

set_target_properties(demo27 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

set_target_properties(demo27_rscgen PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build
)

target_compile_features(demo27_rscgen PRIVATE c_std_11)

target_compile_options(demo27_rscgen
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(demo27_rscgen
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(demo27_rscgen
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

add_dependencies(demo27 demo27_rscgen)

add_custom_target(demo27_resource
    COMMAND $<TARGET_FILE:demo27_rscgen>
        ${CMAKE_CURRENT_SOURCE_DIR}/demo27.rsc
        ${PROJECT_SOURCE_DIR}/bin/demo27.rsc
    DEPENDS demo27_rscgen
)

add_dependencies(demo27 demo27_resource)

target_compile_features(demo27 PRIVATE c_std_11)

target_compile_options(demo27
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(demo27
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(demo27
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
