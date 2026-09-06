add_executable(demo29 main.c)

target_include_directories(demo29
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(demo29 PRIVATE aes vdi)

set_target_properties(demo29 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${GEM_DEMOS_DIR}
)

target_compile_features(demo29 PRIVATE c_std_11)

target_compile_options(demo29
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_compile_options(demo29
    PRIVATE
        $<$<CONFIG:Debug>:-g -fsanitize=address,undefined>
)

target_link_options(demo29
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
