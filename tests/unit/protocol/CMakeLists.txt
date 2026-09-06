add_executable(test_protocol test_protocol.c
    ${PROJECT_SOURCE_DIR}/src/gem/vdi_validate.c
    ${PROJECT_SOURCE_DIR}/src/gem/aes_validate.c
    ${PROJECT_SOURCE_DIR}/src/gem/tree_decode.c
    ${PROJECT_SOURCE_DIR}/src/gem/bitmap_size.c)
target_include_directories(test_protocol PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_compile_options(test_protocol PRIVATE -Wall -Wextra -pedantic -UNDEBUG
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(test_protocol PRIVATE $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
add_test(NAME test_protocol COMMAND test_protocol)
set_tests_properties(test_protocol PROPERTIES LABELS unit)
