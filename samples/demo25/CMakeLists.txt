add_executable(demo25 main.c)

target_include_directories(demo25
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(demo25 PRIVATE aes vdi)

set_target_properties(demo25 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(demo25 PRIVATE c_std_11)

target_compile_options(demo25
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(demo25
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(demo25
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
