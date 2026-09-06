add_executable(demo22 main.c)

target_include_directories(demo22
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(demo22 PRIVATE aes vdi)

set_target_properties(demo22 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${GEM_DEMOS_DIR}
)

target_compile_features(demo22 PRIVATE c_std_11)

target_compile_options(demo22
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(demo22
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(demo22
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
