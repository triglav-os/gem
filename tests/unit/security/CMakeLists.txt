add_executable(test_security_bounds bounds.c)
target_include_directories(test_security_bounds PRIVATE
    $<TARGET_PROPERTY:musashi,BINARY_DIR>/generated)
target_link_libraries(test_security_bounds PRIVATE fat12 musashi m)
target_compile_options(test_security_bounds PRIVATE -Wall -Wextra -pedantic
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(test_security_bounds PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
add_test(NAME test_security_bounds COMMAND test_security_bounds)
set_tests_properties(test_security_bounds PROPERTIES LABELS unit)
set_target_properties(test_security_bounds PROPERTIES
    BUILD_WITH_INSTALL_RPATH FALSE)
target_compile_definitions(test_security_bounds PRIVATE M68K_TRAP_HAS_CALLBACK=1)
