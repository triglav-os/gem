add_executable(stout
    main.c
)

set_target_properties(stout PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${GEM_SAMPLES_DIR}
)

target_include_directories(stout
    PRIVATE
        ${SAMPLES_ROOT}/include
)

target_link_libraries(stout PRIVATE
    gem
    prg
    musashi
    m
)

target_compile_features(stout PRIVATE c_std_11)

target_compile_options(stout
    PRIVATE
        -Wall
        -Wextra
        -pedantic
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(stout
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
