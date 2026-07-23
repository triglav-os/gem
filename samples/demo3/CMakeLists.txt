set(demo3_sources
    main.c
    ${PROJECT_SOURCE_DIR}/samples/manual/manual_demo.c
)

add_executable(demo3 ${demo3_sources})

target_include_directories(demo3
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/samples/manual
)

target_link_libraries(demo3 PRIVATE vdi)

set_target_properties(demo3 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${GEM_DEMOS_DIR}
)

target_compile_features(demo3 PRIVATE c_std_11)

target_compile_options(demo3
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(demo3
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(demo3
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
