add_executable(clock clock.c _clock.c)

add_dependencies(clock gem_runtime_clock_numbers)

target_include_directories(clock
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(clock PRIVATE aes vdi m)

set_target_properties(clock PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
    OUTPUT_NAME clock_hosted
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

add_executable(clock_gemd clock.c _clock.c)

add_dependencies(clock_gemd gem_runtime_clock_numbers)

target_include_directories(clock_gemd
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(clock_gemd PRIVATE gem m)

target_compile_definitions(clock_gemd PRIVATE CLOCK_GEMD_CLIENT=1)

set_target_properties(clock_gemd PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
    OUTPUT_NAME clock
)

target_compile_features(clock_gemd PRIVATE c_std_11)

target_compile_options(clock_gemd
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(clock_gemd
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(clock_gemd
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
