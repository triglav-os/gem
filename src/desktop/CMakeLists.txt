add_executable(desktop
    desktop_assets.c
    main.c
)

target_include_directories(desktop
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/gem
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(desktop PRIVATE gem)

set_target_properties(desktop PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${GEM_CORE_DIR}
)

target_compile_features(desktop PRIVATE c_std_11)

target_compile_options(desktop
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(desktop
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(desktop
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

add_executable(desktop_hosted
    desktop_assets.c
    main.c
)

target_include_directories(desktop_hosted
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/gem
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(desktop_hosted PRIVATE aes vdi)

set_target_properties(desktop_hosted PROPERTIES
    OUTPUT_NAME desktop_hosted
    RUNTIME_OUTPUT_DIRECTORY ${GEM_CORE_DIR}
)

target_compile_features(desktop_hosted PRIVATE c_std_11)

target_compile_options(desktop_hosted
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(desktop_hosted
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(desktop_hosted
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
