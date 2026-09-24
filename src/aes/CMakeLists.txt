add_library(aes SHARED
    system_menu.c
    alert_icons.c
    aes_core.c
    aes_shel.c
    desktop_inf_codec.c
    desktop_inf_config.c
    aes_files.c
    aes_geometry.c
    menu_internal.c
    menu_layout.c
    menu_track.c
    window_geometry.c
    window_redraw.c
    window_chrome.c
    paint.c
    object_parts.c
    object_draw.c
    aes.c
    event.c
    window_track.c
    window_drag.c
    file_selector.c
    fsel_snapshot.c
    fsel_paths.c
    fsel_navigation.c
    fsel_layout.c
    fsel_draw.c
    menu.c
    alert.c
    form.c
    form_edit.c
    resource.c
    window.c
    window_visible.c
)

target_include_directories(aes
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(aes PUBLIC vdi gemctl PRIVATE gemconf)

target_compile_features(aes PUBLIC c_std_11)

target_compile_options(aes
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(aes
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
