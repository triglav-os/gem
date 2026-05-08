add_executable(gemd
    main.c
)

target_include_directories(gemd
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/aes
        ${PROJECT_SOURCE_DIR}/src/gem
)

target_link_libraries(gemd PRIVATE aes)

set_target_properties(gemd PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(gemd PRIVATE c_std_11)

target_compile_options(gemd
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(gemd
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(gemd
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
