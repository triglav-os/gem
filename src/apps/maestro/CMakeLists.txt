add_executable(maestro maestro.c _maestro.c)

add_dependencies(maestro gem_runtime_maestro_tree)

target_include_directories(maestro
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(maestro PRIVATE aes vdi)

set_target_properties(maestro PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(maestro PRIVATE c_std_11)

target_compile_options(maestro
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(maestro
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(maestro
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
