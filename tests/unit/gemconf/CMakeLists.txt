add_executable(test_gemconf test_gemconf.c)

target_include_directories(test_gemconf PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_gemconf PRIVATE gemconf platform_linux)
set_target_properties(test_gemconf PROPERTIES BUILD_WITH_INSTALL_RPATH FALSE
    BUILD_RPATH "${GEM_LIBRARY_DIR}")
target_compile_options(test_gemconf PRIVATE -Wall -Wextra -pedantic -UNDEBUG
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(test_gemconf PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>)

add_test(NAME test_gemconf COMMAND test_gemconf)
set_tests_properties(test_gemconf PROPERTIES LABELS unit
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
