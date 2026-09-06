# Every acceptance demo is built against both public API implementations.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
foreach(number RANGE 1 33)
    foreach(mode direct proxy)
        set(target demo${number}_${mode})
        add_executable(${target} demo${number}/main.c)
        if(number GREATER_EQUAL 3 AND number LESS_EQUAL 16)
            target_sources(${target} PRIVATE manual/manual_demo.c)
        endif()
        target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/include)
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
# The desktop and deployment package still expose the terminal application.
add_executable(terminal demo19/main.c)
target_link_libraries(terminal PRIVATE gem)
target_compile_options(terminal PRIVATE -Wall -Wextra -pedantic
    $<$<CONFIG:Debug>:-g;-fsanitize=address,undefined>)
target_link_options(terminal PRIVATE $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
set_target_properties(terminal PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${GEM_APPS_DIR})
configure_file(demo27/demo27.rsc ${GEM_RESOURCES_DIR}/demo27.rsc COPYONLY)
