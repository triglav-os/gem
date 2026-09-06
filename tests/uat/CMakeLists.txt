# Every acceptance demo is built against both public API implementations.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
foreach(number RANGE 1 33)
    foreach(mode direct proxy)
        set(target demo${number}_${mode})
        if(number EQUAL 19)
            add_executable(${target} ${PROJECT_SOURCE_DIR}/samples/src/terminal/main.c)
        else()
            add_executable(${target} demo${number}/main.c)
        endif()
        if(number GREATER_EQUAL 3 AND number LESS_EQUAL 16)
            target_sources(${target} PRIVATE manual/manual_demo.c)
        endif()
            add_test(NAME uat_demo${number}_${mode}
                COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/run.py
                    --demo ${number} --mode ${mode}
                    --binary $<TARGET_FILE:${target}>
                    --gemd $<TARGET_FILE:gemd>
                    --artifacts ${CMAKE_BINARY_DIR}/uat
                    --resources ${GEM_RESOURCES_DIR})
            set_tests_properties(uat_demo${number}_${mode} PROPERTIES
                ENVIRONMENT "RASTA_BIN=${GEM_TOOLS_DIR}/rasta" LABELS "uat;${mode}" TIMEOUT 45)
        target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/tests/include)
        if(mode STREQUAL "direct")
            target_link_libraries(${target} PRIVATE aes vdi)
        else()
            target_link_libraries(${target} PRIVATE gem)
        endif()
        target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic
            $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
        target_link_options(${target} PRIVATE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${GEM_OUTPUT_ROOT}/tests/uat
            BUILD_WITH_INSTALL_RPATH FALSE
            BUILD_RPATH "${GEM_LIBRARY_DIR}")
    endforeach()
endforeach()
add_executable(demo27_rscgen demo27/make_rsc.c)
target_include_directories(demo27_rscgen PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/tests/include)
target_compile_options(demo27_rscgen PRIVATE -Wall -Wextra -pedantic
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(demo27_rscgen PRIVATE $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
set_target_properties(demo27_rscgen PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
add_custom_command(OUTPUT ${GEM_RESOURCES_DIR}/demo27.rsc
    COMMAND demo27_rscgen ${GEM_RESOURCES_DIR}/demo27.rsc
    DEPENDS demo27_rscgen)
add_custom_target(demo27_resource DEPENDS ${GEM_RESOURCES_DIR}/demo27.rsc)
add_dependencies(demo27_direct demo27_resource)
add_dependencies(demo27_proxy demo27_resource)

# Calculator is an independent sample; exercise the shipped binaries.
foreach(mode direct proxy)
    if(mode STREQUAL "direct")
        set(calculator_target calc)
    else()
        set(calculator_target calc_gemd)
    endif()
    add_test(NAME uat_calc_${mode}
        COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/samples/calculator.py
            --mode ${mode} --binary $<TARGET_FILE:${calculator_target}>
            --gemd $<TARGET_FILE:gemd> --artifacts ${CMAKE_BINARY_DIR}/uat
            --resources ${GEM_RESOURCES_DIR})
    set_tests_properties(uat_calc_${mode} PROPERTIES ENVIRONMENT "RASTA_BIN=${GEM_TOOLS_DIR}/rasta" LABELS "uat;${mode}" TIMEOUT 30)
endforeach()

# Desktop icons and file manager navigation through the shipped binaries.
foreach(mode direct proxy)
    if(mode STREQUAL "direct")
        set(desktop_target desktop_hosted)
    else()
        set(desktop_target desktop)
    endif()
    add_test(NAME uat_desktop_${mode}
        COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/samples/desktop.py
            --mode ${mode} --binary $<TARGET_FILE:${desktop_target}>
            --gemd $<TARGET_FILE:gemd> --artifacts ${CMAKE_BINARY_DIR}/uat
            --resources ${GEM_RESOURCES_DIR})
    set_tests_properties(uat_desktop_${mode} PROPERTIES ENVIRONMENT "RASTA_BIN=${GEM_TOOLS_DIR}/rasta" LABELS "uat;${mode}" TIMEOUT 30)
endforeach()
