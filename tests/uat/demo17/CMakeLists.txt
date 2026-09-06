add_executable(multi
    main.c
)

target_include_directories(multi
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(multi PRIVATE gem)

set_target_properties(multi PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${GEM_DEMOS_DIR}
)

target_compile_features(multi PRIVATE c_std_11)

target_compile_options(multi
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(multi
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(multi
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
