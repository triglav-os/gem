add_executable(demo31 main.c)

target_include_directories(demo31
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(demo31 PRIVATE aes vdi)

set_target_properties(demo31 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(demo31 PRIVATE c_std_11)

target_compile_options(demo31
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_compile_options(demo31
    PRIVATE
        $<$<CONFIG:Debug>:-g -fsanitize=address,undefined>
)

target_link_options(demo31
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
