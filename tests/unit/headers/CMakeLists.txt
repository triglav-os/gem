# Public GEM header self-containment checks.

add_executable(test_gem_header test_gem_header.c)

target_include_directories(test_gem_header
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(test_gem_header PRIVATE c_std_11)

target_compile_options(test_gem_header
    PRIVATE
        -Wall
        -Wextra
        -pedantic
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(test_gem_header
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

add_test(NAME test_gem_header COMMAND test_gem_header)

get_property(local_tests DIRECTORY PROPERTY TESTS)
set_tests_properties(${local_tests} PROPERTIES LABELS unit)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME test_api_surface COMMAND ${Python3_EXECUTABLE} -B
    ${CMAKE_CURRENT_SOURCE_DIR}/test_api_surface.py
    ${PROJECT_SOURCE_DIR} $<TARGET_FILE:gem>)
set_tests_properties(test_api_surface PROPERTIES LABELS unit)
