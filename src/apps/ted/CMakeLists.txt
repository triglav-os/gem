add_executable(ted ted.c document.c view.c toolbar.c)
target_link_libraries(ted PRIVATE gem gemctl)
set_target_properties(ted PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${GEM_APPS_DIR})
target_compile_options(ted PRIVATE -Wall -Wextra -pedantic
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(ted PRIVATE $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
