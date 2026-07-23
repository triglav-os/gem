add_executable(demo24 main.c)

target_include_directories(demo24
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(demo24 PRIVATE aes vdi)

set_target_properties(demo24 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${GEM_DEMOS_DIR}
)

target_compile_features(demo24 PRIVATE c_std_11)

target_compile_options(demo24
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(demo24
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(demo24
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
