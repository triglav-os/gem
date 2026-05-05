add_executable(rcs main.c)

target_include_directories(rcs
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(rcs PRIVATE vdi)

set_target_properties(rcs PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(rcs PRIVATE c_std_11)

target_compile_options(rcs
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(rcs
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(rcs
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
