# The terminal is shared with the demo19 acceptance scenario.
add_executable(terminal main.c)
target_link_libraries(terminal PRIVATE gem)
target_compile_options(terminal PRIVATE -Wall -Wextra -pedantic
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(terminal PRIVATE $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
set_target_properties(terminal PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${GEM_SAMPLES_DIR})
