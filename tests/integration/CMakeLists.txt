add_subdirectory(rpc)
add_subdirectory(session)

add_subdirectory(security)

add_test(NAME test_rasta_polarity
    COMMAND ${CMAKE_BINARY_DIR}/deps/rasta-source/bin/test_rasta)
set_tests_properties(test_rasta_polarity PROPERTIES LABELS integration)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME test_container_identity
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/container_identity.py)
set_tests_properties(test_container_identity PROPERTIES LABELS integration TIMEOUT 15)
add_test(NAME test_container_lock
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/container_lock.py
        ${CMAKE_BINARY_DIR})
set_tests_properties(test_container_lock PROPERTIES LABELS integration TIMEOUT 15)
add_test(NAME test_rasta_startup
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/rasta_startup.py
        --rasta ${GEM_TOOLS_DIR}/rasta --binary $<TARGET_FILE:demo17_proxy>
        --gemd $<TARGET_FILE:gemd> --resources ${GEM_RESOURCES_DIR}
        --artifacts ${CMAKE_BINARY_DIR}/rasta_startup)
set_tests_properties(test_rasta_startup PROPERTIES LABELS integration TIMEOUT 15
    ENVIRONMENT "RASTA_BIN=${GEM_TOOLS_DIR}/rasta")

add_test(NAME test_runtime_paths COMMAND ${Python3_EXECUTABLE} -B
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime_paths.py)
set_tests_properties(test_runtime_paths PROPERTIES LABELS integration TIMEOUT 15)

if(GEM_PLATFORM STREQUAL "rasta")
    add_test(NAME test_install_layout COMMAND ${Python3_EXECUTABLE} -B
        ${CMAKE_CURRENT_SOURCE_DIR}/install_layout.py --build ${CMAKE_BINARY_DIR})
    set_tests_properties(test_install_layout PROPERTIES LABELS integration TIMEOUT 90)
endif()

add_test(NAME test_sdk_layout COMMAND ${Python3_EXECUTABLE} -B
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk_layout.py --build ${CMAKE_BINARY_DIR})
set_tests_properties(test_sdk_layout PROPERTIES LABELS integration TIMEOUT 120)
