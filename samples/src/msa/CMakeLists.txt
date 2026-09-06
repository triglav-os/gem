add_executable(msa_cli
    cli.c
)

set_target_properties(msa_cli PROPERTIES
    OUTPUT_NAME msa
    RUNTIME_OUTPUT_DIRECTORY ${GEM_SAMPLES_DIR}
)

target_include_directories(msa_cli
    PRIVATE
        ${SAMPLES_ROOT}/include
)

target_compile_features(msa_cli PRIVATE c_std_11)

target_compile_options(msa_cli
    PRIVATE
        -Wall
        -Wextra
        -pedantic
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(msa_cli
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_libraries(msa_cli PRIVATE msa fat12)
