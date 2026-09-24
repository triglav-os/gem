# Every acceptance demo is built against both public API implementations.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
foreach(number RANGE 1 34)
    foreach(mode direct proxy)
        set(target demo${number}_${mode})
        if(number EQUAL 19)
            add_executable(${target}
                ${PROJECT_SOURCE_DIR}/src/apps/terminal/main.c
                ${PROJECT_SOURCE_DIR}/src/apps/terminal/screen.c
                ${PROJECT_SOURCE_DIR}/src/apps/terminal/draw.c
                ${PROJECT_SOURCE_DIR}/src/apps/terminal/vt100.c)
        else()
            add_executable(${target} demo${number}/main.c)
        endif()
        if(number GREATER_EQUAL 3 AND number LESS_EQUAL 16)
            target_sources(${target} PRIVATE manual/manual_demo.c)
        endif()
        if(number EQUAL 34)
            target_compile_definitions(${target} PRIVATE
                DEMO34_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/tests/data/uat/demo34_fixture")
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

# Copy and Move drops between two normal File Manager windows.
foreach(mode direct proxy)
    if(mode STREQUAL "direct")
        set(file_drop_desktop_target desktop_hosted)
    else()
        set(file_drop_desktop_target desktop)
    endif()
    add_test(NAME uat_file_drop_${mode}
        COMMAND ${Python3_EXECUTABLE} -B
            ${CMAKE_CURRENT_SOURCE_DIR}/samples/file_drop.py
            --mode ${mode} --binary $<TARGET_FILE:${file_drop_desktop_target}>
            --gemd $<TARGET_FILE:gemd> --artifacts ${CMAKE_BINARY_DIR}/uat
            --resources ${GEM_RESOURCES_DIR})
    set_tests_properties(uat_file_drop_${mode} PROPERTIES
        ENVIRONMENT "RASTA_BIN=${GEM_TOOLS_DIR}/rasta"
        LABELS "uat;${mode}" TIMEOUT 30)
endforeach()

# Trash drop, cancel-default confirmation and confirmed emptying.
foreach(mode direct proxy)
    if(mode STREQUAL "direct")
        set(trash_desktop_target desktop_hosted)
    else()
        set(trash_desktop_target desktop)
    endif()
    add_test(NAME uat_trash_${mode}
        COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/samples/trash.py
            --mode ${mode} --binary $<TARGET_FILE:${trash_desktop_target}>
            --gemd $<TARGET_FILE:gemd> --artifacts ${CMAKE_BINARY_DIR}/uat
            --resources ${GEM_RESOURCES_DIR})
    set_tests_properties(uat_trash_${mode} PROPERTIES
        ENVIRONMENT "RASTA_BIN=${GEM_TOOLS_DIR}/rasta"
        LABELS "uat;${mode}" TIMEOUT 30)
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

# Gemread must expose a usable File/Open menu even before a document is open.
foreach(mode direct proxy)
    if(mode STREQUAL "direct")
        set(gemread_target gemread_hosted)
    else()
        set(gemread_target gemread)
    endif()
    add_test(NAME uat_gemread_${mode}
        COMMAND ${Python3_EXECUTABLE} -B
            ${CMAKE_CURRENT_SOURCE_DIR}/samples/gemread.py
            --mode ${mode} --binary $<TARGET_FILE:${gemread_target}>
            --gemd $<TARGET_FILE:gemd> --artifacts ${CMAKE_BINARY_DIR}/uat
            --resources ${GEM_RESOURCES_DIR})
    set_tests_properties(uat_gemread_${mode} PROPERTIES
        ENVIRONMENT "RASTA_BIN=${GEM_TOOLS_DIR}/rasta"
        LABELS "uat;${mode}" TIMEOUT 45)
endforeach()

add_test(NAME uat_gemread_pdf_proxy
    COMMAND ${Python3_EXECUTABLE} -B
        ${CMAKE_CURRENT_SOURCE_DIR}/samples/gemread.py
        --mode proxy --binary $<TARGET_FILE:gemread>
        --pdf-fixture
        --gemd $<TARGET_FILE:gemd> --artifacts ${CMAKE_BINARY_DIR}/uat
        --resources ${GEM_RESOURCES_DIR})
set_tests_properties(uat_gemread_pdf_proxy PROPERTIES
    ENVIRONMENT "RASTA_BIN=${GEM_TOOLS_DIR}/rasta"
    LABELS "uat;proxy" TIMEOUT 45)

add_test(NAME uat_gemread_epub_proxy
    COMMAND ${Python3_EXECUTABLE} -B
        ${CMAKE_CURRENT_SOURCE_DIR}/samples/gemread.py
        --mode proxy --binary $<TARGET_FILE:gemread> --epub-fixture
        --gemd $<TARGET_FILE:gemd> --artifacts ${CMAKE_BINARY_DIR}/uat
        --resources ${GEM_RESOURCES_DIR})
set_tests_properties(uat_gemread_epub_proxy PROPERTIES
    ENVIRONMENT "RASTA_BIN=${GEM_TOOLS_DIR}/rasta"
    LABELS "uat;proxy" TIMEOUT 45)

# Gemscape's build target prepares its verified CEF dependency. The UAT uses
# local pages and therefore has no network dependency once the target is built.
if(TARGET gemscape)
    add_test(NAME uat_gemscape_proxy
        COMMAND ${Python3_EXECUTABLE} -B
            ${CMAKE_CURRENT_SOURCE_DIR}/samples/gemscape.py
            --binary $<TARGET_FILE:gemscape> --cover $<TARGET_FILE:calc_gemd>
            --gemd $<TARGET_FILE:gemd> --artifacts ${CMAKE_BINARY_DIR}/uat
            --resources ${GEM_RESOURCES_DIR})
    set_tests_properties(uat_gemscape_proxy PROPERTIES
        ENVIRONMENT "RASTA_BIN=${GEM_TOOLS_DIR}/rasta"
        LABELS "uat;proxy" TIMEOUT 75)
endif()

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
