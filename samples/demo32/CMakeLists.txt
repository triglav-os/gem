add_executable(demo32 main.c)

target_include_directories(demo32
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(demo32 PRIVATE aes vdi)

set_target_properties(demo32 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(demo32 PRIVATE c_std_11)

target_compile_options(demo32
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_compile_options(demo32
    PRIVATE
        $<$<CONFIG:Debug>:-g -fsanitize=address,undefined>
)

target_link_options(demo32
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
