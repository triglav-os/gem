# Sample artwork is generated independently of the GEM runtime resources.
set(SAMPLE_RESOURCE_DIR "${GEM_SAMPLES_DIR}/data")
file(MAKE_DIRECTORY "${SAMPLE_RESOURCE_DIR}")
if(NOT TARGET resgen)
    find_program(GEM_RESGEN resgen HINTS "${GEM_SDK_ROOT}/bin" REQUIRED)
    add_executable(resgen IMPORTED GLOBAL)
    set_target_properties(resgen PROPERTIES IMPORTED_LOCATION "${GEM_RESGEN}")
endif()
add_custom_command(OUTPUT ${SAMPLE_RESOURCE_DIR}/desktop_icons.rsc
    COMMAND ${CMAKE_COMMAND} -E env
        ASAN_OPTIONS=detect_leaks=0
        LSAN_OPTIONS=detect_leaks=0
        $<TARGET_FILE:resgen> icons
        -o ${SAMPLE_RESOURCE_DIR}/desktop_icons.rsc
        ${SAMPLES_ROOT}/data/artwork/icons/DISK.PNG
        ${SAMPLES_ROOT}/data/artwork/icons/DOC.PNG
        ${SAMPLES_ROOT}/data/artwork/icons/FOLDER.PNG
        ${SAMPLES_ROOT}/data/artwork/icons/PRG.PNG
        ${SAMPLES_ROOT}/data/artwork/icons/TRASH.PNG
    DEPENDS resgen ${SAMPLES_ROOT}/data/artwork/icons/DISK.PNG ${SAMPLES_ROOT}/data/artwork/icons/DOC.PNG ${SAMPLES_ROOT}/data/artwork/icons/FOLDER.PNG ${SAMPLES_ROOT}/data/artwork/icons/PRG.PNG ${SAMPLES_ROOT}/data/artwork/icons/TRASH.PNG
    COMMENT "Generating GEM desktop icon resource"
    VERBATIM)
add_custom_target(gem_runtime_desktop_icons ALL DEPENDS ${SAMPLE_RESOURCE_DIR}/desktop_icons.rsc)

set(clock_number_sources
    ${SAMPLES_ROOT}/data/artwork/clock/01.png
    ${SAMPLES_ROOT}/data/artwork/clock/02.png
    ${SAMPLES_ROOT}/data/artwork/clock/03.png
    ${SAMPLES_ROOT}/data/artwork/clock/04.png
    ${SAMPLES_ROOT}/data/artwork/clock/05.png
    ${SAMPLES_ROOT}/data/artwork/clock/06.png
    ${SAMPLES_ROOT}/data/artwork/clock/07.png
    ${SAMPLES_ROOT}/data/artwork/clock/08.png
    ${SAMPLES_ROOT}/data/artwork/clock/09.png
    ${SAMPLES_ROOT}/data/artwork/clock/10.png
    ${SAMPLES_ROOT}/data/artwork/clock/11.png
    ${SAMPLES_ROOT}/data/artwork/clock/12.png
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

add_custom_command(OUTPUT ${SAMPLE_RESOURCE_DIR}/clock_numbers.rsc
    COMMAND ${CMAKE_COMMAND} -E env
        ASAN_OPTIONS=detect_leaks=0
        LSAN_OPTIONS=detect_leaks=0
        $<TARGET_FILE:resgen> bitmaps
        -o ${SAMPLE_RESOURCE_DIR}/clock_numbers.rsc
        ${clock_number_icons}
    DEPENDS resgen ${clock_number_icons}
    COMMENT "Generating GEM clock number resource"
    VERBATIM)
add_custom_target(gem_runtime_clock_numbers ALL DEPENDS ${SAMPLE_RESOURCE_DIR}/clock_numbers.rsc)

set(gemscape_toolbar_sources
    ${SAMPLES_ROOT}/data/artwork/toolbars/BACK.PNG
    ${SAMPLES_ROOT}/data/artwork/toolbars/FORE.PNG
    ${SAMPLES_ROOT}/data/artwork/toolbars/RELOAD.PNG
    ${SAMPLES_ROOT}/data/artwork/toolbars/MENU.PNG
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

add_custom_command(OUTPUT ${SAMPLE_RESOURCE_DIR}/gemscape_toolbar.rsc
    COMMAND ${CMAKE_COMMAND} -E env
        ASAN_OPTIONS=detect_leaks=0
        LSAN_OPTIONS=detect_leaks=0
        $<TARGET_FILE:resgen> bitmaps
        -o ${SAMPLE_RESOURCE_DIR}/gemscape_toolbar.rsc
        ${gemscape_toolbar_icons}
    DEPENDS resgen ${gemscape_toolbar_icons}
    COMMENT "Generating Gemscape toolbar resource"
    VERBATIM)
add_custom_target(gem_runtime_gemscape_toolbar ALL DEPENDS ${SAMPLE_RESOURCE_DIR}/gemscape_toolbar.rsc)

set(maestro_tree_sources
    ${SAMPLES_ROOT}/data/artwork/tree/FOLDER.PNG
    ${SAMPLES_ROOT}/data/artwork/tree/DOCUMENT.PNG
    ${SAMPLES_ROOT}/data/artwork/tree/EXPAND.PNG
    ${SAMPLES_ROOT}/data/artwork/tree/COLLAPSE.PNG
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

add_custom_command(OUTPUT ${SAMPLE_RESOURCE_DIR}/maestro_tree.rsc
    COMMAND ${CMAKE_COMMAND} -E env
        ASAN_OPTIONS=detect_leaks=0
        LSAN_OPTIONS=detect_leaks=0
        $<TARGET_FILE:resgen> bitmaps
        -o ${SAMPLE_RESOURCE_DIR}/maestro_tree.rsc
        ${maestro_tree_icons}
    DEPENDS resgen ${maestro_tree_icons}
    COMMENT "Generating Maestro tree resource"
    VERBATIM)
add_custom_target(gem_runtime_maestro_tree ALL DEPENDS ${SAMPLE_RESOURCE_DIR}/maestro_tree.rsc)
