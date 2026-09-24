add_executable(test_aes_shell test_aes_shell.c)
add_executable(test_shell_fixture shell_fixture.c)

set_target_properties(test_aes_shell test_shell_fixture PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${GEM_OUTPUT_ROOT}/tests/shell")
set_target_properties(test_aes_shell PROPERTIES BUILD_WITH_INSTALL_RPATH FALSE
    BUILD_RPATH "${GEM_LIBRARY_DIR}")
set_target_properties(test_shell_fixture PROPERTIES OUTPUT_NAME "clock.app")

target_include_directories(test_aes_shell PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_aes_shell PRIVATE aes gemconf Threads::Threads)
target_compile_options(test_aes_shell PRIVATE -Wall -Wextra -pedantic -UNDEBUG
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(test_aes_shell PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>)

target_compile_options(test_shell_fixture PRIVATE -Wall -Wextra -pedantic)

add_test(NAME test_aes_shell COMMAND test_aes_shell notes.txt)
set_tests_properties(test_aes_shell PROPERTIES LABELS unit
    ENVIRONMENT
        "GEMIX_HOME=$<TARGET_FILE_DIR:test_shell_fixture>;SHELL_TEST_VALUE=works;ASAN_OPTIONS=detect_leaks=0")
