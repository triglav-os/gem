add_executable(demo30 main.c)

target_include_directories(demo30
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(demo30 PRIVATE aes vdi)

set_target_properties(demo30 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${GEM_DEMOS_DIR}
)

target_compile_features(demo30 PRIVATE c_std_11)

target_compile_options(demo30
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_compile_options(demo30
    PRIVATE
        $<$<CONFIG:Debug>:-g -fsanitize=address,undefined>
)

target_link_options(demo30
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
