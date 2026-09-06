add_executable(gemscape gemscape.c gemscape_ui.c)

if(TARGET gem_runtime_gemscape_toolbar)
    add_dependencies(gemscape gem_runtime_gemscape_toolbar)
endif()

target_include_directories(gemscape
    PRIVATE
        ${SAMPLES_ROOT}/include
)

target_link_libraries(gemscape PRIVATE gem)

set_target_properties(gemscape PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${GEM_SAMPLES_DIR}
)

target_compile_features(gemscape PRIVATE c_std_11)

target_compile_options(gemscape
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(gemscape
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(gemscape
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

# The direct variant remains available for isolated display sessions.
add_executable(gemscape_hosted gemscape.c gemscape_ui.c)
target_link_libraries(gemscape_hosted PRIVATE aes vdi)
target_include_directories(gemscape_hosted PRIVATE ${SAMPLES_ROOT}/include)
target_compile_options(gemscape_hosted PRIVATE -Wall -Wextra -pedantic
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(gemscape_hosted PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
set_target_properties(gemscape_hosted PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${GEM_SAMPLES_DIR})
