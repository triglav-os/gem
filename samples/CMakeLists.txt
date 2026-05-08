add_subdirectory(demo1)
add_subdirectory(demo2)
add_subdirectory(demo3)
add_subdirectory(demo20)
add_subdirectory(demo21)
add_subdirectory(demo22)
add_subdirectory(demo23)
add_subdirectory(demo24)
add_subdirectory(demo25)
add_subdirectory(demo26)
add_subdirectory(demo27)
add_subdirectory(demo28)
add_subdirectory(demo29)
add_subdirectory(demo30)
add_subdirectory(demo31)
add_subdirectory(demo32)

function(add_manual_demo target_name source_file)
    add_executable(${target_name}
        ${source_file}
        ${PROJECT_SOURCE_DIR}/samples/manual/manual_demo.c
    )

    target_include_directories(${target_name}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
            ${PROJECT_SOURCE_DIR}/samples/manual
    )

    target_link_libraries(${target_name} PRIVATE vdi)

    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
    )

    target_compile_features(${target_name} PRIVATE c_std_11)

    target_compile_options(${target_name}
        PRIVATE
            -Wall
            -Wextra
            -pedantic
    )

    target_compile_options(${target_name}
        PRIVATE
            $<$<CONFIG:Debug>:-g>
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )

    target_link_options(${target_name}
        PRIVATE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
endfunction()

add_manual_demo(demo4 ${PROJECT_SOURCE_DIR}/samples/demo4/main.c)
add_manual_demo(demo5 ${PROJECT_SOURCE_DIR}/samples/demo5/main.c)
add_manual_demo(demo6 ${PROJECT_SOURCE_DIR}/samples/demo6/main.c)
add_manual_demo(demo7 ${PROJECT_SOURCE_DIR}/samples/demo7/main.c)
add_manual_demo(demo8 ${PROJECT_SOURCE_DIR}/samples/demo8/main.c)
add_manual_demo(demo9 ${PROJECT_SOURCE_DIR}/samples/demo9/main.c)
add_manual_demo(demo10 ${PROJECT_SOURCE_DIR}/samples/demo10/main.c)
add_manual_demo(demo11 ${PROJECT_SOURCE_DIR}/samples/demo11/main.c)
add_manual_demo(demo12 ${PROJECT_SOURCE_DIR}/samples/demo12/main.c)
add_manual_demo(demo13 ${PROJECT_SOURCE_DIR}/samples/demo13/main.c)
add_manual_demo(demo14 ${PROJECT_SOURCE_DIR}/samples/demo14/main.c)
add_manual_demo(demo15 ${PROJECT_SOURCE_DIR}/samples/demo15/main.c)
add_manual_demo(demo16 ${PROJECT_SOURCE_DIR}/samples/demo16/main.c)
