add_executable(clock main.c)

target_include_directories(clock
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(clock PRIVATE aes vdi)

set_target_properties(clock PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(clock PRIVATE c_std_11)

target_compile_options(clock
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(clock
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(clock
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
