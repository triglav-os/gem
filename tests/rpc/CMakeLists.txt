# End-to-end tests link only the client proxy; the runner owns a real server.
add_executable(test_gem_rpc test_gem_rpc.c)
target_include_directories(test_gem_rpc PRIVATE ${PROJECT_SOURCE_DIR}/src/gem)
target_link_libraries(test_gem_rpc PRIVATE gem)
target_compile_options(test_gem_rpc PRIVATE -Wall -Wextra -pedantic -UNDEBUG
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(test_gem_rpc PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
add_test(NAME test_gem_rpc COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/run.sh
    $<TARGET_FILE:gemd> $<TARGET_FILE:test_gem_rpc>)
set_tests_properties(test_gem_rpc PROPERTIES TIMEOUT 30 ENVIRONMENT
    "GEM_RESOURCE_DIR=${GEM_OUTPUT_ROOT}/resources;ASAN_OPTIONS=detect_leaks=0")
