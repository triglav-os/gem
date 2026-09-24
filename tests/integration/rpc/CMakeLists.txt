# End-to-end tests link only the client proxy; the runner owns a real server.
add_executable(test_gem_rpc test_gem_rpc.c)
set_target_properties(test_gem_rpc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${GEM_OUTPUT_ROOT}/tests")
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

add_executable(test_gemd_security test_gemd_security.c)
set_target_properties(test_gemd_security PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${GEM_OUTPUT_ROOT}/tests")
target_include_directories(test_gemd_security PRIVATE ${PROJECT_SOURCE_DIR}/src/gem)
target_link_libraries(test_gemd_security PRIVATE gem)
target_compile_options(test_gemd_security PRIVATE -Wall -Wextra -pedantic -UNDEBUG
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(test_gemd_security PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
add_test(NAME test_gemd_security COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/run.sh
    $<TARGET_FILE:gemd> $<TARGET_FILE:test_gemd_security>)
set_tests_properties(test_gemd_security PROPERTIES TIMEOUT 45 ENVIRONMENT
    "GEM_RESOURCE_DIR=${GEM_OUTPUT_ROOT}/resources;ASAN_OPTIONS=detect_leaks=0")
set_tests_properties(test_gem_rpc test_gemd_security PROPERTIES RESOURCE_LOCK gem_rpc_hid)

if(GEM_PLATFORM STREQUAL "rasta")
    add_executable(test_gemd_host test_gemd_host.c)
    set_target_properties(test_gemd_host PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${GEM_OUTPUT_ROOT}/tests")
    target_link_libraries(test_gemd_host PRIVATE aes)
    target_compile_options(test_gemd_host PRIVATE -Wall -Wextra -pedantic -UNDEBUG
        $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
    target_link_options(test_gemd_host PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
    add_test(NAME test_gemd_host COMMAND test_gemd_host)
    set_tests_properties(test_gemd_host PROPERTIES TIMEOUT 10
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
endif()

get_property(local_tests DIRECTORY PROPERTY TESTS)
set_tests_properties(${local_tests} PROPERTIES LABELS integration)

add_executable(test_vdi_rpc test_vdi_rpc.c)
target_link_libraries(test_vdi_rpc PRIVATE gem)
target_compile_options(test_vdi_rpc PRIVATE -Wall -Wextra -pedantic -UNDEBUG
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(test_vdi_rpc PRIVATE $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
set_target_properties(test_vdi_rpc PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${GEM_OUTPUT_ROOT}/tests)
add_test(NAME test_vdi_rpc COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/run.sh
    $<TARGET_FILE:gemd> $<TARGET_FILE:test_vdi_rpc>)
set_tests_properties(test_vdi_rpc PROPERTIES LABELS integration TIMEOUT 30
    RESOURCE_LOCK gem_rpc_hid
    ENVIRONMENT "GEM_RESOURCE_DIR=${GEM_RESOURCES_DIR};ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1")

add_executable(test_aes_rpc test_aes_rpc.c)
add_executable(shell_clock_child shell_child.c)
add_executable(shell_term_child shell_child.c)
set_target_properties(shell_clock_child shell_term_child PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${GEM_OUTPUT_ROOT}/tests/shell_apps"
    BUILD_WITH_INSTALL_RPATH FALSE BUILD_RPATH "${GEM_LIBRARY_DIR}")
set_target_properties(shell_clock_child PROPERTIES OUTPUT_NAME "clock.app")
set_target_properties(shell_term_child PROPERTIES OUTPUT_NAME "term.app")
target_link_libraries(shell_clock_child PRIVATE gem)
target_link_libraries(shell_term_child PRIVATE gem)
foreach(shell_child IN ITEMS shell_clock_child shell_term_child)
    target_compile_options(${shell_child} PRIVATE -Wall -Wextra -pedantic
        -UNDEBUG $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
    target_link_options(${shell_child} PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
endforeach()
add_dependencies(test_aes_rpc shell_clock_child shell_term_child)
target_include_directories(test_aes_rpc PRIVATE ${PROJECT_SOURCE_DIR}/src/gem)
target_link_libraries(test_aes_rpc PRIVATE gem gemconf)
target_compile_options(test_aes_rpc PRIVATE -Wall -Wextra -pedantic -UNDEBUG
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(test_aes_rpc PRIVATE $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
set_target_properties(test_aes_rpc PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${GEM_OUTPUT_ROOT}/tests)
add_test(NAME test_aes_rpc COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/run.sh
    $<TARGET_FILE:gemd> $<TARGET_FILE:test_aes_rpc>)
set_tests_properties(test_aes_rpc PROPERTIES LABELS integration TIMEOUT 30
    RESOURCE_LOCK gem_rpc_hid
    ENVIRONMENT
        "GEM_RESOURCE_DIR=${GEM_RESOURCES_DIR};GEMIX_HOME=$<TARGET_FILE_DIR:shell_clock_child>;ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1")
