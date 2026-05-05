add_executable(demo23 main.c)

target_include_directories(demo23
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(demo23 PRIVATE aes vdi)

set_target_properties(demo23 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(demo23 PRIVATE c_std_11)

target_compile_options(demo23
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(demo23
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(demo23
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
