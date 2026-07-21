add_executable(terminal main.c)

target_include_directories(terminal
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(terminal PRIVATE gem)

set_target_properties(terminal PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(terminal PRIVATE c_std_11)

target_compile_options(terminal
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_compile_options(terminal
    PRIVATE
        $<$<CONFIG:Debug>:-g -fsanitize=address,undefined>
)

target_link_options(terminal
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
