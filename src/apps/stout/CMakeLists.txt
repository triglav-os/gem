add_executable(stout main.c)

set_target_properties(stout PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${GEM_APPS_DIR}
)

target_link_libraries(stout PRIVATE libstout)

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
