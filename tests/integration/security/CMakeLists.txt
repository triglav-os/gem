find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME security_disk COMMAND ${Python3_EXECUTABLE} -B
    ${CMAKE_CURRENT_SOURCE_DIR}/disk.py $<TARGET_FILE:msa_cli>
    ${CMAKE_BINARY_DIR}/security_disk)
set_tests_properties(security_disk PROPERTIES LABELS integration TIMEOUT 60)
add_test(NAME security_guest COMMAND ${Python3_EXECUTABLE} -B
    ${CMAKE_CURRENT_SOURCE_DIR}/guest.py $<TARGET_FILE:stout>
    ${CMAKE_BINARY_DIR}/security_guest)
set_tests_properties(security_guest PROPERTIES LABELS integration TIMEOUT 60)
