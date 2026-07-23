add_executable(resgen resgen.c)

set_target_properties(resgen PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
)

target_include_directories(resgen
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(resgen PRIVATE c_std_11)

target_compile_options(resgen
    PRIVATE
        -Wall
        -Wextra
        -pedantic
)

target_compile_options(resgen
    PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(resgen
    PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

add_custom_target(gem_runtime_cursors ALL
    COMMAND ${CMAKE_COMMAND} -E env
        ASAN_OPTIONS=detect_leaks=0
        LSAN_OPTIONS=detect_leaks=0
        $<TARGET_FILE:resgen> cursors
        -t 01234567
        ${PROJECT_SOURCE_DIR}/docs/orig/cursors/POINTER.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/cursors/IBEAM.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/cursors/BEE.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/cursors/POINTHAND.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/cursors/FLATHAND.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/cursors/THINCROSS.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/cursors/THICKCROSS.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/cursors/OUTLINECROSS.PNG
        -o ${PROJECT_BINARY_DIR}/cursors.rsc
        -o ${GEM_RESOURCES_DIR}/cursors.rsc
    DEPENDS
        resgen
        ${PROJECT_SOURCE_DIR}/docs/orig/cursors/POINTER.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/cursors/IBEAM.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/cursors/BEE.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/cursors/POINTHAND.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/cursors/FLATHAND.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/cursors/THINCROSS.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/cursors/THICKCROSS.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/cursors/OUTLINECROSS.PNG
    COMMENT "Generating GEM runtime cursor resource"
)

add_custom_target(gem_runtime_desktop_icons ALL
    COMMAND ${CMAKE_COMMAND} -E env
        ASAN_OPTIONS=detect_leaks=0
        LSAN_OPTIONS=detect_leaks=0
        $<TARGET_FILE:resgen> icons
        -o ${PROJECT_BINARY_DIR}/desktop_icons.rsc
        -o ${GEM_RESOURCES_DIR}/desktop_icons.rsc
        ${PROJECT_SOURCE_DIR}/docs/orig/icons/DISK.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/icons/DOC.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/icons/FOLDER.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/icons/PRG.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/icons/TRASH.PNG
    DEPENDS resgen
    COMMENT "Generating GEM desktop icon resource"
)

add_custom_target(gem_runtime_alert_icons ALL
    COMMAND ${CMAKE_COMMAND} -E env
        ASAN_OPTIONS=detect_leaks=0
        LSAN_OPTIONS=detect_leaks=0
        $<TARGET_FILE:resgen> icons
        -o ${PROJECT_BINARY_DIR}/alert_icons.rsc
        -o ${GEM_RESOURCES_DIR}/alert_icons.rsc
        ${PROJECT_SOURCE_DIR}/docs/orig/icons/EXCLAMATION.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/icons/QUESTION.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/icons/STOP.PNG
    DEPENDS resgen
    COMMENT "Generating GEM alert icon resource"
)

set(clock_number_sources
    ${PROJECT_SOURCE_DIR}/docs/orig/clock/01.png
    ${PROJECT_SOURCE_DIR}/docs/orig/clock/02.png
    ${PROJECT_SOURCE_DIR}/docs/orig/clock/03.png
    ${PROJECT_SOURCE_DIR}/docs/orig/clock/04.png
    ${PROJECT_SOURCE_DIR}/docs/orig/clock/05.png
    ${PROJECT_SOURCE_DIR}/docs/orig/clock/06.png
    ${PROJECT_SOURCE_DIR}/docs/orig/clock/07.png
    ${PROJECT_SOURCE_DIR}/docs/orig/clock/08.png
    ${PROJECT_SOURCE_DIR}/docs/orig/clock/09.png
    ${PROJECT_SOURCE_DIR}/docs/orig/clock/10.png
    ${PROJECT_SOURCE_DIR}/docs/orig/clock/11.png
    ${PROJECT_SOURCE_DIR}/docs/orig/clock/12.png
)

set(clock_number_icons)
foreach(clock_number_source IN LISTS clock_number_sources)
    get_filename_component(clock_number_name ${clock_number_source} NAME)
    set(clock_number_icon
        ${PROJECT_BINARY_DIR}/clock_number_icons/${clock_number_name})
    add_custom_command(
        OUTPUT ${clock_number_icon}
        COMMAND ${CMAKE_COMMAND} -E make_directory
            ${PROJECT_BINARY_DIR}/clock_number_icons
        COMMAND convert ${clock_number_source}
            -background none
            -gravity center
            -extent 48x32
            PNG32:${clock_number_icon}
        DEPENDS ${clock_number_source}
        COMMENT "Padding clock numeral icon ${clock_number_name}"
    )
    list(APPEND clock_number_icons ${clock_number_icon})
endforeach()

add_custom_target(gem_runtime_clock_numbers ALL
    COMMAND ${CMAKE_COMMAND} -E env
        ASAN_OPTIONS=detect_leaks=0
        LSAN_OPTIONS=detect_leaks=0
        $<TARGET_FILE:resgen> bitmaps
        -o ${PROJECT_BINARY_DIR}/clock_numbers.rsc
        -o ${GEM_RESOURCES_DIR}/clock_numbers.rsc
        ${clock_number_icons}
    DEPENDS resgen ${clock_number_icons}
    COMMENT "Generating GEM clock number resource"
)

set(gemscape_toolbar_sources
    ${PROJECT_SOURCE_DIR}/docs/orig/toolbars/BACK.PNG
    ${PROJECT_SOURCE_DIR}/docs/orig/toolbars/FORE.PNG
    ${PROJECT_SOURCE_DIR}/docs/orig/toolbars/RELOAD.PNG
    ${PROJECT_SOURCE_DIR}/docs/orig/toolbars/MENU.PNG
)

set(gemscape_toolbar_icons)
foreach(gemscape_toolbar_source IN LISTS gemscape_toolbar_sources)
    get_filename_component(gemscape_toolbar_name
        ${gemscape_toolbar_source} NAME)
    set(gemscape_toolbar_icon
        ${PROJECT_BINARY_DIR}/gemscape_toolbar_icons/${gemscape_toolbar_name})
    add_custom_command(
        OUTPUT ${gemscape_toolbar_icon}
        COMMAND ${CMAKE_COMMAND} -E make_directory
            ${PROJECT_BINARY_DIR}/gemscape_toolbar_icons
        COMMAND convert ${gemscape_toolbar_source}
            -background white
            -alpha remove
            -alpha off
            PNG32:${gemscape_toolbar_icon}
        DEPENDS ${gemscape_toolbar_source}
        COMMENT "Flattening Gemscape toolbar icon ${gemscape_toolbar_name}"
    )
    list(APPEND gemscape_toolbar_icons ${gemscape_toolbar_icon})
endforeach()

add_custom_target(gem_runtime_gemscape_toolbar ALL
    COMMAND ${CMAKE_COMMAND} -E env
        ASAN_OPTIONS=detect_leaks=0
        LSAN_OPTIONS=detect_leaks=0
        $<TARGET_FILE:resgen> bitmaps
        -o ${PROJECT_BINARY_DIR}/gemscape_toolbar.rsc
        -o ${GEM_RESOURCES_DIR}/gemscape_toolbar.rsc
        ${gemscape_toolbar_icons}
    DEPENDS resgen ${gemscape_toolbar_icons}
    COMMENT "Generating Gemscape toolbar resource"
)

set(maestro_tree_sources
    ${PROJECT_SOURCE_DIR}/docs/orig/tree/FOLDER.PNG
    ${PROJECT_SOURCE_DIR}/docs/orig/tree/DOCUMENT.PNG
    ${PROJECT_SOURCE_DIR}/docs/orig/tree/EXPAND.PNG
    ${PROJECT_SOURCE_DIR}/docs/orig/tree/COLLAPSE.PNG
)

set(maestro_tree_icons)
foreach(maestro_tree_source IN LISTS maestro_tree_sources)
    get_filename_component(maestro_tree_name ${maestro_tree_source} NAME)
    set(maestro_tree_icon
        ${PROJECT_BINARY_DIR}/maestro_tree_icons/${maestro_tree_name})
    add_custom_command(
        OUTPUT ${maestro_tree_icon}
        COMMAND ${CMAKE_COMMAND} -E make_directory
            ${PROJECT_BINARY_DIR}/maestro_tree_icons
        COMMAND convert ${maestro_tree_source}
            -background white
            -alpha remove
            -alpha off
            PNG32:${maestro_tree_icon}
        DEPENDS ${maestro_tree_source}
        COMMENT "Flattening Maestro tree icon ${maestro_tree_name}"
    )
    list(APPEND maestro_tree_icons ${maestro_tree_icon})
endforeach()

add_custom_target(gem_runtime_maestro_tree ALL
    COMMAND ${CMAKE_COMMAND} -E env
        ASAN_OPTIONS=detect_leaks=0
        LSAN_OPTIONS=detect_leaks=0
        $<TARGET_FILE:resgen> bitmaps
        -o ${PROJECT_BINARY_DIR}/maestro_tree.rsc
        -o ${GEM_RESOURCES_DIR}/maestro_tree.rsc
        ${maestro_tree_icons}
    DEPENDS resgen ${maestro_tree_icons}
    COMMENT "Generating Maestro tree resource"
)
