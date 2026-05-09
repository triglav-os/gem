add_executable(gemscape gemscape.c _gemscape.c)

add_dependencies(gemscape gem_runtime_gemscape_toolbar)

target_include_directories(gemscape
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(gemscape PRIVATE aes vdi)

set_target_properties(gemscape PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(gemscape PRIVATE c_std_11)

target_compile_options(gemscape
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(gemscape
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(gemscape
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
