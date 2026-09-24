add_executable(test_prg test_prg.c)

target_include_directories(test_prg PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_prg PRIVATE prg platform_linux)
set_target_properties(test_prg PROPERTIES BUILD_WITH_INSTALL_RPATH FALSE
    BUILD_RPATH "${GEM_LIBRARY_DIR}")
target_compile_options(test_prg PRIVATE -Wall -Wextra -pedantic -UNDEBUG
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(test_prg PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>)

add_test(NAME test_prg COMMAND test_prg)
set_tests_properties(test_prg PROPERTIES LABELS unit
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
