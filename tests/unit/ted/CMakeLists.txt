add_executable(test_ted_document test_document.c
    ${PROJECT_SOURCE_DIR}/src/apps/ted/document.c)
target_include_directories(test_ted_document PRIVATE ${PROJECT_SOURCE_DIR}/src/apps/ted)
target_compile_options(test_ted_document PRIVATE -Wall -Wextra -pedantic -UNDEBUG
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(test_ted_document PRIVATE $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
add_test(NAME test_ted_document COMMAND test_ted_document)
set_tests_properties(test_ted_document PROPERTIES LABELS unit)
