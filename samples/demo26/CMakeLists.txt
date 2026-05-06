add_executable(demo26 main.c)

target_include_directories(demo26
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(demo26 PRIVATE aes vdi)

set_target_properties(demo26 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(demo26 PRIVATE c_std_11)

target_compile_options(demo26
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(demo26
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(demo26
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
