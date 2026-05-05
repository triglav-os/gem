add_executable(calc main.c)

target_include_directories(calc
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(calc PRIVATE aes vdi)

set_target_properties(calc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(calc PRIVATE c_std_11)

target_compile_options(calc
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(calc
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(calc
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
