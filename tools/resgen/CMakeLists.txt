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
        -t ab
        ${PROJECT_SOURCE_DIR}/docs/orig/cursors/POINTER.PNG
        ${PROJECT_SOURCE_DIR}/docs/orig/cursors/BEE.PNG
        -o ${PROJECT_BINARY_DIR}/cursors.rsc
        -o ${PROJECT_SOURCE_DIR}/bin/cursors.rsc
    DEPENDS resgen
    COMMENT "Generating GEM runtime cursor resource"
)

add_custom_target(gem_runtime_desktop_icons ALL
    COMMAND ${CMAKE_COMMAND} -E env
        ASAN_OPTIONS=detect_leaks=0
        LSAN_OPTIONS=detect_leaks=0
        $<TARGET_FILE:resgen> icons
        -o ${PROJECT_BINARY_DIR}/desktop_icons.rsc
        -o ${PROJECT_SOURCE_DIR}/bin/desktop_icons.rsc
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
        -o ${PROJECT_SOURCE_DIR}/bin/alert_icons.rsc
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
        -o ${PROJECT_SOURCE_DIR}/bin/clock_numbers.rsc
        ${clock_number_icons}
    DEPENDS resgen ${clock_number_icons}
    COMMENT "Generating GEM clock number resource"
)
