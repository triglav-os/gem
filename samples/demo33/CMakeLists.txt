add_executable(demo33 main.c)

target_include_directories(demo33
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(demo33 PRIVATE aes vdi)

set_target_properties(demo33 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${GEM_DEMOS_DIR}
)

target_compile_features(demo33 PRIVATE c_std_11)

target_compile_options(demo33
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(demo33
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(demo33
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
