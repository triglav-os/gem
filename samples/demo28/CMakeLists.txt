add_executable(demo28 main.c)

target_include_directories(demo28
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(demo28 PRIVATE aes vdi)

set_target_properties(demo28 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(demo28 PRIVATE c_std_11)

target_compile_options(demo28
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_compile_options(demo28
    PRIVATE
        $<$<CONFIG:Debug>:-g -fsanitize=address,undefined>
)

target_link_options(demo28
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
