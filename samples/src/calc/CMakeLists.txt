add_executable(calc calc.c calc_ui.c)

target_include_directories(calc
    PRIVATE
        ${SAMPLES_ROOT}/include
)

target_link_libraries(calc PRIVATE aes vdi)

set_target_properties(calc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${GEM_SAMPLES_DIR}
    OUTPUT_NAME calc_hosted
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

# Same sources, unmodified, built as a gemd RPC client instead of a
# hosted single-process app -- so it can run concurrently with other
# independent GEM processes sharing one gemd desktop.
add_executable(calc_gemd calc.c calc_ui.c)

target_include_directories(calc_gemd
    PRIVATE
        ${SAMPLES_ROOT}/include
)

target_link_libraries(calc_gemd PRIVATE gem)

set_target_properties(calc_gemd PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${GEM_SAMPLES_DIR}
    OUTPUT_NAME calc
)

target_compile_features(calc_gemd PRIVATE c_std_11)

target_compile_options(calc_gemd
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(calc_gemd
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(calc_gemd
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
