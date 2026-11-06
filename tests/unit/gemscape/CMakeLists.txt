add_executable(test_browser_preferences test_browser_preferences.c
    ${PROJECT_SOURCE_DIR}/src/apps/gemscape/preferences.c
    ${PROJECT_SOURCE_DIR}/lib/gemctl/text_edit.c)
target_include_directories(test_browser_preferences PRIVATE
    ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/apps/gemscape)
target_link_libraries(test_browser_preferences PRIVATE gemconf platform_linux)
set_target_properties(test_browser_preferences PROPERTIES BUILD_WITH_INSTALL_RPATH FALSE
    BUILD_RPATH "${GEM_LIBRARY_DIR}")
target_compile_options(test_browser_preferences PRIVATE -Wall -Wextra -pedantic -UNDEBUG
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(test_browser_preferences PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
add_test(NAME test_browser_preferences COMMAND test_browser_preferences)
set_tests_properties(test_browser_preferences PROPERTIES LABELS unit)
