add_executable(maestro maestro.c maestro_ui.c)

if(TARGET gem_runtime_maestro_tree)
    add_dependencies(maestro gem_runtime_maestro_tree)
endif()

target_include_directories(maestro
    PRIVATE
        ${SAMPLES_ROOT}/include
)

target_link_libraries(maestro PRIVATE gem)

set_target_properties(maestro PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${GEM_SAMPLES_DIR}
)

target_compile_features(maestro PRIVATE c_std_11)

target_compile_options(maestro
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(maestro
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(maestro
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

# The direct variant remains available for isolated display sessions.
add_executable(maestro_hosted maestro.c maestro_ui.c)
target_link_libraries(maestro_hosted PRIVATE aes vdi)
target_include_directories(maestro_hosted PRIVATE ${SAMPLES_ROOT}/include)
target_compile_options(maestro_hosted PRIVATE -Wall -Wextra -pedantic
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(maestro_hosted PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
set_target_properties(maestro_hosted PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${GEM_SAMPLES_DIR})
