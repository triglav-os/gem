add_executable(menu_demo
    main.c
)

target_include_directories(menu_demo
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(menu_demo PRIVATE gem)

set_target_properties(menu_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
)

target_compile_features(menu_demo PRIVATE c_std_11)

target_compile_options(menu_demo
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(menu_demo
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(menu_demo
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
