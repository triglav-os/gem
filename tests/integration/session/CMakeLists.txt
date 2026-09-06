# Exercise the same supervisor as F5 in a private, headless session.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME test_sample_session
    COMMAND ${Python3_EXECUTABLE} -B
        ${PROJECT_SOURCE_DIR}/tools/scripts/sample_session.py check
        --directory ${CMAKE_BINARY_DIR}/sample_session_test)
set_tests_properties(test_sample_session PROPERTIES
    LABELS integration TIMEOUT 60
    ENVIRONMENT "SDL_VIDEODRIVER=dummy;STOUT_TRACE=1;RASTA_BIN=${GEM_TOOLS_DIR}/rasta;GEM_RUNTIME_ROOT=${GEM_OUTPUT_ROOT}")
