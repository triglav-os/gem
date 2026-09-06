add_subdirectory(rpc)
add_subdirectory(session)

add_subdirectory(security)

add_test(NAME test_rasta_polarity
    COMMAND ${CMAKE_BINARY_DIR}/deps/rasta-source/bin/test_rasta)
set_tests_properties(test_rasta_polarity PROPERTIES LABELS integration)
