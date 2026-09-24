add_executable(desktop
    desktop_assets.c
    main.c
    browser.c
    browser_launch.c
    browser_drop.c
    browser_list.c
    browser_view.c
    gem_trash.c
    trash_ui.c
    disks.c
    icons.c
    desk_redraw.c
)

target_include_directories(desktop
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src/gem
)

target_link_libraries(desktop PRIVATE gem gemconf libstout)

set_target_properties(desktop PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${GEM_APPS_DIR}
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
    browser.c
    browser_launch.c
    browser_drop.c
    browser_list.c
    browser_view.c
    gem_trash.c
    trash_ui.c
    disks.c
    icons.c
    desk_redraw.c
)

target_include_directories(desktop_hosted
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(desktop_hosted PRIVATE aes vdi gemconf libstout)
target_compile_definitions(desktop_hosted PRIVATE GEM_DESKTOP_HOSTED=1)

set_target_properties(desktop_hosted PROPERTIES
    OUTPUT_NAME desktop_hosted
    RUNTIME_OUTPUT_DIRECTORY ${GEM_APPS_DIR}
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
