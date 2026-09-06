add_subdirectory(vdi)
add_subdirectory(headers)
add_subdirectory(protocol)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME test_report COMMAND ${Python3_EXECUTABLE} -B
    ${CMAKE_CURRENT_SOURCE_DIR}/test_report.py ${CMAKE_BINARY_DIR}/report_probe)
set_tests_properties(test_report PROPERTIES LABELS unit)

add_subdirectory(security)
