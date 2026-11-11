# Frame geometry, client clipping and resize-corner ownership.
add_executable(test_window_frame test_window_frame.c)
target_include_directories(test_window_frame PRIVATE ${PROJECT_SOURCE_DIR}/src/aes)
target_link_libraries(test_window_frame PRIVATE aes)
target_compile_options(test_window_frame PRIVATE -Wall -Wextra -pedantic -UNDEBUG
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(test_window_frame PRIVATE $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
set_target_properties(test_window_frame PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${GEM_OUTPUT_ROOT}/tests")
add_test(NAME test_window_frame COMMAND test_window_frame)
set_tests_properties(test_window_frame PROPERTIES LABELS unit
    ENVIRONMENT "GEM_RESOURCE_DIR=${GEM_RESOURCES_DIR};ASAN_OPTIONS=detect_leaks=0")
